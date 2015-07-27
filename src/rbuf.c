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
 * Lock-free ringbuffers													*
 ****************************************************************************/

#include "internal.h"

#define rbuf_has_pipe(rud) ((rud)->pipefd[0] != -1)
#define rbuf_readfd(rud) (rud)->pipefd[0]
#define rbuf_writefd(rud) (rud)->pipefd[1]


static int rbuf_new(lua_State *L, cud_t *cud, size_t sz, int mlock, int usepipe)
/* Creates a new ringbuffer and returns the key.
 * On error, calls luaL_error
 */
	{
	int pipefd[2];
	rud_t *rud;

	if(usepipe)
		{
		/* create a pipe to associate with this ringbuffer */
		if(syncpipe_new(pipefd) == -1)
			return luaL_error(L, "cannot create pipe");
		}
	else
		pipefd[0] = pipefd[1] = -1;
	
	if((rud = rud_new()) == NULL)
		return luaL_error(L, "cannot create userdata");

	rud->cud = cud;
	rud->pipefd[0] = pipefd[0];
	rud->pipefd[1] = pipefd[1];
	
	if((rud->rbuf = ringbuffer_new(sz, mlock)) == NULL)
		{ 
		if(usepipe) { close(pipefd[0]);	close(pipefd[1]); }
		CancelRudValid(rud);
		return luaL_error(L, "cannot create ringbuffer");
		}
	luajack_verbose("created ringbuffer %u (size=%u, mlock=%d, usepipe=%d)\n", 
			rud->key, sz, mlock ? 1 : 0, usepipe ? 1 : 0);

	return rud->key;
	}

void rbuf_pipe_write(lua_State *L, rud_t *rud)
	{
	if(!rbuf_has_pipe(rud)) return;
	if(syncpipe_write(rbuf_writefd(rud)) < 0)
		luaL_error(L, "rbuf pipe error");
	}

void rbuf_pipe_read(lua_State *L, rud_t *rud)
	{
	if(!rbuf_has_pipe(rud)) return;
	if(syncpipe_read(rbuf_readfd(rud)) < 0)
		luaL_error(L, "rbuf pipe error");
	}


static int Ringbuffer(lua_State *L)
	{
	cud_t *cud;
	size_t sz;
	int mlock, usepipe, key;

	luajack_checkcreate();

	cud = cud_check(L, 1);
	sz = luaL_checkinteger(L, 2);
	mlock = lua_toboolean(L, 3);
	usepipe = lua_toboolean(L, 4);
	key = rbuf_new(L, cud, sz, mlock, usepipe);
	lua_pushinteger(L, key);	
	return 1;
	}

static int RingbufferGetFd(lua_State *L)
	{
	rud_t *rud = rud_check(L, 1);
	if(!rbuf_has_pipe(rud)) return 0;
	lua_pushinteger(L, rbuf_readfd(rud));
	return 1;
	}

static int RingbufferWrite(lua_State *L)
	{
	int rc;
	rud_t *rud = rud_check(L, 1);
	rc = ringbuffer_luawrite(rud->rbuf, L, 2);
	rbuf_pipe_write(L, rud);
	return rc;
	}

static int RingbufferRead(lua_State *L)
	{
	int rc;
	rud_t *rud = rud_check(L, 1);
	rc = ringbuffer_luaread(rud->rbuf, L);
	rbuf_pipe_read(L, rud);
	return rc;
	}

static int RingbufferPeek(lua_State *L)
	{
	rud_t *rud = rud_check(L, 1);
	return ringbuffer_luapeek(rud->rbuf, L);
	}

static int RingbufferReset(lua_State *L)
	{
	rud_t *rud = rud_check(L, 1);
	luajack_verbose("reset ringbuffer %u\n", rud->key);
	return ringbuffer_creset(rud->rbuf);
	}

#define METHODS  \
		{ "ringbuffer_getfd", RingbufferGetFd },	\
		{ "ringbuffer_write", RingbufferWrite },	\
		{ "ringbuffer_read", RingbufferRead },		\
		{ "ringbuffer_peek", RingbufferPeek },		\
		{ "ringbuffer_reset", RingbufferReset }		\

static const struct luaL_Reg MFunctions[] = 
	{
		{ "ringbuffer", Ringbuffer },
		METHODS,
		{ NULL, NULL } /* sentinel */
	};

static const struct luaL_Reg TFunctions[] = 
	{
		METHODS,
		{ NULL, NULL } /* sentinel */
	};

#define PFunctions TFunctions

static void rbuf_free(rud_t *rud)
	{
	ringbuffer_free(rud->rbuf);
	CancelRudValid(rud);
	}

void rbuf_free_all(cud_t *cud)
	{
	rud_t *rud = rud_first(0);
	while(rud)
		{
		if(IsRudValid(rud) && (rud->cud == cud))
			rbuf_free(rud);
		rud = rud_next(rud);
		}
	}

int luajack_open_rbuf(lua_State *L, int state_type)
	{
	switch(state_type)
		{
		case ST_MAIN: luaL_setfuncs(L, MFunctions, 0); break;
		case ST_PROCESS: luaL_setfuncs(L, PFunctions, 0); break;
		case ST_THREAD: luaL_setfuncs(L, TFunctions, 0); break;
		default:
			break;
		}
	return 1;
	}

