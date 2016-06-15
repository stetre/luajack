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
 * Managing and determining latency                                         *
 ****************************************************************************/

#include "internal.h"

int latency_pushmode(lua_State *L, jack_latency_callback_mode_t mode)
    {
    switch(mode)
        {
        case JackCaptureLatency:    lua_pushstring(L, "capture"); break;
        case JackPlaybackLatency:   lua_pushstring(L, "playback"); break;
        default:
            luaL_error(L, "invalid callback mode %u", mode);
        }
    return 1;
    }

static jack_latency_callback_mode_t CheckMode(lua_State *L, int arg)
    {
    const char* mode = luaL_checkstring(L, arg);
    if(strncmp(mode, "capture", strlen("capture")) == 0) return JackCaptureLatency;
    if(strncmp(mode, "playback", strlen("playback")) == 0) return JackPlaybackLatency;
    return (jack_latency_callback_mode_t)luaL_error(L, "invalid latency callback mode '%s'", mode);
    }

/*--------------------------------------------------------------------------*
 | Functions                                                                |
 *--------------------------------------------------------------------------*/

static int RecomputeTotalLatencies(lua_State *L)
    {
    cud_t *cud = cud_check(L, 1);
    int rc = jack_recompute_total_latencies(cud->client);
    if(rc!=0)
        return luajack_strerror(L, rc);
    return 0;
    }

static int LatencyRange(lua_State *L)
    {
    jack_latency_range_t range;
    pud_t *pud = pud_check(L, 1);
    jack_latency_callback_mode_t mode = CheckMode(L, 2);
    jack_port_get_latency_range(pud->port, mode, &range);
    lua_pushinteger(L, range.min);
    lua_pushinteger(L, range.max);
    return 2;
    }

static int SetLatencyRange(lua_State *L)
    {
    jack_latency_range_t range;
    pud_t *pud = pud_check(L, 1);
    jack_latency_callback_mode_t mode = CheckMode(L, 2);
    range.min = luaL_checkinteger(L, 3);
    range.max = luaL_checkinteger(L, 4);
    jack_port_set_latency_range(pud->port, mode, &range);
    return 0;
    }

/*--------------------------------------------------------------------------*
 | Registration                                                             |
 *--------------------------------------------------------------------------*/

static const struct luaL_Reg MFunctions[] = 
    {
        { "recompute_total_latencies", RecomputeTotalLatencies },
        { "latency_range", LatencyRange },
        { "set_latency_range", SetLatencyRange },
        { NULL, NULL } /* sentinel */
    };

int luajack_open_latency(lua_State *L, int state_type)
    {
    switch(state_type)
        {
        case ST_MAIN: luaL_setfuncs(L, MFunctions, 0); break;
        default:
            break;
        }
    return 1;
    }

