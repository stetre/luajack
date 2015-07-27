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
 * Controlling & querying JACK server operation
 ****************************************************************************/

#include "internal.h"

/*--------------------------------------------------------------------------*
 | Client methods                   										|
 *--------------------------------------------------------------------------*/

static int Freewheel(lua_State *L)
	{
	cud_t *cud = cud_check(L, 1);
	int onoff = luajack_checkonoff(L, 2);
	int rc = jack_set_freewheel(cud->client, onoff);
	if(rc != 0) 
		return luajack_strerror(L, rc);
	return 0;
	}

static int SetBufferSize(lua_State *L)
	{
	cud_t *cud = cud_check(L, 1);
	nframes_t nframes = luaL_checkinteger(L, 2);
	int rc = jack_set_buffer_size(cud->client, nframes);
	if(rc != 0) 
		return luajack_strerror(L, rc);
	return 0;
	}

static int BufferSize(lua_State *L)
	{
	cud_t *cud = cud_check(L, 1);
	lua_pushinteger(L, cud->buffer_size);
	/* jack_get_buffer_size() should only be used when the client is activated */
	return 1;
	}

static int SampleRate(lua_State *L)
	{
	cud_t *cud = cud_check(L, 1);
	lua_pushinteger(L, jack_get_sample_rate(cud->client));
	return 1;
	}

static int CpuLoad(lua_State *L)
	{
	cud_t *cud = cud_check(L, 1);
	lua_pushnumber(L, jack_cpu_load(cud->client));
	return 1;	
	}


/*--------------------------------------------------------------------------*
 | Registration                              								|
 *--------------------------------------------------------------------------*/

static const struct luaL_Reg MFunctions[] = 
	{
		{ "freewheel",  Freewheel },	
		{ "set_buffer_size", SetBufferSize },
		{ "buffer_size", BufferSize },
		{ "sample_rate", SampleRate },
		{ "cpu_load", CpuLoad },
		{ NULL, NULL } /* sentinel */
	};

#define PFunctions MFunctions

static const struct luaL_Reg TFunctions[] = 
	{
		{ "buffer_size", BufferSize },
		{ "sample_rate", SampleRate },
		{ "cpu_load", CpuLoad },
		{ NULL, NULL } /* sentinel */
	};


int luajack_open_srvctl(lua_State *L, int state_type)
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

