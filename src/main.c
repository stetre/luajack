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
 * LuaJack library main thread     											*
 ****************************************************************************/

#include <sys/select.h>
#include "internal.h"

static pthread_key_t key_main; /* only the main thread has data bound to this key */

int luajack_ismainthread(void)
	{ return (pthread_getspecific(key_main) != NULL); }

/*------------------------------------------------------------------------------*
 | signals                     													|
 *------------------------------------------------------------------------------*/

static sigset_t Sigfullset;  /* full mask (constant) */
static sigset_t Sigemptyset; /* empty mask (constant) */

int luajack_sigblock(void)
/* Blocks signals in the current pthread. Signals are blocked in any pthread 
 * other than the main one, so that if a signal is sent to this process, it
 * will be delivered to the main pthread.
 * Note: signals are actually blocked also in the main pthread, except in the
 * pselect() window, where they are all unblocked.
 */
	{
	int rc;
	if(luajack_ismainthread()) return 0;
	DBG("blocking signals in pthread %u/%u\n", gettid(), getpid());
	if((rc = pthread_sigmask(SIG_BLOCK, &Sigfullset, NULL))!=0)
		fprintf(stderr, "cannot block signals in thread %u\n", gettid());
	return rc; /* should be 0 */
	}

static void SigExit(int signo) /* exit signal handler */
	{ luajack_error("caught signal %d (%s)", signo, strsignal(signo)); }

/*------------------------------------------------------------------------------*
 | error() and os.exit()       													|
 *------------------------------------------------------------------------------*/

#define LUAJACK_ERRMSG_LEN 256
static char luajack_errmsg[LUAJACK_ERRMSG_LEN];
int luajack_exit_status = 0;
#define EXITING		1
#define CODE		2	/* os.exit() 'code' argument = true */
#define ERRMSG		4	/* luajack_errmsg was set */
static pthread_mutex_t luajack_errlock = PTHREAD_MUTEX_INITIALIZER;
static int luajack_errpipe[2]; /* pipe for errors */
int luajack_evtpipe[2]; /* pipe for non-rt callbacks queue */

static void AtExit(void)
	{
	lua_State *L = (lua_State*)pthread_getspecific(key_main);
	DBG("AtExit %u/%u %p\n", gettid(), getpid(), (void*)L);
	client_close_all(); 
	evt_free_all();
	}

static int luajack_errorv_(int code, const char *fmt, va_list ap)
/* This function is meant to be called when an error occurs in pthreads other
 * than the main one. Since it is not safe to call lua_error() from non main
 * threads, it registers that an error occurred, saves the error message, and
 * writes to the errpipe so to make pselect() return in the main pthread, where
 * the error will be processed.
 * To check if an error occurred, the main pthread uses luajack_checkerror().
 *
 * Note that this mechanism works as long as the main script implements a loop
 * based on jack.sleep().
 */
	{
	int n, status;
	if(pthread_mutex_trylock(&luajack_errlock) == 0)
		/* if locked, an error is already being registered, so this one
		 * is simply ignored (it may be a consequence) */
		{
		if(luajack_exiting()) /* already called by somebody */
			{
			pthread_mutex_unlock(&luajack_errlock);
			return 0;
			}

		status = EXITING;
		if(code) status |= CODE;

		if(!fmt)  /* no error message */
			luajack_errmsg[0] = '\0';
		else
			{
			/* store message */
			n = vsnprintf(luajack_errmsg, LUAJACK_ERRMSG_LEN-2, fmt, ap);
			if(n<0) 
				luajack_errmsg[0] = '\0';
			else
				{
				luajack_errmsg[n] = '\n';
				luajack_errmsg[n+1] = '\0';
				}
			status |= ERRMSG;
			}
		luajack_exit_status = status;
		syncpipe_write(luajack_errpipe[1]); /* to make pselect() return */
		pthread_mutex_unlock(&luajack_errlock);
		}
	return 0;
	}

int luajack_errorv(const char *fmt, va_list ap)
	{ return luajack_errorv_(1, fmt, ap); }

