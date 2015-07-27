/* The MIT License (MIT)
 *
 * Copyright (c) 2015 Stefano Trettel
 *
 * Software repository: LuaJack, https://github.com/stetre/luajack
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

/****************************************************************************
 * Creating and managing client threads										*
 ****************************************************************************/

#include "internal.h"
#include <jack/thread.h>


/* tud->status codes */
#define TUD_CONFIGURING	0
#define TUD_READY		1
#define TUD_RUNNING		2
#define TUD_DONE		3
#define TUD_FAILED		4

static pthread_key_t key_tud; /* only the client's threads have data bound to this key */

static tud_t* thread_tud(void)
/* retrieves the tud for the current thread, or null if not called within a client thread 
 */
	{ 
	return (tud_t*)pthread_getspecific(key_tud);
	}

#if 0
static int PushStatus(lua_State *L, tud_t *tud)
	{
	switch(tud->status)
		{
		case TUD_CONFIGURING:
		case TUD_READY: lua_pushstring(L, "starting"); return 1;
		case TUD_RUNNING: lua_pushstring(L, "running"); return 1;
		case TUD_DONE: lua_pushstring(L, "done"); return 1;
		case TUD_FAILED: lua_pushstring(L, "failed"); return 1;
		}
	return luaL_error(L, UNEXPECTED_ERROR);
	}

static int ThreadStatus(lua_State *L)
	{
	cud_t *cud = cud_check(L, 1);
	tud_t *tud = tud_check(L, 2);
	if(tud->cud != cud)
		return luaL_error(L, "thread is not owned by this client");
	return PushStatus(L, tud);
	}
#endif

/*--------------------------------------------------------------------------*
 | Thread function                              		            		|
 *--------------------------------------------------------------------------*/

static void* ThreadFunc(void *arg)
	{
#define tud ((tud_t*)arg)
#define T tud->state
	int nargs;
	lua_State *TT;

	pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, NULL);

	luajack_sigblock();

	pthread_mutex_lock(&(tud->lock));
	pthread_setspecific(key_tud, tud);

	while(tud->status == TUD_CONFIGURING) 
		{
		/* do nothing until everything is ready for the script to be executed */
		}

	nargs = lua_gettop(T) - 1;

	tud->status = TUD_RUNNING;
	if(lua_pcall(T, nargs, 0 , 0) != LUA_OK)
		{
		/* An error occurred when executing the thread script.
		 * Call luajack_error() so to allow the detection of the failure
		 * from the main thread, and let this thread exit normally.
		 */
		tud->status = TUD_FAILED;
		TT = T; T = NULL; /* mark as already closed */
		luajack_error(lua_tostring(TT, -1));
		}
	else
		{
		/* no return values: use ringbuffers to communicate between threads */
		tud->status = TUD_DONE; /* thread exited normally */
		TT = T; T = NULL; /* mark as already closed */
		lua_close(TT);
		}

	pthread_mutex_unlock(&(tud->lock));
	return NULL; /* the client will join the thread at its closure */
#undef T
#undef tud
	}

/*--------------------------------------------------------------------------*
 | luajack functions                             		            		|
 *--------------------------------------------------------------------------*/

