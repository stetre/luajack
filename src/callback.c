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
 * Non real-time callbacks                                                  *
 ****************************************************************************/

#include "internal.h"

/*--------------------------------------------------------------------------*
 | Lua callbacks                                                            |
 *--------------------------------------------------------------------------*/

#define BEGIN(cbname) do {                                              \
    if(luajack_exiting())                                               \
        { evt_free(evt); return 0; }                                    \
    /* push the callback on the stack */                                \
    if(lua_rawgeti(L, LUA_REGISTRYINDEX, cud->cbname) != LUA_TFUNCTION) \
        { evt_free(evt); return luaL_error(L, UNEXPECTED_ERROR); }      \
    /* push arg[1] = client key */                                      \
    lua_pushinteger(L, evt->client_key);                                \
} while(0);

#define EXEC(nargs) do {                                                \
    /* execute the script code */                                       \
    if(lua_pcall(L, (nargs) + 1 /* for client key */, 0, 0) != LUA_OK)  \
        { evt_free(evt); return lua_error(L); }                         \
} while(0)

#define END() do {                                                      \
    return 0;                                                           \
} while(0)

static int Lua_SampleRate(lua_State *L, cud_t *cud, evt_t *evt)
    {
    BEGIN(SampleRate);
    lua_pushinteger(L, evt->nframes);
    EXEC(1);
    END();
    }

static int Lua_Xrun(lua_State *L, cud_t *cud, evt_t *evt)
    {
    BEGIN(Xrun);
    EXEC(0);
    END();
    }

static int Lua_GraphOrder(lua_State *L, cud_t *cud, evt_t *evt)
    {
    BEGIN(GraphOrder);
    EXEC(0);
    END();
    }

static int Lua_Freewheel(lua_State *L, cud_t *cud, evt_t *evt)
    {
    BEGIN(Freewheel);
    lua_pushstring(L, evt->op ? "starting" : "stopping");
    EXEC(1);
    END();
    }

static int Lua_ClientRegistration(lua_State *L, cud_t *cud, evt_t *evt)
    {
    BEGIN(ClientRegistration);
    lua_pushstring(L, evt->arg1);
    lua_pushstring(L, evt->op ? "registered" : "unregistered");
    EXEC(2);
    END();
    }

static int Lua_PortRegistration(lua_State *L, cud_t *cud, evt_t *evt)
    {
    BEGIN(PortRegistration);
    lua_pushstring(L, evt->arg1);
    lua_pushstring(L, evt->op ? "registered" : "unregistered");
    EXEC(2);
    END();
    }

static int Lua_PortRename(lua_State *L, cud_t *cud, evt_t *evt)
    {
    BEGIN(PortRename);
    lua_pushstring(L, evt->arg1);
    lua_pushstring(L, evt->arg2);
    EXEC(2);
    END();
    }

static int Lua_PortConnect(lua_State *L, cud_t *cud, evt_t *evt)
    {
    BEGIN(PortConnect);
    lua_pushstring(L, evt->arg1);
    lua_pushstring(L, evt->arg2);
    lua_pushstring(L, evt->op ? "connected" : "disconnected");
    EXEC(3);
    END();
    }

static int Lua_Shutdown(lua_State *L, cud_t *cud, evt_t *evt)
    {
    BEGIN(Shutdown);
    client_pushstatus(L, evt->code);
    lua_pushstring(L, evt->arg1); /* reason */
    EXEC(2);
    END();
    }

static int Lua_Latency(lua_State *L, cud_t *cud, evt_t *evt)
    {
    BEGIN(Latency);
    latency_pushmode(L, evt->mode);
    EXEC(1);
    END();
    }