int luajack_error(const char *fmt, ...)
	{
	va_list ap;
	if(fmt)
		{
		va_start(ap, fmt);
		luajack_errorv_(1, fmt, ap);
		va_end(ap);
		}
	else
		luajack_errorv_(1, NULL, 0);
	return 0;
	}

static int OriginalOsExit(lua_State *L, int code, int close) 
/* retrieves and executes the original os.exit() */
	{
	lua_getfield(L, LUA_REGISTRYINDEX, LUAJACK_OSEXIT);
	lua_pushboolean(L, code);
	lua_pushboolean(L, close);
	lua_call(L, 2, 0);
	return 0; /* not reached */	
	}

static int luajack_checkerror(lua_State *L)
/* this shall be called only in the main thread */
	{
	if(!luajack_exiting()) return 0;
	if(luajack_exit_status & ERRMSG) /* someone raised an error */
		{
		lua_pushstring(L, luajack_errmsg);
		return lua_error(L);
		}
	/* someone called os.exit() */
	return OriginalOsExit(L, luajack_exit_status & CODE, 1);
	}

static int OsExit(lua_State *L) 
/* Wraps os.exit(). If called in the main pthread, it executes the original
 * os.exit() forcing the 'close' parameter to 'true'. Otherwise it calls
 * luajack_errorv_() so that the program will exit as soon as it will be in
 * the main pthread.
 */
	{
	int code = lua_toboolean(L, 1);
	DBG("os.exit() called by pthread %u/%u\n", gettid(), getpid());
	if(luajack_ismainthread()) /* exit called in main thread */
		return OriginalOsExit(L, code, 1);
	/* exit called by thread: wait to be in the main thread to exit */
	return luajack_errorv_(code, NULL, 0);
	}


/*------------------------------------------------------------------------------*
 | jack.sleep() 																|
 *------------------------------------------------------------------------------*/

static fd_set Readfds;
static int Nfds;
#define AddReadfd(fd) 	do { 					\
	FD_SET((fd), &Readfds); 					\
	if((fd) >= Nfds) Nfds = (fd)+1; 			\
	DBG("added fd = %d Nfds=%d\n", fd, Nfds);	\
} while(0)


static int Sleep(lua_State *L)
/* This is the building block of the main context loop. 
 * It is based on pselect(), and reacts to:
 * - writes to luajack_errpipe, denoting errors occurred in other pthreads, and
 * - writes to luajack_evtpipe, denoting non-rt callbacks events to be dispatched
 * In order for LuaJack to work properly, the main script must implement a loop
 * based on this function.
 */
	{
	double seconds, exptime = 0, interval = 0, now;
	struct timespec ts;
	struct timespec* timeout = NULL;
	int rc;
	fd_set readfds;
	int nfds;
	
	luajack_checkmain();

	seconds = luaL_optnumber(L, 1, -1);
	
	if(seconds>=0)
		{
		timeout = &ts;
		interval = seconds;
		exptime = luajack_now() + seconds;
		}

	while(1)
		{
		luajack_checkerror(L);
		if(timeout)
			{
			luajack_sectots(timeout, interval);
			DBG("pselect, timeout = %.2f s\n", interval);
			}
		else
			DBG("pselect, blocking\n");

		readfds = Readfds;
		nfds = Nfds;
		rc = pselect(nfds, &readfds, NULL, NULL, timeout, &Sigemptyset);
		DBG("pselect rc=%d\n",rc);
		if(rc>0) /* some fd is ready */
			{
			if(FD_ISSET(luajack_errpipe[0], &readfds))
				{
				luajack_checkerror(L);
				syncpipe_read(luajack_errpipe[0]);
				}
			if(FD_ISSET(luajack_evtpipe[0], &readfds))
				{
				/* flush non-rt callbacks queue and execute callbacks */
				callback_flush(L);
				}
			}
		else if((rc<0) && (errno !=EINTR)) /* EINVAL, ENOMEM or EBADF */
			luaL_error(L, "select error");
		/* else interrupted by a signal, or timeout expired */
		if(timeout)
			{
			if(seconds == 0) return 0; 
			now = luajack_now();
			if(now > exptime) return 0;
			interval = exptime - now;
			}
		}
	return 0;
	}

