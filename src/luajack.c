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
 * LuaJack C API                   											*
 ****************************************************************************/

#include "internal.h"

/*------------------------------------------------------------------------------*
 | Raw objects retrieval														|
 *------------------------------------------------------------------------------*/
/* These functions are meant to be used only in C functions called by Lua from
 * the process context.
 */

luajack_t* luajack_checkclient(lua_State *L, int arg)
	{
	cud_t *cud = cud_check(L, arg);
	return &(cud->obj);
	}

luajack_t* luajack_checkport(lua_State *L, int arg)
	{
	pud_t *pud = pud_check(L, arg);
	return &(pud->obj);
	}

luajack_t* luajack_checkthread(lua_State *L, int arg)
	{
	tud_t *tud = tud_check(L, arg);
	return &(tud->obj);
	}

luajack_t* luajack_checkringbuffer(lua_State *L, int arg)
	{
	rud_t *rud = rud_check(L, arg);
	return &(rud->obj);
	}

/*------------------------------------------------------------------------------*
 | Real-time wrappers															|
 *------------------------------------------------------------------------------*/
/* All the functions below are to be used in the real-time callbacks
 */

#define GETXUD(obj, ot, xud_t, isvalid) 									\
	{																		\
	if((obj)==NULL)															\
		{ luajack_error("null luajack_t object"); return NULL; }			\
	if((obj)->type != LUAJACK_T##ot)										\
		{ luajack_error("invalid luajack_t object"); return NULL; }			\
	if(!isvalid((xud_t*)(obj->xud)))										\
		{ luajack_error("invalid luajack_t object"); return NULL; }			\
	return (xud_t*)(obj->xud);												\
	}

static cud_t *get_cud(luajack_t *obj) GETXUD(obj, CLIENT, cud_t, IsCudValid)
static pud_t *get_pud(luajack_t *obj) GETXUD(obj, PORT, pud_t, IsPudValid)
static tud_t *get_tud(luajack_t *obj) GETXUD(obj, THREAD, tud_t, IsTudValid)
static rud_t *get_rud(luajack_t *obj) GETXUD(obj, RINGBUFFER, rud_t, IsRudValid)

/*------------------------------------------------------------------------------*
 | Real-time callbacks registration												|
 *------------------------------------------------------------------------------*/
/* Wrappers of the corresponding jack_set_xxx_callback() functions, they have the
 * same semantics but accept a luajack_t* instead of a jack_client_t*.
 */

int luajack_set_process_callback(luajack_t *client, JackProcessCallback callback, void* arg)
	{
	cud_t *cud = get_cud(client);
	if(!cud) return 0;
	return process_ccallback_process(cud, callback, arg);
	}

int luajack_set_buffer_size_callback(luajack_t *client, JackBufferSizeCallback callback, void *arg)
	{
	cud_t *cud = get_cud(client);
	if(!cud) return 0;
	return process_ccallback_buffer_size(cud, callback, arg);
	}

int luajack_set_sync_callback(luajack_t *client, JackSyncCallback callback, void *arg)
	{
	cud_t *cud = get_cud(client);
	if(!cud) return 0;
	return process_ccallback_sync(cud, callback, arg);
	}

int luajack_set_timebase_callback(luajack_t *client, int conditional,  JackTimebaseCallback callback, void *arg)
	{
	cud_t *cud = get_cud(client);
	if(!cud) return 0;
	return process_ccallback_timebase(cud, conditional, callback, arg);
	}

int luajack_release_timebase(luajack_t *client)
	{
	cud_t *cud = get_cud(client);
	if(!cud) return 0;
	return process_ccallback_release_timebase(cud);
	}

/*------------------------------------------------------------------------------*
 | Port buffers																	|
 *------------------------------------------------------------------------------*/

void* luajack_get_buffer(luajack_t *port)
/* Wrapper of jack_port_get_buffer() */
	{
#define cud ((pud)->cud)
	pud_t *pud;
	if(!(pud = get_pud(port))) return NULL;
	if(!IsProcessCallback(cud))
		{ luajack_error("function available only in process callback"); return NULL; }
	if(pud->buf)
		{ luajack_error("buffer already retrieved"); return NULL; }
	pud->nframes = cud->nframes;
	pud->buf = jack_port_get_buffer(pud->port, pud->nframes);
	if(!pud->buf)
		{ luajack_error("cannot get port buffer"); return NULL; }
	pud->bufp = 0;
	if(PortIsMidi(pud))
		{
		if(PortIsOutput(pud))
			jack_midi_clear_buffer(pud->buf);
		}	
	return pud->buf;
#undef cud
	}

#if 0
/*@@ buffer.c functions (fare?) */
jack_nframes_t luajack_get_buffer_audio(luajack_t *port);
uint32_t luajack_get_buffer_midi_in(luajack_t *port, uint32_t *lostcount);
size_t luajack_get_buffer_midi_out(luajack_t *port);
#endif

/*------------------------------------------------------------------------------*
 | Ringbuffers																	|
 *------------------------------------------------------------------------------*/

int luajack_ringbuffer_write(luajack_t *ringbuffer, uint32_t tag, const void *data, size_t len)
	{
	rud_t *rud = get_rud(ringbuffer);
	if(!rud) return 0;
	return ringbuffer_cwrite(rud->rbuf, tag, data, len);
	}

int luajack_ringbuffer_read(luajack_t *ringbuffer, uint32_t *tag, void *buf, size_t bufsz, size_t *len)
	{
	rud_t *rud = get_rud(ringbuffer);
	if(!rud) return 0;
	return ringbuffer_cread(rud->rbuf, buf, bufsz, tag, len);
	}

int luajack_ringbuffer_peek(luajack_t *ringbuffer)
	{
	rud_t *rud = get_rud(ringbuffer);
	if(!rud) return 0;
	return ringbuffer_cpeek(rud->rbuf);
	}

int luajack_ringbuffer_reset(luajack_t *ringbuffer)
	{
	rud_t *rud = get_rud(ringbuffer);
	if(!rud) return 0;
	return ringbuffer_creset(rud->rbuf);
	}

/*------------------------------------------------------------------------------*
 | Real-time scheduling															|
 *------------------------------------------------------------------------------*/

int luajack_is_realtime(luajack_t *client)
	{
	cud_t *cud = get_cud(client);	
	if(!cud) return 0;
	return jack_is_realtime(cud->client);
	}

int luajack_max_real_time_priority(luajack_t *client)
	{
	int rc;	
	cud_t *cud = get_cud(client);
	if(!cud) return 0;
	rc = jack_client_max_real_time_priority(cud->client);
	if(rc!=0)
		return luajack_error("jack_client_max_real_time_priority() error");
	return rc;
	}

int luajack_real_time_priority(luajack_t *client)
	{
	int rc;	
	cud_t *cud = get_cud(client);
	if(!cud) return 0;
	rc = jack_client_real_time_priority(cud->client);
	if(rc!=0)
		return luajack_error("jack_client_real_time_priority() error");
	return rc;
	}

int luajack_acquire_real_time_scheduling(luajack_t *client, int priority)
	{
	int rc;	
	cud_t *cud = get_cud(client);
	if(!cud) return 0;
	rc = jack_acquire_real_time_scheduling((jack_native_thread_t)pthread_self(), priority);
	if(rc!=0)
		return luajack_error("jack_acquire_real_time_scheduling() error");
	return rc;
	}

int luajack_drop_real_time_scheduling(luajack_t *client)
	{
	int rc;	
	cud_t *cud = get_cud(client);	
	if(!cud) return 0;
	rc = jack_drop_real_time_scheduling((jack_native_thread_t)pthread_self());
	if(rc!=0)
		return luajack_error("jack_drop_real_time_scheduling() error");
	return rc;
	}

/*------------------------------------------------------------------------------*
 | Thread signalling															|
 *------------------------------------------------------------------------------*/

int luajack_signal(luajack_t *client, luajack_t *thread)
	{
	cud_t *cud = get_cud(client);	
	tud_t *tud = get_tud(thread);
	if(cud && tud)
		return thread_signal(cud, tud);
	return 0;
	}


/*------------------------------------------------------------------------------*
 | Server control																|
 *------------------------------------------------------------------------------*/

int	luajack_set_freewheel(luajack_t *client, int onoff) 
	{
	cud_t *cud = get_cud(client);	
	if(!cud) return 0;
	return jack_set_freewheel(cud->client, onoff);
	}

int	luajack_set_buffer_size(luajack_t *client, jack_nframes_t nframes) 
	{
	cud_t *cud = get_cud(client);	
	if(!cud) return 0;
	return jack_set_buffer_size(cud->client, nframes);
	}

jack_nframes_t luajack_get_sample_rate(luajack_t *client) 
	{
	cud_t *cud = get_cud(client);	
	if(!cud) return 0;
	return jack_get_sample_rate(cud->client);
	}

jack_nframes_t luajack_get_buffer_size(luajack_t *client) 
	{
	cud_t *cud = get_cud(client);	
	if(!cud) return 0;
	return jack_get_buffer_size(cud->client);
	}

float luajack_cpu_load(luajack_t *client) 
	{
	cud_t *cud = get_cud(client);	
	if(!cud) return 0;
	return jack_cpu_load(cud->client);
	}

/*------------------------------------------------------------------------------*
 | Time functions																|
 *------------------------------------------------------------------------------*/

jack_nframes_t luajack_frames_since_cycle_start(luajack_t *client) 
	{
	cud_t *cud = get_cud(client);	
	if(!cud) return 0;
	return jack_frames_since_cycle_start(cud->client);
	}

jack_nframes_t luajack_frame_time(luajack_t *client) 
	{
	cud_t *cud = get_cud(client);	
	if(!cud) return 0;
	return jack_frame_time(cud->client);
	}

jack_nframes_t luajack_last_frame_time(luajack_t *client) 
	{
	cud_t *cud = get_cud(client);	
	if(!cud) return 0;
	return jack_last_frame_time(cud->client);
	}

int	luajack_get_cycle_times (luajack_t *client, jack_nframes_t *current_frames, jack_time_t *current_usecs, jack_time_t *next_usecs, float *period_usecs) 
	{
	cud_t *cud = get_cud(client);	
	if(!cud) return 0;
	return jack_get_cycle_times(cud->client,
		current_frames, current_usecs, next_usecs, period_usecs);
	}

jack_time_t luajack_frames_to_time(luajack_t *client, jack_nframes_t nframes) 
	{
	cud_t *cud = get_cud(client);	
	if(!cud) return 0;
	return jack_frames_to_time(cud->client, nframes);
	}

jack_nframes_t luajack_time_to_frames(luajack_t *client, jack_time_t usec) 
	{
	cud_t *cud = get_cud(client);	
	if(!cud) return 0;
	return jack_time_to_frames(cud->client, usec);
	}

/*------------------------------------------------------------------------------*
 | Transport																	|
 *------------------------------------------------------------------------------*/

int	luajack_set_sync_timeout(luajack_t *client, jack_time_t timeout)
	{
	cud_t *cud = get_cud(client);	
	if(!cud) return 0;
	return jack_set_sync_timeout(cud->client, timeout);
	}

int	luajack_transport_locate(luajack_t *client, jack_nframes_t frame)
	{
	cud_t *cud = get_cud(client);	
	if(!cud) return 0;
	return jack_transport_locate(cud->client, frame);
	}

jack_transport_state_t luajack_transport_query(luajack_t *client, jack_position_t *pos)
	{
	cud_t *cud = get_cud(client);	
	if(!cud) return 0;
	return jack_transport_query(cud->client, pos);
	}

jack_nframes_t luajack_get_current_transport_frame(luajack_t *client)
	{
	cud_t *cud = get_cud(client);	
	if(!cud) return 0;
	return jack_get_current_transport_frame(cud->client);
	}

int	luajack_transport_reposition(luajack_t *client, jack_position_t *pos)
	{
	cud_t *cud = get_cud(client);	
	if(!cud) return 0;
	return jack_transport_reposition(cud->client, pos);
	}

void luajack_transport_start(luajack_t *client)
	{
	cud_t *cud = get_cud(client);	
	if(cud)
		jack_transport_start(cud->client);
	}

void luajack_transport_stop(luajack_t *client) 
	{
	cud_t *cud = get_cud(client);	
	if(cud)
		jack_transport_stop(cud->client);
	}


