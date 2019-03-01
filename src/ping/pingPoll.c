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
static void Alarmhandler(int signo)
{
	longjmp(AlarmBuf, 1);
}

static int HostNameToIp(const char *srcIp, char *dstIp, int timeOut)
{
	int ret = 0;
	//int i = 0;
	char buf[1024];
	struct hostent hostinfo,*phost;
	struct sockaddr_in addr;

	if (0 == inet_aton(srcIp, &addr.sin_addr))    //主机名，调用接口查询ip地址
	{
		if(signal(SIGALRM, Alarmhandler) == SIG_ERR)
		{
			printf("signal(SIGALRM) Error\n");
			ret = -1;	//系统错误
			goto error;
		}
		
		if(setjmp(AlarmBuf) != 0)
		{
			printf("alarm timeout...\n");
			ret = -2;	//转换超时，一般情况是网络ap端断电或更换网关导致的网络不通
			goto error;
		}
		
	  	alarm(timeOut/1000);

		if(gethostbyname_r(srcIp, &hostinfo, buf, sizeof(buf), &phost, &ret))                                    
		{
			//printf("gethostbyname_r error..\n");
			ret = -1;	//转换失败，错误主机名或ap端断网了
			goto error;                                                                                               
		}   
		else                                                                                                          
		{
			if(phost)                                                                                                 
			{
				#if 0
				printf("phost[%d] name[%s] addrtype[%d](AF_INET:%d) len:[%d] addr0:[%d] addr1:[%d]\n", (int)phost,phost->h_name,phost->h_addrtype,AF_INET, phost->h_length, (int)phost->h_addr_list[0],
	               									 phost->h_addr_list[0] == NULL?0:(int)phost->h_addr_list[1]);
	     		for(i = 0;hostinfo.h_aliases[i];i++)
	       				printf("host(%d) alias is:%s\n",(int)&hostinfo,hostinfo.h_aliases[i]);
	    		for(i = 0;hostinfo.h_addr_list[i];i++)    		
					printf("host addr is:%s\n",inet_ntoa(*(struct in_addr*)hostinfo.h_addr_list[i]));
				#endif
	    		strcpy(dstIp,inet_ntoa(*(struct in_addr*)hostinfo.h_addr_list[0]));
	        				
			}   
			else                                                                                                      
			{
				ret = -1;	//转换失败，错误主机名或ap端断网了
				goto error;                                                                                           
			}   
		}   

	}
	else
	{
		strncpy(dstIp, srcIp, strlen(srcIp));
	}

	
	return 0;

error:                                                                                                            
 	res_init();                                                                                                   
	return ret;
	
}

 /*两个timeval结构相减*/
 static void Icmp_tv_sub(struct timeval *out,struct timeval *in)
 {
	 if((out->tv_usec-=in->tv_usec)<0)
	 {
		 --out->tv_sec;
		 out->tv_usec+=1000000;
	 }
	 out->tv_sec-=in->tv_sec;
 }



 /* 校验和函数：校验icmp报头+数据（8+56） */
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

///*发送ICMP报文*/
static int icmp_send(pid_t pid, int sockfd, struct sockaddr_in dest_addr, int SerialNum)
{
	char sendpacket[1024*4] = {0};
	struct icmp *icmp;
	struct timeval *tval;
	
	icmp=(struct icmp*)sendpacket;
	icmp->icmp_type=ICMP_ECHO;
	icmp->icmp_code=0;
	icmp->icmp_cksum=0;
	icmp->icmp_seq=SerialNum;
	icmp->icmp_id=pid& 0xffff;
	
	tval= (struct timeval *)icmp->icmp_data;
	/*记录发送时间*/
	gettimeofday(tval,NULL);  
	/*校验算法*/
	icmp->icmp_cksum=icmp_cksum( (unsigned char *)icmp,64);	
	if(sendto(sockfd,sendpacket,64,0,(struct sockaddr *)&dest_addr,sizeof(dest_addr) ) < 0)
	{
		printf("sendto error %s %d \n", __FUNCTION__, __LINE__);
		return -1;
	}
	return 0;
}

/*接收所有ICMP报文*/
static int icmp_rec(pid_t pid, int sockfd)
{
	char recvpacket[4096] = {0};
	int n;
	int iphdrlen;
	struct sockaddr_in from;
	struct ip *ip;
	struct icmp *icmp;
	struct timeval *tvsend;
	struct timeval tvrecv;
	double rtt;
	//extern int errno;
	socklen_t fromlen=sizeof(from);
	
	while((n=recvfrom(sockfd,recvpacket,sizeof(recvpacket),0,(struct sockaddr *)&from,&fromlen)) < 0)
	{
		if(errno==EINTR)
			continue;
		//printf("recvfrom Error  \n");
		break;
	}
	if(n < 0)
	{
		printf("FUNCTION:%s	LINE:%d \n", __FUNCTION__, __LINE__);
		return -1;
	}

	gettimeofday(&tvrecv,NULL);  /*记录接收时间*/    
	ip=(struct ip *)recvpacket;
	iphdrlen=ip->ip_hl<<2;    /*求ip报头长度,即ip报头的长度标志乘4*/
	icmp=(struct icmp *)(recvpacket+iphdrlen);  /*越过ip报头,指向ICMP报头*/
	n-=iphdrlen;            /*ICMP报头及ICMP数据报的总长度*/
	if( n<8)                /*小于ICMP报头长度则不合理*/
	{
		printf("ICMP packets\'s length is less than 8\n");
		return -1;
	}
	/*确保所接收的是我所发的的ICMP的回应*/
	if( (icmp->icmp_type==ICMP_ECHOREPLY) && (icmp->icmp_id==pid) )
	{
		tvsend=(struct timeval *)icmp->icmp_data;
		Icmp_tv_sub(&tvrecv,tvsend);  /*接收和发送的时间差*/
		rtt=tvrecv.tv_sec*1000+tvrecv.tv_usec/1000;  /*以毫秒为单位计算rtt*/
		/*显示相关信息*/
		printf("%d byte from %s: icmp_seq=%u ttl=%d time=%.3f ms\n", n,inet_ntoa(from.sin_addr),icmp->icmp_seq,ip->ip_ttl,rtt);
		return 0;
	}
	else
	{
		//printf("FUNCTION:%s	LINE:%d \n", __FUNCTION__, __LINE__);
		return -1;
	}
}