/*------------------------------------------------------------------------------*
 | jack.xxx() functions															|
 *------------------------------------------------------------------------------*/

static int Getpid(lua_State *L)
	{
	lua_pushinteger(L, getpid());
	lua_pushinteger(L, gettid());
	return 2;
	}

static void NoPrint(const char *msg)
	{
	if(!msg) return;
	}

static void Print(const char *msg)
	{
	if(!msg) return;
#if 0
	DBG("error/info callback %u/%u\n", gettid(), getpid());
#endif
	fprintf(stderr, msg);
	fprintf(stderr, "\n");
	}

int (*luajack_verbose)(const char*, ...) = luajack_noprintf;

static int Verbose_(int onoff)
	{
	if(onoff)
		{
		luajack_verbose = printf;
		jack_set_error_function(Print);	
		jack_set_info_function(Print);	
		}
	else
		{
		luajack_verbose = luajack_noprintf;
		jack_set_error_function(NoPrint);	
		jack_set_info_function(NoPrint);
		}
	return 0;
	}

static int Verbose(lua_State *L)
	{
	int onoff = luajack_checkonoff(L, 1);
	return Verbose_(onoff);
	}

/*--------------------------------------------------------------------------*
 | luaopen_luajack                           								|
 *--------------------------------------------------------------------------*/

static const struct luaL_Reg MFunctions[] = 
	{
		{ "sleep", Sleep },
		{ "getpid", Getpid },
		{ "verbose", Verbose },
		{ NULL, NULL } /* sentinel */
	};

static const struct luaL_Reg PFunctions[] = 
	{
		{ "getpid", Getpid },
		{ NULL, NULL } /* sentinel */
	};

#define TFunctions PFunctions 

int luajack_open_client(lua_State *L, int state_type);
int luajack_open_callback(lua_State *L, int state_type);
int luajack_open_port(lua_State *L, int state_type);
int luajack_open_latency(lua_State *L, int state_type);
int luajack_open_srvctl(lua_State *L, int state_type);
int luajack_open_time(lua_State *L, int state_type);
int luajack_open_statistics(lua_State *L, int state_type);
int luajack_open_transport(lua_State *L, int state_type);
int luajack_open_rbuf(lua_State *L, int state_type);
int luajack_open_thread(lua_State *L, int state_type);
int luajack_open_process(lua_State *L, int state_type);
int luajack_open_buffer(lua_State *L, int state_type);
int luajack_open_session(lua_State *L, int state_type);

static int main_open(lua_State* L, int state_type)
	{
	/* overload os.exit() with OsExit() */
	if(lua_getglobal(L, "os") != LUA_TTABLE)
		return luajack_error(UNEXPECTED_ERROR);
	lua_pushcfunction(L, OsExit);
	lua_setfield(L, -2, "exit");
	lua_pop(L, 1); /* "os" */

	if(state_type != ST_MAIN)
		{
		/* used by luaopen_luajack() to prevent loading the full module
		 * in the thread-dedicated Lua state: */
		lua_pushboolean(L, 1);
		lua_setglobal(L, "_LUAJACK_THREAD");
		}

	switch(state_type)
		{
		case ST_MAIN: luaL_setfuncs(L, MFunctions, 0); break;
		case ST_PROCESS: luaL_setfuncs(L, PFunctions, 0); break;
		case ST_THREAD: luaL_setfuncs(L, TFunctions, 0); break;
		default:
			break;
		}

	lua_pushstring(L, "_JACK_VERSION");
	lua_pushfstring(L, "JACK %s", jack_get_version_string());
	lua_settable(L, -3);

	lua_pushstring(L, "_VERSION");
	lua_pushstring(L, "LuaJack "LUAJACK_VERSION);
	lua_settable(L, -3);

	lua_pushstring(L, "MAX_FRAMES");
	lua_pushinteger(L, JACK_MAX_FRAMES);
	lua_settable(L, -3);

	lua_pushstring(L, "LOAD_INIT_LIMIT");
	lua_pushinteger(L, JACK_LOAD_INIT_LIMIT);
	lua_settable(L, -3);

	lua_pushstring(L, "DEFAULT_AUDIO_TYPE");
	lua_pushstring(L, JACK_DEFAULT_AUDIO_TYPE);
	lua_settable(L, -3);

	lua_pushstring(L, "DEFAULT_MIDI_TYPE");
	lua_pushstring(L, JACK_DEFAULT_MIDI_TYPE);
	lua_settable(L, -3);

	lua_pushstring(L, "RINGBUFFER_HDRLEN");
	lua_pushinteger(L, ringbuffer_header_len());
	lua_settable(L, -3);

	luajack_open_client(L, state_type);
	luajack_open_callback(L, state_type);
	luajack_open_port(L, state_type);
	luajack_open_latency(L, state_type);
	luajack_open_srvctl(L, state_type);
	luajack_open_time(L, state_type);
	luajack_open_statistics(L, state_type);
	luajack_open_transport(L, state_type);
	luajack_open_rbuf(L, state_type);
	luajack_open_thread(L, state_type);
	luajack_open_process(L, state_type);
	luajack_open_buffer(L, state_type);
	luajack_open_session(L, state_type);
	return 0;
	}