static int ThreadCreate_(lua_State *L, int isscript)
	{
	tud_t *tud;
	lua_State *T;
	int rc;
	int chunk_index, nlast;
	cud_t *cud;


	luajack_checkcreate();

	cud = cud_check(L, 1);
	chunk_index = 2;

	if(lua_type(L, chunk_index) != LUA_TSTRING)
		luaL_error(L, "missing thread script");

	/* create the thread's state (unrelated to the client state) */
	if((T = luajack_newstate(L, ST_THREAD, NULL, NULL)) == NULL)
		return luaL_error(L, "cannot create Lua state for thread");

	nlast = lua_gettop(L); /* last optional argument */

	/* load the Lua code */
	luajack_loadchunk(T, L, chunk_index, isscript);

	/* copy script and arguments on the thread's Lua state */
	luajack_xmove(T, L, chunk_index, nlast);

	if((tud = tud_new()) == NULL)
		{
		lua_close(T);
		return luaL_error(L, "cannot create userdata for thread");
		}
	tud->cud = cud;
	tud->state = T;

	/* create lock and condition variable */
	if(pthread_mutex_init(&(tud->lock), NULL) != 0)
		{
		lua_close(T);
		return luaL_error(L, "cannot initialize mutex");
		}
	if(pthread_cond_init(&(tud->cond), NULL) != 0)
		{
		pthread_mutex_destroy(&(tud->lock));
		lua_close(T);
		return luaL_error(L, "cannot initialize condition");
		}

	/* create thread */
	tud->status = TUD_CONFIGURING;
	rc = jack_client_create_thread(cud->client, &(tud->thread), 0, 0, ThreadFunc, (void*)tud);
	if(rc)
		{
		pthread_mutex_destroy(&(tud->lock));
		pthread_cond_destroy(&(tud->cond));
		lua_close(T);
		return luaL_error(L, "jack_client_create_thread returned %d", rc);
		}

	DBG("new thread: tud=%p, thread=%d\n", (void*)tud,tud->key);
	luajack_verbose("created client thread %u\n", tud->key);

	tud->status = TUD_READY; /* now ThreadFunc() can finally execute the script */		
	lua_pushinteger(L, tud->key);
	return 1;
	}	

static int ThreadLoadfile(lua_State *L)
	{ return ThreadCreate_(L, 1); }

static int ThreadLoad(lua_State *L)
	{ return ThreadCreate_(L, 0); }


static int RealTimePriority(lua_State *L)
	{
	cud_t *cud = cud_check(L, 1);
	int priority = jack_client_real_time_priority(cud->client);
	if(priority != -1)
		lua_pushinteger(L, priority);
	else
		lua_pushnil(L);
	return 1;
	}

static int MaxRealTimePriority(lua_State *L)
	{
	cud_t *cud = cud_check(L, 1);
	int priority = jack_client_max_real_time_priority(cud->client);
	if(priority != -1)
		lua_pushinteger(L, priority);
	else
		lua_pushnil(L);
	return 1;
	}

static int AcquireRealTimeScheduling(lua_State *L)
	{
	int priority, rc;
	priority = luaL_checkinteger(L, 1);
	rc = jack_acquire_real_time_scheduling((jack_native_thread_t)pthread_self(), priority);
		/*@@ who manages the garbage collector ? */
	if(rc!=0) 
		return luajack_strerror(L, rc);
	return 0;
	}

static int DropRealTimeScheduling(lua_State *L)
	{
	int rc;
	rc = jack_drop_real_time_scheduling((jack_native_thread_t)pthread_self());
	if(rc!=0) 
		return luajack_strerror(L, rc);
	return 0;
	}

int thread_signal(cud_t *cud, tud_t *tud)
	{
	tud_t *current_tud = thread_tud();
	if(current_tud && !IsTudValid(current_tud))
		return 0;
	if(current_tud && (current_tud == tud))
		return luajack_error("thread can not signal() to itself");
	if(tud->cud != cud)
		return luajack_error("thread is not owned by this client");

	if(tud->status == TUD_RUNNING)
		{
		if(pthread_mutex_trylock(&(tud->lock)) == 0)
			{
			pthread_cond_signal(&(tud->cond));
			pthread_mutex_unlock(&(tud->lock));
			}
		}
	return 0;
	}

static int Signal(lua_State *L)
	{
	cud_t *cud = cud_check(L, 1);
	tud_t *tud = tud_check(L, 2);
	tud_t *current_tud = thread_tud();
	if(current_tud && !IsTudValid(current_tud))
		return 0;
	if(current_tud && (current_tud == tud))
		return luaL_error(L, "thread can not signal() to itself");
	if(tud->cud != cud)
		return luaL_error(L, "thread is not owned by this client");

	if(tud->status == TUD_RUNNING)
		{
		if(pthread_mutex_trylock(&(tud->lock)) == 0)
			{
			pthread_cond_signal(&(tud->cond));
			pthread_mutex_unlock(&(tud->lock));
			}
		}
	return 0;
	}

