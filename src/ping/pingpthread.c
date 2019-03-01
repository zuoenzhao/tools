
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <unistd.h>
#include <signal.h>
#include <arpa/inet.h>
#include <errno.h>
#include <sys/time.h>
#include <stdio.h>
#include <string.h>
#include <netdb.h>
#include <pthread.h>
#include <setjmp.h>
#include <signal.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <resolv.h>

static jmp_buf AlarmBuf;

//保存发送包的状态值
typedef struct PingPacket{
	struct timeval tv_begin;     //发送时间
	struct timeval tv_end;       //接收到的时间
	short seq;                   //序列号
	int flag;          //1，表示已经发送但是没有接收到回应，0，表示接收到回应
}PingPacket_s;

#define PING_PACKET_NUM 50
PingPacket_s pingpacket[PING_PACKET_NUM];



typedef struct PingParam
{
	char alive;
	char dstStr[64];
	int socket;
	int nums;
	int sedNum;
	int recNum;
	int timeout;
	int pid;
	struct timeval tv_begin;
	struct timeval tv_end;
	struct timeval tv_interval;
	struct sockaddr_in dest;
	
}PingParam_s;

static PingParam_s g_pingParam;




/*计算时间差time_sub*/
static struct timeval icmp_tvsub(struct timeval end, struct timeval begin)
{
	struct timeval tv;
	//计算差值
	tv.tv_sec = end.tv_sec - begin.tv_sec;
	tv.tv_usec = end.tv_usec - begin.tv_usec;
	//如果接收的时间的usec值小于发送时的usec,从uesc域借位
	if(tv.tv_usec < 0){
		tv.tv_sec --;
		tv.tv_usec += 1000000;
	}
 
	return tv;
}

/*icmp 校验算法*/
static unsigned short icmp_cksum(unsigned char *data, int len)
{
	int sum = 0;   //计算结果
	int odd = len & 0x01;  //是否为奇数
    /*将数据按照2字节为单位累加起来*/
    while(len & 0xfffe){
    	sum += *(unsigned short*)data;
    	data += 2;
    	len -= 2;
    }
    /*判断是否为奇数个数据,若ICMP报头为奇数个字节,会剩下最后一个字节*/
    if(odd){
    	unsigned short tmp = ((*data)<<8)&0xff00;
    	sum += tmp;
    }
    sum = (sum >> 16) + (sum & 0xffff);   //高地位相加
    sum += (sum >> 16);                    //将溢出位加入
 
    return ~sum;                           //返回取反值
}


/*终端信号处理函数SIGINT*/
static void icmp_sigint(int signo)
{
	g_pingParam.alive = 0;
	gettimeofday(&g_pingParam.tv_end,NULL);
	g_pingParam.tv_interval = icmp_tvsub(g_pingParam.tv_end, g_pingParam.tv_begin);
 
	return;
}




/*查找数组中的标识函数, 查找合适的包的位置, 当seq为1时，表示查找空包, 其他值表示查找seq对应的包*/
static PingPacket_s *icmp_findpacket(int seq)
{
   int i;
   PingPacket_s *found = NULL;
   //查找包的位置
   if(seq == -1){
	   for(i=0;i<128;i++){
		   if(pingpacket[i].flag == 0){
			   found = &pingpacket[i];
			   break;
		   }
	   }
   }
   else if(seq >= 0){
	   for(i =0 ;i< 128;i++){
		   if(pingpacket[i].seq == seq){
			   found = &pingpacket[i];
			   break;
		   }
	   }
   }
   return found;
}

/*进行ICMP头部校验，设置ICMP报头*/
static void icmp_pack(struct icmp *icmph, int seq, struct timeval *tv, int length)
{
	unsigned char i = 0;
	//设置报头
	icmph->icmp_type = ICMP_ECHO;   //ICMP回显请求
	icmph->icmp_code = 0;           //code的值为0
	icmph->icmp_cksum = 0;          //先将cksum的值填为0，便于以后的cksum计算
	icmph->icmp_seq = seq;          //本报的序列号
	icmph->icmp_id = g_pingParam.pid & 0xffff;  //填写PID
	for(i=0; i< length; i++)
		icmph->icmp_data[i] = i;   //计算校验和
	icmph->icmp_cksum = icmp_cksum((unsigned char*)icmph, length);
}

