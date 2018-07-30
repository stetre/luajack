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
 * Ringbuffers database               										*
 ****************************************************************************/

#include "internal.h"

static int cmp(rud_t *rud1, rud_t *rud2) /* the compare function */
	{ return (rud1->key < rud2->key ? -1 : rud1->key > rud2->key); } 

static RB_HEAD(rudtree_s, rud_s) Head = RB_INITIALIZER(&Head);

RB_PROTOTYPE_STATIC(rudtree_s, rud_s, entry, cmp) 
RB_GENERATE_STATIC(rudtree_s, rud_s, entry, cmp) 
 
static rud_t *rud_remove(rud_t *rud) 
	{ return RB_REMOVE(rudtree_s, &Head, rud); }
static rud_t *rud_insert(rud_t *rud) 
	{ return RB_INSERT(rudtree_s, &Head, rud); }
static rud_t *rud_search(uintptr_t key) 
	{ rud_t tmp; tmp.key = key; return RB_FIND(rudtree_s, &Head, &tmp); }
rud_t *rud_first(uintptr_t key) 
	{ rud_t tmp; tmp.key = key; return RB_NFIND(rudtree_s, &Head, &tmp); }
rud_t *rud_next(rud_t *rud)
	{ return RB_NEXT(rudtree_s, &Head, rud); }
#if 0
rud_t *rud_prev(rud_t *rud)
	{ return RB_PREV(rudtree_s, &Head, rud); }
rud_t *rud_min(void)
	{ return RB_MIN(rudtree_s, &Head); }
rud_t *rud_max(void)
	{ return RB_MAX(rudtree_s, &Head); }
rud_t *rud_root(void)
	{ return RB_ROOT(&Head); }
#endif

rud_t *rud_new(void)
	{
	rud_t *rud;
	if((rud = (rud_t*)Malloc(sizeof(rud_t))) == NULL)  return NULL;
	memset(rud, 0, sizeof(rud_t));
	rud->key = (uintptr_t)rud;
	if(rud_search(rud->key))
		{ Free(rud); luajack_error(UNEXPECTED_ERROR); return NULL; }
	rud->obj.type = LUAJACK_TRINGBUFFER;
	rud->obj.xud = (void*)rud;
	rud_insert(rud);
	MarkRudValid(rud);
	return rud;
	}

static void rud_free(rud_t* rud)
	{
	if(rud_search(rud->key) == rud)
		rud_remove(rud);
	Free(rud);	
	DBG("rud_free()\n");
	}

void rud_free_all(void)
	{
	rud_t *rud;
	while((rud = rud_first(0)))
		rud_free(rud);
	}


rud_t* rud_check(lua_State *L, int arg)
	{
	uintptr_t key = luaL_checkinteger(L, arg);
	rud_t *rud = rud_search(key);
	if(!rud || !IsRudValid(rud))
		luaL_error(L, "invalid ringbuffer reference");
	return rud;
	}

