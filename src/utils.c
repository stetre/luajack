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

#include "internal.h"

/*------------------------------------------------------------------------------*
 | Time utilities          														|
 *------------------------------------------------------------------------------*/

double luajack_now(void)
	{
#if _POSIX_C_SOURCE >= 199309L
	struct timespec ts;
	if(clock_gettime(CLOCK_MONOTONIC,&ts)!=0)
		{ printf("clock_gettime error\n"); return -1; }
	return ts.tv_sec + ts.tv_nsec*1.0e-9;
#else
	struct timeval tv;
	if(gettimeofday(&tv, NULL) != 0)
		{ printf("gettimeofday error\n"); return -1; }
	return tv.tv_sec + tv.tv_usec*1.0e-6;
#endif
	}

double luajack_tstosec(const struct timespec *ts)
	{
	return ts->tv_sec*1.0+ts->tv_nsec*1.0e-9;
	}

void luajack_sectots(struct timespec *ts, double seconds)
	{
	ts->tv_sec=(time_t)seconds;
	ts->tv_nsec=(long)((seconds-((double)ts->tv_sec))*1.0e9);
	}

/*------------------------------------------------------------------------------*
 | Print/debug utilities          												|
 *------------------------------------------------------------------------------*/

int luajack_noprintf(const char *fmt, ...)
	{
	(void)fmt; /* unused */
	return 0; 
	}

#if defined(DEBUG)
void luajack_printstack(lua_State *L, const char* fmat, ...)
/* prints the Lua stack */
	{
    va_list args;
	int i,r,t;
	int top = lua_gettop(L); /* stack top level */
	printf("###Lua-stack: thread=%d, state=%p top=[%d] ",gettid(), (void*)L, top);
	if(fmat)
		{
	    va_start(args,fmat);
   	 	vprintf(fmat,args);
		va_end(args);
		}
	printf("\n");

	for(i=top;i>=1;i--)
		{
		r=i-top-1; /* relative index */
		t = lua_type(L,i);
		printf("[%d][%d] ",i,r);
		switch(t)
			{
			case LUA_TSTRING: printf("'%s'",lua_tostring(L,i));	break;
			case LUA_TBOOLEAN: printf(lua_toboolean(L,i) ? "true" : "false"); break;
			case LUA_TNUMBER: printf("%g", lua_tonumber(L,i));	break;
			default:
					printf("%s", lua_typename(L,t));
			}
		printf("\n");
		}
	printf("\n");
	}
#endif



#define STRERROR_BUFSZ	256
int strerror_r(int errnum, char *buf, size_t buflen);
/* char *strerror_r(int errnum, char *buf, size_t buflen); */


int luajack_strerror(lua_State *L, int en)
	{
	int rc;
	char buf[STRERROR_BUFSZ];
	if((rc = strerror_r((en), buf, STRERROR_BUFSZ)) != 0)
		snprintf(buf, STRERROR_BUFSZ, "error %d", (en));
	lua_pushstring((L), buf);
	return lua_error(L);
	}

int luajack_checkonoff(lua_State *L, int arg)
	{
	char *onoff = luaL_checkstring(L, arg);
	if(strncmp(onoff, "on", strlen(onoff)) == 0)
		return 1;
	if(strncmp(onoff, "off", strlen(onoff)) == 0)
		return 0;
	return luaL_error(L, "invalid onoff argument");
	}


/*------------------------------------------------------------------------------*
 | Chunck loading                 												|
 *------------------------------------------------------------------------------*/

