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
 * Session API																*
 ****************************************************************************/

#include "internal.h"

int session_pushtype(lua_State *L, jack_session_event_type_t type)
	{
	switch(type)
		{
		case JackSessionSave:			lua_pushstring(L, "save"); break;
		case JackSessionSaveAndQuit:	lua_pushstring(L, "save_and_quit"); break;
		case JackSessionSaveTemplate:	lua_pushstring(L, "save_template"); break;
		default:
			luaL_error(L, "invalid session event type %u", type);
		}
	return 1;

	return 0;
	}

static jack_session_event_type_t session_checktype(lua_State *L, int arg)
	{
	const char* type = luaL_checkstring(L, arg);
#define CASE(s,t) do { if(strncmp(type, s, strlen(s)) == 0) return t; } while(0)
	CASE("save",JackSessionSave);
	CASE("save_and_quit",JackSessionSaveAndQuit);
	CASE("save_template",JackSessionSaveTemplate);
#undef CASE
	return (jack_session_event_type_t)luaL_error(L, "invalid session event type '%s'", type);
	}

jack_session_flags_t session_checkflag(lua_State *L, int arg)
	{
	const char* flag = luaL_optstring(L, arg, NULL);
	if(!flag) return (jack_session_flags_t )0;
#define CASE(s,f) do { if(strncmp(flag, s, strlen(s)) == 0) return f; } while(0)
	CASE("save_error",JackSessionSaveError);
	CASE("need_terminal",JackSessionNeedTerminal);
#undef CASE
	return (jack_session_flags_t)luaL_error(L, "invalid session flag '%s'", flag);
	}

static int session_pushcommands(lua_State *L, jack_session_command_t *command)
/* pushes: { command1, command2, ..., commandN }
 * where: 
 * commandK = { uuid=uuid, name=client_name, command=command,    (strings)
 *              save_error=true|nil, need_terminal=true|nil      (booleans)
 *            }
 */
	{
	int index, i = 0;
	lua_newtable(L);
	index = lua_gettop(L);
	while(command[i].uuid)
		{
		lua_newtable(L);
DBG("session_notify %d: %s %s %s\n",i, command[i].uuid, command[i].client_name, command[i].command);
		lua_pushstring(L, command[i].uuid); lua_setfield(L, -2, "uuid");
		lua_pushstring(L, command[i].client_name);	lua_setfield(L, -2, "name");
		lua_pushstring(L, command[i].command); lua_setfield(L, -2, "command");
#define PushFlag(f, s) do {	\
		lua_pushboolean(L, command[i].flags & (f)); lua_setfield(L, -2, (s)); \
} while (0)
		PushFlag(JackSessionSaveError, "save_error");
		PushFlag(JackSessionNeedTerminal, "need_terminal");
#undef PushFlag
		lua_rawseti(L, index, ++i);
		}
	return i;
	}

/*--------------------------------------------------------------------------*
 | Client       				  											|
 *--------------------------------------------------------------------------*/

/* see callbacks.c */

/*--------------------------------------------------------------------------*
 | Manager      				  											|
 *--------------------------------------------------------------------------*/

static int SessionNotify(lua_State *L)
	{
	int n;
	jack_session_command_t *command;
	cud_t *cud = cud_check(L, 1);
	const char *target = luaL_optstring(L, 2, NULL);
	jack_session_event_type_t type = session_checktype(L, 3);
	const char *path = luaL_optstring(L, 4, NULL);
	command = jack_session_notify(cud->client, target, type, path);
	n = session_pushcommands(L, command);
	luajack_verbose("session_notify ('%s'): target=%s, path=%s -> %d replies\n"
		,lua_tostring(L,3), target ? target : "all", path ? path : "(none)", n);
	jack_session_commands_free(command);	
	return 1;
	}

static int HasSessionCallback(lua_State *L)
	{
	cud_t *cud = cud_check(L, 1);
	const char *name = luaL_checkstring(L, 2);
	int rc = jack_client_has_session_callback(cud->client, name);
	if(rc==-1)
		luaL_error(L, "jack_client_has_session_callback() error");
	lua_pushboolean(L, rc);
	return 1;
	}

static int ReserveClientName(lua_State *L)
	{
	cud_t *cud = cud_check(L, 1);
	const char *name = luaL_checkstring(L, 2);
	const char *uuid = luaL_checkstring(L, 3);
	int rc = jack_reserve_client_name(cud->client, name, uuid);
	if(rc!=0)
		luaL_error(L, "jack_reserve_client_name() returned %d", rc);
	return 0;
	}


/*--------------------------------------------------------------------------*
 | Registration                              								|
 *--------------------------------------------------------------------------*/

static const struct luaL_Reg MFunctions[] = 
	{
		{ "session_notify", SessionNotify },
		{ "has_session_callback", HasSessionCallback },
		{ "reserve_client_name", ReserveClientName },
		{ NULL, NULL } /* sentinel */
	};

int luajack_open_session(lua_State *L, int state_type)
	{
	switch(state_type)
		{
		case ST_MAIN: luaL_setfuncs(L, MFunctions, 0); break;
		default:
			break;
		}
	return 1;
	}

