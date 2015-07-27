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
 * Ports database               											*
 ****************************************************************************/

#include "internal.h"

static int cmp(tud_t *tud1, tud_t *tud2) /* the compare function */
	{ return (tud1->key < tud2->key ? -1 : tud1->key > tud2->key); } 

static RB_HEAD(tudtree_s, tud_s) head = RB_INITIALIZER(&head);

RB_PROTOTYPE_STATIC(tudtree_s, tud_s, entry, cmp) 
RB_GENERATE_STATIC(tudtree_s, tud_s, entry, cmp) 
 
static tud_t *tud_remove(tud_t *tud) 
	{ return RB_REMOVE(tudtree_s, &head, tud); }
static tud_t *tud_insert(tud_t *tud) 
	{ return RB_INSERT(tudtree_s, &head, tud); }
static tud_t *tud_search(uintptr_t key) 
	{ tud_t tmp; tmp.key = key; return RB_FIND(tudtree_s, &head, &tmp); }
tud_t *tud_first(uintptr_t key) 
	{ tud_t tmp; tmp.key = key; return RB_NFIND(tudtree_s, &head, &tmp); }
tud_t *tud_next(tud_t *tud)
	{ return RB_NEXT(tudtree_s, &head, tud); }
#if 0
tud_t *tud_prev(tud_t *tud)
	{ return RB_PREV(tudtree_s, &head, tud); }
tud_t *tud_min(void)
	{ return RB_MIN(tudtree_s, &head); }
tud_t *tud_max(void)
	{ return RB_MAX(tudtree_s, &head); }
tud_t *tud_root(void)
	{ return RB_ROOT(&head); }
#endif

tud_t *tud_new(void)
	{
	tud_t *tud;
	if((tud = (tud_t*)Malloc(sizeof(tud_t))) == NULL) return NULL;
	memset(tud, 0, sizeof(tud_t));
	tud->key = (uintptr_t)tud;
	if(tud_search(tud->key))
		{ Free(tud); luajack_error(UNEXPECTED_ERROR); return NULL; }
	tud->obj.type = LUAJACK_TTHREAD;
	tud->obj.xud = (void*)tud;
	tud_insert(tud);
	MarkTudValid(tud);
	return tud;
	}

static void tud_free(tud_t* tud)
	{
	if(tud_search(tud->key) == tud)
		tud_remove(tud);
	Free(tud);	
	DBG("tud_free()\n");
	}

void tud_free_all(void)
	{
	tud_t *tud;
	while((tud = tud_first(0)))
		tud_free(tud);
	}


tud_t* tud_check(lua_State *L, int arg)
	{
	int key = luaL_checkinteger(L, arg);
	tud_t *tud = tud_search(key);
	if(!tud || !IsTudValid(tud))
		luaL_error(L, "invalid thread reference");
	return tud;
	}

