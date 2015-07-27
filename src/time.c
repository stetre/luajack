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
 * Handling time															*
 ****************************************************************************/

#include "internal.h"

#define CheckProcess(L, cud) do {										\
	if(!IsProcessCallback((cud)))												\
		luaL_error((L), "function available only in process callback");	\
} while(0)

/*--------------------------------------------------------------------------*
 | Functions                        										|
 *--------------------------------------------------------------------------*/

static int Time(lua_State *L)
	{
	lua_pushinteger(L, jack_get_time());
	return 1;
	}

static int FrameTime(lua_State *L)
	{
	cud_t *cud = cud_check(L, 1);
	lua_pushinteger(L, jack_frame_time(cud->client));
	return 1;	
	}

static int Since(lua_State *L) /* */
	{
	jack_time_t t1 = luaL_checkinteger(L, 1);
	lua_pushinteger(L, jack_get_time() - t1);
	return 1;
	}

static int SinceFrame(lua_State *L) /* */
	{
	cud_t *cud = cud_check(L, 1);
	nframes_t t1 = luaL_checkinteger(L, 2);
	lua_pushinteger(L, jack_frame_time(cud->client) - t1);
	return 1;
	}

static int FramesToTime(lua_State *L)
	{
	cud_t *cud = cud_check(L, 1);
	nframes_t nframes = luaL_checkinteger(L, 2);
	lua_pushinteger(L, jack_frames_to_time(cud->client, nframes));
	return 1;
	}

static int TimeToFrames(lua_State *L)
	{
	cud_t *cud = cud_check(L, 1);
	jack_time_t time = luaL_checkinteger(L, 2);
	lua_pushinteger(L, jack_time_to_frames(cud->client, time));
	return 1;	
	}

static int FramesSinceCycleStart(lua_State *L)
	{
	cud_t *cud = cud_check(L, 1);
	lua_pushinteger(L, jack_frames_since_cycle_start(cud->client));
	return 1;	
	}

static int LastFrameTime(lua_State *L)
	{
	cud_t *cud = cud_check(L, 1);
	CheckProcess(L, cud);
	lua_pushinteger(L, jack_last_frame_time(cud->client));
	return 1;	
	}

static int CycleTimes(lua_State *L)
	{
	nframes_t current_frames;
	jack_time_t current_usecs, next_usecs;
	float	period_usecs;
	int rc;
	cud_t *cud = cud_check(L, 1);
	CheckProcess(L, cud);
	rc = jack_get_cycle_times(cud->client, 
			&current_frames, &current_usecs,&next_usecs, &period_usecs);
	if(rc)
		luaL_error(L, "jack_get_cycle_times() returned %d", rc);
	lua_pushinteger(L, current_frames);
	lua_pushinteger(L, current_usecs);
	lua_pushinteger(L, next_usecs);
	lua_pushnumber(L, period_usecs);
	return 4;
	}


/*--------------------------------------------------------------------------*
 | Registration                              								|
 *--------------------------------------------------------------------------*/

#define COMMON_FUNCTIONS					\
		{ "time", Time },					\
		{ "since", Since },					\
		{ "since_frame", SinceFrame },		\
		{ "frames_to_time", FramesToTime },	\
		{ "time_to_frames", TimeToFrames },	\
		{ "frames_since_cycle_start", FramesSinceCycleStart },	\
		{ "frame_time", FrameTime },							\
		{ "frame", FrameTime }


static const struct luaL_Reg MFunctions[] = 
	{
		COMMON_FUNCTIONS,
		{ NULL, NULL } /* sentinel */
	};

#define TFunctions MFunctions

static const struct luaL_Reg PFunctions[] = 
	{
		COMMON_FUNCTIONS,
		{ "last_frame_time", LastFrameTime },
		{ "cycle_times", CycleTimes },
		{ NULL, NULL } /* sentinel */
	};


int luajack_open_time(lua_State *L, int state_type)
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

