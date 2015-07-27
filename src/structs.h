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

/********************************************************************************
 * LuaJack library - structs definitions										*
 ********************************************************************************/

#ifndef structsDEFINED
#define structsDEFINED

/* Marks utilities 
 * m_ = marks word (uint32_t) , i_ = bit number (0 .. 31)
 */
#define MarkGet(m_,i_)	(((m_) & ((uint32_t)1<<(i_))) == ((uint32_t)1<<(i_)))
#define MarkSet(m_,i_)	do { (m_) = ((m_) | ((uint32_t)1<<(i_))); } while(0)
#define MarkReset(m_,i_) do { (m_) = ((m_) & (~((uint32_t)1<<(i_)))); } while(0)

/* Types redefinitions (I know, I know... ) */
#define client_t 	jack_client_t
#define port_t		jack_port_t
#define nframes_t 	jack_nframes_t 	
#define sample_t	jack_default_audio_sample_t 	
#define cud_t		luajack_cud_t
#define cud_s		luajack_cud_s
#define pud_t		luajack_pud_t
#define pud_s		luajack_pud_s
#define tud_t		luajack_tud_t
#define tud_s		luajack_tud_s
#define rud_t		luajack_rud_t
#define rud_s		luajack_rud_s
#define evt_t		luajack_evt_t
#define evt_s		luajack_evt_s
#define stat_t luajack_stat_t


typedef struct { 
	size_t n;
	double max;
	double min;
	double mean;
	double variance;
	double m2;
} luajack_stat_t;

#define luajack_cud_t struct luajack_cud_s /* client 'userdata' */
#define luajack_pud_t struct luajack_pud_s /* port 'userdata' */
#define luajack_tud_t struct luajack_tud_s /* thread 'userdata' */
#define luajack_rud_t struct luajack_rud_s /* ringbuffer 'userdata' */

struct luajack_pud_s;
#define luajack_pudfifo_t struct luajack_pudfifo_s /* ports queue */
SIMPLEQ_HEAD(luajack_pudfifo_s, luajack_pud_s); /* ports queue */

struct luajack_cud_s {
	RB_ENTRY(luajack_cud_s) entry;
	uintptr_t	key;			/* search key */
	uint32_t 	marks;
	client_t	*client; 		/* it's identity at the Jack level */
	lua_State   *process_state;	/* dedicated state for rt-callbacks (process() etc) */
	nframes_t	sample_rate;
	nframes_t   buffer_size; /* the value passed to process() (updated) */
	nframes_t   nframes; 	 /* the value passed to process() (set to 0 outside process()) */
	luajack_pudfifo_t fifo;  /* ports queue */
	/* references for non-rt callbacks in Lua registry */
	int SampleRate;
	int Xrun;
	int GraphOrder;
	int Freewheel;
	int ClientRegistration;
	int PortRegistration;
	int PortRename;
	int PortConnect;
	int Shutdown;
	int Latency;
	int Session;
	/* references for rt callbacks in process_state Lua registry */
	int Process;
	int BufferSize;
	int Sync;
	int Timebase;
	int TimebaseConditional;
	/* C real-time callbacks */
	JackProcessCallback	CProcess;
	JackBufferSizeCallback CBufferSize;
	JackSyncCallback CSync;
	JackTimebaseCallback CTimebase;
	void *CProcess_arg;
	void *CBufferSize_arg;
	void *CSync_arg;
	void *CTimebase_arg;
	luajack_t obj; /* object for raw interface */
	luajack_stat_t	stat; 	/* for profiling */
};

#define IsCudValid(cud) 			MarkGet((cud)->marks, 0)
#define MarkCudValid(cud) 			MarkSet((cud)->marks, 0) 
#define CancelCudValid(cud)  		MarkReset((cud)->marks, 0)

#define IsProcessCallback(cud) 		MarkGet((cud)->marks, 1)
#define MarkProcessCallback(cud) 	MarkSet((cud)->marks, 1) 
#define CancelProcessCallback(cud)  MarkReset((cud)->marks, 1)

#define IsCudProfile(cud) 			MarkGet((cud)->marks, 2)
#define MarkCudProfile(cud) 		MarkSet((cud)->marks, 2) 
#define CancelCudProfile(cud)  		MarkReset((cud)->marks, 2)

