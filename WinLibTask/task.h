/* Copyright (c) 2005 Russ Cox, MIT; see COPYRIGHT */

#ifndef _TASK_H_
#define _TASK_H_ 1

#ifdef __cplusplus
extern "C" {
#endif

#include <stdarg.h>
#include <WinSock2.h>
#include <Windows.h>
#include <tchar.h>

/*
 * basic procs and threads
 */

typedef struct Task Task;
typedef struct Tasklist Tasklist;

int		anyready(void);
int		taskcreate(void (*f)(void *arg), void *arg, unsigned int stacksize);
void		taskexit(int);
void		taskexitall(int);
void		taskmain(int argc, char *argv[]);
int		taskyield(void);
void**		taskdata(void);
void		taskname(char*, ...);
void		taskstate(char*, ...);
char*		taskgetname(void);
char*		taskgetstate(void);
void		tasksystem(void);
unsigned int	taskdelay(unsigned int);
unsigned int	taskid(void);

struct Tasklist	/* used internally */
{
	Task	*head;
	Task	*tail;
};

/*
 * queuing locks
 */
typedef struct QLock QLock;
struct QLock
{
	Task	*owner;
	Tasklist waiting;
};

void	qlock(QLock*);
int	canqlock(QLock*);
void	qunlock(QLock*);

/*
 * reader-writer locks
 */
typedef struct RWLock RWLock;
struct RWLock
{
	int	readers;
	Task	*writer;
	Tasklist rwaiting;
	Tasklist wwaiting;
};

void	rlock(RWLock*);
int	canrlock(RWLock*);
void	runlock(RWLock*);

void	wlock(RWLock*);
int	canwlock(RWLock*);
void	wunlock(RWLock*);

/*
 * sleep and wakeup (condition variables)
 */
typedef struct Rendez Rendez;

struct Rendez
{
	QLock	*l;
	Tasklist waiting;
};

void	tasksleep(Rendez*);
int	taskwakeup(Rendez*);
int	taskwakeupall(Rendez*);

/*
 * channel communication
 */
typedef struct Alt Alt;
typedef struct Altarray Altarray;
typedef struct Channel Channel;

enum
{
	CHANEND,
	CHANSND,
	CHANRCV,
	CHANNOP,
	CHANNOBLK,
};

struct Alt
{
	Channel		*c;
	void		*v;
	unsigned int	op;
	Task		*task;
	Alt		*xalt;
};

struct Altarray
{
	Alt		**a;
	unsigned int	n;
	unsigned int	m;
};

struct Channel
{
	unsigned int	bufsize;
	unsigned int	elemsize;
	unsigned char	*buf;
	unsigned int	nbuf;
	unsigned int	off;
	Altarray	asend;
	Altarray	arecv;
	char		*name;
};

int		chanalt(Alt *alts);
Channel*	chancreate(int elemsize, int elemcnt);
void		chanfree(Channel *c);
int		chaninit(Channel *c, int elemsize, int elemcnt);
int		channbrecv(Channel *c, void *v);
void*		channbrecvp(Channel *c);
unsigned long	channbrecvul(Channel *c);
int		channbsend(Channel *c, void *v);
int		channbsendp(Channel *c, void *v);
int		channbsendul(Channel *c, unsigned long v);
int		chanrecv(Channel *c, void *v);
void*		chanrecvp(Channel *c);
unsigned long	chanrecvul(Channel *c);
int		chansend(Channel *c, void *v);
int		chansendp(Channel *c, void *v);
int		chansendul(Channel *c, unsigned long v);

typedef struct FD
{
	HANDLE h;
	OVERLAPPED o;
} FD;

/*
 * Threaded I/O.
 */
FD*		taskopen(  __in      LPCTSTR lpFileName,
  __in      DWORD dwDesiredAccess,
  __in      DWORD dwShareMode,
  __in      DWORD dwCreationDisposition,
  __in      DWORD dwFlagsAndAttributes
);
int		fdread(FD*, void*, int);
int		fdwrite(FD*, void*, int);
void	fdwait(SOCKET, int);
void	handlewait(HANDLE h);
int		fdnoblock(SOCKET);

void		fdtask(void*);

/*
 * Network dialing - sets non-blocking automatically
 */
enum
{
	UDP = 0,
	TCP = 1,
};

SOCKET		netannounce(int, char*, int);
SOCKET		netaccept(SOCKET, char*, int*);
SOCKET		netdial(int, char*, int);
int		netlookup(char*, unsigned int*);	/* blocks entire program! */

#ifdef __cplusplus
}
#endif
#endif

