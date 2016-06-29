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
 * Creating & manipulating ports / Looking up ports							*
 ****************************************************************************/

#include "internal.h"

static int port_pushflags(lua_State *L, int flags)
	{
	int index;
#define Add(x) do { lua_pushstring(L, x); lua_pushstring(L, "|"); n+=2; } while(0)
	int n=0;
	luaL_checkstack(L, 20, "cannot grow Lua stack");
	if(flags & JackPortIsInput) Add("input");
	if(flags & JackPortIsOutput) Add("output");
	if(flags & LuaJackPortIsAudio) Add("audio");
	if(flags & LuaJackPortIsMidi) Add("midi");
	if(flags & LuaJackPortIsCustom) Add("custom");
	if(flags & JackPortIsPhysical) Add("is_physical");
	if(flags & JackPortCanMonitor) Add("can_monitor");
	if(flags & JackPortIsTerminal) Add("is_terminal");
	if(n>0)
		{ lua_pop(L, 1); lua_concat(L, n-1); }
	else
		lua_pushstring(L, ""); /* should not happen if status is well formed */
#undef Add
	lua_newtable(L);
	index = lua_gettop(L);
#define Add(x) do {	lua_pushboolean(L,1); lua_setfield(L,index, x); } while(0)
	if(flags & JackPortIsInput) Add("input");
	if(flags & JackPortIsOutput) Add("output");
	if(flags & LuaJackPortIsAudio) Add("audio");
	if(flags & LuaJackPortIsMidi) Add("midi");
	if(flags & LuaJackPortIsCustom) Add("custom");
	if(flags & JackPortIsPhysical) Add("is_physical");
	if(flags & JackPortCanMonitor) Add("can_monitor");
	if(flags & JackPortIsTerminal) Add("is_terminal");
#undef Add
	return 2;
	}

/*--------------------------------------------------------------------------*
 | Port creation   															|
 *--------------------------------------------------------------------------*/


static int PortRegister(lua_State *L, unsigned long flags)
	{
#define options_index 3
	int custom = 0;
	const char *port_name, *port_type;
	unsigned long bufsize = 0;
	port_t *port;
	pud_t * pud;
	cud_t *cud;

	luajack_checkcreate();
	
	cud = cud_check(L, 1);
	port_name = luaL_checkstring(L, 2);

	if(flags & LuaJackPortIsAudio)
		port_type = JACK_DEFAULT_AUDIO_TYPE;
	else if(flags & LuaJackPortIsMidi)
		port_type = JACK_DEFAULT_MIDI_TYPE;
	else
		{ custom = 1; port_type = NULL; }

	if(lua_istable(L, options_index))
		{
		lua_getfield(L, options_index, "is_physical");
		if(lua_toboolean(L, -1)) flags |= JackPortIsPhysical;
		lua_getfield(L, options_index, "can_monitor");
		if(lua_toboolean(L, -1)) flags |= JackPortCanMonitor;
		lua_getfield(L, options_index, "is_terminal");
		if(lua_toboolean(L, -1)) flags |= JackPortIsTerminal;
		if(custom)
			{
			lua_getfield(L, options_index, "port_type");
			port_type = luaL_checkstring(L, -1);
			lua_getfield(L, options_index, "buffer_size");
			bufsize = luaL_checkinteger(L, -1);
			}
		}

	if(custom && (port_type == NULL))
		return luaL_error(L, "missing port_type");
	
	/* register the port in JACK */
	port = jack_port_register(cud->client, port_name, port_type, 
									flags & ~LuaJackPortFlagsMask, bufsize);
	if(!port)
		return luaL_error(L, "cannot register port");

	if((pud = pud_new(cud)) == NULL)
		{
		jack_port_unregister(cud->client, port);
		return luaL_error(L, "cannot create userdata for port");
		}

	pud->port = port;
	pud->flags = flags;
	pud->samplesize = bufsize;
	pud->buf = NULL; 
	pud->nframes = 0; 
	pud->bufp = 0;
	luajack_verbose("registering port '%s'\n", jack_port_name(port));
	lua_pushinteger(L, pud->key);
	return 1;
#undef options_index
	}	

