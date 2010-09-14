/* Copyright (c) 2005 Russ Cox, MIT; see COPYRIGHT */

#include <fcntl.h>
#include <stdio.h>
#include "taskimpl.h"

typedef unsigned int uint;
typedef unsigned long ulong;

int	taskdebuglevel;
int	taskcount;
int	tasknswitch;
int	taskexitval;
Task	*taskrunning;

Context	taskschedcontext;
Tasklist	taskrunqueue;

Task	**alltask;
int		nalltask;

static char *argv0;
static	void		contextswitch(Context *from, Context *to);


static void CALLBACK taskstart(LPVOID lpParameter)
{
	Task *t = (Task*)lpParameter;

	t->startfn(t->startarg);
	taskexit(0);
	//not reacehd
}

static int taskidgen;

static Task*
taskalloc(void (*fn)(void*), void *arg, uint stack)
{
	Task *t;	

	/* allocate the task and stack together */
	t = (Task*)malloc(sizeof(Task));
	if(t == nil){
		fprint(GetStdHandle(STD_ERROR_HANDLE), "taskalloc malloc: %r\n");
		abort();
	}
	memset(t, 0, sizeof(Task));
	t->stksize = stack;
	t->id = ++taskidgen;
	t->startfn = fn;
	t->startarg = arg;

	/* call CreateFiber to do the real work. */
	//Stack sizes 4k and lower get rounded up to 1MB.
	if ((t->context.fiber = CreateFiberEx(0, max(stack, 4*1024+1), 0, (LPFIBER_START_ROUTINE)taskstart, t)) == NULL)
	{
		fprint(GetStdHandle(STD_ERROR_HANDLE), "CreateFiber: %d\n", GetLastError());
		abort();
	}

	return t;
}

int
taskcreate(void (*fn)(void*), void *arg, uint stack)
{
	int id;
	Task *t;

	t = taskalloc(fn, arg, stack);
	taskcount++;
	id = t->id;
	if(nalltask%64 == 0){
		alltask = (Task**)realloc(alltask, (nalltask+64)*sizeof(alltask[0]));
		if(alltask == nil){
			fprint(GetStdHandle(STD_ERROR_HANDLE), "out of memory\n");
			abort();
		}
	}
	t->alltaskslot = nalltask;
	alltask[nalltask++] = t;
	taskready(t);
	return id;
}

void
tasksystem(void)
{
	if(!taskrunning->system){
		taskrunning->system = 1;
		--taskcount;
	}
}

void
taskswitch(void)
{
	contextswitch(&taskrunning->context, &taskschedcontext);
}

void
taskready(Task *t)
{
	t->ready = 1;
	addtask(&taskrunqueue, t);
}

int
taskyield(void)
{
	int n;
	
	n = tasknswitch;
	taskready(taskrunning);
	taskstate("yield");
	taskswitch();
	return tasknswitch - n - 1;
}

int
anyready(void)
{
	return taskrunqueue.head != nil;
}

void
taskexitall(int val)
{
	exit(val);
}

void
taskexit(int val)
{
	taskexitval = val;
	taskrunning->exiting = 1;
	taskswitch();
}

static void
contextswitch(Context *from, Context *to)
{
	from->fiber = GetCurrentFiber();
	SwitchToFiber(to->fiber);
}

static void
taskscheduler(void)
{
	int i;
	Task *t;

	for(;;){
		if(taskcount == 0)
			exit(taskexitval);
		t = taskrunqueue.head;
		if(t == nil){
			fprint(GetStdHandle(STD_ERROR_HANDLE), "no runnable tasks! %d tasks stalled\n", taskcount);
			exit(1);
		}
		deltask(&taskrunqueue, t);
		t->ready = 0;
		taskrunning = t;
		tasknswitch++;
		contextswitch(&taskschedcontext, &t->context);
		//back in scheduler
		taskrunning = (Task*)nil;
		if(t->exiting){
			if(!t->system)
				taskcount--;
			i = t->alltaskslot;
			alltask[i] = alltask[--nalltask];
			alltask[i]->alltaskslot = i;
			DeleteFiber(t->context.fiber);
			free(t);
		}
	}
}

void**
taskdata(void)
{
	return &taskrunning->udata;
}

/*
 * debugging
 */
void
taskname(char *fmt, ...)
{
	va_list arg;
	Task *t;

	t = taskrunning;
	va_start(arg, fmt);
	vsnprint(t->name, sizeof t->name, fmt, arg);
	va_end(arg);
}

char*
taskgetname(void)
{
	return taskrunning->name;
}

void
taskstate(char *fmt, ...)
{
	va_list arg;
	Task *t;

	t = taskrunning;
	va_start(arg, fmt);
	vsnprint(t->state, sizeof t->name, fmt, arg);
	va_end(arg);
}

char*
taskgetstate(void)
{
	return taskrunning->state;
}

static void
taskinfo(int s)
{
	int i;
	Task *t;
	char *extra;

	fprint(GetStdHandle(STD_ERROR_HANDLE), "task list:\n");
	for(i=0; i<nalltask; i++){
		t = alltask[i];
		if(t == taskrunning)
			extra = " (running)";
		else if(t->ready)
			extra = " (ready)";
		else
			extra = "";
		fprint(GetStdHandle(STD_ERROR_HANDLE), "%6d%c %-20s %s%s\n", 
			t->id, t->system ? 's' : ' ', 
			t->name, t->state, extra);
	}
}

/*
 * startup
 */

static int taskargc;
static char **taskargv;
int mainstacksize;

static void
taskmainstart(void *v)
{
	taskname("taskmain");
	taskmain(taskargc, taskargv);
}

int
main(int argc, char **argv)
{
	WSADATA wsaData;

	if (ConvertThreadToFiber(0) == NULL)
	{
		fprint(GetStdHandle(STD_ERROR_HANDLE), "ConvertThreadToFiber: %d", GetLastError());
		abort();
	}

	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
	{
		fprint(GetStdHandle(STD_ERROR_HANDLE), "Failed to start winsock.");
		abort();
	}

	argv0 = argv[0];
	taskargc = argc;
	taskargv = argv;

	if(mainstacksize == 0)
		mainstacksize = 256*1024;
	taskcreate(taskmainstart, nil, mainstacksize);
	taskscheduler();
	fprint(GetStdHandle(STD_ERROR_HANDLE), "taskscheduler returned in main!\n");
	abort();
	return 0;
}

/*
 * hooray for linked lists
 */
void
addtask(Tasklist *l, Task *t)
{
	if(l->tail){
		l->tail->next = t;
		t->prev = l->tail;
	}else{
		l->head = t;
		t->prev = (Task*)nil;
	}
	l->tail = t;
	t->next = (Task*)nil;
}

void
deltask(Tasklist *l, Task *t)
{
	if(t->prev)
		t->prev->next = t->next;
	else
		l->head = t->next;
	if(t->next)
		t->next->prev = t->prev;
	else
		l->tail = t->prev;
}

unsigned int
taskid(void)
{
	return taskrunning->id;
}

