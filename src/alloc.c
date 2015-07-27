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
 * Memory allocator                                                         *
 ****************************************************************************/

#include "internal.h"

/* LuaJack does not use malloc(), free() etc directly, but it inherits the
 * memory allocator from the main Lua state instead (see lua_getallocf in
 * the Lua manual).
 *
 * By doing so, one can use an alternative malloc() implementation without
 * recompiling LuaJack (one needs to recompile lua only, or execute it with 
 * LD_PRELOAD set to the path to the malloc library).
 *
 */
static lua_Alloc alloc = NULL;
static void* ud = NULL;
static lua_Alloc rt_alloc = NULL;
static void* rt_ud = NULL;

lua_Alloc luajack_allocf(void **ud_)
	{
	/* @@ change this if you want a different memory allocator for the rt threads */
	*ud_ = ud;
	return alloc;	
	}

void luajack_malloc_init(lua_State *L)
    {
    if(alloc)
        luaL_error(L, UNEXPECTED_ERROR);
    alloc = lua_getallocf(L, &ud);
	rt_alloc = luajack_allocf(&rt_ud);
	if(!rt_alloc)
		luajack_error("cannot initialize luajack_malloc()");
    }

void* luajack_main_malloc(size_t size)
    { return alloc ? alloc(ud, NULL, 0, size) : NULL; }

void luajack_main_free(void *ptr)
    {
    if(alloc) 
        alloc(ud, ptr, 0, 0);
    }

void* luajack_malloc(size_t size)
    { return rt_alloc ? rt_alloc(rt_ud, NULL, 0, size) : NULL; }

void luajack_free(void *ptr)
    { 
    if(rt_alloc) 
        rt_alloc(rt_ud, ptr, 0, 0);
    }

