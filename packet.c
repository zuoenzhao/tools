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












#define SED_BUFFER_SIZE 1300 
#define REC_BUFFER_SIZE 4000
#define PORT 9988
#include <errno.h>

int g_recnum;
int g_sednum;


static int client_packet_sed(const char *serverAddr, int sleepTime)
{
	int selres;
	int clientfd;  
	int connected = 0;
	int ret = 0;
	
	struct sockaddr_in serveraddr;  
	struct  timeval tv;
	char sed_buffer[SED_BUFFER_SIZE] = {0}; 
	
	fd_set rfds, wfds ;

	if((clientfd = socket(AF_INET, SOCK_STREAM, 0)) == -1)  
	{  
		printf("socket error");  
		return -1;  
	}  
	else  
	{  
		printf("clientfd:%d\n",clientfd);  
	}  
	//设置服务端的IP地址和端口号  
	memset(&serveraddr,0,sizeof(serveraddr));  
	serveraddr.sin_family = AF_INET;  
	serveraddr.sin_port = htons(PORT);  
	serveraddr.sin_addr.s_addr = inet_addr(serverAddr);  

	unsigned long ul = 1;
	ioctl(clientfd, FIONBIO, &ul); //设置为非阻塞模式

	ret = connect(clientfd, (struct sockaddr *) &serveraddr, sizeof(serveraddr));	

	if(ret < 0)
	{
		printf("get connect result by select().\n");
		printf("the errno is %d.\n", errno);
		if (errno == EINPROGRESS)
		{
			int times = 0;  
			while (times++ < 5) 
			{
				tv.tv_sec = 10;
				tv.tv_usec = 0;

				FD_ZERO(&rfds);
				FD_ZERO(&wfds);
				FD_SET(clientfd, &rfds);
				FD_SET(clientfd, &wfds);
				selres = select(clientfd+1, &rfds, &wfds, NULL, &tv);
				switch (selres)  
				{  
					case -1:  
						printf("select error\n");  
						connected = -1;  
						break;  
					case 0:  
						printf("select time out\n");  
						connected = -1;  
						break;  
					default:  
						if (FD_ISSET(clientfd, &rfds) || FD_ISSET(clientfd, &wfds))  
						{  
							printf("\nFD_ISSET(clientfd, &rfds): %d\nFD_ISSET(clientfd, &wfds): %d\n", FD_ISSET(clientfd, &rfds) , FD_ISSET(clientfd, &wfds));  
							connect(clientfd, (struct sockaddr *)&serveraddr, sizeof(struct sockaddr_in));  
							
							int err = errno;  
							if  (err == EISCONN)  
							{  
								printf("connect finished 111.\n");  
								connected = 0;  
							}  
							else  
							{  
								printf("connect failed. errno = %d\n", errno);  
								
								connected = errno;  
							}  

						}  
						else  
						{  
							printf("haha\n");  
						}  
				}  

				if (-1 != selres && (connected != 0))  
				{  
					printf("check connect result again... %d\n", times);  
					continue;  
				}  
				else  
				{  
					break;  
				}  


			}
		}
	}

	while(connected == 0)  
	{   
		//设置socket为阻塞模式
		unsigned long u2 = 0;
		ioctl(clientfd, FIONBIO, &u2); 
		
	
		memset(sed_buffer, 0, sizeof(sed_buffer));
		sprintf(sed_buffer,"%d", g_sednum);
		if(send(clientfd,sed_buffer,SED_BUFFER_SIZE,0) == -1)  
		{  
			perror("send error"); 
			printf("begin to close clientfd.\n");
			close(clientfd);
			exit(-1);  
		}  
		else
		{
			g_sednum++;
			printf("send success[%d]!\n", g_sednum);
			
			if(sleepTime != 0)
			{
				usleep(sleepTime);
			}
					
		}		
		
	}  
	close(clientfd);  

	return 0;
}

static int server_packet_rec(void)
{
	int listenfd, clientfd;  
    int n;  
	int on = 1;
    struct sockaddr_in serveraddr,clientaddr;  
    socklen_t peerlen;  
   
	char rec_buffer[REC_BUFFER_SIZE] = {0};  
	
	
    if((listenfd = socket(AF_INET, SOCK_STREAM, 0)) == -1)  
    {  
        perror("socket error");  
        exit(-1);  
    }  
    else  
    {  
        printf("listenfd:%d\n",listenfd);  
    }  
  
    memset(&serveraddr,0,sizeof(serveraddr));  
    serveraddr.sin_family = AF_INET;  
    serveraddr.sin_port = htons(PORT);  
    serveraddr.sin_addr.s_addr = htonl(INADDR_ANY); 
	
	if ((setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)))<0)
	{
		printf("set sockopt reuseaddr error...\n");
		close(listenfd);
		exit(-1);
		
	}
	
  
    if(bind(listenfd,(struct sockaddr *)&serveraddr,sizeof(serveraddr)) < 0) //绑定IP地址和端口号  
    {  
        perror("bind error");  
        exit(-1);  
    }  
    else  
    {  
        printf("bind successfully!\n");  
    }
	
	
  
    if(listen(listenfd,10) == -1)  
    {  
        perror("listen error");  
        exit(-1);  
    }  
    else  
    {  
        printf("listening....\n");  
    }  
  
    peerlen = sizeof(clientaddr);  
    while(1)  
    {  
        if((clientfd = accept(listenfd,(struct sockaddr *)&clientaddr,&peerlen)) < 0) //循环等待客户端的连接  
        {  
            perror("accept error");  
            exit(-1);  
        }  
        else  
        {  
            printf("connection from [%s:%d]\n",inet_ntoa(clientaddr.sin_addr)  
                    ,ntohs(clientaddr.sin_port)); 
			printf("the socket is %d.\n", clientfd);
        } 

  
 
        while(1)  
        {  
			memset(rec_buffer,0,sizeof(rec_buffer)); 
			if((n = recv(clientfd,rec_buffer,REC_BUFFER_SIZE,0)) == -1) //循环接收客户端发送的数据  
			{  
				perror("recv error");  
				exit(-1);  
			}  
			else if(n == 0) //此时，客户端断开连接  
			{
				printf("the socket had not connect..\n");
				break;  
			}  
			else  
			{ 
				
				printf("Received message:%s\n",rec_buffer); 
				
			}  
			
        }  
        close(clientfd); //客户端断开连接后，服务端也断开  
    }  
    close(listenfd); 


	return 0;
}
  
int main(int argc, char *argv[])  
{ 
	char serverAddr[20] = {0};
	int sleepTime = 0;
	
   	if(argc < 2)
   	{
   		printf("Invalid parameter..,sould such as:\npacket [-s/-c] ....\n");
		return -1;
	}

	if(strncmp(argv[1], "-s", 2) == 0)
	{
		server_packet_rec();
	}
	else if(strncmp(argv[1], "-c", 2) ==0)
	{
		if(argc < 3)
		{
			printf("please put:packet -c [server addr]\n");
			return -1;
		}
		else if(argc ==3)
		{
			strncpy(serverAddr, argv[2], strlen(argv[2]));
			client_packet_sed(serverAddr, 0);
		}
		else if(argc == 5)
		{
			strncpy(serverAddr, argv[2], strlen(argv[2]));
			if(strncmp(argv[3], "-i", 2) == 0)
			{
				sleepTime = 1000*atoi(argv[4]);
				client_packet_sed(serverAddr, sleepTime);
			}
			

		}
		
	}
		
  
    return 0;  
}































