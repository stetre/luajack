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

#include <string.h>
#include "luajack.h"

#define nframes_t   jack_nframes_t  
#define sample_t    jack_default_audio_sample_t     

/* LuaJack objects */
luajack_t *client = NULL;
luajack_t *port_in = NULL;
luajack_t *port_out = NULL;
luajack_t *gui = NULL;      /* the gui thread */
luajack_t *rbuf_in = NULL;  /* gui to process ringbuffer */
luajack_t *rbuf_out = NULL; /* process to gui ringbuffer */

#define MAX_DELAY   10.0    /* max echo delay (seconds) */
static nframes_t Fs;    /* sample rate */
static nframes_t Mmax;  /* max delay line length */
static nframes_t M;     /* current delay line length */
static sample_t Delay = 0;  /* current delay (seconds) */
static sample_t Gain = 0;    /* current gain */
static sample_t *Dline = NULL; /* delay line */
static nframes_t Ptr1, Ptr2; /* read and write pointers */


/*----------------------------------------------------------------------*
 | GUI <-> Process protocol                                             |
 *----------------------------------------------------------------------*/

/* Message tags */
#define TAG_ON              1 /* echo on (params_t) */
#define TAG_OFF             2 /* echo off (no data) */
#define TAG_STATUS_REQ      3 /* status request (no data) */
#define TAG_STATUS_RSP      4 /* status response (params_t) */

typedef struct {
    float   gain;
    float   delay;
} params_t;

#define MAX_DATA     sizeof(params_t) /* max length of data in a ringbuffer message */

static int SendToGui(uint32_t tag, const params_t *par)
    {
    size_t len = par!=NULL ? sizeof(params_t) : 0;
    luajack_ringbuffer_write(rbuf_out, tag, par, len);
    luajack_signal(client, gui);
    return 0;
    }
    
static int Off(void)
    {
    Gain = 0; 
    Delay = 0;
    return 0;
    }

static int On(params_t *par)
    {
    sample_t gain = (sample_t)par->gain;
    sample_t delay = (sample_t)par->delay;
    if((gain <= 0) || (delay <= 0))
        return Off();
        
    Gain = gain > 1.0 ? 1.0 : gain;
    Delay = delay > MAX_DELAY ? MAX_DELAY : delay;
    M = Delay*Fs;
    Ptr2 = (Ptr1 + M) % Mmax;
    //printf("Delay=%g M=%u Gain=%g\n", Delay, M, Gain);
    return 0;
    }

static int Status(void)
    {
    params_t par;
    par.gain = (float)Gain;
    par.delay = (float)Delay;
    return SendToGui(TAG_STATUS_RSP, &par);
    }

static int CheckRingbuffer(void)
/* Receives messages from the GUI thread */
    {
    uint32_t tag;
    params_t par;
    size_t len;

    if(!luajack_ringbuffer_read(rbuf_in, &tag, &par, sizeof(params_t), &len))
        return 0; /* no messages available to read */
                                
    //printf("tag=%u len=%lu\n", tag, len);
    
    switch(tag)
        {
        case TAG_ON:    if(len!=sizeof(params_t))
                            return luajack_error("invalid message length from gui");
                        return On(&par);
        case TAG_OFF:   return Off();
        case TAG_STATUS_REQ:    return Status();
        default:
            return luajack_error("invalid message from gui");
        }
    return 0;
    }


/*----------------------------------------------------------------------*
 | Process callback                                                     |
 *----------------------------------------------------------------------*/

static void AtExit(void)
    {
//  printf("Free Dline=%p\n", (void*)Dline); // don't print here!
    if(Dline) { free(Dline); Dline=NULL; }
    }

static int InitDelayLine(void)
    {
    Fs = luajack_get_sample_rate(client);
    Mmax = MAX_DELAY*Fs + 1;
    Dline = (sample_t*)malloc(Mmax * sizeof(sample_t));
    memset(Dline, 0, Mmax * sizeof(sample_t));
    atexit(AtExit); 
//   printf("Mmax=%u Fs=%u Dline=%p\n", Mmax, Fs, (void*)Dline);
    return 0;
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


static int Process(nframes_t nframes, void *arg) 
    {
    (void)arg; /* not used */
    nframes_t i;
    sample_t *buf_in, *buf_out;
    sample_t x;

    CheckRingbuffer();

    buf_in = luajack_get_buffer(port_in);
    buf_out = luajack_get_buffer(port_out);

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

/*----------------------------------------------------------------------*
 | Initialization                                                       |
 *----------------------------------------------------------------------*/

static int Init(lua_State *L)
    {
    client = luajack_checkclient(L, 1);
    port_in = luajack_checkport(L, 2);
    port_out = luajack_checkport(L, 3);
    gui = luajack_checkthread(L, 4);
    rbuf_in = luajack_checkringbuffer(L, 5);
    rbuf_out = luajack_checkringbuffer(L, 6);

    InitDelayLine();

    if(luajack_set_process_callback(client, Process, NULL) != 0)
        return luaL_error(L, "cannot register process callback");
    return 0;
    }

static const struct luaL_Reg Functions[] = 
    {
        { "init", Init },
        { NULL, NULL } /* sentinel */
    };

int luaopen_myecho(lua_State *L)
    {
    lua_newtable(L); 
    luaL_setfuncs(L, Functions, 0);
    return 1;
    }

