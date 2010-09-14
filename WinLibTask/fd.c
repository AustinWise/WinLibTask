#include "taskimpl.h"

#define NET_ERRNO (WSAGetLastError())
#define NET_EAGAIN WSAEWOULDBLOCK

enum
{
	MAXFD = MAXIMUM_WAIT_OBJECTS
};

static HANDLE pollfd[MAXFD];
static Task *polltask[MAXFD];
static int npollfd = 0;
static int startedfdtask = 0;
static Tasklist sleeping;
static int sleepingcounted;
static uvlong nsec(void);

void
fdtask(void *v)
{
	int i, ms;
	Task *t;
	uvlong now;
	DWORD waitResult;
	
	tasksystem();
	taskname("fdtask");
	for(;;){
		/* let everyone else run */
		while(taskyield() > 0)
			;
		/* we're the only one runnable - poll for i/o */
		errno = 0;
		taskstate("poll");
		if((t=sleeping.head) == nil)
			ms = -1;
		else{
			/* sleep at most 5s */
			now = nsec();
			if(now >= t->alarmtime)
				ms = 0;
			else if(now+5*1000*1000*1000LL >= t->alarmtime)
				ms = (t->alarmtime - now)/1000000;
			else
				ms = 5000;
		}

		if (npollfd == 0)
			SleepEx(ms, TRUE);
		else
		{
			waitResult = WaitForMultipleObjectsEx(npollfd, pollfd, FALSE, ms, TRUE);
			if (waitResult == WAIT_FAILED)
			{
				fprint(GetStdHandle(STD_ERROR_HANDLE), "WaitForMultipleObjectsEx: %d\n", GetLastError());
				taskexitall(0);
			}

			if (waitResult >= WAIT_OBJECT_0 && waitResult <= (WAIT_OBJECT_0 + npollfd))
			{
				/* wake up the guys who deserve it */
				for(i = (waitResult - WAIT_OBJECT_0); i < npollfd; i++)
				{
					while(i < npollfd && WaitForSingleObject(pollfd[i], 0) == WAIT_OBJECT_0)
					{
						taskready(polltask[i]);
						--npollfd;
						pollfd[i] = pollfd[npollfd];
						polltask[i] = polltask[npollfd];
					}
				}
			}
		}
		
		now = nsec();
		while((t=sleeping.head) && now >= t->alarmtime){
			deltask(&sleeping, t);
			if(!t->system && --sleepingcounted == 0)
				taskcount--;
			taskready(t);
		}
	}
}

static __inline void startfdtask()
{
	if(!startedfdtask)
	{
		startedfdtask = 1;
		taskcreate(fdtask, 0, 32768);
	}
}

uint
taskdelay(uint ms)
{
	uvlong when, now;
	Task *t;
	
	startfdtask();

	now = nsec();
	when = now+(uvlong)ms*1000000;
	for(t=sleeping.head; t!=nil && t->alarmtime < when; t=t->next)
		;

	if(t){
		taskrunning->prev = t->prev;
		taskrunning->next = t;
	}else{
		taskrunning->prev = sleeping.tail;
		taskrunning->next = (Task*)nil;
	}
	
	t = taskrunning;
	t->alarmtime = when;
	if(t->prev)
		t->prev->next = t;
	else
		sleeping.head = t;
	if(t->next)
		t->next->prev = t;
	else
		sleeping.tail = t;

	if(!t->system && sleepingcounted++ == 0)
		taskcount++;
	taskswitch();

	return (nsec() - now)/1000000;
}

void handlewait(HANDLE h)
{
	startfdtask();

	if(npollfd >= MAXFD)
	{
		fprint(GetStdHandle(STD_ERROR_HANDLE), "too many poll file descriptors\n");
		abort();
	}
	
	polltask[npollfd] = taskrunning;
	pollfd[npollfd] = h;
	npollfd++;

	taskswitch();
}