/*解压接收到的包，并打印信息*/
static int icmp_unpack(char *buf, int len)
{
	int iphdrlen;
	struct ip *ip = NULL;
	struct icmp *icmp = NULL;
	int rtt;
 
	ip = (struct ip *)buf;            //IP报头
	iphdrlen = ip->ip_hl * 4;         //IP头部长度
	icmp = (struct icmp *)(buf+iphdrlen);  //ICMP段的地址
	len -= iphdrlen;
	//判断长度是否为ICMP包
	if(len < 8)
	{
		printf("ICMP packets\'s length is less than 8\n");
		return -1;
	}
	//ICMP类型为ICMP_ECHOREPLY并且为本进程的PID
	if((icmp->icmp_type == ICMP_ECHOREPLY) && (icmp->icmp_id == g_pingParam.pid))
	{
		struct timeval tv_interval,tv_recv,tv_send;
		//在发送表格中查找已经发送的包，按照seq
		PingPacket_s *packet = icmp_findpacket(icmp->icmp_seq);
		if(packet == NULL)
			return -1;
		packet->flag = 0;          //取消标志
		tv_send = packet->tv_begin;  //获取本包的发送时间
 
		gettimeofday(&tv_recv,NULL);  //读取此时间，计算时间差
		tv_interval = icmp_tvsub(tv_recv,tv_send);
		rtt = tv_interval.tv_sec * 1000 + tv_interval.tv_usec/1000;
		/*打印结果包含
		  ICMP段的长度
		  源IP地址
		  包的序列号
		  TTL
		  时间差
		*/
		printf("%d byte from %s: icmp_seq=%u ttl=%d rtt=%d ms\n", len,inet_ntoa(ip->ip_src),icmp->icmp_seq,ip->ip_ttl,rtt);
		g_pingParam.recNum ++;
		if(icmp->icmp_seq == g_pingParam.nums)
		{
			//g_pingParam.alive = 0;
		}
			
	}
	else 
	{
		return -1;
	}
	return 0;
}



/*发送报文*/
static void *icmp_send(void *argv)
{
	unsigned char send_buff[72]; 
	//保存程序开始发送数据的时间
	
	g_pingParam.pid = getpid();  
	gettimeofday(&g_pingParam.tv_begin, NULL);
	
	while(g_pingParam.alive)
	{
		if(g_pingParam.sedNum == g_pingParam.nums)
		{
			//g_pingParam.alive = 0;
			//continue;
		}
			
		int size = 0;
		struct timeval tv;
		gettimeofday(&tv, NULL);     //当前包的发送时间
		
		//在发送包状态数组中找到一个空闲位置
		PingPacket_s *packet = icmp_findpacket(-1);
		if(packet)
		{
			packet->seq = g_pingParam.sedNum;
			packet->flag = 1;
			gettimeofday(&packet->tv_begin,NULL);
		}

		//填充数据		
		icmp_pack((struct icmp *)send_buff,g_pingParam.sedNum,&tv, 64);
		
		//发送数据
		size = sendto(g_pingParam.socket, send_buff,64,0,(struct sockaddr *)&g_pingParam.dest, sizeof(g_pingParam.dest));
		if(size < 0){
			perror("sendto error");
			continue;
		}		
		g_pingParam.sedNum ++;
		
		//每隔1s发送一个ICMP回显请求包
		usleep(1000*100);
	}
	return NULL;
}

/**********接收ping目的主机的回复***********/
static void * icmp_recv(void *argv)
{
	//轮询等待时间
	struct timeval tv;
	tv.tv_usec = 200;
	tv.tv_sec = 0;
	fd_set readfd;
	char recv_buff[2*1024];

	
	
	//当没有信号发出一直接收数据
	while(g_pingParam.alive)
	{
		
		int ret = 0;
		FD_ZERO(&readfd);
		FD_SET(g_pingParam.socket,&readfd);
		ret = select(g_pingParam.socket+1,&readfd,NULL,NULL,&tv);
		
		switch(ret)
		{
			case -1:
				//错误发生
				printf("select error\n");
				break;
			case 0:
				//超时
				//printf("select timeout\n");
				break;
			default :
				{				
					//接收数据
				
					memset(recv_buff, 0, sizeof(recv_buff));
					int size = recv(g_pingParam.socket,recv_buff,sizeof(recv_buff),0);
					if(errno == EINTR){
						perror("recvfrom error");
						continue;
					}
					
                                        //解包
					ret = icmp_unpack(recv_buff,size);
					if(ret == 1){
						continue;
					}
				}
				break;
		}
	}

	return NULL;
}



/*定时器超时处理函数*/
static void Alarmhandler(int signo)
{
	longjmp(AlarmBuf, 1);
}

