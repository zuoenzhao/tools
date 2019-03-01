/*************************************************************************************************
** 	�ļ�������		socket tcpͨ�ŵķ����ʵ������
**	���ڣ�			2019.1.26
**************************************************************************************************/
#include <stdio.h>  
#include <stdlib.h>  
#include <string.h>  
#include <sys/types.h>  
#include <sys/socket.h>  
#include <sys/time.h>
#include <sys/select.h>
#include <sys/ioctl.h>
#include <unistd.h>  
#include <netinet/in.h>  
#include <arpa/inet.h>  
#include <unistd.h>  
#include <errno.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <pthread.h>
#include <net/if.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>
#include <time.h>




#define FILE_FLAG "./server_flag"		//�������ļ����Ϳͻ���ͨ��ʱ����ȡ�ļ��е�ֵ������ֵ���ظ���ͬ���������ݣ��������



#define PORT 9988						//�������˿ں�
#define REC_BUFFER_SIZE 1500


char sedbuf[][64] = {"ACK0", "ACK1", "ACK2", "ACK3", "ACK4"};	//�ظ�������


/*	���ļ���ȡ */
int FileSimpleRead(const char * path,char *buf,int count)
{
	if (!path || !buf || count<=0)
		return -1;

	int fd = -1;	

	if ((fd=open(path,O_RDONLY))<0)
	{
		perror("open");
		return -1;
	}
	
	if (read(fd,buf,count)<=0)
	{
		perror("read");
		close(fd);
		return -1;
	}

	close(fd);

	return 0;

}


/*	���ļ�д�� 	*/
int FileSimpleWrite(const char * path,const char *buf,int count)
{
	if (!path || !buf || count<=0)
		return -1;

	int fd = -1;	

	if ((fd=open(path,O_WRONLY|O_CREAT))<0)
	{
		perror("open");
		return -1;
	}
	
	if (write(fd,buf,count)!=count)
	{
		perror("write");
		close(fd);
		return -1;
	}

	close(fd);

	return 0;
}


/*******************************************************************
**������			server��socket����
**param[in]:	checkConnect�Ƿ�ʹ�ܱ���̽�⣬���ڼ��ͻ��˳����Ƿ���ϵ絼�µ����ӶϿ�����ʹ�ܵĻ������ܹ���⵽�ͻ����Ѿ��Ͽ���ռ����Դ
**param[out]:	��
*******************************************************************/
int ServerSocketCreat(int checkConnect) 
{ 
	int option = 1;
	int keepAlive = 1; // ����keepalive����
	int keepIdle =checkConnect; // ���������checkConnect����û���κ���������,�����̽��
	int keepInterval = 5; // ̽��ʱ������ʱ����Ϊ5 ��
	int keepCount = 3; // ̽�Ⳣ�ԵĴ���.�����1��̽������յ���Ӧ��,���2�εĲ��ٷ�.
	
	int sock = socket(AF_INET, SOCK_STREAM , 0);
	if(sock < 0)
	{ 
		perror("socket\n");
		exit(2);
	}
	printf("socket success..\n");

	setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (char *)&option, sizeof(option));  //���õ�ַ������
	setsockopt(sock, SOL_SOCKET, SO_KEEPALIVE, (void *)&keepAlive, sizeof(keepAlive));	//���ô����ԣ����Ա��ֳ�����
	if(checkConnect)
	{
		setsockopt(sock, SOL_TCP, TCP_KEEPIDLE, (void*)&keepIdle, sizeof(keepIdle));
		setsockopt(sock, SOL_TCP, TCP_KEEPINTVL, (void *)&keepInterval, sizeof(keepInterval));
		setsockopt(sock, SOL_TCP, TCP_KEEPCNT, (void *)&keepCount, sizeof(keepCount)); 
	}
	

	struct sockaddr_in local;
	memset(&local, 0, sizeof(local));
	local.sin_family = AF_INET;
	local.sin_port = htons(PORT);
	local.sin_addr.s_addr = htonl(INADDR_ANY);


	if(bind(sock, (struct sockaddr*)&local, sizeof(local))<0)
	{
		perror("bind\n");
		exit(3);
	}
	printf("bind success..\n");

	if(listen(sock, 10) < 0)
	{ 
		perror("listen\n");
		exit(4);
	}
	printf("listenning..........\n");
	return sock;
}