lua_State* luajack_newstate(lua_State *L, int state_type, lua_Alloc alloc, void *alloc_ud)
/* prepares the thread-dedicated Lua state */
	{
	lua_State *T;
	lua_Alloc alloc_ = alloc;
	void *ud = alloc_ud;
	if(!alloc_)
		{
		/* inherit from parent */
		alloc_ = lua_getallocf(L, &ud);
		}
	T = lua_newstate(alloc_, ud);
	if(!T) return NULL; 
	luaL_openlibs(T);
	lua_newtable(T); 
	main_open(T, state_type);
	lua_setglobal(T, "jack");
	return T;
	}


void luajack_malloc_init(lua_State *L);

int luaopen_luajack(lua_State *L)
/* Lua calls this function to load the luajack module */
	{
	int rc;

	/* check that luajack is not require()d in a thread script ... */
	if(lua_getglobal(L, "_LUAJACK_THREAD") != LUA_TNIL)
		{
		/* require("luajack") called from a thread script (either as an error,
         * or to rename the module); return the already loaded table: */
		lua_pop(L, 1);
		lua_getglobal(L, "jack"); 
		return 1;
		}
	lua_pop(L, 1);
	
	/* get the memory allocator */
	luajack_malloc_init(L);

	sigfillset(&Sigfullset);
	sigemptyset(&Sigemptyset);

	if((rc = pthread_key_create(&key_main, NULL)) != 0)
		return luaL_error(L, "pthread_key_create returned %d", rc);
	pthread_setspecific(key_main, L);
	atexit(AtExit);

	FD_ZERO(&Readfds);
	Nfds = 0;

	syncpipe_init();
	if(syncpipe_new(luajack_errpipe) < 0)
		luaL_error(L, "cannot create pipe");
	AddReadfd(luajack_errpipe[0]);
	if(syncpipe_new(luajack_evtpipe) < 0)
		luaL_error(L, "cannot create pipe");
	AddReadfd(luajack_evtpipe[0]);

	/* block all signals */
	sigprocmask(SIG_SETMASK, &Sigfullset, NULL);
	/* set signal handlers (signals will be unblocked in the pselect() loop) */
	signal(SIGINT, SigExit);
	signal(SIGTERM, SigExit);
	signal(SIGQUIT, SigExit);
	signal(SIGABRT, SigExit);
	signal(SIGHUP, SigExit);

	/* save the original os.exit() */
	if(lua_getglobal(L, "os") != LUA_TTABLE)
		luaL_error(L, "cannot find os table");
	if(lua_getfield(L, -1, "exit") != LUA_TFUNCTION)
		luaL_error(L, "cannot find os.exit()");
	lua_setfield(L, LUA_REGISTRYINDEX, LUAJACK_OSEXIT);
	lua_pop(L, 1); /* "os" */

	/* create the 'jack' table and add the functions to it */
	lua_newtable(L); 
	main_open(L, ST_MAIN);

	Verbose_(0);
	lua_gc(L, LUA_GCCOLLECT, 0);
#if 0
	lua_gc(L, LUA_GCSTOP, 0);
#endif
	return 1;
	}