/*主机名转Ip函数*/
static int IsHostName(void)
{
	int ret = -1;	
	char buf[1024] = {0};
	
	struct sockaddr_in dest_addr;
	struct hostent hostinfo,*phost;

	if (0 == inet_aton(g_pingParam.dstStr, &dest_addr.sin_addr))                                                                   
	{
		//初始化定时器
		if(signal(SIGALRM, Alarmhandler) == SIG_ERR)
		{
			printf("signal(SIGALRM) Error\n");
			exit(-1);
		}
		//超时后跳转到这里
		if(setjmp(AlarmBuf) != 0)
		{
			printf("alarm timeout...\n");
			ret = -2;
			goto error;
		}
		//设置定时器，超时后跳出主机名获取ip地址API
	  	alarm(g_pingParam.timeout/1000);

		//开始转换地址
	  	if(gethostbyname_r(g_pingParam.dstStr, &hostinfo, buf, sizeof(buf), &phost, &ret))                                    
		{
			printf("gethostbyname_r error..\n");
			goto error;                                                                                               
		}   
		else                                                                                                          
		{
			if(phost)                                                                                                 
			{
				#if 0
				int i = 0;
				printf("phost[%d] name[%s] addrtype[%d](AF_INET:%d) len:[%d] addr0:[%d] addr1:[%d]\n", (int)phost,phost->h_name,phost->h_addrtype,AF_INET, phost->h_length, (int)phost->h_addr_list[0],
	               									 phost->h_addr_list[0] == NULL?0:(int)phost->h_addr_list[1]);
	     		for(i = 0;hostinfo.h_aliases[i];i++)
	       				printf("host(%d) alias is:%s\n",(int)&hostinfo,hostinfo.h_aliases[i]);
	    		for(i = 0;hostinfo.h_addr_list[i];i++)    		
					printf("host addr is:%s\n",inet_ntoa(*(struct in_addr*)hostinfo.h_addr_list[i]));
				#endif
				memset(g_pingParam.dstStr, 0, sizeof(g_pingParam.dstStr));
	    		strcpy(g_pingParam.dstStr,inet_ntoa(*(struct in_addr*)hostinfo.h_addr_list[0]));	        				
			}   
			else                                                                                                      
			{
				goto error;                                                                                           
			}   
		}   
	  	
	}   
	
	
	return 0;

error:                                                                                                            
 	res_init();                                                                                                   
	return ret;

}

/*ping的socket创建函数*/
static int PingSocetCreat(void)
{
	int sockfd = -1;
	int size = 128*1024;

	/*生成使用ICMP的原始套接字,这种套接字只有root才能生成*/
	if((sockfd=socket(AF_INET,SOCK_RAW,IPPROTO_ICMP) )<0)
	{
		printf("FUNCTION:%s	LINE:%d \n", __FUNCTION__, __LINE__);
		return -1;
	}
	
	/*扩大套接字接收缓冲区到50K这样做主要为了减小接收缓冲区溢出的的可能性,若无意中ping一个广播地址或多播地址,将会引来大量应答*/
	if(setsockopt(sockfd,SOL_SOCKET,SO_RCVBUF,&size,sizeof(size)) == -1)
	{
		printf("FUNCTION:%s	LINE:%d \n", __FUNCTION__, __LINE__);
		close(sockfd);
		return -1;
	}

	g_pingParam.socket = sockfd;


	return 0;
}


//主函数入口，和正常ping功能差不多
int MyPingPthread(const char *IP, int Num, int timeOut)
{
	int ret =0;	
	
	memset(pingpacket, 0, sizeof(pingpacket));
	memset(&g_pingParam, 0, sizeof(g_pingParam));
	
	strncpy(g_pingParam.dstStr, IP, strlen(IP));
	g_pingParam.nums = Num;
	g_pingParam.timeout = timeOut;
	
	//先检查是主机地址还是IP地址
	ret = IsHostName();
	if( ret != 0)
	{
		printf("IsHostName error..\n");
		return ret;
	}

	//创建socket用于ping发包
	ret = PingSocetCreat();
	if(ret != 0)
	{
		printf("creat socket error..\n");
		return ret;
	}

	bzero(&g_pingParam.dest,sizeof(g_pingParam.dest));
	g_pingParam.dest.sin_family=AF_INET;
	g_pingParam.dest.sin_addr.s_addr = inet_addr(g_pingParam.dstStr); 
	printf("PING %s (%s) 56(84) bytes of data.\n",IP, g_pingParam.dstStr);
	
	//截取信号SIGINT,将icmp_sigint挂接上
	signal(SIGINT,icmp_sigint);
	g_pingParam.alive = 1;

	//创建发送和接收线程
	pthread_t send_id = -1, recv_id = -1;    
	ret = pthread_create(&send_id, NULL, icmp_send, NULL); //发送
	if(ret <  0)
	{
		return -1;
	}
	ret = pthread_create(&recv_id, NULL, icmp_recv, NULL); //接收
	if(ret < 0)
	{
		return -1;
	}
	
    //等待线程结束
	pthread_join(send_id, NULL);
	pthread_join(recv_id, NULL);
	
    //清理并打印统计结果
	close(g_pingParam.socket);

	
	return 0;

}

int main(int argc, char *argv[])
{
	if(argc < 3)
		printf("invald parameber\nmyping [ipstr] [num] [timeout]\n");

	//MyPingPthread(argv[1], atoi(argv[2]), atoi(argv[3]));



	return 0;
}



