static int Lua_Session(lua_State *L, cud_t *cud, evt_t *evt)
    {
#define event evt->session_event
    const char *command;
    BEGIN(Latency);
    session_pushtype(L, event->type);
    lua_pushstring(L, event->session_dir);
    lua_pushstring(L, event->client_uuid);
/*  EXEC() */
#define nres 3
    if(lua_pcall(L, 4, nres, 0) != LUA_OK) 
        { evt_free(evt); return lua_error(L); }
    command = luaL_optstring(L, -3, NULL);
    event->command_line = command ? strdup(command) : NULL;
    event->flags = 0;
    event->flags |= session_checkflag(L, -2);
    event->flags |= session_checkflag(L, -1);
    jack_session_reply(cud->client, event);
    lua_pop(L, nres);
    /* jack_session_event_free(event) is called by evt_free() */
    END();
#undef nres
#undef event
    }

#undef BEGIN
#undef EXEC
#undef END

int callback_flush(lua_State* L)
    {
    evt_t *evt;
    cud_t *cud;
    unsigned int n;

    luajack_checkmain();    

    if(luajack_exiting()) return 0;

    n = evt_count(); 
    /* Only events scheduled up to now are dispatched.
     * The corresponding callbacks are executed in the main context, so that
     * the main script can be considered virtually single-threaded. */

    while( n>0 && ((evt = evt_remove()) != NULL))
        {
        n--;
        cud = cud_search(evt->client_key);
        if(!cud || !IsCudValid(cud)) /* client was closed: skip event */
            { evt_free(evt); continue; }
        switch(evt->type)
            {
            case CT_SampleRate: Lua_SampleRate(L, cud, evt); break;
            case CT_Xrun: Lua_Xrun(L, cud, evt); break;
            case CT_GraphOrder: Lua_GraphOrder(L, cud, evt); break;
            case CT_Freewheel: Lua_Freewheel(L, cud, evt); break;
            case CT_ClientRegistration: Lua_ClientRegistration(L, cud, evt); break;
            case CT_PortRegistration: Lua_PortRegistration(L, cud, evt); break;
            case CT_PortRename: Lua_PortRename(L, cud, evt); break;
            case CT_PortConnect: Lua_PortConnect(L, cud, evt); break;
            case CT_Shutdown: Lua_Shutdown(L, cud, evt); break;
            case CT_Latency: Lua_Latency(L, cud, evt); break;
            case CT_Session: Lua_Session(L, cud, evt); break;
            default:
                evt_free(evt);
                return luaL_error(L, UNEXPECTED_ERROR);
            }
        evt_free(evt);
        }
#if 1
    lua_gc(L, LUA_GCCOLLECT, 0);
#endif
    return 0;
    }


/*--------------------------------------------------------------------------*
 | Pre - callbacks                                                          |
 *--------------------------------------------------------------------------*/
/* These are the callbacks as directly called by JACK.
 * Each callback is entirely executed in C and does involve execution of Lua
 * code). It just queues an event and promptly returns the control to JACK.
 * The event will then be dispatched in the main context.
 * Note that the pthread that each callback is executed in may or may not be
 * the main pthread: this is a JACK's implementation detail, and we must not
 * make any assumption.
 */

#define cud ((cud_t*)arg)

#define BEGIN(cbname) evt_t *evt; do {                          \
    if(!IsCudValid(cud)) return 0;                              \
    if(cud->cbname == LUA_NOREF) return 0;                      \
    if((evt = evt_new()) == NULL)                               \
        return luajack_error("cannot allocate callback event"); \
    evt->client_key = cud->key;                                 \
    evt->type = CT_##cbname;                                    \
} while(0)

#define END(rc) do {                        \
    evt_insert(evt);                        \
    return rc;                              \
} while(0)

#define MAX_ARG_LEN 1024 /* should be enough ... */
#define Copy(dst, src)  do {                                        \
    size_t len = strnlen((src), MAX_ARG_LEN);                       \
    if(len == MAX_ARG_LEN) luajack_error(UNEXPECTED_ERROR);         \
    (dst) = Malloc(len+1); /* deallocated by evt_free() */  \
    if(!(dst)) luajack_error(UNEXPECTED_ERROR);                     \
    strncpy((dst), (src), len);                                     \
    (dst)[len]='\0';                                                \
} while(0)

