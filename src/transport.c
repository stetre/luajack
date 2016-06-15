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
 * Transport and Timebase control											*
 ****************************************************************************/

#include "internal.h"

int transport_pushstate(lua_State *L, jack_transport_state_t state)
	{
	switch(state)
		{
		case JackTransportStopped: lua_pushstring(L, "stopped"); break;
		case JackTransportRolling: lua_pushstring(L, "rolling"); break;
/* 		case JackTransportLooping: lua_pushstring(L, "looping"); break; */
		case JackTransportStarting: lua_pushstring(L, "starting"); break;
		default:
			luaL_error(L, "invalid transport state (%d)", state);
		}
	return 1;
	}

int transport_pushposition(lua_State *L, jack_position_t *pos)
	{
#define AddInteger(x) do { lua_pushinteger(L, pos->x); 	lua_setfield(L, -2, #x); } while(0)
#define AddNumber(x) do { lua_pushnumber(L, pos->x); 	lua_setfield(L, -2, #x); } while(0)
	lua_newtable(L);
	/* mandatory */
	AddInteger(unique_1);
	AddInteger(usecs);
	AddInteger(frame_rate);
	AddInteger(frame);
	if(pos->valid & JackPositionBBT )
		{
		AddInteger(bar);
		AddInteger(beat);
		AddInteger(tick);
		AddNumber(bar_start_tick);
		AddNumber(beats_per_bar);
		AddNumber(beat_type);
		AddNumber(ticks_per_beat);
		AddNumber(beats_per_minute);
		}
	if(pos->valid & JackPositionTimecode )
		{
		AddNumber(frame_time);
		AddNumber(next_time);
		}
	if(pos->valid & JackBBTFrameOffset )
		{
		AddInteger(bbt_offset);
		}
	if(pos->valid & JackAudioVideoRatio )
		{
		AddNumber(audio_frames_per_video_frame);
		}
	if(pos->valid & JackVideoFrameOffset )
		{
		AddInteger(video_offset);
		}
	AddInteger(unique_2);
#undef AddInteger
#undef AddNumber
	return 1;
	}

static int transport_checkposition(lua_State *L, int arg, jack_position_t *pos)
/* Encodes the position at index 'arg' (a table) in pos */
	{
	if(!lua_istable(L, arg))
		return luaL_error(L, "missing position table");
	memset(pos, 0, sizeof(jack_position_t)); 
#define GetInteger(x) do { 	\
	lua_getfield(L, arg, #x); pos->x = luaL_checkinteger(L, -1); lua_pop(L, 1);	} while(0)
#define GetNumber(x) do { 	\
	lua_getfield(L, arg, #x); pos->x = luaL_checknumber(L, -1); lua_pop(L, 1);	} while(0)
#define GetValid(bit, x) do { \
		if(lua_getfield(L,arg,#x)!=LUA_TNIL) \
			pos->valid =(jack_position_bits_t)(pos->valid | bit); lua_pop(L, 1); } while(0)
	GetInteger(unique_1);
	GetInteger(usecs);
	GetInteger(frame_rate);
	GetInteger(frame);

	GetValid(JackPositionBBT, bar);
	if(pos->valid & JackPositionBBT )
		{
		GetInteger(bar);
		GetInteger(beat);
		GetInteger(tick);
		GetNumber(bar_start_tick);
		GetNumber(beats_per_bar);
		GetNumber(beat_type);
		GetNumber(ticks_per_beat);
		GetNumber(beats_per_minute);
		}

	GetValid(JackPositionTimecode, frame_time);
	if(pos->valid & JackPositionTimecode )
		{
		GetNumber(frame_time);
		GetNumber(next_time);
		}

	GetValid(JackBBTFrameOffset, bbt_offset);
	if(pos->valid & JackBBTFrameOffset )
		{
		GetInteger(bbt_offset);
		}

	GetValid(JackAudioVideoRatio, audio_frames_per_video_frame);
	if(pos->valid & JackAudioVideoRatio )
		{
		GetNumber(audio_frames_per_video_frame);
		}

	GetValid(JackVideoFrameOffset, video_offset);
	if(pos->valid & JackVideoFrameOffset )
		{
		GetInteger(video_offset);
		}

	GetInteger(unique_2);
#undef GetValid
#undef GetNumber
#undef GetInteger
	return 0;
	}	

/*--------------------------------------------------------------------------*
 | Functions                        										|
 *--------------------------------------------------------------------------*/

static int SetSyncTimeout(lua_State *L) 
	{
	int rc;
	cud_t *cud = cud_check(L, 1);
	jack_time_t timeout = luaL_checkinteger(L, 2);
	rc = jack_set_sync_timeout(cud->client,timeout);
	if(rc!=0) 
		return luajack_strerror(L, rc);
	return 0;
	}

static int TransportLocate(lua_State *L) 
	{
	int rc;
	cud_t *cud = cud_check(L, 1);
	jack_nframes_t frame = luaL_checkinteger(L, 2);
	rc = jack_transport_locate(cud->client, frame);
	if(rc!=0) 
		return luajack_strerror(L, rc);
	return 0;
	}

static int TransportState(lua_State *L)
	{
	cud_t *cud = cud_check(L, 1);
	transport_pushstate(L, jack_transport_query(cud->client, NULL));
	return 1;
	}


static int TransportQuery(lua_State *L)
	{
	jack_transport_state_t state;
	jack_position_t pos;
	cud_t *cud = cud_check(L, 1);
	state = jack_transport_query(cud->client, &pos);
	transport_pushstate(L, state);
	transport_pushposition(L, &pos);
	return 2;
	}

static int CurrentTransportFrame(lua_State *L)
	{
	cud_t *cud = cud_check(L, 1);
	lua_pushinteger(L, jack_get_current_transport_frame(cud->client));
	return 1;
	}

static int TransportReposition(lua_State *L)
	{
	int rc;
	jack_position_t pos;
	cud_t *cud = cud_check(L, 1);
	transport_checkposition(L, 2, &pos);
	rc = jack_transport_reposition(cud->client, &pos);	
	if(rc!=0) 
		return luajack_strerror(L, rc);
	return 0;
	}


static int TransportStart(lua_State *L)
	{
	cud_t *cud = cud_check(L, 1);
	jack_transport_start(cud->client);
	return 0;
	}

static int TransportStop(lua_State *L)
	{
	cud_t *cud = cud_check(L, 1);
	jack_transport_stop(cud->client);
	return 0;
	}


/*--------------------------------------------------------------------------*
 | Registration                              								|
 *--------------------------------------------------------------------------*/

static const struct luaL_Reg MFunctions[] = 
	{
		{ "set_sync_timeout", SetSyncTimeout },
		{ "transport_locate", TransportLocate },
		{ "transport_state", TransportState },
		{ "transport_query", TransportQuery },
		{ "current_transport_frame", CurrentTransportFrame },
		{ "transport_reposition", TransportReposition },
		{ "transport_start", TransportStart },
		{ "transport_stop", TransportStop },
		{ NULL, NULL } /* sentinel */
	};

#define PFunctions MFunctions
#define TFunctions MFunctions

int luajack_open_transport(lua_State *L, int state_type)
	{
	switch(state_type)
		{
		case ST_MAIN: luaL_setfuncs(L, MFunctions, 0); break;
		case ST_PROCESS: luaL_setfuncs(L, PFunctions, 0); break;
		case ST_THREAD: luaL_setfuncs(L, TFunctions, 0); break;
		default:
			break;
		}
	return 1;
	}

