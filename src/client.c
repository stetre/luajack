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
 * Creating & manipulating clients                                          *
 ****************************************************************************/

#include "internal.h"
#include <jack/thread.h>

int client_pushstatus(lua_State *L, int status)
    {
#define Add_(s) do { lua_pushstring(L, s); lua_pushstring(L, "|"); n+=2; } while(0)
#define Add(x, s) do { if(status & x) Add_(s); } while(0)
    int n=0;
    luaL_checkstack(L, 30, "cannot grow Lua stack");
    if(status==0)
        { Add_("no_error"); }
    else
        {
        Add(JackFailure, "failure");
        Add(JackInvalidOption, "invalid_option");
        Add(JackNameNotUnique, "name_not_unique");
        Add(JackServerStarted, "server_started");
        Add(JackServerFailed, "server_failed");
        Add(JackServerError, "server_error");
        Add(JackNoSuchClient, "no_such_client");
        Add(JackLoadFailure, "load_failure");
        Add(JackInitFailure, "init_failure");
        Add(JackShmFailure, "shm_failure");
        Add(JackVersionError, "version_error");
        Add(JackBackendError, "backend_error");
        Add(JackClientZombie, "client_zombie");
        }
    if(n>0)
        { lua_pop(L, 1); lua_concat(L, n-1); }
    else
        lua_pushstring(L, ""); /* should not happen if status is well formed */
    return 1;
#undef Add_
#undef Add
    }

static void ThreadInitCallback(void *arg)
/* Called for every additional pthread created by JACK for this client
 * (callbacks are executed in these threads). */
    {
#define cud ((cud_t*)arg)
    luajack_sigblock();
    DBG("ThreadInitCallback %u/%u %p\n",gettid(), getpid(), (void*)cud);
#undef cud
    }

/*--------------------------------------------------------------------------*
 | Functions                                                                |
 *--------------------------------------------------------------------------*/

static int ClientOpen(lua_State *L)
    {
#define options_index 2
    const char *name;
    const char *server_name = NULL;
    const char *session_id = NULL;
    cud_t *cud = NULL;
    client_t *cli = NULL;
    jack_options_t options = (jack_options_t)0;
    jack_status_t status;
    
	luajack_checkcreate();

    name = lua_tostring(L, 1);
    if(!name)
        return luaL_error(L, "bad argument #1 (string expected)");

    if(lua_istable(L, options_index))
        {
        lua_getfield(L, options_index, "use_exact_name");
        if(lua_toboolean(L, -1)) options = (jack_options_t)(options | JackUseExactName);
        lua_getfield(L, options_index, "no_start_server");
        if(lua_toboolean(L, -1)) options = (jack_options_t)(options | JackNoStartServer);
        lua_getfield(L, options_index, "server_name");
        server_name = luaL_optstring(L, -1, NULL);
        if(server_name != NULL) options = (jack_options_t)(options | JackServerName);
        lua_getfield(L, options_index, "session_id");
        session_id = luaL_optstring(L, -1, NULL);
        if(session_id != NULL) options = (jack_options_t)(options | JackSessionID);
        }

    /* create the client */
    if((server_name != NULL) && (session_id !=NULL))
        cli = jack_client_open(name, options, &status, server_name, session_id);
    else if(server_name!=NULL)
        cli = jack_client_open(name, options, &status, server_name);
    else if(session_id !=NULL)
        cli = jack_client_open(name, options, &status, session_id);
    else
        cli = jack_client_open(name, options, &status);

    if(!cli)
        {
        client_pushstatus(L, status);
        return luaL_error(L, "cannot create client (%s)", lua_tostring(L, -1));
        }

    if((cud = cud_new()) == NULL)
        {
        jack_client_close(cli);
        return luaL_error(L, "cannot create userdata for client");
        }

    cud->client = cli;
    jack_set_thread_init_callback(cud->client, ThreadInitCallback, (void*)cud);
    luajack_verbose("created client '%s'\n", jack_get_client_name(cud->client));

    cud->sample_rate = jack_get_sample_rate(cud->client);
    cud->buffer_size = jack_get_buffer_size(cud->client);

    lua_pushinteger(L, cud->key);
    return 1;
#undef options_index
    }

static int client_close(lua_State *L, cud_t *cud);

static int ClientClose(lua_State *L)
    {
    cud_t *cud = cud_check(L, 1);
    return client_close(L, cud);
    }

static int ClientNameSize(lua_State *L)
    {
    lua_pushinteger(L, jack_client_name_size()); 
    return 1;
    }

static int ClientName(lua_State *L)
    {
    cud_t *cud = cud_check(L, 1);
    const char *name = jack_get_client_name(cud->client);
    if(!name) return 0;
    lua_pushstring(L, name);
    return 1;
    }

static int ClientUuid(lua_State *L)
    {
    cud_t *cud = cud_check(L, 1);
    const char *uuid = jack_client_get_uuid(cud->client);
    if(!uuid) return 0;
    lua_pushstring(L, uuid);
    return 1;
    }

