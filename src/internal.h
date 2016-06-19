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

/********************************************************************************
 * LuaJack library - internal common header										*
 ********************************************************************************/

#ifndef internalDEFINED
#define internalDEFINED

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>
#include <unistd.h>
#include <signal.h>
#include <pthread.h>
#include <jack/session.h>
#include "luajack.h"
#include "tree.h"
#include "queue.h"

#define TOSTR_(x) #x
#define TOSTR(x) TOSTR_(x)

/* Note: all the dynamic symbols of this library (should) start with 'luajack_' .
 * The only exception is the luaopen_luajack() function, which is searched for
 * with that name by Lua.
 * LuaJack's string references on the Lua registry also start with 'luajack_'.
 */

/* Aligned to JACK Api v0.124.1  ( at http://jackaudio.org/api/ ) */

#if LUA_VERSION_NUM < 503 /* defined in lua.h */
#error "LuaJack requires Lua v5.3 or greater"
#endif

/*
#if (LUAVER != LUA_VERSION_NUM)
#pragma message ("lua.h version is "TOSTR(LUA_VERSION_NUM))
#error "Lua version mismatch"
#endif
*/


/* Registry keys used by luajack */
#define LUAJACK_OSEXIT "luajack_osexit"  /* the original os.exit() */

/* Lua-state types */
#define ST_MAIN		1
#define ST_PROCESS	2
#define ST_THREAD	3

#include "structs.h"

#if 0
/* .c */
#define  luajack_
#endif

/* default allocator */
#define Malloc luajack_main_malloc
#define Free luajack_main_free


/* main.c */
extern int luajack_exit_status;
extern int (*luajack_verbose)(const char*, ...);
extern int luajack_evtpipe[2];
int luajack_ismainthread(void);
lua_State* luajack_newstate(lua_State *L, int state_type, lua_Alloc alloc, void *alloc_ud);
int luajack_sigblock(void);
extern size_t luajack_active_clients;

#define luajack_exiting() (luajack_exit_status & 1)

#define luajack_checkmain() do {												\
/* checks that the current thread is the main thread */							\
	if(!luajack_ismainthread())													\
		return luajack_error("function can be called only in main thread"); 	\
} while(0)

#define luajack_checkcreate() do { 												\
/* checks that the current thread is the main thread and that there are no active clients */\
	luajack_checkmain();														\
	if(luajack_active_clients > 0) 												\
		return luajack_error("operation not allowed with active clients"); 		\
} while(0) 

/* client.c */
#define client_pushstatus luajack_client_pushstatus
int client_pushstatus(lua_State *L, int status);
#define client_close_all luajack_client_close_all
void client_close_all(void);

/* port.c */
#define port_close_all luajack_port_close_all
void port_close_all(cud_t *cud);

/* process.c */
#define process_unregister luajack_process_unregister
void process_unregister(cud_t *cud);
#define process_ccallback_process luajack_process_ccallback_process
int process_ccallback_process(cud_t *cud, JackProcessCallback cb, void *arg);
#define process_ccallback_buffer_size luajack_process_ccallback_buffer_size
int process_ccallback_buffer_size(cud_t *cud, JackBufferSizeCallback cb, void *arg);
#define process_ccallback_sync luajack_process_ccallback_sync
int process_ccallback_sync(cud_t *cud, JackSyncCallback cb, void *arg);
#define process_ccallback_timebase luajack_process_ccallback_timebase
int process_ccallback_timebase(cud_t *cud, int conditional,  JackTimebaseCallback cb, void *arg);
#define process_ccallback_release_timebase luajack_process_ccallback_release_timebase
int process_ccallback_release_timebase(cud_t *cud);

/* callback.c */
#define callback_flush luajack_callback_flush
int callback_flush(lua_State* L);
#define callback_unregister luajack_callback_unregister
void callback_unregister(lua_State *L, cud_t *cud);

