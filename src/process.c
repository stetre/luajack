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
 * Real-time callbacks														*
 ****************************************************************************/

#include "internal.h"
#include <jack/thread.h>

/*--------------------------------------------------------------------------*
 | Process context creation                     		            		|
 *--------------------------------------------------------------------------*/

static int ProcessLoad_(lua_State *L, int isscript)
/* Creates the process_state, loads and executes the process chunk in it */
	{
#define P cud->process_state
	int nargs, last_index, chunk_index = 2;
	cud_t *cud = cud_check(L, 1);

	luajack_checkmain();

	if(P)
		luaL_error(L, "process chunk already loaded");

	if(lua_type(L, chunk_index) != LUA_TSTRING)
		luaL_error(L, "missing process chunk");

	/* create the process_state (unrelated to the client state) */
	if((P = luajack_newstate(L, ST_PROCESS, NULL, NULL)) == NULL)
		return luaL_error(L, "cannot create Lua state");

#if 0
	/* store the cud on the registry */
	lua_pushlightuserdata(P, (void*)cud);
	lua_setfield(P, LUA_REGISTRYINDEX, "luajack_cud");
#endif

	last_index = lua_gettop(L); /* last optional argument */
	
	/* load the Lua code */
	luajack_loadchunk(P, L, chunk_index, isscript);

	/* copy script and arguments on the process_state */
	luajack_xmove(P, L, chunk_index, last_index);

	nargs = lua_gettop(P) - 1;

	/* execute the script (note that we still are in the main thread) */
	if(lua_pcall(P, nargs, 0 , 0) != LUA_OK)
		return luaL_error(L, lua_tostring(P, -1));
	
	/* since we still are in a non-rt thread, do a complete garbage collection,
	 * then stop; garbage collection in the rt-thread will be made at indivisible
	 * steps, one step at the end of each callback */
	lua_gc(P, LUA_GCCOLLECT, 0);
	lua_gc(P, LUA_GCSTOP, 0);
#undef P
	return 0;
	}

static int ProcessLoadfile(lua_State *L)
	{ return ProcessLoad_(L, 1); }

static int ProcessLoad(lua_State *L)
	{ return ProcessLoad_(L, 0); }


/*--------------------------------------------------------------------------*
 | Callbacks                                    		            		|
 *--------------------------------------------------------------------------*/

#define cud ((cud_t*)arg)
#define P cud->process_state

#define BEGIN(cb) 														\
	double ts=0;														\
do {																	\
	if(IsCudProfile(cud)) ts = luajack_now();							\
	if(luajack_exiting()) return 0; 									\
	if(!IsCudValid(cud)) return 0;										\
	/* if(!P) return luajack_error("2 "UNEXPECTED_ERROR); */			\
	/* if(cud->cb == LUA_NOREF) return luajack_error("3 "UNEXPECTED_ERROR); */	\
	/* push the callback on the stack */								\
	if(lua_rawgeti(P, LUA_REGISTRYINDEX, cud->cb) != LUA_TFUNCTION)		\
		return luajack_error("4 "UNEXPECTED_ERROR); 					\
} while(0);

#define EXEC(nargs, nres) do {											\
	/* execute the script code */										\
	if(lua_pcall(P, (nargs) , (nres), 0) != LUA_OK)						\
		return luajack_error(lua_tostring(P, -1));						\
} while(0)

#define END(rc_, gcwhat) do { 											\
	lua_gc(P, (gcwhat), 0);												\
	if(ts!=0) /* if(IsCudProfile(cud))	*/								\
		luajack_stat_update(&(cud->stat), luajack_since(ts));			\
	return (rc_);														\
} while(0)

static int 	Process(nframes_t nframes, void *arg)
	{
	BEGIN(Process);
	MarkProcessCallback(cud);
	cud->buffer_size = cud->nframes = nframes;
	lua_pushinteger(P, nframes);
	EXEC(1, 0);
	buffer_drop_all(cud);
	cud->nframes = 0;
	CancelProcessCallback(cud);
	END(0, LUA_GCSTEP);
	}

static int BufferSize(nframes_t nframes, void *arg)
	{
	/* the normal process cycle is suspended during this callback,
	 * which may perform non real-time safe operations */
	BEGIN(BufferSize);
	cud->buffer_size = nframes;
	lua_pushinteger(P, nframes);
	EXEC(1, 0);
	END(0, LUA_GCCOLLECT); 
	}

static int	Sync(jack_transport_state_t state, jack_position_t *pos, void *arg)
	{
	int rc;
	BEGIN(Sync);
	transport_pushstate(P, state);
	transport_pushposition(P, pos);
	EXEC(2, 1);
	rc = lua_toboolean(P, -1);
	END(rc, LUA_GCSTEP);
	}

