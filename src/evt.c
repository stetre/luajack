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
 * Non-rt callbacks queue                                                   *
 ****************************************************************************/

#include "internal.h"

static pthread_mutex_t Lock = PTHREAD_MUTEX_INITIALIZER;
#define evt_lock() pthread_mutex_lock(&Lock)
#define evt_unlock() pthread_mutex_unlock(&Lock)

static SIMPLEQ_HEAD(myfifo_s, evt_s) head = SIMPLEQ_HEAD_INITIALIZER(head);
static unsigned int counter = 0;

evt_t* evt_new(void)
    { 
    evt_t *evt = Malloc(sizeof(evt_t));
    if(!evt) return NULL;
    memset(evt, 0, sizeof(evt_t));
    return evt;
    }

unsigned int evt_count(void)
    { return counter; }

void evt_free(evt_t *evt)
    { 
    if(evt->arg1) Free(evt->arg1);
    if(evt->arg2) Free(evt->arg2);
    if(evt->session_event) jack_session_event_free(evt->session_event);
    Free(evt);
    }

void evt_insert(evt_t *evt) 
    { 
    evt_lock();
    SIMPLEQ_INSERT_TAIL(&head, evt, entry);
    counter++;
    syncpipe_write(luajack_evtpipe[1]); /* to make pselect() return */
    evt_unlock();
    }

static evt_t* remove_nolock(void)
    { 
    evt_t *evt = SIMPLEQ_FIRST(&head);
    if(evt) 
        { 
        SIMPLEQ_REMOVE_HEAD(&head, entry);
        syncpipe_read(luajack_evtpipe[0]);
        counter--;
        }
    return evt;
    }

evt_t* evt_remove(void)
    { 
    evt_t *evt;
    evt_lock();
    evt = remove_nolock();
    evt_unlock();
    return evt;
    }

evt_t* evt_first(void)
    { 
    evt_t *evt;
    evt_lock();
    evt = SIMPLEQ_FIRST(&head);
    evt_unlock();
    return evt; 
    }

evt_t* evt_next(evt_t *evt)
    { 
    evt_t *evt1;
    evt_lock();
    evt1 = SIMPLEQ_NEXT(evt, entry);
    evt_unlock();
    return evt1; 
    }

void evt_free_all(void)
    {
    evt_t *evt;
/* called only at exit, from the main thread and with all other threads inactive */
/* evt_lock(); */
    while((evt = remove_nolock()) != NULL)
        evt_free(evt);
/* evt_lock(); */
    }

