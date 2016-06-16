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

/*----------------------------------------------------------------------*
 | RT Callbacks                                                         |
 *----------------------------------------------------------------------*/

static int Process(nframes_t nframes, void *arg) 
/* This is the process callback (JackProcessCallback).
 * It just copies the input port buffer to the output port buffer.
 */
    {
    (void)arg; /* not used */
    sample_t *buf_in, *buf_out;

    /* Retrieve the buffers */
    buf_in = luajack_get_buffer(port_in);
    buf_out = luajack_get_buffer(port_out);

    /* Copy the input to the output */
    if(buf_in && buf_out)
        memcpy(buf_out, buf_in, nframes *sizeof(sample_t));

    return 0;
    }

/*----------------------------------------------------------------------*
 | Initialization                                                       |
 *----------------------------------------------------------------------*/

static int Init(lua_State *L)
/* This is the module initialization function. It must be called by
 * The process chunk must call it when loaded (before the client is
 * activated) and pass it the LuaJack objects (in this case a client
 * and two ports) and any other relevant parameters.
 *
 * Since this function is called when the client is not active, it need
 * not be real-time safe.
 */ 
    {
    client = luajack_checkclient(L, 1);
    port_in = luajack_checkport(L, 2);
    port_out = luajack_checkport(L, 3);

    /* Register the RT process callback */
    if(luajack_set_process_callback(client, Process, NULL/*arg*/) != 0)
        return luaL_error(L, "Cannot register process callback");
    return 0;
    }

static const struct luaL_Reg Functions[] = 
    {
        { "init", Init },
        { NULL, NULL } /* sentinel */
    };

int luaopen_mymodule(lua_State *L)
/* Lua calls this function when the module is require()d.
 * It creates the 'mymodule' table and add its function to it
 * (in this case we only have the mymodule.init() function). 
 */
    {
    lua_newtable(L); 
    luaL_setfuncs(L, Functions, 0);
    return 1;
    }

