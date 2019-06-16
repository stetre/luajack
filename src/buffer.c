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
 * Reading from and writing to port buffer                                  *
 ****************************************************************************/

#include "internal.h"

#define CheckProcess(L, pud) do {                                           \
    if(!IsProcessCallback((pud)->cud))                                      \
        luaL_error((L), "function available only in process callback");     \
} while(0)

#define CheckBuffer(L, pud) do {                                    \
    if((pud)->buf == NULL)                                          \
        luaL_error((L), "missing get_buffer() call?");              \
} while(0)

#define CheckIsOutput(L, pud) do {                                  \
    if(!PortIsOutput((pud)))                                        \
        luaL_error((L), "operation allowed only on output ports");  \
} while(0)

#define CheckIsInput(L, pud) do {                                   \
    if(!PortIsInput((pud)))                                         \
        luaL_error((L), "operation allowed only on input ports");   \
} while(0)

#define CheckSpace(pud, n)      (((pud)->nframes - (pud)->bufp) >= (n))
static nframes_t Avail(pud_t * pud) /* available space / frames */
    { return pud->nframes >= pud->bufp ? pud->nframes - pud->bufp : 0; }

#define NOT_AVAILABLE {                                             \
    (void)L; (void)pud; /* unused */                                \
    return luaL_error(L, "method not available for this port type");\
}

static size_t MidiSpace(pud_t *pud);

static int GetBuffer(lua_State *L)
    {
    pud_t *pud = pud_check(L, 1);
    
    CheckProcess(L, pud);
    if(pud->buf != NULL) 
        luaL_error(L, "buffer already retrieved");

    pud->nframes = pud->cud->nframes;
    pud->buf = jack_port_get_buffer(pud->port, pud->nframes);
    if(pud->buf == NULL) 
        luaL_error((L), "cannot get port buffer");
    pud->bufp = 0;

    if(PortIsMidi(pud))
        {
        if(PortIsOutput(pud))
            {
            jack_midi_clear_buffer(pud->buf);
            lua_pushinteger(L, MidiSpace(pud));
            return 1;
            }
        else /* PortIsInput(pud) */
            {
            pud->nframes = jack_midi_get_event_count(pud->buf); /* no. of events */
            lua_pushinteger(L, pud->nframes);
            lua_pushinteger(L, jack_midi_get_lost_event_count(pud->buf));
            return 2;
            }
        }
    lua_pushinteger(L, pud->nframes);
    return 1;
    }

static int RawBuffer(lua_State *L)
    {
    pud_t *pud = pud_check(L, 1);
    
    CheckProcess(L, pud);
    if(pud->buf == NULL) 
        luaL_error(L, "buffer not retrieved");

    lua_pushlightuserdata(L, pud->buf);
    lua_pushinteger(L, pud->nframes);
    return 2;
    }


/*--------------------------------------------------------------------------*
 | Default audio type                                                       |
 *--------------------------------------------------------------------------*/

#define BUF(pud) ((sample_t*)((pud)->buf))

static int AudioWrite(lua_State *L, pud_t *pud)
    {
    int i;
    nframes_t n, space;
    CheckBuffer(L, pud);
    CheckIsOutput(L, pud);
    space = Avail(pud);
    n = 0; i = 2;
    while(lua_isnumber(L, i) & (n < space))
        {
        BUF(pud)[pud->bufp + n] = lua_tonumber(L, i);
        i++; n++;
        }
    pud->bufp += n;
    lua_pushinteger(L, n);
    return 1;
    }


static int AudioClear(lua_State *L, pud_t *pud) 
    {
    nframes_t n, space;
    CheckBuffer(L, pud);
    CheckIsOutput(L, pud);
    space = Avail(pud);
    n = luaL_optinteger(L, 2, space);
    n = n < space ? n : space;
    if(n > 0)
        memset( &BUF(pud)[pud->bufp], 0, sizeof(sample_t)*n);
    pud->bufp += n;
    lua_pushnumber(L, n);
    return 1;
    }


static int AudioRead(lua_State *L, pud_t *pud) 
    {
    nframes_t n, avail;
    nframes_t count = luaL_optinteger(L, 2, 1);
    CheckBuffer(L, pud);
    CheckIsInput(L, pud);
    avail = Avail(pud);
    if(avail == 0) /* end of buffer */
        { lua_pushnil(L); return 1; }
    count = count <= avail ? count : avail;
    luaL_checkstack(L, count, "cannot grow Lua stack");
    n = 0;
    while(n < count)
        { lua_pushnumber(L, BUF(pud)[pud->bufp+n]); n++; }
    pud->bufp += n;
    return n;   
    }