static int InputAudioPort(lua_State *L)
	{ return PortRegister(L, JackPortIsInput | LuaJackPortIsAudio); }

static int OutputAudioPort(lua_State *L)
	{ return PortRegister(L, JackPortIsOutput | LuaJackPortIsAudio); }

static int InputMidiPort(lua_State *L)
	{ return PortRegister(L, JackPortIsInput | LuaJackPortIsMidi); }

static int OutputMidiPort(lua_State *L)
	{ return PortRegister(L, JackPortIsOutput | LuaJackPortIsMidi); }

#if 0 /* @@ CUSTOM PORT TYPE */
static int InputCustomPort(lua_State *L)
	{ return PortRegister(L, JackPortIsInput | LuaJackPortIsCustom); }

static int OutputCustomPort(lua_State *L)
	{ return PortRegister(L, JackPortIsOutput | LuaJackPortIsCustom); }
#endif


static void port_close(pud_t *pud);

static int PortUnregister(lua_State *L)
	{
	pud_t *pud = pud_check(L, 1);
	port_close(pud);
	return 0;	
	}


static int PortTypeSize (lua_State *L)
	{ lua_pushinteger(L, jack_port_type_size()); return 1; }

static int PortNameSize(lua_State *L)
	{ lua_pushinteger(L, jack_port_name_size()); return 1; }

/*--------------------------------------------------------------------------*
 | Connect/disconnect functions    											|
 *--------------------------------------------------------------------------*/

static int CheckPorts(lua_State *L, client_t *client, const char* name1, const char* name2,
				/* return values: */ const char **src, const char **dst)
/* Checks if ports exist and determines which is the source and which is the destination.
 * Returns 1 on success, raises an error if an error occurs.
 */
	{
	port_t *port1, *port2;
	/* find ports */
	if((port1 = jack_port_by_name(client, name1)) == NULL)
		return luaL_error(L, "unknown port '%s'", name1);
	if((port2 = jack_port_by_name(client, name2)) == NULL)
		return luaL_error(L, "unknown port '%s'", name2);

	/* check which port is the source (output) and which is the destination (input) */
	*src = *dst = NULL;
	if(jack_port_flags(port1) & JackPortIsOutput)
		{
		*src = name1;
		if(jack_port_flags(port2) & JackPortIsInput)
			*dst = name2;
		}
	else
		{
		*dst = name1;
		if(jack_port_flags(port2) & JackPortIsOutput)
			*src = name2;
		}
	if(*dst ==  NULL)
		return luaL_error(L, "both output ports");
	if(*src ==  NULL)
		return luaL_error(L, "both input ports");
	return 1;
	}

static int Connect_(lua_State *L, client_t *client, const char* name1, const char* name2)
	{
	int rc;
	const char *src, *dst;
	
	CheckPorts(L,client,name1,name2, &src, &dst);
	rc = jack_connect(client, src, dst);
	if(rc)
		{
		if(rc == EEXIST)
			return luaL_error(L, "connection exists");
		return luaL_error(L, "jack_connect() error %d", rc);
		}
	luajack_verbose("connected ports '%s' and '%s'\n", src, dst);
	return 0;
	}

static int DisconnectAll_(lua_State *L, client_t *client, const char* name, port_t *port1)
/* port and name are alternatives */
	{
	int rc;
	port_t *port = port1;
	if(name)
		{
		port = jack_port_by_name(client, name);
		if(!port)
			return luaL_error(L, "unknown port '%s'", name);
		}
	else
		name = jack_port_name(port);
	rc = jack_port_disconnect(client, port);
	if(rc)
		return luaL_error(L, "jack_port_disconnect() error %d", rc); 
	luajack_verbose("disconnected port '%s' from all ports\n", name!=NULL ? name : "???");
	return 0;
	}