static int ClientNameToUuid(lua_State *L)   
    {
    cud_t *cud = cud_check(L, 1);
    const char *name = luaL_checkstring(L, 2);
    char *uuid = jack_get_uuid_for_client_name(cud->client, name);
    if(uuid)
        { lua_pushstring(L, uuid); jack_free(uuid); }
    else
        lua_pushnil(L);
    return 1;
    }

static int ClientUuidToName(lua_State *L)
    {
    cud_t *cud = cud_check(L, 1);
    const char *uuid = luaL_checkstring(L, 2);
    char *name = jack_get_client_name_by_uuid(cud->client, uuid);
    if(name)
        { lua_pushstring(L, name); jack_free(name); }
    else
        lua_pushnil(L);
    return 1;
    }

size_t luajack_active_clients; /* no. of active clients */

static int Activate_(cud_t *cud)
    {
	if(!luajack_ismainthread())	return -1;
	luajack_active_clients++;
    if((jack_activate(cud->client)) != 0)
		{ luajack_active_clients--; return -1; }
    luajack_verbose("client activated (active=%u)\n", luajack_active_clients);
    return 0;
    }


static int Deactivate_(cud_t *cud)
    {
	if(!luajack_ismainthread()) return -1;
	if(luajack_active_clients == 0) return -1;
	luajack_active_clients--;
    if((jack_deactivate(cud->client)) != 0)
		{ luajack_active_clients++; return -1; }
    luajack_verbose("client deactivated (active=%u)\n", luajack_active_clients);
    return 0;
    }


static int ClientActivate(lua_State *L)
    {
    cud_t *cud = cud_check(L, 1);
    if(Activate_(cud) !=0)
		return luaL_error(L, "cannot activate client");
    return 0;
    }

static int ClientDeactivate(lua_State *L)
    {
    cud_t *cud = cud_check(L, 1);
    if(Deactivate_(cud) !=0)
		return luaL_error(L, "cannot deactivate client");
    return 0;
    }

static int IsRealtime(lua_State *L)
    {
    cud_t *cud = cud_check(L, 1);
    lua_pushboolean(L, jack_is_realtime(cud->client));
    return 1;
    }
    
/*--------------------------------------------------------------------------*
 | Registration                                                             |
 *--------------------------------------------------------------------------*/

static int client_close(lua_State *L, cud_t *cud)
    {
    const char* name;
    Deactivate_(cud); /* no more callbacks, please... */
    thread_free_all(cud);
    rbuf_free_all(cud);
    port_close_all(cud);
    /* close client */
    name = jack_get_client_name(cud->client);
    luajack_verbose("closing client '%s'\n", name ? name : "???");
    jack_client_close(cud->client);
    /* release callbacks references from the registry */
    if(L)
        { callback_unregister(L, cud); process_unregister(cud); }
#if 0 
    DBG("cud->process_state %p\n", (void*) cud->process_state); 
    if(cud->process_state)  
        lua_close(cud->process_state);
    /* @@ This is commented out because in some exit combinations it causes a double
	 * free (when the Lua state was already closed elsewhere before arriving at this
	 * point, I suspect: to investigate better). 
     * Without closing the state there is a memory leak, but it should cause no harm
     * as long as the application does not open and close plenty of clients.
     * The OS will take care of releasing the memory.
     */
#endif
    CancelCudValid(cud);
    return 0;
    }

void client_close_all(void)
/* close all clients (to be called at exit) */
    {
    cud_t *cud;
    DBG("client_close_all %u/%u\n", gettid(), getpid());

    cud = cud_first(0);
    while(cud)
        {
        if(IsCudValid(cud))
            client_close(NULL, cud);
        cud = cud_next(cud);
        }
    /* Eventually release all LuaJak objects entries.*/ 
    tud_free_all();
    rud_free_all();
    pud_free_all();
    cud_free_all();
    /* Note: objects entries are released only now because their databases
     * are not thread-safe (we cannot use locks because databases are used
     * from within the real-time thread). So, when for example a port is
     * deleted, we just mark its pud entry as 'not valid' and leave it in
     * the database. The creation of objects should give no race condition
     * problems as long as it is done only from the main context and without
     * **any** active client.
     */
    }

static const struct luaL_Reg MFunctions[] = 
    {
        { "client_open", ClientOpen },
        { "client_name_size", ClientNameSize },
        { "is_realtime", IsRealtime },
        { "client_close", ClientClose },
        { "client_name", ClientName },
        { "client_uuid", ClientUuid },
        { "client_name_to_uuid", ClientNameToUuid },
        { "client_uuid_to_name", ClientUuidToName },
        { "activate", ClientActivate },
        { "deactivate", ClientDeactivate },
        { NULL, NULL } /* sentinel */
    };

int luajack_open_client(lua_State *L, int state_type)
    {
    if(state_type == ST_MAIN) 
		{
		luajack_active_clients = 0;
        luaL_setfuncs(L, MFunctions, 0);
		}
    return 1;
    }