/*ping的socket创建函数*/
static int IcmpSocetCreat(void)
{
	int sockfd = -1;
	int size = 50*1024;
	int timeOut = 300;
	
	struct timeval timeVal;

	/*生成使用ICMP的原始套接字,这种套接字只有root才能生成*/
	if((sockfd=socket(AF_INET,SOCK_RAW,IPPROTO_ICMP) )<0)
	{
		printf("FUNCTION:%s	LINE:%d \n", __FUNCTION__, __LINE__);
		return -1;
	}

	//设定接收超时时间
	if(timeOut <= 0)
	{
		timeOut = 300;
	}
	timeVal.tv_sec = timeOut/1000;
	timeVal.tv_usec = timeOut%1000*1000;
	if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &timeVal, sizeof(timeVal)) == -1)
	{
		printf("FUNCTION:%s	LINE:%d \n", __FUNCTION__, __LINE__);
		close(sockfd);
		return -1;
	}
	
	/*扩大套接字接收缓冲区到50K这样做主要为了减小接收缓冲区溢出的的可能性,若无意中ping一个广播地址或多播地址,将会引来大量应答*/
	if(setsockopt(sockfd,SOL_SOCKET,SO_RCVBUF,&size,sizeof(size)) == -1)
	{
		printf("FUNCTION:%s	LINE:%d \n", __FUNCTION__, __LINE__);
		close(sockfd);
		return -1;
	}

	return sockfd;
}



/*********************************************
**函数功能			：供调用ping，主要API，测试外网或局域网是否通
**param [in]	：srcStr 	主机名或点分十进制形式ip地址
				  num 		ping发包的次数
				timeout		ping超时设置				
**param[out]	：无
**返回值			：0 成功		-2 超时失败  -1错误的主机名或网络不通
**********************************************/
int PingPollMain(const char *srcStr, int num, int timeout, double *times)
{
	int sockfd = -1;
	int ret = -1;
	int pid = 0;
	int sednum = 0, recnum = 0;
	char dstIp[20] = {0};
	
	struct sockaddr_in dstAddr;
	struct timeval timeStart;
	struct timeval timeEnd;;

	gettimeofday(&timeStart,NULL);  //记录api运行开始时间  

	//创建socket接口，用于Icmp应答
	ret = IcmpSocetCreat() ;
	if(ret < 0)
	{
		printf("creat socket error..\n");
		return -1;
	}
	sockfd = ret;
	
	//主机名转ip形式字符
	ret = HostNameToIp(srcStr, dstIp, timeout);
	if(ret < 0)
	{
		printf("hostname to ip error..\n");
		return ret;		
	}

	bzero(&dstAddr,sizeof(dstAddr));
	dstAddr.sin_family=AF_INET;
	dstAddr.sin_addr.s_addr = inet_addr(dstIp); 
	printf("PING %s (%s) 56 bytes of data.\n",srcStr, dstIp);

	

	/*获取main的进程id,用于设置ICMP的标志符*/
	pid=getpid();

	//轮询方式发包和解包
	while(num > 0)
	{
		/*发送所有ICMP报文*/
		if(icmp_send(pid, sockfd, dstAddr, sednum) == 0)
		{
			sednum++;	
		}
		usleep(100);
		/*接收所有ICMP报文*/
		if(icmp_rec(pid, sockfd) == 0)
		{
			recnum++;
		}
		num--;

	}

	gettimeofday(&timeEnd,NULL);  //记录api结束时间  
	Icmp_tv_sub(&timeEnd,&timeStart);  /*接收和发送的时间差*/
	*times=timeEnd.tv_sec+(double)timeEnd.tv_usec/1000000;  /*以秒为单位计算times*/

	printf("--------------------MYPING statistics-------------------\n%d packets transmitted, %d received , %d lost\n\n\n",sednum,recnum,sednum-recnum);
	printf("time	%6.3lfs\n", *times);
	close(sockfd);

	return recnum;
}

int main(int argc, char *argv[])
{
	double times = 0;
	if(argc < 3)
		printf("invalid param\npingPoll [ip] [num] [timeout]\n");
		
	PingPollMain(argv[1], atoi(argv[2]), atoi(argv[3]), &times);

	return 0;
}


