static int Disconnect_(lua_State *L, client_t *client, const char* name1, const char* name2)
	{
	int rc;
	const char *src, *dst;
	CheckPorts(L,client,name1,name2, &src, &dst);
	rc = jack_disconnect(client, src, dst);
	if(rc)
		return luaL_error(L, "jack_disconnect() error %d", rc);
	luajack_verbose("disconnected ports '%s' and '%s'\n", src, dst);
	return 0;
	}

static int PortnameConnect(lua_State *L)
	{
	cud_t *cud = cud_check(L, 1);
	const char *portname1 = luaL_checkstring(L, 2);
	const char *portname2 = luaL_checkstring(L, 3);
	return Connect_(L, cud->client, portname1, portname2);
	}

static int PortConnect(lua_State *L) 
	{
	const char *portname1, *portname2;
	pud_t *pud = pud_check(L, 1);
	portname2 = luaL_checkstring(L, 2);
	portname1 = jack_port_name(pud->port);
	if(!portname1)
		luaL_error(L, UNEXPECTED_ERROR);
	return Connect_(L, pud->cud->client, portname1, portname2);
	}

static int PortnameDisconnect(lua_State *L)
	{
	cud_t *cud = cud_check(L, 1);
	const char *portname1 = luaL_checkstring(L, 2);
	const char *portname2 = luaL_optstring(L, 3, NULL);
	if(!portname2)
		return DisconnectAll_(L, cud->client, portname1, NULL);
	return Disconnect_(L, cud->client, portname1, portname2);
	}

static int PortDisconnect(lua_State *L) 
	{
	const char *portname1;
	pud_t *pud = pud_check(L, 1);
	const char *portname2 = luaL_optstring(L, 2, NULL);
	if(!portname2)
		return DisconnectAll_(L, pud->cud->client, NULL, pud->port);
	portname1 = jack_port_name(pud->port);
	if(!portname1)
		luaL_error(L, UNEXPECTED_ERROR);
	return Disconnect_(L, pud->cud->client, portname1, portname2);
	}

/*--------------------------------------------------------------------------*
 | Other functions    														|
 *--------------------------------------------------------------------------*/

static int PortName(lua_State *L)
	{
	pud_t *pud = pud_check(L, 1);
	const char* name = jack_port_name(pud->port);
	lua_pushstring(L, name ? name : "???");
	return 1;
	}
 
static int PortShortName(lua_State *L)
	{
	pud_t *pud = pud_check(L, 1);
	const char* name = jack_port_short_name(pud->port);
	lua_pushstring(L, name ? name : "???");
	return 1;
	}

static int PortFlags(lua_State *L)
	{
	pud_t *pud = pud_check(L, 1);
	pud->flags = (pud->flags & LuaJackPortFlagsMask) | jack_port_flags(pud->port);
	return port_pushflags(L, pud->flags);
	}

static int PortnameFlags(lua_State *L)
	{
	int flags;
	const char *type;
	cud_t *cud = cud_check(L, 1);
	const char *name = luaL_checkstring(L, 2);
	port_t *port = jack_port_by_name(cud->client, name);
	if(!port)
		{ 
		lua_pushnil(L); 
		lua_pushstring(L, "unknown port");
		return 2;
		}
	flags = jack_port_flags(port);
	type = jack_port_type(port);
	if(strncmp(type, JACK_DEFAULT_AUDIO_TYPE, strlen(type)) == 0)
		flags = flags | LuaJackPortIsAudio;
	else if(strncmp(type, JACK_DEFAULT_MIDI_TYPE, strlen(type)) == 0)
		flags = flags | LuaJackPortIsMidi;
	else
		flags = flags | LuaJackPortIsCustom;
	return port_pushflags(L, flags);
	}
  

static int Uuid_(lua_State *L, port_t *port)
	{
	uint64_t uuid = jack_port_uuid(port);
	lua_pushinteger(L, uuid);
	return 1;
	}

static int PortUuid(lua_State *L)
	{ 
	pud_t *pud = pud_check(L, 1);
	return Uuid_(L, pud->port);
	}
  