static int SampleRate(nframes_t nframes, void *arg)
    {
    BEGIN(SampleRate);
    evt->nframes = nframes;
    cud->sample_rate = nframes; /* sniff.. */
    END(0);
    }

static int Xrun(void *arg)
    {
    BEGIN(Xrun);
    END(0);
    }

static int GraphOrder(void *arg)
    {
    BEGIN(GraphOrder);
    END(0);
    }

static int  Freewheel_(int starting, void *arg)
    {
    BEGIN(Freewheel);
    evt->mode = starting;
    END(0);
    }

static void Freewheel(int starting, void *arg)
    { Freewheel_(starting,arg); }


static int ClientRegistration_(const char *name, int reg, void *arg)
    {
    BEGIN(ClientRegistration);
    Copy(evt->arg1, name);
    evt->op = reg;
    END(0);
    }

static void ClientRegistration(const char *name, int reg, void *arg)
    { ClientRegistration_(name,reg,arg); } 

static int  PortRegistration_(jack_port_id_t portid, int reg, void *arg)
    {
    port_t *port; 
    const char *name;
    BEGIN(PortRegistration);
    port = jack_port_by_id(cud->client, portid);
    name = port ? jack_port_name(port) : "???";
    Copy(evt->arg1, name);
    evt->op = reg;
    END(0);
    }

static void PortRegistration(jack_port_id_t port, int reg, void *arg)
    { PortRegistration_(port, reg, arg); }

static int PortRename_(jack_port_id_t portid, const char *old_name, const char *new_name, void *arg)
    {
    BEGIN(PortRename);
    (void) portid; /* unused */
    Copy(evt->arg1, old_name);
    Copy(evt->arg2, new_name);
    END(0);
    }

static void PortRename(jack_port_id_t portid, const char *old_name, const char *new_name, void *arg)
	{ PortRename_(portid, old_name, new_name, arg); }

static int  PortConnect_(jack_port_id_t a, jack_port_id_t b, int connect, void *arg)
    {
    port_t *port1, *port2; 
    const char *name1, *name2;
    BEGIN(PortConnect);
    port1 = jack_port_by_id(cud->client, a);
    port2 = jack_port_by_id(cud->client, b);
    name1 = port1 ? jack_port_name(port1) : "???";
    name2 = port2 ? jack_port_name(port2) : "???";
    Copy(evt->arg1, name1);
    Copy(evt->arg2, name2);
    evt->op = connect;
    END(0);
    }

static void PortConnect(jack_port_id_t a, jack_port_id_t b, int connect, void *arg)
    { PortConnect_(a,b,connect,arg); }

static int Shutdown_(jack_status_t code, const char *reason, void *arg)
/* InfoShutdown, actually */
    {
    BEGIN(Shutdown);
    evt->code = code;
    Copy(evt->arg1, reason);
    END(0);
    }

static void Shutdown(jack_status_t code, const char *reason, void *arg)
    { Shutdown_(code,reason,arg); }

static int Latency_(jack_latency_callback_mode_t mode, void *arg)
    {
    BEGIN(Latency);
    evt->mode = mode;
    END(0);
    }

static void Latency(jack_latency_callback_mode_t mode, void *arg)
    { Latency_(mode,arg); }

static int Session_(jack_session_event_t *event, void *arg)
    {
    BEGIN(Session);
    evt->session_event = event;
    END(0);
    }

static void Session(jack_session_event_t *event, void *arg)
    { Session_(event,arg); }

#undef cud
#undef BEGIN
#undef END


/*--------------------------------------------------------------------------*
 | Callbacks Registration                                                   |
 *--------------------------------------------------------------------------*/

static int SetShutdown(client_t* cli, JackInfoShutdownCallback func, void *arg) 
    { jack_on_info_shutdown(cli, func, arg); return 0; }