static int AudioSeek(lua_State *L, pud_t *pud)
    {
    size_t pos;
    int isnum;
    CheckBuffer(L, pud);
    pos = lua_tointegerx(L, 2, &isnum);
    if(isnum)
        {
        if(pos > pud->nframes)
            luaL_error(L, "position is out of range");
        pud->bufp = pos;
        }
    lua_pushinteger(L, pud->bufp);
    lua_pushinteger(L, Avail(pud));
    return 2;
    }

static int AudioCopy(lua_State *L, pud_t *pud) 
    {
    nframes_t srccount, dstcount, countmax, count;
    pud_t *srcpud = pud_check(L, 2);
    /* check ports compatibility: */
    if(!PortIsAudio(srcpud)) 
        return luaL_error(L, "invalid source port type");
    if(!PortIsInput(srcpud))
        return luaL_error(L, "source port is not an input port");
    if(!PortIsOutput(pud))
        return luaL_error(L, "destination port is not an output port");
    /* check that both buffers have been retrieved */
    CheckBuffer(L, pud);
    CheckBuffer(L, srcpud);
    /* determine the number of samples to copy */
    if(((dstcount = Avail(pud)) == 0) || ((srccount = Avail(srcpud)) == 0))
        { lua_pushinteger(L, 0); return 1; }
    countmax = srccount > dstcount ? dstcount : srccount;
    count = luaL_optinteger(L, 3, countmax);
    if(count > countmax)
        count = countmax;

    /* eventually copy the samples and advance the positions */
    if(count > 0)
        {
        memcpy(pud->buf, srcpud->buf, count * sizeof(nframes_t));
        srcpud->bufp += count;
        pud->bufp += count;
        }

    lua_pushinteger(L, count);
    return 1;
    }

#undef BUF

/*--------------------------------------------------------------------------*
 | Default MIDI type port                                                   |
 *--------------------------------------------------------------------------*/

static size_t MidiSpace(pud_t *pud) /* space for data */
    { 
    return jack_midi_max_event_size(pud->buf) -
        sizeof(nframes_t) - sizeof(size_t); /* time and size */
    }

static int MidiWrite(lua_State *L, pud_t *pud)
    {
    size_t space, size;
    nframes_t time;
    jack_midi_data_t *data, *dst;
    CheckBuffer(L, pud);
    CheckIsOutput(L, pud);
    space = MidiSpace(pud);
    time = luaL_checkinteger(L, 2);
    data = (jack_midi_data_t*)luaL_checklstring(L, 3, &size);
    if(size == 0)
        luaL_error(L, "midi data must have at least one byte");
    if(size > space) return 0;
    if((dst = jack_midi_event_reserve(pud->buf, time, size)) == NULL)
        return 0;
    memcpy(dst, data, size);
    lua_pushinteger(L, MidiSpace(pud));
    return 1;
    }


static int MidiRead(lua_State *L, pud_t *pud)
    {
    uint32_t index;
    jack_midi_event_t event;
    CheckBuffer(L, pud);
    CheckIsInput(L, pud);
    index = luaL_optinteger(L, 2, pud->bufp);
    if(Avail(pud)==0) /* no more events */
        { lua_pushnil(L); return 1; }
    pud->bufp++;
    if(jack_midi_event_get(&event, pud->buf, index) != 0) /* ENODATA */
        { lua_pushnil(L); return 1; }
    lua_pushinteger(L, event.time);
    lua_pushlstring(L, (const char*)(event.buffer), event.size);
    return 2;
    }

static int MidiSeek(lua_State *L, pud_t *pud) 
    {
    uint32_t index;
    int isnum;
    CheckBuffer(L, pud);
    CheckIsInput(L, pud);
        
    index = lua_tointegerx(L, 2, &isnum);
    if(isnum)
        {
        if(index > pud->nframes)
            luaL_error(L, "index is out of range");
        pud->bufp = index;
        }
    lua_pushinteger(L, pud->bufp);
    lua_pushinteger(L, Avail(pud));
    return 2;
    }

static int MidiClear(lua_State *L, pud_t *pud) NOT_AVAILABLE

static int MidiCopy(lua_State *L, pud_t *pud)
    {
    nframes_t countmax, count, i;
    jack_midi_event_t event;
    pud_t *srcpud = pud_check(L, 2);
    /* check ports compatibility: */
    if(!PortIsMidi(srcpud)) 
        return luaL_error(L, "invalid source port type");
    if(!PortIsInput(srcpud))
        return luaL_error(L, "source port is not an input port");
    if(!PortIsOutput(pud))
        return luaL_error(L, "destination port is not an output port");
    /* check that both buffers have been retrieved */
    CheckBuffer(L, pud);
    CheckBuffer(L, srcpud);
    if((countmax = Avail(srcpud)) == 0)
        { lua_pushinteger(L, 0); return 1; }
    count = luaL_optinteger(L, 3, countmax);
    if(count > countmax)
        count = countmax;
    for(i = 0; i < count; i++)
        {
        /* read event from source port */
        if(jack_midi_event_get(&event, srcpud->buf, srcpud->bufp) != 0) /* ENODATA */
            { lua_pushinteger(L, i); return 1; }
        /* write event to destination port */
        if(jack_midi_event_write(pud->buf, event.time, event.buffer, event.size) !=0)
            { lua_pushinteger(L, i); return 1; }
        srcpud->bufp++; 
        }
    lua_pushinteger(L, count); 
    return 1;
    }