static int PortnameUuid(lua_State *L)
	{
	cud_t *cud = cud_check(L, 1);
	const char *name = luaL_checkstring(L, 2);
	port_t *port = jack_port_by_name(cud->client, name);
	if(!port) return 0;
	return Uuid_(L, port);
	}

static int Type_(lua_State *L, port_t *port)
	{
	const char* type = jack_port_type(port);
	lua_pushstring(L, type ? type : "???");
	return 1;
	}

static int PortType(lua_State *L)
	{
	pud_t *pud = pud_check(L, 1);
	return Type_(L, pud->port);
	}
	
static int PortnameType(lua_State *L)
	{
	cud_t *cud = cud_check(L, 1);
	const char *name = luaL_checkstring(L, 2);
	port_t *port = jack_port_by_name(cud->client, name);
	if(!port) return 0;
	return Type_(L, port);
	}

static int PortnameExists(lua_State *L)
	{
	cud_t *cud = cud_check(L, 1);
	const char *name = luaL_checkstring(L, 2);
	lua_pushboolean(L, jack_port_by_name(cud->client, name) != NULL );
	return 1;
	}
	

static int PortIsMine(lua_State *L)
	{
	cud_t *cud = cud_check(L, 1);
	pud_t *pud = pud_check(L, 2);
	lua_pushboolean(L, jack_port_is_mine(cud->client, pud->port));
	return 1;
	}

static int PortnameIsMine(lua_State *L)
	{
	cud_t *cud = cud_check(L, 1);
	const char *name = luaL_checkstring(L, 2);
	port_t *port = jack_port_by_name(cud->client, name);
	if(port)
		lua_pushboolean(L, jack_port_is_mine(cud->client, port));
	else
		lua_pushnil(L);
	return 1;
	}

#if 0 /* DEPRECATED */
static int PortSetName(lua_State *L)
	{
	pud_t *pud = pud_check(L, 1);
	const char *port_name = luaL_checkstring(L, 2);
	int rc = jack_port_set_name(pud->port, port_name);
	if(rc != 0) 
		return luajack_strerror(L, rc);
	return 0;
	}
#endif

static int GetPorts(lua_State *L)
	{
#define filter_index 2
	const char** ports;
	int n = 0;
	unsigned long flags = 0;
	const char *dir;
	cud_t *cud = cud_check(L, 1);
	const char *name_pattern = NULL;
	const char *type_pattern = NULL;
	
	if(lua_istable(L, filter_index))
		{
		lua_getfield(L, filter_index, "name_pattern");
		name_pattern = luaL_optstring(L, -1, NULL);
		lua_getfield(L, filter_index, "type_pattern");
		type_pattern = luaL_optstring(L, -1, NULL);
		lua_getfield(L, filter_index, "direction");
		dir = luaL_optstring(L, -1,  NULL);
		if(dir)
			{
			if(strncmp(dir, "input", strlen(dir)) == 0)
				flags |= JackPortIsInput;
			else if(strncmp(dir, "output", strlen(dir)) == 0)
				flags |= JackPortIsOutput;
			else
				return luaL_error(L, "invalid 'direction' in filter");
			}
		lua_getfield(L, filter_index, "is_physical");
		if(lua_toboolean(L, -1)) flags |= JackPortIsPhysical;
		lua_getfield(L, filter_index, "can_monitor");
		if(lua_toboolean(L, -1)) flags |= JackPortCanMonitor;
		lua_getfield(L, filter_index, "is_terminal");
		if(lua_toboolean(L, -1)) flags |= JackPortIsTerminal;
		}
		
/* @@ known problem: SIGSEGV with some patterns (e.g "*audio")
 *    Replace it with lpeg? i.e., get all ports and use lpeg.re ?
 */
DBG("get_ports() name pattern = '%s'\n", name_pattern ? name_pattern : "");
DBG("get_ports() type pattern = '%s'\n", type_pattern ? type_pattern : "");
	ports = jack_get_ports(cud->client, name_pattern, type_pattern, flags);
	lua_newtable(L);
	if(ports)
		{
		while(ports[n])
			{ 
			lua_pushstring(L, ports[n]); 
			lua_seti(L, -2, ++n);
			}
		jack_free(ports);
		}
	return 1;	
#undef filter_index
	}

  