#define SetFreewheel jack_set_freewheel_callback
#define SetSampleRate jack_set_sample_rate_callback
#define SetClientRegistration jack_set_client_registration_callback
#define SetPortRegistration jack_set_port_registration_callback
#define SetPortRename jack_set_port_rename_callback
#define SetPortConnect jack_set_port_connect_callback
#define SetGraphOrder jack_set_graph_order_callback
#define SetXrun jack_set_xrun_callback
#define SetLatency jack_set_latency_callback
#define SetSession jack_set_session_callback

#define Unregister(cud, cb) do {                                        \
    if((cud)->cb != LUA_NOREF)                                          \
        {                                                               \
        luaL_unref(L, LUA_REGISTRYINDEX, (cud)->cb);                    \
        (cud)->cb = LUA_NOREF;                                          \
        }                                                               \
} while(0)

#define Register(cud, cb) do {                                          \
    Unregister((cud), cb);                                              \
    lua_pushvalue(L, 2); /* the function */                             \
    (cud)->cb = luaL_ref(L, LUA_REGISTRYINDEX);                         \
    if(Set##cb((cud)->client, (cb), (void*)(cud)) != 0)                 \
        {                                                               \
        Unregister((cud), cb);                                          \
        return luaL_error(L, "cannot register callback");               \
        }                                                               \
    /* DBG("reference = %p\n", (void*)((cud)->cb));     */              \
} while(0)

#define CheckFunction() do {                                            \
    if(!lua_isfunction(L, 2))                                           \
        return luaL_error(L, "bad argument #2 (function expected)");    \
} while(0)

#define REGISTRATION_FUNCTION(cb)       \
static int Callback##cb (lua_State *L)  \
    {                                   \
    cud_t *cud = cud_check(L, 1);       \
    CheckFunction();                    \
    Register(cud, cb);                  \
    return 0;                           \
    }

REGISTRATION_FUNCTION(Shutdown)
REGISTRATION_FUNCTION(Freewheel)
REGISTRATION_FUNCTION(SampleRate)
REGISTRATION_FUNCTION(ClientRegistration)
REGISTRATION_FUNCTION(PortRegistration)
REGISTRATION_FUNCTION(PortRename)
REGISTRATION_FUNCTION(PortConnect)
REGISTRATION_FUNCTION(GraphOrder)
REGISTRATION_FUNCTION(Xrun)
REGISTRATION_FUNCTION(Latency)
REGISTRATION_FUNCTION(Session)

void callback_unregister(lua_State *L, cud_t *cud)
/* release callbacks references from the the registry */
    {
    Unregister(cud, Shutdown);
    Unregister(cud, Freewheel);
    Unregister(cud, SampleRate);
    Unregister(cud, ClientRegistration);
    Unregister(cud, PortRegistration);
    Unregister(cud, PortRename);
    Unregister(cud, PortConnect);
    Unregister(cud, GraphOrder);
    Unregister(cud, Xrun);
    Unregister(cud, Latency);
    }

/*--------------------------------------------------------------------------*
 | Registration                                                             |
 *--------------------------------------------------------------------------*/

static const struct luaL_Reg MFunctions[] = 
    {
        { "shutdown_callback", CallbackShutdown },
        { "freewheel_callback", CallbackFreewheel },
        { "sample_rate_callback", CallbackSampleRate },
        { "client_registration_callback", CallbackClientRegistration },
        { "port_registration_callback", CallbackPortRegistration },
        { "port_rename_callback", CallbackPortRename },
        { "port_connect_callback", CallbackPortConnect },
        { "graph_order_callback", CallbackGraphOrder },
        { "xrun_callback", CallbackXrun },
        { "latency_callback", CallbackLatency },
        { "session_callback", CallbackSession },
        { NULL, NULL } /* sentinel */
    };

int luajack_open_callback(lua_State *L, int state_type)
    {
    if(state_type == ST_MAIN) 
        luaL_setfuncs(L, MFunctions, 0);
    return 1;
    }
