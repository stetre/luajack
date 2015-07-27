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
 * Synchronization pipes													*
 ****************************************************************************/

#include "internal.h"
#include <fcntl.h>

static sigset_t Sigpipe; /* constant */

int syncpipe_new(int pipefd[2]) 
	{
	int flags, rc;
	if(pipe(pipefd) == -1)
		return -1;

	/* make both ends non-blocking */
	flags = fcntl(pipefd[0], F_GETFL, 0);
	rc = fcntl(pipefd[0], F_SETFL, flags | O_NONBLOCK);
	flags = fcntl(pipefd[1], F_GETFL, 1);
	rc += fcntl(pipefd[1], F_SETFL, flags | O_NONBLOCK);
	if(rc < 0)
		return -1;
	return 0;
	}

int syncpipe_write(int writefd)
/* write a byte to the pipe so to make select() return */
	{
	int rc;
	sigset_t oldset;
	char zero = 0;
	/* mask SIGPIPE otherwise it would cause disasters if received in a thread... */
	pthread_sigmask(SIG_BLOCK, &Sigpipe, &oldset);
	rc = write(writefd, &zero, 1);
	/* rc = 1 : ok, 1 byte written, rc = 0: nothing was written (don't bother) */
	if(rc<0)
		{
		if(errno==EAGAIN) /* (EWOULDBLOCK) the write would block (skip it) */
			rc = 0; 
		else /* EPIPE, EBADF, ... : all fatal errors */
			rc = -1;
		}
	pthread_sigmask(SIG_SETMASK, &oldset, NULL);
	return rc;
	}

int syncpipe_read(int readfd)
/* read a byte from the pipe to avoid filling it and because otherwise
 * the readfd would remain 'ready' for select() */
	{
	int rc;
	char zero;
	rc = read(readfd, &zero, 1);
	if(rc>0) return 0; /* ok */
	if(rc==0) return -1; /* pipe is closed */
	/* rc<0 */
	if(errno==EAGAIN) return 0; /* (EWOULDBLOCK) the read would block (skip it) */
	return -1; /* EPIPE, EBADF, ... : all fatal errors */
	}

int syncpipe_init(void)
	{
	sigemptyset(&Sigpipe);
	sigaddset(&Sigpipe, SIGPIPE);
	return 1;
	}