static int Timebase_(jack_transport_state_t state, jack_nframes_t nframes,
						jack_position_t *pos, int new_pos, void *arg)
	{
	BEGIN(Timebase);
	transport_pushstate(P, state);
	lua_pushinteger(P, nframes);
	transport_pushposition(P, pos);
	lua_pushboolean(P, new_pos);
	EXEC(4, 0);
	END(0, LUA_GCSTEP);
	}

static void	Timebase(jack_transport_state_t state, jack_nframes_t nframes,
						jack_position_t *pos, int new_pos, void *arg)
	{ Timebase_(state,nframes,pos,new_pos,arg); }

#define TimebaseConditional Timebase

#undef cud
#undef P
#undef BEGIN
#undef EXEC
#undef END

/*--------------------------------------------------------------------------*
 | Callbacks registration                          		            		|
 *--------------------------------------------------------------------------*/

#define SetProcess jack_set_process_callback
#define SetBufferSize jack_set_buffer_size_callback
#define SetSync	jack_set_sync_callback
static int SetTimebase(client_t *client, JackTimebaseCallback func, void *arg)
	{ return jack_set_timebase_callback(client, 0, func, arg); }
static int SetTimebaseConditional(client_t *client, JackTimebaseCallback func, void *arg)
	{ return jack_set_timebase_callback(client, 1, func, arg); }

/* P is cud->process_state, here */

#define Unregister(cud, cb) do {										\
	if((cud)->cb != LUA_NOREF) 											\
		{																\
		luaL_unref(cud->process_state, LUA_REGISTRYINDEX, (cud)->cb);	\
		(cud)->cb = LUA_NOREF;											\
		} 																\
} while(0)

#define Register(cud, cb, index) do {									\
	Unregister((cud), cb); 												\
	lua_pushvalue(P, (index)); /* the function */						\
	(cud)->cb = luaL_ref(P, LUA_REGISTRYINDEX);							\
	if(Set##cb((cud)->client, (cb), (void*)(cud)) != 0)					\
		{																\
		Unregister((cud), cb); 											\
		return luaL_error(P, "cannot register callback");				\
		}																\
} while(0)


#define CheckFunction()	do {											\
	if(!lua_isfunction(P, 2)) 											\
		return luaL_error(P, "bad argument #2 (function expected)");	\
} while(0)


static int ProcessCallback(lua_State *P) 
	{
	cud_t *cud = cud_check(P, 1);
	CheckFunction();
	Register(cud, Process, 2);
	return 0;
	}

static int BufferSizeCallback(lua_State *P)
	{
	cud_t *cud = cud_check(P, 1);
	CheckFunction();
	Register(cud, BufferSize, 2);
	return 0;
	}

static int SyncCallback(lua_State *P)
	{
	cud_t *cud = cud_check(P, 1);
	CheckFunction();
	Register(cud, Sync, 2);
	return 0;
	}


static int TimebaseCallback(lua_State *P)
	{
	int conditional;
	cud_t *cud = cud_check(P, 1);
	CheckFunction();
	conditional = lua_toboolean(P, 3);
	if(conditional)
		Register(cud, TimebaseConditional, 2);
	else
		Register(cud, Timebase, 2);
	return 0;
	}

static int ReleaseTimebase(lua_State *P)
	{
	int rc;
	cud_t *cud = cud_check(P, 1);
	rc = jack_release_timebase(cud->client);	
	Unregister(cud, Timebase);
	Unregister(cud, TimebaseConditional);
	if(rc!=0)
		return luajack_strerror(P, rc);
	return 0;
	}


/*--------------------------------------------------------------------------*
 | C callbacks 																|
 *--------------------------------------------------------------------------*/

#define cud ((cud_t*)arg)

#define BEGIN(cb) 												\
	double ts=0;												\
do {															\
	if(IsCudProfile(cud)) ts = luajack_now();					\
	if(luajack_exiting()) return 0; 							\
	if(!IsCudValid(cud)) return 0;								\
	/* if((cud->C##cb)==NULL)	return luajack_error("1 "UNEXPECTED_ERROR); */\
} while(0);

#define END(rc_) do { 											\
	if(IsCudProfile(cud)) 										\
		luajack_stat_update(&(cud->stat), luajack_since(ts));	\
	return (rc_);												\
} while(0)

static int 	CProcess(nframes_t nframes, void *arg)
	{
	int rc;
	BEGIN(Process);
	MarkProcessCallback(cud);
	cud->buffer_size = cud->nframes = nframes;
	rc = cud->CProcess(nframes, cud->CProcess_arg);
	if(rc!=0)
		return luajack_error("error in process() callback");
	buffer_drop_all(cud);
	cud->nframes = 0;
	CancelProcessCallback(cud);
	END(0);
	}

static int CBufferSize(nframes_t nframes, void *arg)
	{
	int rc;
	BEGIN(BufferSize)
	cud->buffer_size = nframes;
	rc = cud->CBufferSize(nframes, cud->CBufferSize_arg);
	if(rc!=0)
		return luajack_error("error in buffer_size() callback");
	END(0);
	}

