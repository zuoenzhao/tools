#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <pthread.h>

int g_setNum = 0;
int g_sleepTime = 0;
int g_printfNum= 0;
void Thread1()
{
 	while(1)
	{
		usleep(g_sleepTime);
		int i,j;
		int policy;
		struct sched_param param;
		pthread_getschedparam(pthread_self(),&policy,&param);
		if(policy == SCHED_OTHER)
			printf("thread1	SCHED_OTHER priority[%d]\n", param.__sched_priority);
		if(policy == SCHED_RR)
			printf("thread1	SCHED_RR  priority[%d]\n", param.__sched_priority);
		if(policy==SCHED_FIFO)
			printf("thread1	SCHED_FIFO priority[%d]\n", param.__sched_priority);

		for(i=1;i<g_setNum;i++)
		{
			printf("thread 1\n");
		}
		if((++g_printfNum%2) == 0)
			printf("\n");
		
	}
}

void Thread2()
{
 	while(1)
	{
		usleep(g_sleepTime);
		int i,j;
		int policy;
		struct sched_param param;
		pthread_getschedparam(pthread_self(),&policy,&param);
		if(policy == SCHED_OTHER)
			printf("thread2	SCHED_OTHER priority[%d]\n", param.__sched_priority);
		if(policy == SCHED_RR)
			printf("thread2	SCHED_RR  priority[%d]\n", param.__sched_priority);
		if(policy==SCHED_FIFO)
			printf("thread2	SCHED_FIFO priority[%d]\n", param.__sched_priority);

		for(i=1;i<g_setNum;i++)
		{
			printf("thread 2\n");
		}
		if((++g_printfNum%2) == 0)
			printf("\n");
	}
}

void Thread3()
{

	while(1)
	{
		usleep(g_sleepTime);
		int i,j;
		int policy;
		struct sched_param param;
		pthread_getschedparam(pthread_self(),&policy,&param);
		if(policy == SCHED_OTHER)
			printf("thread3	SCHED_OTHER priority[%d]\n", param.__sched_priority);
		if(policy == SCHED_RR)
			printf("thread3	SCHED_RR  priority[%d]\n", param.__sched_priority);
		if(policy==SCHED_FIFO)
			printf("thread3	SCHED_FIFO priority[%d]\n", param.__sched_priority);

		for(i=1;i<g_setNum;i++)
		{
			printf("thread 3\n");
		}
		
		if((++g_printfNum%3) == 0)
			printf("\n");
	}

 
}

int main()
{
	int i;
	i = getuid();
	if(i==0)
		printf("The current user is root\n");
	else
		printf("The current user is not root\n");

	g_setNum = 1;
	g_sleepTime = 1000*100;

	pthread_t ppid1,ppid2,ppid3;
	struct sched_param param;

	pthread_attr_t attr1,attr2,attr3;

	pthread_attr_init(&attr1);
	pthread_attr_init(&attr2);
	pthread_attr_init(&attr3);
	param.sched_priority = 90;
	pthread_attr_setschedpolicy(&attr1,SCHED_RR);
	pthread_attr_setschedparam(&attr1,&param);
	pthread_attr_setinheritsched(&attr1,PTHREAD_EXPLICIT_SCHED);//要使优先级其作用必须要有这句话

	#if 0
	param.sched_priority = 30;
	pthread_attr_setschedpolicy(&attr2,SCHED_RR);
	pthread_attr_setschedparam(&attr2,&param);
	pthread_attr_setinheritsched(&attr2,PTHREAD_EXPLICIT_SCHED);
	#endif
	#if 0
	param.sched_priority = 10;
	pthread_attr_setschedpolicy(&attr3,SCHED_FIFO);
	pthread_attr_setschedparam(&attr3,&param);
	pthread_attr_setinheritsched(&attr3,PTHREAD_EXPLICIT_SCHED);
	#endif

	//pthread_create(&ppid3,&attr3,(void *)Thread3,NULL);
	pthread_create(&ppid1,&attr1,(void *)Thread1,NULL);
	pthread_create(&ppid2,&attr2,(void *)Thread2,NULL);
	

	//pthread_join(ppid3,NULL);
	pthread_join(ppid2,NULL);
	pthread_join(ppid1,NULL);
	//pthread_attr_destroy(&attr3);
	pthread_attr_destroy(&attr2);
	pthread_attr_destroy(&attr1);
	return 0;
}