int luajack_xmove(lua_State *T, lua_State *L, int chunck_index, int last_index)
/* Checks and copies arguments between unrelated states L and T (L to T)
 * (lua_xmove() cannot be used here, because the states are not related).
 *
 * L[chunck_index] -----> arg[0] script name, or chunk
 * L[chunck_index +1] --> arg[1]
 * L[chunck_index +2] --> arg[2]
 * ...
 * L[last_index]      --> arg[N]
 * Since arguments are to be passed between unrelated states, the only admitted
 * types are: nil, boolean, number and string.
 */ 
	{
	int arg_index, argn, n, typ, nargs;
	nargs = last_index - chunck_index; /* no. of optional arguments */
	luaL_checkstack(T, nargs, "cannot grow Lua stack for thread");

	lua_newtable(T); /* "arg" table */
	arg_index = lua_gettop(T);

	lua_pushstring(T, lua_tostring(L, chunck_index)); /* arg[0] = full scriptname */
	lua_seti(T, arg_index, 0);

	argn = 1;
	for(n = chunck_index+1; n <= last_index; n++)
		{
		typ = lua_type(L, n);
		switch(typ)
			{
#define SetArg() do { lua_pushvalue(T, -1); lua_seti(T, arg_index, argn++); } while(0)
			case LUA_TNIL: lua_pushnil(T); SetArg(); break;
			case LUA_TBOOLEAN: lua_pushboolean(T, lua_toboolean(L, n)); SetArg(); break;
			case LUA_TNUMBER: lua_pushnumber(T, lua_tonumber(L, n)); SetArg(); break;
			case LUA_TSTRING: lua_pushstring(T, lua_tostring(L, n)); SetArg(); break;
			default:
				return luaL_error(L, "invalid type '%s' of argument #%d ", 
									lua_typename(L, typ), n - chunck_index);
#undef SetArg
			}
		}
	lua_pushvalue(T, arg_index);
	lua_setglobal(T, "arg");
	lua_remove(T, arg_index);
	return 0;
	}


int luajack_loadchunk(lua_State *T, lua_State *L, int chunk_index, int isscript)
/* Loads, on the state T, the Lua chunk positioned on stack L at chunk_index.
 * isscript = 1: the chunk is given as a filename (searched as a package)
 * isscript = 0: the chunk is given as a string
 */
	{
	int package_index, rc;
	if(isscript)
		{
		/* search for the full script name in package.path 
	     * fullname, errmsg = package.search(script, package.path)
	     */
		lua_getglobal(L, "package");
		package_index = lua_gettop(L);
		lua_getfield(L, package_index, "searchpath");
		lua_pushvalue(L, chunk_index);
		lua_getfield(L, package_index, "path");
		lua_remove(L, package_index);
		if((lua_pcall(L, 2, 2, 0) != LUA_OK) || lua_isnil(L, -2)) 
			{ lua_close(T); lua_error(L); }
		DBG("full script name = %s\n", lua_tostring(L, -2));
		lua_pop(L, 1); 	/* errmsg (which should be nil) */

		/* replace the passed 'script' argument with the fullname, 
		 * which should be now on top of the stack */
		lua_replace(L, chunk_index);

		/* load the script */
		if((rc = luaL_loadfile(T, lua_tostring(L, chunk_index))) != 0)
			{
			if(lua_tostring(T, -1)) 
				lua_pushstring(L, lua_tostring(T, -1));
			else
				lua_pushfstring(L, "cannot load script (luaL_loadfile() error %d)", rc);
			lua_close(T);
			return lua_error(L);
			}
		}
	else
		{
		if((rc = luaL_loadstring(T, lua_tostring(L, chunk_index))) != 0)
			{
			if(lua_tostring(T, -1)) 
				lua_pushstring(L, lua_tostring(T, -1));
			else
				lua_pushfstring(L, "cannot load string (luaL_loadstring() error %d)", rc);
			lua_close(T);
			return lua_error(L);
			}
		}
	return 0;
	}


/*------------------------------------------------------------------------------*
 | Stats                          												|
 *------------------------------------------------------------------------------*/


#define n stat->n
#define min stat->min
#define max stat->max
#define mean stat->mean
#define m2 stat->m2

void luajack_stat_reset(stat_t *stat)
	{
	min = 1e10; /* should be DBL_MAX, but this is enough... */
	max = 0;
	mean = 0;
	m2 = 0;
	n = 0;
	}

void luajack_stat_update(stat_t *stat, double sample)
	{
	double delta;
	n++;
	/* Welford algorithm */
	if(sample < min) min = sample;
	if(sample > max) max = sample;
	delta = sample - mean;
	mean = mean + delta/n;
	m2 = m2 + delta *(sample-mean);
	}

double luajack_stat_variance(stat_t *stat)
	{ return n > 1 ? m2 / (n-1) : 0; }

#undef n
#undef min
#undef max
#undef mean
#undef m2