static void * handlerRequest(void* arg)
{ 
	int new_sock = (int)arg;
	int n = 0;
	int selected = 0;
	char buffer[1024] = {0};
	char flag[6] = {0};
		
	
	int HeatInterval = 60;					//������ʱ��������
	fd_set rfds;
	struct  timeval tv;
	time_t seconds_start = 0;
	time_t seconds = 0;
	time_t seconds_last = 0;
	
	
	
	seconds_start = time(NULL);
	//printf("[%d]start....[%d]......\n", new_sock, seconds_start);

	unsigned long ul = 1;
	ioctl(new_sock, FIONBIO, &ul); //����Ϊ������ģʽ
	
	while(1)
	{    
		
		seconds = (time(NULL) - seconds_start);
		
		printf("[%d]time:%ld last:%ld\n", new_sock, (long int)(seconds - seconds_last),  (long int)seconds_last);

		//��ʱ�����߳�ʹ��
		if((seconds-seconds_last) >= (HeatInterval + 60))
		{
			printf("[%d]*************************begin to close socket**************\n", new_sock);
			close(new_sock);
			return NULL;
		}
	
		tv.tv_sec = 3;
		tv.tv_usec = 0;

		FD_ZERO(&rfds);		
		FD_SET(new_sock, &rfds);		

		selected = select(new_sock+1, &rfds, NULL, NULL, &tv);

				
		if(selected<0)
		{
			printf("[%d]select error..\n", new_sock);
			continue;
		}
		else if (selected == 0 )
		{
			//printf("[%d]select timeout\n",new_sock);
			continue;
		}
			
		if(FD_ISSET(new_sock, &rfds))
		{
			memset(buffer, 0, sizeof(buffer));
			if((n = recv(new_sock,buffer,sizeof(buffer),0)) == -1) //ѭ�����տͻ��˷��͵�����
			{
				printf("[%d]recv error..\n", new_sock);
				perror("recv error");
				close(new_sock);
				break;
			}
			else if(n == 0) //��ʱ���ͻ��˶Ͽ�����
			{
				printf("[%d]client connect break\n ", new_sock);
				close(new_sock);
				break;
			}
			printf("[%d]Received:%s\n",new_sock, buffer);


			memset(flag, 0, sizeof(flag));		
			if(FileSimpleRead(FILE_FLAG, flag, sizeof(flag)) == 0)
			{
				
				printf("[%d]the flag is [%d]\n",new_sock, atoi(flag));
				if(strncmp(buffer, "IPC", 3) == 0)
				{
								
					FileSimpleWrite(FILE_FLAG, "0", 2);
					if(send(new_sock, sedbuf[atoi(flag)], strlen(sedbuf[atoi(flag)]), 0) == -1)
					{
						printf("[%d]send error..\n", new_sock);
						perror("send error");
						close(new_sock);
						return NULL;
					}
					else
					{
						printf("[%d]send:%s\n",new_sock, sedbuf[atoi(flag)]);
					}

				}
	
									
				//���Դ�ӡ
				seconds_last = time(NULL) - seconds_start;
				
			}

			
		}					
		
	}

	return NULL;
}


int ServerHeartPthreadHandle(void)
{ 
	if(access(FILE_FLAG, F_OK) != 0)
	{
		//printf("write [%s] [%s] \n", FILE_FLAG, "0");
		FileSimpleWrite(FILE_FLAG, "0", 1);
	}
 
    int listen_sock = ServerSocketCreat(30); //����״̬
    struct sockaddr_in client;
    socklen_t len = sizeof(client);

    while(1)
    { 
    	printf("server socket wait accept..\n");
        int new_sock = accept(listen_sock, (struct sockaddr*)&client, &len);
        if(new_sock < 0)
        { 
            perror("accept\n");
            continue;  //����������ֱ�����¿ͻ�����
        }
        //��ȡ�¿ͻ�
        printf("get a new client,%s:%d\n", inet_ntoa(client.sin_addr), ntohs(client.sin_port));
        //�����¿ͻ�

        pthread_t id;
        pthread_create(&id, NULL, handlerRequest, (void*)new_sock);
        pthread_detach(id); //�̷߳���

    }
}



int main(void)
{
	
	ServerHeartPthreadHandle();
	
	return 0;
}

