/*--------------------------------------------------------------------------*
 | Connections lookup             											|
 *--------------------------------------------------------------------------*/

static int PortConnections_(lua_State *L, port_t *port, int list)
	{
	int n = 0;
	const char **con;
	lua_pushinteger(L, jack_port_connected(port));
	if(list == 0) return 1;
	/* list all connections */	
	con = jack_port_get_connections(port);
	lua_newtable(L);
	if(con)
		{
		while(con[n])
			{ 
			lua_pushstring(L, con[n]); 
			lua_seti(L, -2, ++n); 
			}
		jack_free(con);
		}
	return 2;
	}

static int PortConnections(lua_State *L)
	{
	pud_t *pud = pud_check(L, 1);
	int list = lua_toboolean(L, 2);
	return PortConnections_(L, pud->port, list);
	}

static int PortnameConnections(lua_State *L)
	{
	cud_t *cud = cud_check(L, 1);
	const char *name = luaL_checkstring(L, 2);
	int list = lua_toboolean(L, 3);
	port_t *port = jack_port_by_name(cud->client, name);
	if(!port)
		return luaL_error(L, "unknown port");
	return PortConnections_(L, port, list);
	}

static int PortConnectedTo(lua_State *L)
	{
	pud_t *pud = pud_check(L, 1);
	const char *port_name = luaL_checkstring(L, 2);
	int rc = jack_port_connected_to(pud->port, port_name);
	lua_pushboolean(L, rc);
	return 1;
	}

static int PortnameConnectedTo(lua_State *L)
	{
	port_t *port;
	const char *portname1, *portname2;
	cud_t *cud = cud_check(L, 1);
	portname1 = luaL_checkstring(L, 2);
	portname2 = luaL_checkstring(L, 3);
	port = jack_port_by_name(cud->client, portname1);
	if(!port)
		luaL_error(L, "unknown port");
	lua_pushboolean(L, jack_port_connected_to(port, portname2));
	return 1;
	}


/*--------------------------------------------------------------------------*
 | Alias handling                              								|
 *--------------------------------------------------------------------------*/

static int SetAlias_(lua_State *L, port_t *port, int arg)
	{
	const char *port_name = luaL_checkstring(L, arg);
	int rc = jack_port_set_alias(port, port_name);
	if(rc != 0) 
		return luajack_strerror(L, rc);
	return 0;
	}

static int UnsetAlias_(lua_State *L, port_t *port, int arg)
	{
	const char *port_name = luaL_checkstring(L, arg);
	int rc = jack_port_unset_alias(port, port_name);
	if(rc != 0) 
		return luajack_strerror(L, rc);
	return 0;
	}

static int Aliases_(lua_State *L, port_t *port)
	{
	int n, rc;
	char *alias[2];

	alias[0] = (char*)Malloc(jack_port_name_size());
	alias[1] = (char*)Malloc(jack_port_name_size());

	if(!alias[0])
		return luaL_error(L, "cannot allocate memory");
	if(!alias[1])
		{ Free(alias[0]); return luaL_error(L, "cannot allocate memory"); }
		
	n = jack_port_get_aliases(port, alias);  

	rc = 0;
	switch(n)
		{
		case 0: lua_pushnil(L); rc = 1; break;
		case 1: lua_pushstring(L, alias[0]); rc = 1; break;
		case 2: lua_pushstring(L, alias[0]); lua_pushstring(L, alias[1]); rc = 2; break;
		default: break;
		}
	Free(alias[0]);
	Free(alias[1]);
	if(rc==0)
		return luaL_error(L, "unexpected result %d from jack_port_get_aliases()", n);
	return rc;	
	}

static int PortSetAlias(lua_State *L)
	{
	pud_t *pud = pud_check(L, 1);
	return SetAlias_(L, pud->port, 2);
	}

static int PortUnsetAlias(lua_State *L)
	{
	pud_t *pud = pud_check(L, 1);
	return UnsetAlias_(L, pud->port, 2);
	}