/* evt.c */
#define evt_new luajack_evt_new
evt_t* evt_new(void);
#define evt_free luajack_evt_free
void evt_free(evt_t *evt);
#define evt_insert luajack_evt_insert
void evt_insert(evt_t *evt); 
#define evt_remove luajack_evt_remove
evt_t* evt_remove(void);
#define evt_first luajack_evt_first
evt_t* evt_first(void);
#define evt_next luajack_evt_next
evt_t* evt_next(evt_t *evt);
#define evt_count luajack_evt_count
unsigned int evt_count(void);
#define evt_free_all luajack_evt_free_all
void evt_free_all(void);

/* transport.c */
#define transport_pushstate luajack_transport_pushstate
int transport_pushstate(lua_State *L, jack_transport_state_t state);
#define transport_pushposition luajack_transport_pushposition
int transport_pushposition(lua_State *L, jack_position_t *pos);

/* buffer.c */
#define buffer_drop_all luajack_buffer_drop_all
void buffer_drop_all(cud_t *cud);

/* syncpipe.c */
#define syncpipe_new luajack_syncpipe_new
int syncpipe_new(int pipefd[2]);
#define syncpipe_write luajack_syncpipe_write
int syncpipe_write(int writefd);
#define syncpipe_read luajack_syncpipe_read
int syncpipe_read(int readfd);
#define syncpipe_init luajack_syncpipe_init
int syncpipe_init(void);

/* cud.c */
#define cud_new luajack_cud_new
cud_t *cud_new(void);
#define cud_search luajack_cud_search
cud_t *cud_search(uintptr_t key); 
#define cud_first luajack_cud_first
cud_t *cud_first(uintptr_t key);
#define cud_next luajack_cud_next
cud_t *cud_next(cud_t *cud);
#define cud_check luajack_cud_check
cud_t* cud_check(lua_State *L, int arg);
#define cud_fifo_insert luajack_cud_fifo_insert
void cud_fifo_insert(cud_t* cud, pud_t *pud);
#define cud_fifo_remove luajack_cud_fifo_remove
void cud_fifo_remove(pud_t *pud);
#define cud_free_all luajack_cud_free_all
void cud_free_all(void);

/* pud.c */
#define pud_new luajack_pud_new
pud_t *pud_new(cud_t *cud);
#define pud_first luajack_pud_first
pud_t *pud_first(uintptr_t key);
#define pud_next luajack_pud_next
pud_t *pud_next(pud_t *pud);
#define pud_check luajack_pud_check
pud_t* pud_check(lua_State *L, int arg);
#define pud_free_all luajack_pud_free_all
void pud_free_all(void);

/* tud.c */
#define tud_new luajack_tud_new
tud_t *tud_new(void);
#define tud_first luajack_tud_first
tud_t *tud_first(uintptr_t key);
#define tud_next luajack_tud_next
tud_t *tud_next(tud_t *tud);
#define tud_check luajack_tud_check
tud_t* tud_check(lua_State *L, int arg);
#define tud_free_all luajack_tud_free_all
void tud_free_all(void);

/* rud.c */
#define rud_new luajack_rud_new
rud_t *rud_new(void);
#define rud_first luajack_rud_first
rud_t *rud_first(uintptr_t key);
#define rud_next luajack_rud_next
rud_t *rud_next(rud_t *rud);
#define rud_check luajack_rud_check
rud_t* rud_check(lua_State *L, int arg);
#define rud_free_all luajack_rud_free_all
void rud_free_all(void);