struct luajack_pud_s {
	RB_ENTRY(luajack_pud_s) entry;
	SIMPLEQ_ENTRY(luajack_pud_s) cudfifoentry; /* entry for cud->fifo */
	uintptr_t	key;			/* search key */
	uint32_t 	marks;
	luajack_t obj; /* object for raw interface */
	port_t		*port;
	cud_t	*cud;	/* the client it belongs to */
	unsigned long 	flags;  /* port flags */
	void			*buf;	/* port buffer */
	nframes_t	nframes;	/* buffer size (number of frames) */
	nframes_t	bufp;	/* position in buffer ( 0 ... nframes-1 ) */
	unsigned long 	samplesize; /* the buffer_size passed to jack_port_register() */
};

#define IsPudValid(pud) 			MarkGet((pud)->marks, 0)
#define MarkPudValid(pud) 			MarkSet((pud)->marks, 0) 
#define CancelPudValid(pud)  		MarkReset((pud)->marks, 0)


/* Additional JackPortFlags (valid within LuaJack only) */
#define LuaJackPortFlagsMask	0xf0000000
#define LuaJackPortIsAudio		0x10000000 	/* port is of the default audio type */
#define LuaJackPortIsMidi		0x20000000 	/* midi port */
#define LuaJackPortIsCustom		0x40000000	/* port of custom type */

#define PortIsInput(pud)	((pud)->flags & JackPortIsInput)
#define PortIsOutput(pud)	((pud)->flags & JackPortIsOutput)
#define PortIsPhysical(pud)	((pud)->flags & JackPortIsPhysical)
#define PortCanMonitor(pud)	((pud)->flags & JackPortCanMonitor)
#define PortIsTerminal(pud)	((pud)->flags & JackPortIsTerminal)
#define PortIsAudio(pud)	((pud)->flags & LuaJackPortIsAudio)
#define PortIsMidi(pud)		((pud)->flags & LuaJackPortIsMidi)
#define PortIsCustom(pud)	((pud)->flags & LuaJackPortIsCustom)


struct luajack_tud_s {
	RB_ENTRY(luajack_tud_s) entry;
	uintptr_t	key;			/* search key */
	uint32_t 	marks;
	luajack_t obj; /* object for raw interface */
	volatile int status;	/* tud status (TUD_XXX codes) */
	cud_t	*cud;			/* the client it belongs to */
	pthread_t thread; 		/* the thread */
	lua_State	*state; 	/* thread state (unrelated to parent's state) */
	pthread_mutex_t	lock;
	pthread_cond_t cond;
};

#define IsTudValid(tud) 			MarkGet((tud)->marks, 0)
#define MarkTudValid(tud) 			MarkSet((tud)->marks, 0) 
#define CancelTudValid(tud)  		MarkReset((tud)->marks, 0)

struct luajack_rud_s {
	RB_ENTRY(luajack_rud_s) entry;
	uintptr_t key; 	/* search key */
	uint32_t 	marks;
	luajack_t obj; /* object for raw interface */
	cud_t	*cud;	/* the client it belongs to */
	jack_ringbuffer_t	*rbuf;
	int	pipefd[2];
};

#define IsRudValid(rud) 			MarkGet((rud)->marks, 0)
#define MarkRudValid(rud) 			MarkSet((rud)->marks, 0) 
#define CancelRudValid(rud)  		MarkReset((rud)->marks, 0)

#define rbuf_has_pipe(rud) ((rud)->pipefd[0] != -1)
#define rbuf_readfd(rud) (rud)->pipefd[0]
#define rbuf_writefd(rud) (rud)->pipefd[1]

#define luajack_evt_t struct luajack_evt_s /* callback entry */
struct luajack_evt_s {
	SIMPLEQ_ENTRY(luajack_evt_s) entry;
	uintptr_t client_key;
	int	type;		/* CT_ codes */
	/* parameters */
	nframes_t	nframes;
	int	op; /* registered, starting, ... */
	jack_status_t code;
	jack_latency_callback_mode_t mode;
	jack_session_event_t *session_event;
	char *arg1;
	char *arg2;
};

/* callback types */
#define CT_SampleRate			1
#define CT_Xrun					2
#define CT_GraphOrder			3
#define CT_Freewheel			4
#define CT_ClientRegistration	5
#define CT_PortRegistration		6
#define CT_PortRename			7
#define CT_PortConnect			8
#define CT_Shutdown				9
#define CT_Latency				10
#define CT_Session				11

#endif /* structsDEFINED */
