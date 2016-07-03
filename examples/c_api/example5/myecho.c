/* The MIT License (MIT)
 *
 * Copyright (c) 2016 Stefano Trettel
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

#include <string.h>
#include "luajack.h" /* automatically includes <jack.h> */

#define nframes_t   jack_nframes_t  
#define sample_t    jack_default_audio_sample_t 
/* usually a float, but JACK may have been compiled defining it as a double */

#define MAX_DELAY   10.0    /* max echo delay (seconds) */
static nframes_t Fs;    /* sample rate */
static nframes_t Mmax;  /* max delay line length */
static nframes_t M;     /* current delay line length */
static sample_t Delay = 0;  /* current delay (seconds) */
static sample_t Gain = 0;    /* current gain */
static sample_t *Dline = NULL; /* delay line */
static nframes_t Ptr1, Ptr2; /* read and write pointers */

static int Off(lua_State *L)
    {
//	printf("myecho Off\n");
	(void)L;
    Gain = 0; 
    Delay = 0;
    return 0;
    }

static int On(lua_State *L)
    {
    sample_t gain = luaL_checknumber(L, 1);
    sample_t delay = luaL_checknumber(L, 2);
    if((gain <= 0) || (delay <= 0))
        return Off(L);
        
    Gain = gain > 1.0 ? 1.0 : gain;
    Delay = delay > MAX_DELAY ? MAX_DELAY : delay;
    M = Delay*Fs;
    Ptr2 = (Ptr1 + M) % Mmax;
//	printf("myecho On: Delay=%g M=%u Gain=%g\n", Delay, M, Gain);
    return 0;
    }

static int Status(lua_State *L)
    {
	lua_pushnumber(L, Gain);
	lua_pushnumber(L, Delay);
    return 2;
    }

static sample_t DelayLine(sample_t x)
    {
    sample_t y = Dline[Ptr1];
    Dline[Ptr1] = 0; /* to keep the delay line clear */
    Dline[Ptr2] = x;
    Ptr1 = (Ptr1 + 1)%Mmax;
    Ptr2 = (Ptr2 + 1)%Mmax;
    return y;
    }

static void *checklightuserdata(lua_State *L, int arg)
    {
	if(!lua_islightuserdata(L, arg))
		{ luaL_argerror(L, arg, "not a lightuserdata"); return NULL; }
	return lua_touserdata(L, arg);
	}


static int Process(lua_State *L) 
    {
    nframes_t nframes, i;
    sample_t x;
    sample_t *buf_in, *buf_out;

    buf_in = checklightuserdata(L, 1);
    buf_out = checklightuserdata(L, 2);
	nframes = luaL_checkinteger(L, 3);

    if(buf_in && buf_out)
        {
        if(Gain==0) /* no echo */
            memcpy(buf_out, buf_in, nframes *sizeof(sample_t));
        else
            {
            for(i=0; i<nframes; i++)
                {
                x = (buf_in[i]);
                buf_out[i] = x + Gain * DelayLine(x);
                }
            }
        }

    return 0;
    }


static void AtExit(void)
/* Cleanup function */
    {
//	printf("myecho AtExit: Dline=%p\n", (void*)Dline); // don't print here!
    if(Dline) { free(Dline); Dline=NULL; }
    }


static int Init(lua_State *L)
/* Initialization function. This allocates the buffer for the delay line,
 * and must be called at the beginning of the process chunk (not within
 * the process callback)
 */
    {
    Fs = luaL_checkinteger(L, 1);
    Mmax = MAX_DELAY*Fs + 1;
    Dline = (sample_t*)malloc(Mmax * sizeof(sample_t));
    memset(Dline, 0, Mmax * sizeof(sample_t));
    atexit(AtExit); 
//	printf("myecho Init\n");
//  printf("Mmax=%u Fs=%u Dline=%p\n", Mmax, Fs, (void*)Dline);
    return 0;
    }

static const struct luaL_Reg Functions[] = 
    {
        { "init", Init },
        { "process", Process },
        { "on", On },
        { "off", Off  },
        { "status", Status },
        { NULL, NULL } /* sentinel */
    };

int luaopen_myecho(lua_State *L)
    {
    lua_newtable(L); 
    luaL_setfuncs(L, Functions, 0);
    return 1;
    }

