/*************************************************************************************************
** 	�ļ�������		socket tcpͨ�ŵĿͻ���ʵ������
**	���ڣ�			2019.1.26
**************************************************************************************************/
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <net/if.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>
#include <errno.h>






#define BUFFER_SIZE 128
#define PORT 9988


static int ClientConnect(const char *serverIp)
{
	int sockfd = -1;
	struct sockaddr_in serv_addr;	
	
	struct  timeval tv;
	fd_set rfds, wfds;
	int ret = 0;
	int selected = 0;
	int connected = -1;
	int err = 0;
	int times = 0;
	
	//sock����
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0)
	{
		printf("socket error:%s\n", strerror(errno));
		return -1;
	}
	printf("the sockfd is %d.\n", sockfd);
	
	//���ý��ճ�ʱ
	tv.tv_sec = 2;
	tv.tv_usec = 0;
	if(setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) != 0)
	{
		printf("setopt SO_RCVTIMEO error.the error is %d	%s.\n", errno, strerror(errno));
		close(sockfd);
		sockfd = -1;
		return sockfd;
	}	

	bzero(&serv_addr, sizeof(serv_addr));

	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(PORT);
	serv_addr.sin_addr.s_addr = inet_addr(serverIp);

	unsigned long ul = 1;
	ioctl(sockfd, FIONBIO, &ul); //����Ϊ������ģʽ
	printf("begin to connect server..\n");
	
	ret = connect(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr));
	
	err = errno;
	if ( ret < 0)
	{
		if (err == EINPROGRESS)		//�������»��������ش�ֵ
		{
			while(times++ < 4)
			{
				printf("Connect timeout, waiting: %d, %s\n",err, strerror(err));
				tv.tv_sec = 2;
				tv.tv_usec = 0;

				FD_ZERO(&rfds);			
				FD_ZERO(&wfds);	
				FD_SET(sockfd, &rfds);
				FD_SET(sockfd, &wfds);

				selected = select(sockfd+1, &rfds, &wfds, NULL, &tv);

				if (selected > 0)
				{
					//selected > 0 ˵�����ӵĵ�һ���ɹ�
					if (FD_ISSET(sockfd, &rfds) || FD_ISSET(sockfd, &wfds))  
					{  
						printf("FD_ISSET(rfds):[%d]	FD_ISSET(wfds):[%d]\n", FD_ISSET(sockfd, &rfds), FD_ISSET(sockfd, &wfds));  
						
						//�ٴ����ӣ����ݷ��ص�errȷ���Ƿ�������������server��socket��
						connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(struct sockaddr_in));  
						
						err = errno;  
						if (err == EISCONN)  	//���ش˴���˵����һ��ȷʵ���ӳɹ��ˣ�����ѭ��
						{  
							printf("Connect succeed, the sockfd is  %d\n", sockfd);  
							connected = 0;
							break;
							
						}  
						else  					//������������˵����һ��α���ӳɹ���ѭ��������������
						{  
							printf("Connect failed<%d>!%s\n", err, strerror(err));
							connected = -1;
							continue;
							
						}  

					}  
								
					
				}
				else if(selected == 0)
				{
					 printf("select time out\n"); 
					 connected = -1;
					 continue;					 
				}
				else
				{
					printf("Connect failed!%s\n", strerror(errno));
					connected = -1;
					continue;
					
				}

			}
			

			if(connected != 0)
			{				
				close(sockfd);
				sockfd = -1;
				return -1;
			}
			
		
		}
		else
		{
			printf("the errno is %d.\n", errno);
			printf("Connect failed!%s\n", strerror(errno));
			close(sockfd);
			sockfd = -1;
			return -1;
		}

	}

	return sockfd;
}


/*client��API*/
int ClientHearHandle(const char *serverIp)
{
	int sockfd = 0;
	int selected = 0;
	int ret = 0;
	char recBuf[64] = {0};
	char hearBeat[] = "IPCtest";
	fd_set rfds, wfds;
	struct  timeval tv;
	
	sockfd = ClientConnect(serverIp);
	if(sockfd < 0)
	{
		printf("connect error..\n");
		return -1;
	}

	while(1)
	{
		tv.tv_sec = 4;
		tv.tv_usec = 0;

		FD_ZERO(&rfds);			
		FD_ZERO(&wfds);	
		FD_SET(sockfd, &rfds);
		FD_SET(sockfd, &wfds);

		selected = select(sockfd+1, &rfds, &wfds, NULL, &tv);

			
		if(selected<0)
		{
			printf("select error..\n");
			continue;
		}
		else if (selected == 0 )
		{
			printf("select timeout\n");
			continue;
		}
			
		ret = send(sockfd,hearBeat,strlen(hearBeat),0);
		if(-1 == ret)
		{
			perror("write"),exit(-1);
		}
		else
		{
			printf("send:%s\n", hearBeat);
		}
		
		if(FD_ISSET(sockfd, &rfds))
		{
			ret = recv(sockfd,recBuf,sizeof(recBuf),0);
			if(-1 == ret)
			{
				perror("recv"),exit(-1);
			}
			printf("recv:%s\n",recBuf);
		}	

		memset(recBuf, 0, sizeof(recBuf));		
		sleep(120);

	}


	return 0;
}
 

int main(int argc, char *argv[])
{
	if(argc < 1)
	{
		printf("invalid param\nclient [server_ip]\n");
		return -1;
	}

	ClientHearHandle(argv[1]);
	return 0;
}