static int	CSync(jack_transport_state_t state, jack_position_t *pos, void *arg)
	{
	int rc;
	BEGIN(Sync)
	rc = cud->CSync(state, pos, cud->CSync_arg);
	END(rc);
	}

static int CTimebase_(jack_transport_state_t state, jack_nframes_t nframes,
						jack_position_t *pos, int new_pos, void *arg)
	{
	BEGIN(Timebase)
	cud->CTimebase(state, nframes, pos, new_pos, cud->CTimebase_arg);
	END(0);
	}

static void	CTimebase(jack_transport_state_t state, jack_nframes_t nframes,
						jack_position_t *pos, int new_pos, void *arg)
	{	CTimebase_(state, nframes, pos, new_pos, arg); }

#undef cud
#undef BEGIN
#undef END

/*--------------------------------------------------------------------------*
 | C callbacks registration													|
 *--------------------------------------------------------------------------*/

int process_ccallback_process(cud_t *cud, JackProcessCallback cb, void *arg)
	{
	cud->CProcess = cb;
	cud->CProcess_arg = arg;
	return jack_set_process_callback(cud->client, CProcess, (void*)cud);
	}

int process_ccallback_buffer_size(cud_t *cud, JackBufferSizeCallback cb, void *arg)
	{
	cud->CBufferSize = cb;
	cud->CBufferSize_arg = arg;
	return jack_set_buffer_size_callback(cud->client, CBufferSize, (void*)cud);
	}

int process_ccallback_sync(cud_t *cud, JackSyncCallback cb, void *arg)
	{
	cud->CSync = cb;
	cud->CSync_arg = arg;
	return jack_set_sync_callback(cud->client, CSync, (void*)cud);
	}

int process_ccallback_timebase(cud_t *cud, int conditional,  JackTimebaseCallback cb, void *arg)
	{
	cud->CTimebase = cb;
	cud->CTimebase_arg = arg;
	return jack_set_timebase_callback(cud->client, conditional, CTimebase, (void*)cud);
	}

int process_ccallback_release_timebase(cud_t *cud)
	{
	int rc = jack_release_timebase(cud->client);
	cud->CTimebase = NULL;
	cud->CTimebase_arg = NULL;
	return rc;
	}


/*--------------------------------------------------------------------------*
 | Profiling                                 								|
 *--------------------------------------------------------------------------*/


static int Profile(lua_State *L)
	{
	const char* what;
	cud_t *cud = cud_check(L, 1);
	if(lua_isnoneornil(L, 2))
		{
		lua_pushinteger(L, luajack_stat_n(&(cud->stat)));
		lua_pushnumber(L, luajack_stat_min(&(cud->stat)));
		lua_pushnumber(L, luajack_stat_max(&(cud->stat)));
		lua_pushnumber(L, luajack_stat_mean(&(cud->stat)));
		lua_pushnumber(L, luajack_stat_variance(&(cud->stat)));
		return 5;
		}
	what = luaL_checkstring(L, 2);
	if(strncmp(what, "start", strlen(what)) == 0)
		{
		luajack_stat_reset(&(cud->stat));
		MarkCudProfile(cud);
		}
	else if(strncmp(what, "restart", strlen(what)) == 0)
		{
		MarkCudProfile(cud);
		}
	else if(strncmp(what, "stop", strlen(what)) == 0)
		CancelCudProfile(cud);
	else
		return luaL_error(L, "invalid onoff argument");
	return 0;
	}

/*--------------------------------------------------------------------------*
 | Registration                              								|
 *--------------------------------------------------------------------------*/

void process_unregister(cud_t *cud)
/* release callbacks references from the the registry */
	{
	if(!cud->process_state) return;
	Unregister(cud, Process);
	Unregister(cud, BufferSize);
	Unregister(cud, Sync);
	Unregister(cud, Timebase);
	Unregister(cud, TimebaseConditional);
	}


static const struct luaL_Reg MFunctions[] = 
	{
		{ "process_loadfile", ProcessLoadfile },
		{ "process_load", ProcessLoad },
		{ "profile", Profile },
		{ NULL, NULL } /* sentinel */
	};

static const struct luaL_Reg PFunctions [] = 
	{
		{ "process_callback", ProcessCallback },
		{ "buffer_size_callback", BufferSizeCallback },
		{ "sync_callback", SyncCallback },
		{ "timebase_callback", TimebaseCallback },
		{ "release_timebase", ReleaseTimebase },
		{ NULL, NULL } /* sentinel */
	};

int luajack_open_process(lua_State *L, int state_type)
	{
	switch(state_type)
		{
		case ST_PROCESS: luaL_setfuncs(L, PFunctions, 0); break;
		case ST_MAIN: luaL_setfuncs(L, MFunctions, 0); break;
		default:
			break;
		}
	return 1;
	}

