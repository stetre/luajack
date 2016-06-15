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

static int cmp(pud_t *pud1, pud_t *pud2) /* the compare function */
	{ return (pud1->key < pud2->key ? -1 : pud1->key > pud2->key); } 

static RB_HEAD(pudtree_s, pud_s) Head = RB_INITIALIZER(&Head);

RB_PROTOTYPE_STATIC(pudtree_s, pud_s, entry, cmp) 
RB_GENERATE_STATIC(pudtree_s, pud_s, entry, cmp) 
 
static pud_t *pud_remove(pud_t *pud) 
	{ return RB_REMOVE(pudtree_s, &Head, pud); }
static pud_t *pud_insert(pud_t *pud) 
	{ return RB_INSERT(pudtree_s, &Head, pud); }
static pud_t *pud_search(uintptr_t key) 
	{ pud_t tmp; tmp.key = key; return RB_FIND(pudtree_s, &Head, &tmp); }
pud_t *pud_first(uintptr_t key) 
	{ pud_t tmp; tmp.key = key; return RB_NFIND(pudtree_s, &Head, &tmp); }
pud_t *pud_next(pud_t *pud)
	{ return RB_NEXT(pudtree_s, &Head, pud); }
#if 0
pud_t *pud_prev(pud_t *pud)
	{ return RB_PREV(pudtree_s, &Head, pud); }
pud_t *pud_min(void)
	{ return RB_MIN(pudtree_s, &Head); }
pud_t *pud_max(void)
	{ return RB_MAX(pudtree_s, &Head); }
pud_t *pud_root(void)
	{ return RB_ROOT(&Head); }
#endif
pud_t *pud_new(cud_t *cud)
	{
	pud_t *pud;
	if((pud = (pud_t*)Malloc(sizeof(pud_t))) == NULL) return NULL;
	memset(pud, 0, sizeof(pud_t));
	pud->key = (uintptr_t)pud;
	if(pud_search(pud->key))
		{ Free(pud); luajack_error(UNEXPECTED_ERROR); return NULL; }
	cud_fifo_insert(cud, pud);
	pud->obj.type = LUAJACK_TPORT;
	pud->obj.xud = (void*)pud;
	pud_insert(pud);
	MarkPudValid(pud);
	return pud;
	}

static void pud_free(pud_t* pud)
	{
	if(pud_search(pud->key) == pud)
		pud_remove(pud);
	cud_fifo_remove(pud);
	Free(pud);	
	DBG("pud_free()\n");
	}

void pud_free_all(void)
	{
	pud_t *pud;
	while((pud = pud_first(0)))
		pud_free(pud);
	}


pud_t* pud_check(lua_State *L, int arg)
	{
	int key = luaL_checkinteger(L, arg);
	pud_t *pud = pud_search(key);
	if(!pud || !IsPudValid(pud))
		luaL_error(L, "invalid port reference");
	return pud;
	}