static int Wait(lua_State *T) /* thread only */
	{
	tud_t *tud = thread_tud();
	if(!tud || !IsTudValid(tud))
		return luaL_error(T, UNEXPECTED_ERROR);
	pthread_cond_wait(&(tud->cond), &(tud->lock));
	return 0;
	}

static int Self(lua_State *T) /* thread only */ 
/* returns client reference and thread reference */
	{
	tud_t *tud = thread_tud();
	if(!tud || !IsTudValid(tud))
		return luaL_error(T, UNEXPECTED_ERROR);
	lua_pushinteger(T, tud->cud->key);
	lua_pushinteger(T, tud->key);
	return 2;
	}

static int Sleep(lua_State *T) /* thread only */
	{
#if 1 /* with nanosleep() */
	int rc;
	double seconds;
	struct timespec ts, ts1;
	struct timespec *req, *rem, *tmp;

	seconds = luaL_optnumber(T, 1, -1);

	if(seconds<=0)
		{
		/* endless loop */
		luajack_sectots(&ts, 60);
		while(1)
			nanosleep(&ts, NULL);
		}
	else
		{
		luajack_sectots(&ts, seconds);
		req = &ts; rem = &ts1;
		while(1)
			{
			if((rc = nanosleep(req, rem)) == 0) return 0;
			tmp = req; req = rem; rem = tmp;
			}
		}
	return 0;
#else /* with usleep() */
	double seconds;
	seconds = luaL_optnumber(T, 1, -1);
	if(seconds<=0)
		{
		/* endless loop */
		while(1)
			usleep((useconds_t)-1);
		}
	else
		usleep((useconds_t)(seconds*1.0e6));
	return 0;
#endif
	}


/*--------------------------------------------------------------------------*
 | Registration                              								|
 *--------------------------------------------------------------------------*/

static void thread_free(tud_t *tud)
/* To be called from the parent's state. */
	{
	luajack_verbose("closing client thread %u\n", tud->key);
	pthread_cancel(tud->thread);
	pthread_join(tud->thread, NULL);
	if(tud->state) lua_close(tud->state);
	pthread_mutex_destroy(&(tud->lock));
	pthread_cond_destroy(&(tud->cond));
	CancelTudValid(tud);
	}

void thread_free_all(cud_t *cud)
	{
	tud_t *tud = tud_first(0);
	while(tud)
		{
		if(IsTudValid(tud) && (tud->cud == cud)) /* belongs to this client */
			thread_free(tud);
		tud = tud_next(tud);
		}
	}

#define COMMON_FUNCTIONS \
		{ "max_real_time_priority", MaxRealTimePriority },		\
		{ "real_time_priority", RealTimePriority },				\
		{ "acquire_real_time_scheduling", AcquireRealTimeScheduling },	\
		{ "drop_real_time_scheduling", DropRealTimeScheduling }

static const struct luaL_Reg TFunctions [] = 
	{
		{ "self", Self },
		{ "sleep", Sleep },
		{ "signal", Signal },
		{ "wait", Wait },
		COMMON_FUNCTIONS,
		{ NULL, NULL } /* sentinel */
	};


static const struct luaL_Reg MFunctions[] = 
	{
		{ "thread_loadfile", ThreadLoadfile },
		{ "thread_load", ThreadLoad },
		{ "signal", Signal },
		COMMON_FUNCTIONS,
		{ NULL, NULL } /* sentinel */
	};

static const struct luaL_Reg PFunctions[] = 
	{
		{ "signal", Signal },
		COMMON_FUNCTIONS,
		{ NULL, NULL } /* sentinel */
	};


int luajack_open_thread(lua_State *L, int state_type)
	{
	int rc;
	switch(state_type)
		{
		case ST_MAIN: 
			if((rc = pthread_key_create(&key_tud, NULL)) != 0)
				return luaL_error(L, "pthread_key_create returned %d", rc);
			luaL_setfuncs(L, MFunctions, 0);
			break;
		case ST_PROCESS: luaL_setfuncs(L, PFunctions, 0); break;
		case ST_THREAD: luaL_setfuncs(L, TFunctions, 0); break;
		default:
			break;
		}
	return 1;
	}