static int PortAliases(lua_State *L)
	{
	pud_t *pud = pud_check(L, 1);
	return Aliases_(L, pud->port);
	}
	
static int PortnameSetAlias(lua_State *L)
	{
	cud_t *cud = cud_check(L, 1);
	const char *name = luaL_checkstring(L, 2);
	port_t *port = jack_port_by_name(cud->client, name);
	if(!port)
		{ lua_pushboolean(L, 0); return 1; }
	return SetAlias_(L, port, 3);
	}
	
static int PortnameUnsetAlias(lua_State *L)
	{
	cud_t *cud = cud_check(L, 1);
	const char *name = luaL_checkstring(L, 2);
	port_t *port = jack_port_by_name(cud->client, name);
	if(!port)
		{ lua_pushboolean(L, 0); return 1; }
	return UnsetAlias_(L, port, 3);
	}

static int PortnameAliases(lua_State *L)
	{
	cud_t *cud = cud_check(L, 1);
	const char *name = luaL_checkstring(L, 2);
	port_t *port = jack_port_by_name(cud->client, name);
	if(!port) return 0;
	return Aliases_(L, port);
	}

/*--------------------------------------------------------------------------*
 | Monitoring                                  								|
 *--------------------------------------------------------------------------*/

static int Monitor_(lua_State *L, port_t *port, int onoff)
	{
	int rc;
	/* jack_port_request_monitor(port, onoff); */
	rc = jack_port_ensure_monitor(port, onoff);
	if(rc != 0) 
		return luajack_strerror(L, rc);
	return 0;
	}

static int PortMonitor(lua_State *L)
	{
	pud_t *pud = pud_check(L, 1);
	int onoff = luajack_checkonoff(L, 2);
	return Monitor_(L, pud->port, onoff);
	}

static int PortMonitoring(lua_State *L)
	{
	pud_t *pud = pud_check(L, 1);
	lua_pushboolean(L, jack_port_monitoring_input(pud->port));
	return 1;	
	}

static int PortnameMonitor(lua_State *L)
	{
	cud_t *cud = cud_check(L, 1);
	const char *portname = luaL_checkstring(L, 2);
	int onoff = luajack_checkonoff(L, 3);
	port_t *port = jack_port_by_name(cud->client, portname);
	if(!port)
		return luaL_error(L, "unknown port");
	/* jack_port_request_monitor_by_name(client, name, onoff); */
	return Monitor_(L, port, onoff);
	}

static int PortnameMonitoring(lua_State *L)
	{
	int rc = 0;
	cud_t *cud = cud_check(L, 1);
	const char *name = luaL_checkstring(L, 2);
	port_t *port = jack_port_by_name(cud->client, name);
	if(port)
		rc = jack_port_monitoring_input(port);
	lua_pushboolean(L, rc);
	return 1;	
	}

#if 0

static int PortTypeBufferSize(lua_State *L)
	{
	cud_t *cud = cud_check(L, 1);
	const char *port_type = luaL_checkstring(L, 2);
	size_t bufsize;
	bufsize = jack_port_type_get_buffer_size(cud->client, port_type);
	lua_pushinteger(L, bufsize);
	return 1;
	}

static pud_t* PortToPud(lua_State *L, port_t *port)
	{
	pud_t *pud;
	lua_getfield(L, LUA_REGISTRYINDEX, PORT_LIST);
	lua_pushlightuserdata(L, (void*)port);
	lua_gettable(L, -2);
	pud = pud_check(L, -1);
	lua_pop(L, 2); /* PORT_LIST and pud */
	return pud;
	}

static int PortByName(lua_State *L)
	{
	pud_t *pud;
	cud_t *cud = cud_check(L, 1);
	const char *name = luaL_checkstring(L, 2);
	port_t *port = jack_port_by_name(cud->client, name);
	if(!port)
		{ lua_pushnil(L); return 1; }
	pud = PortToPud(L, port);
	if(pud->cud != cud) /* only the owner is allowed */
		lua_pushnil(L);
	return 1;
	}

