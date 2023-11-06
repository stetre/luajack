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
 * Clients database                                                         *
 ****************************************************************************/

#include "internal.h"

static int cmp(cud_t *cud1, cud_t *cud2) /* the compare function */
    { return (cud1->key < cud2->key ? -1 : cud1->key > cud2->key); } 

static RB_HEAD(cudtree_s, cud_s) Head = RB_INITIALIZER(&Head);

RB_PROTOTYPE_STATIC(cudtree_s, cud_s, entry, cmp) 
RB_GENERATE_STATIC(cudtree_s, cud_s, entry, cmp) 

static uintptr_t last_cud_key=128;
 
static cud_t *cud_remove(cud_t *cud) 
    { return RB_REMOVE(cudtree_s, &Head, cud); }
static cud_t *cud_insert(cud_t *cud) 
    { return RB_INSERT(cudtree_s, &Head, cud); }
cud_t *cud_search(uintptr_t key) 
    { cud_t tmp; tmp.key = key; return RB_FIND(cudtree_s, &Head, &tmp); }
cud_t *cud_first(uintptr_t key) 
    { cud_t tmp; tmp.key = key; return RB_NFIND(cudtree_s, &Head, &tmp); }
cud_t *cud_next(cud_t *cud)
    { return RB_NEXT(cudtree_s, &Head, cud); }
#if 0
cud_t *cud_prev(cud_t *cud)
    { return RB_PREV(cudtree_s, &Head, cud); }
cud_t *cud_min(void)
    { return RB_MIN(cudtree_s, &Head); }
cud_t *cud_max(void)
    { return RB_MAX(cudtree_s, &Head); }
cud_t *cud_root(void)
    { return RB_ROOT(&Head); }
#endif

cud_t *cud_new(void)
    {
    cud_t *cud;
    if((cud = (cud_t*)Malloc(sizeof(cud_t))) == NULL) return NULL;
    memset(cud, 0, sizeof(cud_t));
    cud->key = last_cud_key++;
    if(cud_search(cud->key))
        { Free(cud); luajack_error(UNEXPECTED_ERROR); return NULL; }
    cud->obj.type = LUAJACK_TCLIENT;
    cud->obj.xud = (void*)cud;
    cud->SampleRate = LUA_NOREF;
    cud->Xrun = LUA_NOREF;
    cud->GraphOrder = LUA_NOREF;
    cud->Freewheel = LUA_NOREF;
    cud->ClientRegistration = LUA_NOREF;
    cud->PortRegistration = LUA_NOREF;
    cud->PortRename = LUA_NOREF;
    cud->PortConnect = LUA_NOREF;
    cud->Shutdown = LUA_NOREF;
    cud->Latency = LUA_NOREF;
    cud->Session = LUA_NOREF;
    cud->Process = LUA_NOREF;
    cud->BufferSize = LUA_NOREF;
    cud->Sync = LUA_NOREF;
    cud->Timebase = LUA_NOREF;
    cud->TimebaseConditional = LUA_NOREF;
    SIMPLEQ_INIT(&(cud->fifo));
	luajack_stat_reset(&(cud->stat));
    cud_insert(cud);
    MarkCudValid(cud);
    return cud;
    }

static void cud_free(cud_t* cud)
    {
    if(cud_search(cud->key) == cud)
        cud_remove(cud);
    Free(cud);  
    DBG("cud_free()\n");
    }

void cud_free_all(void)
    {
    cud_t *cud;
    while((cud = cud_first(0)))
        cud_free(cud);
    }

cud_t* cud_check(lua_State *L, int arg)
    {
    uintptr_t key = luaL_checkinteger(L, arg);
    cud_t *cud = cud_search(key);
    if(!cud || !IsCudValid(cud))
        luaL_error(L, "invalid client reference");
    return cud;
    }

void cud_fifo_insert(cud_t* cud, pud_t *pud)
    {
    pud->cud = cud;
    SIMPLEQ_INSERT_TAIL(&(cud->fifo), pud, cudfifoentry);
    }

void cud_fifo_remove(pud_t *pud)
    {
#define cud ((cud_t*)(pud->cud))
    pud_t *node, *prev;
    node = SIMPLEQ_FIRST(&(cud->fifo));
    if(node)
        { 
        if(node == pud)
            { 
            SIMPLEQ_REMOVE_HEAD(&(cud->fifo), cudfifoentry);
            return;
            }
        prev = node;
        while((node = SIMPLEQ_NEXT(prev, cudfifoentry)))
            {
            if(node == pud)
                {
                SIMPLEQ_REMOVE_AFTER(&(cud->fifo), prev, cudfifoentry);
                return;
                }
            prev = node;
            }
        }
    luajack_error(UNEXPECTED_ERROR);
#undef cud
    }

