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
 * LuaJack library - C API 													*
 ****************************************************************************/

#ifndef luajackDEFINED
#define luajackDEFINED

#include <lua.h>
#include "lualib.h"
#include "lauxlib.h"
#include <jack/jack.h>
#include <jack/ringbuffer.h>
#include <jack/midiport.h>
#include <jack/transport.h>
#include <jack/thread.h>

#define LUAJACK_VERSION		"0.3"

/* opaque luajack object */
typedef struct luajack_s { 
	int	type;  /* LUAJACK_TXXX codes */
	void *xud; /* cud, pud, tud or rud */
} luajack_t;

#define LUAJACK_TCLIENT			0x1
#define LUAJACK_TPORT			0x2
#define LUAJACK_TTHREAD			0x3
#define LUAJACK_TRINGBUFFER		0x4

luajack_t* luajack_checkclient(lua_State *L, int arg);
luajack_t* luajack_checkport(lua_State *L, int arg);
luajack_t* luajack_checkthread(lua_State *L, int arg);
luajack_t* luajack_checkringbuffer(lua_State *L, int arg);

/* malloc/free (uses the allocator function set for the Lua state) */
void* luajack_main_malloc(size_t size);
void luajack_main_free(void *ptr);
lua_Alloc luajack_allocf(void **ud);

void* luajack_malloc(size_t size);
void luajack_free(void *ptr);

/* to be used inside the callbacks: */
int luajack_error(const char *fmt, ...);
int luajack_errorv(const char *fmt, va_list ap);


/* real-time scheduling */
int luajack_is_realtime(luajack_t *client);
int luajack_max_real_time_priority(luajack_t *client);
int luajack_real_time_priority(luajack_t *client);
int luajack_acquire_real_time_scheduling(luajack_t *client, int priority);
int luajack_drop_real_time_scheduling(luajack_t *client);

/* thread signalling */
int luajack_signal(luajack_t *client, luajack_t *thread);

/* callbacks */
int luajack_set_process_callback(luajack_t *client, JackProcessCallback cb, void* arg);
int luajack_set_buffer_size_callback(luajack_t *client, JackBufferSizeCallback cb, void *arg);
int luajack_set_sync_callback(luajack_t *client, JackSyncCallback cb, void *arg);
int luajack_set_timebase_callback(luajack_t *client, int cond, JackTimebaseCallback cb, void *arg);
int luajack_release_timebase(luajack_t *client);

/* buffers */
void* luajack_get_buffer(luajack_t *port);

/* ringbuffers */
int luajack_ringbuffer_write(luajack_t *ringbuffer, uint32_t tag, const void *data, size_t len);
int luajack_ringbuffer_read(luajack_t *ringbuffer, uint32_t *tag, void *buf, size_t bufsz, size_t *len);
int luajack_ringbuffer_peek(luajack_t *ringbuffer);
int luajack_ringbuffer_reset(luajack_t *ringbuffer);

/* server operations control */
int	luajack_set_freewheel(luajack_t *client, int onoff); 
int	luajack_set_buffer_size(luajack_t *client, jack_nframes_t nframes);
jack_nframes_t luajack_get_sample_rate(luajack_t *client);
jack_nframes_t luajack_get_buffer_size(luajack_t *client);
float luajack_cpu_load(luajack_t *client);

/* time handling */
jack_nframes_t luajack_frames_since_cycle_start(luajack_t *client);
jack_nframes_t luajack_frame_time(luajack_t *client);
jack_nframes_t luajack_last_frame_time(luajack_t *client); 
int	luajack_get_cycle_times (luajack_t *client, jack_nframes_t *current_frames, jack_time_t *current_usecs, jack_time_t *next_usecs, float *period_usecs); 
jack_time_t luajack_frames_to_time(luajack_t *client, jack_nframes_t nframes); 
jack_nframes_t luajack_time_to_frames(luajack_t *client, jack_time_t usec); 
#define luajack_get_time jack_get_time

/* transport */
int	luajack_set_sync_timeout(luajack_t *client, jack_time_t timeout);
int	luajack_transport_locate(luajack_t *client, jack_nframes_t frame);
jack_transport_state_t luajack_transport_query(luajack_t *client, jack_position_t *pos);
jack_nframes_t luajack_get_current_transport_frame(luajack_t *client);
int	luajack_transport_reposition(luajack_t *client, jack_position_t *pos);
void luajack_transport_start(luajack_t *client); 
void luajack_transport_stop(luajack_t *client); 


/* debug/profiling utilities */
double  luajack_now(void);
#define luajack_since(t) (luajack_now() - (t))
double luajack_tstosec(const struct timespec *ts);
void luajack_sectots(struct timespec *ts, double seconds);


#define LUAJACK_TSTART double ts = luajack_now();
#define LUAJACK_TSTOP do {										\
	ts = luajack_since(ts); ts = ts*1e6;					\
	printf("%s %d %.3f us\n", __FILE__, __LINE__, ts);			\
	ts = luajack_now();									\
} while(0);

void luajack_printstack(lua_State *L, const char* fmat, ...);
int luajack_noprintf(const char *fmt, ...);

#define LUAJACK_BREAK() do { 								\
	printf("break %s %d\n",__FILE__,__LINE__); 				\
	getchar(); 												\
} while(0)

#define LUAJACK_TRACE() 	\
	printf("trace %s %d %u/%u\n",__FILE__,__LINE__, gettid(), getpid())

#define LUAJACK_TRACE_STACK(L) 	\
	luajack_printstack((L),"%s %d %u/%u\n",__FILE__,__LINE__, gettid(), getpid())

#define LUAJACK_BREAK_STACK(L) do { 								\
	luajack_printstack((L),"%s %d\n",__FILE__,__LINE__); 			\
	getchar(); 														\
} while(0)

#define LUAJACK_TRACE_MEM(L) do { 	\
	printf("memory trace %s %d bytes=%d\n",__FILE__,__LINE__, 				\
		lua_gc((L), LUA_GCCOUNT, 0)*1024 + lua_gc((L), LUA_GCCOUNTB, 0)); 	\
} while(0)

/* luaL_check -style functions */
int luajack_checkonoff(lua_State *L, int arg);

int luajack_strerror(lua_State *L, int en);

/* utilities for chunck loading */
int luajack_xmove(lua_State *T, lua_State *L, int script_index, int last_index);
int luajack_loadchunk(lua_State *T, lua_State *L, int chunk_index, int isscript);

#endif /* luajackDEFINED */

