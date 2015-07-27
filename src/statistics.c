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
 * Monitoring the performance of a running JACK server                      *
 ****************************************************************************/

#include "internal.h"
#include <jack/statistics.h>

/*--------------------------------------------------------------------------*
 | Functions                        										|
 *--------------------------------------------------------------------------*/

static int MaxDelayedUsecs(lua_State *L)
/* delay = max_delayed_usecs(client) */
	{
	float dly;
	cud_t *cud = cud_check(L, 1);
	dly = jack_get_max_delayed_usecs(cud->client);
	lua_pushnumber(L, dly);
	return 1;
	}

static int XrunDelayedUsecs(lua_State *L)
/* delay = xrun_delayed_usecs(client) */
	{
	cud_t *cud = cud_check(L, 1);
	lua_pushnumber(L, jack_get_xrun_delayed_usecs(cud->client));
	return 1;
	}

static int ResetMaxDelayedUsecs(lua_State *L)
/* reset_max_delayed_usecs(client) */
	{
	cud_t *cud = cud_check(L, 1);
	jack_reset_max_delayed_usecs(cud->client);
	return 0;
	}

/*--------------------------------------------------------------------------*
 | Registration                              								|
 *--------------------------------------------------------------------------*/

static const struct luaL_Reg MFunctions[] = 
	{
		{ "max_delayed_usecs", MaxDelayedUsecs },
		{ "xrun_delayed_usecs", XrunDelayedUsecs },
		{ "reset_max_delayed_usecs", ResetMaxDelayedUsecs },
		{ NULL, NULL } /* sentinel */
	};

int luajack_open_statistics(lua_State *L, int state_type)
	{
	switch(state_type)
		{
		case ST_MAIN: luaL_setfuncs(L, MFunctions, 0); break;
		default:
			break;
		}
	return 1;
	}