/*--------------------------------------------------------------------------*
 | Custom port type                                                         |
 *--------------------------------------------------------------------------*/

static int CustomWrite(lua_State *L, pud_t *pud)
    {
    int i;
    size_t sz;
    const char *sample;
    nframes_t n, space;
    CheckBuffer(L, pud);
    CheckIsOutput(L, pud);
    space = Avail(pud);
    n = 0; i = 2;
    while(lua_isstring(L, i) & (n < space))
        {
        sample = luaL_checklstring(L, i, &sz);
        if(sz != pud->samplesize)
            return luaL_error(L, "invalid sample size");
        memcpy((void*)((char*)pud->buf + pud->bufp*pud->samplesize), sample, pud->samplesize);
        i++; n++;
        pud->bufp++;
        }
    lua_pushinteger(L, n);
    return 1;
    }

static int CustomClear(lua_State *L, pud_t *pud)
    {
    nframes_t n, space;
    CheckBuffer(L, pud);
    CheckIsOutput(L, pud);
    space = Avail(pud);
    n = luaL_optinteger(L, 2, space);
    n = n < space ? n : space;
    if(n > 0)
        memset(((char*)pud->buf + pud->bufp*pud->samplesize), 0, pud->samplesize*n);
    pud->bufp += n;
    lua_pushnumber(L, n);
    return 1;
    }


static int CustomRead(lua_State *L, pud_t *pud)
    {
    nframes_t n, avail;
    nframes_t count = luaL_optinteger(L, 2, 1);
    CheckBuffer(L, pud);
    avail = Avail(pud);
    if(avail == 0) /* end of buffer */
        { lua_pushnil(L); return 1; }
    count = count <= avail ? count : avail;
    luaL_checkstack(L, count, "cannot grow Lua stack");
    n = 0;
    while(n < count)
        { 
        lua_pushlstring(L, (char*)pud->buf + (pud->bufp+n)*pud->samplesize, pud->samplesize);
        n++; 
        }
    pud->bufp += n;
    return n;   
    }

#define CustomSeek AudioSeek

static int CustomCopy(lua_State *L, pud_t *pud) NOT_AVAILABLE /* @@ CUSTOM PORT TYPE */

/*--------------------------------------------------------------------------*
 | Port-type dependent functions                                            |
 *--------------------------------------------------------------------------*/

#define SWITCH(x)                                                       \
static int Switch##x (lua_State *L)                                     \
    {                                                                   \
    pud_t *pud = pud_check(L, 1);                                       \
    CheckProcess(L, pud);                                               \
    if(PortIsAudio(pud)) { return Audio##x(L, pud); }                   \
    if(PortIsMidi(pud)) { return Midi##x(L, pud); }                     \
    if(PortIsCustom(pud)) { return Custom##x(L, pud); }                 \
    return luaL_error(L, UNEXPECTED_ERROR); /* unknown port type */     \
    }

SWITCH(Write)
SWITCH(Clear)
SWITCH(Read)
SWITCH(Seek)
SWITCH(Copy)

#undef SWITCH

/*--------------------------------------------------------------------------*
 | Registration                                                             |
 *--------------------------------------------------------------------------*/

void buffer_drop_all(cud_t *cud)
/* reset buffers pointers for all this client's ports
 * (called at the end of the process callback)
 */
    {
    pud_t *pud;
    pud = SIMPLEQ_FIRST(&(cud->fifo));
    while(pud)
        {
        pud->buf = NULL; 
        pud->nframes = 0; 
        pud->bufp = 0;
        pud = SIMPLEQ_NEXT(pud, cudfifoentry);
        }
    }

static const struct luaL_Reg PFunctions [] = 
    {
        { "get_buffer", GetBuffer },
        { "raw_buffer", RawBuffer },
        { "write", SwitchWrite },
        { "clear", SwitchClear },
        { "read", SwitchRead },
        { "seek", SwitchSeek },
        { "copy", SwitchCopy },
        { NULL, NULL } /* sentinel */
    };


int luajack_open_buffer(lua_State *L, int state_type)
    {
    if(state_type==ST_PROCESS)
        luaL_setfuncs(L, PFunctions, 0);
    return 1;
    }