/* ringbuffer.c */
#define ringbuffer_header_len luajack_ringbuffer_header_len
size_t ringbuffer_header_len(void);
#define ringbuffer_new luajack_ringbuffer_new
jack_ringbuffer_t* ringbuffer_new(size_t sz, int mlock);
#define ringbuffer_free luajack_ringbuffer_free
void ringbuffer_free(jack_ringbuffer_t *rbuf);
#define ringbuffer_creset luajack_ringbuffer_creset
int ringbuffer_creset(jack_ringbuffer_t *rbuf);
#define ringbuffer_write_space luajack_ringbuffer_write_space
size_t ringbuffer_write_space(jack_ringbuffer_t *rbuf);
#define ringbuffer_read_space luajack_ringbuffer_read_space
size_t ringbuffer_read_space(jack_ringbuffer_t *rbuf);
#define ringbuffer_cwrite luajack_ringbuffer_cwrite
int ringbuffer_cwrite(jack_ringbuffer_t *rbuf, uint32_t tag, const void *data, size_t len);
#define ringbuffer_cread luajack_ringbuffer_cread
int ringbuffer_cread(jack_ringbuffer_t *rbuf, void *buf, size_t bufsz, uint32_t *tag, size_t *len);
#define ringbuffer_cpeek luajack_ringbuffer_cpeek
int ringbuffer_cpeek(jack_ringbuffer_t *rbuf);
#define ringbuffer_luawrite luajack_ringbuffer_luawrite
int ringbuffer_luawrite(jack_ringbuffer_t *rbuf, lua_State *L, int arg);
#define ringbuffer_luaread luajack_ringbuffer_luaread
int ringbuffer_luaread(jack_ringbuffer_t *rbuf, lua_State *L, int advance);
#define ringbuffer_luaread_advance luajack_ringbuffer_luaread_advance
int ringbuffer_luaread_advance(jack_ringbuffer_t *rbuf, lua_State *L);

/* latency.c */
#define latency_pushmode luajack_latency_pushmode
int latency_pushmode(lua_State *L, jack_latency_callback_mode_t mode);

/* session.c */
#define session_pushtype luajack_session_pushtype
int session_pushtype(lua_State *L, jack_session_event_type_t type);
#define session_checkflag luajack_session_checkflag
jack_session_flags_t session_checkflag(lua_State *L, int arg);

/* rbuf.c */
#define rbuf_get luajack_rbuf_get
rud_t* rbuf_get(lua_State *L, int ref);
#define rbuf_free_all luajack_rbuf_free_all
void rbuf_free_all(cud_t *cud);
#define rbuf_tables luajack_rbuf_tables
int rbuf_tables(lua_State *L);
#define rbuf_pipe_write luajack_rbuf_pipe_write
void rbuf_pipe_write(lua_State *L, rud_t *rud);
#define rbuf_pipe_read luajack_rbuf_pipe_read
void rbuf_pipe_read(lua_State *L, rud_t *rud);

/* thread.c */
#define thread_free_all luajack_thread_free_all
void thread_free_all(cud_t *cud);
#define thread_signal luajack_thread_signal
int thread_signal(cud_t *cud, tud_t *tud);

/* alloc.c */
void luajack_malloc_init(lua_State *L);

/* main.c */
int luaopen_luajack(lua_State *L);
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

/*----------------------------------------------------------------------*
 | Debug utilities                                    					|
 *----------------------------------------------------------------------*/


void luajack_stat_reset(stat_t *stat);
void luajack_stat_update(stat_t *stat, double sample);
#define luajack_stat_n(stat)	(stat)->n
#define luajack_stat_min(stat)	(stat)->min
#define luajack_stat_max(stat)	(stat)->max
#define luajack_stat_mean(stat)	(stat)->mean
double luajack_stat_variance(stat_t *stat);

#if 1 /*@@ Linux */
#include <sys/syscall.h>
#define gettid() ((pid_t)(syscall(SYS_gettid)))
#elif 0 /*@@ platform has arithmetic pthread_t ids */
#define gettid() (uintptr_t)pthread_self() 
#else
#define gettid() 0
#endif

/* If this is printed, it denotes a suspect LuaJack bug: */
#define UNEXPECTED_ERROR "unexpected error (%s, %d)", __FILE__, __LINE__


#define TSTART		LUAJACK_TSTART
#define TSTOP 		LUAJACK_TSTOP

#if defined(DEBUG)

#define DBG 		printf
#define BK			LUAJACK_BREAK
#define TR 			LUAJACK_TRACE
#define TR_STACK	LUAJACK_TRACE_STACK 
#define BK_STACK	LUAJACK_BREAK_STACK
#define TR_MEM		LUAJACK_TRACE_MEM

#else

#define DBG 		luajack_noprintf
#define BK()
#define TR()
#define TR_STACK(L)
#define BK_STACK(L)
#define TR_MEM(L)

#endif

#endif /* internalDEFINED */