static int PortById(lua_State *L)
/* port, mine = client:port_by_id(port_id) */
	{
	cud_t *cud = cud_check(L, 1);
	jack_port_id_t id = luaL_checkinteger(L, 2);
	port_t *port = jack_port_by_id(cud->client, id);
	if(!port)
		{ lua_pushnil(L); return 1; }
	lua_getfield(L, LUA_REGISTRYINDEX, PORT_LIST);
	lua_pushlightuserdata(L, (void*)port);
	lua_gettable(L, -2);
	pud_check(L, -1);
	lua_pushboolean(L, jack_port_is_mine(cud->client, port));
	return 2;	
	}
#endif


/*--------------------------------------------------------------------------*
 | Registration                              								|
 *--------------------------------------------------------------------------*/

static void port_close(pud_t *pud)
	{
	const char *name = jack_port_name(pud->port);
	luajack_verbose("unregistering port '%s'\n", name);
	jack_port_unregister(pud->cud->client, pud->port);
	CancelPudValid(pud);
	}

void port_close_all(cud_t *cud)
/* unregister all ports for this client */
	{
	pud_t *pud = pud_first(0);
	while(pud)
		{
		if(IsPudValid(pud) && (pud->cud == cud))
			port_close(pud);
		pud = pud_next(pud);
		}
	}

#if 0
/* NOT IMPLEMENTED: */
	{ "port_type_buffer_size", PortTypeBufferSize },
	{ "port_by_id", PortById },  port_ids are hidden by LuaJack callbacks
	{ "port_by_name", PortByName },
#endif

static const struct luaL_Reg MFunctions[] = 
	{
		{ "port_type_size", PortTypeSize },
		{ "port_name_size", PortNameSize },
		{ "input_audio_port", InputAudioPort },
		{ "output_audio_port", OutputAudioPort },
		{ "input_midi_port", InputMidiPort },
		{ "output_midi_port", OutputMidiPort },
#if 0 /* @@ CUSTOM PORT TYPE */
		{ "input_custom_port", InputCustomPort }, 
		{ "output_custom_port", OutputCustomPort }, 
#endif
		{ "port_unregister", PortUnregister },
		{ "port_connect", PortConnect },
		{ "nport_connect", PortnameConnect },
		{ "connect", PortnameConnect },
		{ "port_disconnect", PortDisconnect },
		{ "nport_disconnect", PortnameDisconnect },
		{ "disconnect", PortnameDisconnect },
		{ "port_name", PortName },
		{ "port_short_name", PortShortName },
		{ "port_flags", PortFlags },
		{ "nport_flags", PortnameFlags },
		{ "port_uuid", PortUuid },
		{ "nport_uuid", PortnameUuid },
		{ "port_type", PortType },
		{ "nport_type", PortnameType },
		{ "nport_exists", PortnameExists },
		{ "port_is_mine", PortIsMine },
		{ "nport_is_mine", PortnameIsMine },
/*		{ "port_set_name", PortSetName }, DEPRECATED */
		{ "get_ports", GetPorts },
		{ "port_set_alias", PortSetAlias },
		{ "nport_set_alias", PortnameSetAlias },
		{ "port_unset_alias", PortUnsetAlias },
		{ "nport_unset_alias", PortnameUnsetAlias },
		{ "port_aliases", PortAliases },
		{ "nport_aliases", PortnameAliases },
		{ "port_monitor", PortMonitor },
		{ "nport_monitor", PortnameMonitor },
		{ "port_monitoring", PortMonitoring },
		{ "nport_monitoring", PortnameMonitoring },
		{ "port_connections", PortConnections },
		{ "nport_connections", PortnameConnections },
		{ "port_connected", PortConnectedTo },
		{ "nport_connected", PortnameConnectedTo },
		{ NULL, NULL } /* sentinel */
	};

int luajack_open_port(lua_State *L, int state_type)
	{
	if(state_type == ST_MAIN)
		luaL_setfuncs(L, MFunctions, 0);
	return 1;
	}

