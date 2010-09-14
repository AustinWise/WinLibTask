// TestWinLibTask.cpp : Defines the entry point for the console application.
//

#include <Task.h>
#include <stdio.h>
#include <tchar.h>

void test(void *data)
{
	while (1)
	{
		puts("test");
		taskdelay(500);
	}
}

void taskmain(int argc, char* argv[])
{
	char buf[100];
	//HANDLE ev = CreateEvent(0, TRUE, FALSE, 0);
	FD* h = taskopen(_T("D:\\Tracer.java"), GENERIC_READ, 0, OPEN_EXISTING, 0);
	if (h == 0)
	{
		puts("fucked");
		abort();
	}

	//handlewait(ev);

	while (1)
	{
	if (fdread(h, buf, 99) == -1)
	{
		abort();
	}
	buf[99] = '\0';
	puts(buf);
	}

/*
	if (fdread(h, buf, 100) == -1)
	{
		abort();
	}
	buf[99] = '\0';
	puts(buf);*/


	taskexit(0);


	//taskcreate(test, 0, 32768);
	//while (1)
	//{
	//	puts("turtles");
	//	taskdelay(500);
	//	taskyield();
	//}
}


/* Copyright (c) 2005 Russ Cox, MIT; see COPYRIGHT */
//
//#include <stdio.h>
//#include <stdlib.h>
//#include <task.h>
//
//int quiet;
//int goal;
//int buffer;
//
//#define TASKSTACK 4096
//
//void
//primetask(void *arg)
//{
//	Channel *c, *nc;
//	int p, i;
//	c = arg;
//
//	p = chanrecvul(c);
//	if(p > goal)
//		taskexitall(0);
//	if(!quiet)
//		printf("%d\n", p);
//	nc = chancreate(sizeof(unsigned long), buffer);
//	taskcreate(primetask, nc, TASKSTACK);
//	for(;;){
//		i = chanrecvul(c);
//		if(i%p)
//			chansendul(nc, i);
//	}
//}
//
//void
//taskmain(int argc, char **argv)
//{
//	int i;
//	Channel *c;
//
//	if(argc>1)
//		goal = atoi(argv[1]);
//	else
//		goal = 100000;
//	printf("goal=%d\n", goal);
//
//	c = chancreate(sizeof(unsigned long), buffer);
//	taskcreate(primetask, c, TASKSTACK);
//	for(i=2;; i++)
//		chansendul(c, i);
//}
//
//void*
//emalloc(unsigned long n)
//{
//	return calloc(n ,1);
//}
//
//long
//lrand(void)
//{
//	return rand();
//}