void
fdwait(SOCKET fd, int rw)
{
	long bits;
	HANDLE hEvent = CreateEvent(0, TRUE, FALSE, 0);

	if (hEvent == NULL)
	{
		fprint(GetStdHandle(STD_ERROR_HANDLE), "Failed to create event.\n");
		abort();
	}

	startfdtask();

	if(npollfd >= MAXFD){
		fprint(GetStdHandle(STD_ERROR_HANDLE), "too many poll file descriptors\n");
		abort();
	}
	
	taskstate("fdwait for %s", rw=='r' ? "read" : rw=='w' ? "write" : "error");
	bits = 0;
	switch(rw)
	{
	case 'r':
		bits |= FD_READ;
		break;
	case 'w':
		bits |= FD_WRITE;
		break;
	}

	if (WSAEventSelect(fd, hEvent, bits))
	{
		fprint(GetStdHandle(STD_ERROR_HANDLE), "WSAEventSelect failed.\n");
		abort();
	}

	handlewait(hEvent);

	CloseHandle(hEvent);
}

static VOID CALLBACK iodone(DWORD dwErrorCode, DWORD dwNumberOfBytesTransfered, LPOVERLAPPED lpOverlapped)
{
	taskready((Task*)lpOverlapped->hEvent);
}

FD*		taskopen(
  __in      LPCTSTR lpFileName,
  __in      DWORD dwDesiredAccess,
  __in      DWORD dwShareMode,
  __in      DWORD dwCreationDisposition,
  __in      DWORD dwFlagsAndAttributes
)
{
	FD *fd = (FD*)calloc(1, sizeof(FD));
	if (fd == NULL)
		return 0;

	fd->h = CreateFile(lpFileName, dwDesiredAccess, dwShareMode, 0, dwCreationDisposition,
		FILE_FLAG_OVERLAPPED | dwFlagsAndAttributes, 0);

	if (fd->h == INVALID_HANDLE_VALUE)
	{
		free(fd);
		return 0;
	}

	return fd;
}

int
fdread(FD* fd, void *buf, int n)
{
	fd->o.Internal = 0;
	fd->o.InternalHigh = 0;
	fd->o.hEvent = taskrunning;

	if (!ReadFileEx(fd->h, buf, n, &fd->o, &iodone))
		return -1;
	else if (GetLastError() != ERROR_SUCCESS)
		return -1;
	
	startfdtask();
	taskswitch();

	if (fd->o.Internal == ERROR_SUCCESS)
	{
		INT64 off = fd->o.OffsetHigh << 16;
		off <<= 16;
		off |= fd->o.Offset;
		off += fd->o.InternalHigh;

		fd->o.OffsetHigh = (off >> 32) & 0xffffffff;
		fd->o.Offset = off & 0xffffffff;

		return fd->o.InternalHigh;
	}
	else
	{
		int asdf = ERROR_HANDLE_EOF;
		DWORD err = GetLastError();
		return -1;
	}
}

int
fdwrite(FD* fd, void *buf, int n)
{
	fd->o.hEvent = taskrunning;

	if (!WriteFileEx(fd->h, buf, n, &fd->o, &iodone))
		return -1;
	else if (GetLastError() != ERROR_SUCCESS)
		return -1;
	
	startfdtask();
	taskswitch();

	if (fd->o.Internal == ERROR_SUCCESS)
		return fd->o.InternalHigh;
	else
		return -1;
}

//
//int
//fdsend(SOCKET fd, void *buf, int n)
//{
//    int m, tot;
//    
//    for(tot = 0; tot < n; tot += m){
//        while((m=send(fd, (char*)buf+tot, n-tot, 0)) < 0 && WSAGetLastError() == WSAEWOULDBLOCK) {
//            if(fdwait(fd, 'w') == -1) {
//                return -1;
//            }
//        }
//
//        if(m < 0) return m;
//        if(m == 0) break;
//    }
//
//    return tot;
//}

int
fdnoblock(SOCKET fd)
{
	u_long iMode = 1;
	return ioctlsocket(fd, FIONBIO, &iMode);
}

static uvlong
nsec(void)
{
	FILETIME ft;
	uvlong ret;

	GetSystemTimeAsFileTime(&ft);

    ret = ((uvlong)ft.dwHighDateTime) << 32;
	ret |= ft.dwLowDateTime;
	return ret * 100;
}

