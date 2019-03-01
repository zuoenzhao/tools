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

	if (0 == inet_aton(srcIp, &addr.sin_addr))    //�����������ýӿڲ�ѯip��ַ
	{
		if(signal(SIGALRM, Alarmhandler) == SIG_ERR)
		{
			printf("signal(SIGALRM) Error\n");
			ret = -1;	//ϵͳ����
			goto error;
		}
		
		if(setjmp(AlarmBuf) != 0)
		{
			printf("alarm timeout...\n");
			ret = -2;	//ת����ʱ��һ�����������ap�˶ϵ��������ص��µ����粻ͨ
			goto error;
		}
		
	  	alarm(timeOut/1000);

		if(gethostbyname_r(srcIp, &hostinfo, buf, sizeof(buf), &phost, &ret))                                    
		{
			//printf("gethostbyname_r error..\n");
			ret = -1;	//ת��ʧ�ܣ�������������ap�˶�����
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
				ret = -1;	//ת��ʧ�ܣ�������������ap�˶�����
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

 /*����timeval�ṹ���*/
 static void Icmp_tv_sub(struct timeval *out,struct timeval *in)
 {
	 if((out->tv_usec-=in->tv_usec)<0)
	 {
		 --out->tv_sec;
		 out->tv_usec+=1000000;
	 }
	 out->tv_sec-=in->tv_sec;
 }



 /* У��ͺ�����У��icmp��ͷ+���ݣ�8+56�� */
static unsigned short icmp_cksum(unsigned char *data, int len)
{
	int sum = 0;   //������
	int odd = len & 0x01;  //�Ƿ�Ϊ����
    /*�����ݰ���2�ֽ�Ϊ��λ�ۼ�����*/
    while(len & 0xfffe){
    	sum += *(unsigned short*)data;
    	data += 2;
    	len -= 2;
    }
    /*�ж��Ƿ�Ϊ����������,��ICMP��ͷΪ�������ֽ�,��ʣ�����һ���ֽ�*/
    if(odd){
    	unsigned short tmp = ((*data)<<8)&0xff00;
    	sum += tmp;
    }
    sum = (sum >> 16) + (sum & 0xffff);   //�ߵ�λ���
    sum += (sum >> 16);                    //�����λ����
 
    return ~sum;                           //����ȡ��ֵ
}

///*����ICMP����*/
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
	/*��¼����ʱ��*/
	gettimeofday(tval,NULL);  
	/*У���㷨*/
	icmp->icmp_cksum=icmp_cksum( (unsigned char *)icmp,64);	
	if(sendto(sockfd,sendpacket,64,0,(struct sockaddr *)&dest_addr,sizeof(dest_addr) ) < 0)
	{
		printf("sendto error %s %d \n", __FUNCTION__, __LINE__);
		return -1;
	}
	return 0;
}

/*��������ICMP����*/
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

	gettimeofday(&tvrecv,NULL);  /*��¼����ʱ��*/    
	ip=(struct ip *)recvpacket;
	iphdrlen=ip->ip_hl<<2;    /*��ip��ͷ����,��ip��ͷ�ĳ��ȱ�־��4*/
	icmp=(struct icmp *)(recvpacket+iphdrlen);  /*Խ��ip��ͷ,ָ��ICMP��ͷ*/
	n-=iphdrlen;            /*ICMP��ͷ��ICMP���ݱ����ܳ���*/
	if( n<8)                /*С��ICMP��ͷ�����򲻺���*/
	{
		printf("ICMP packets\'s length is less than 8\n");
		return -1;
	}
	/*ȷ�������յ����������ĵ�ICMP�Ļ�Ӧ*/
	if( (icmp->icmp_type==ICMP_ECHOREPLY) && (icmp->icmp_id==pid) )
	{
		tvsend=(struct timeval *)icmp->icmp_data;
		Icmp_tv_sub(&tvrecv,tvsend);  /*���պͷ��͵�ʱ���*/
		rtt=tvrecv.tv_sec*1000+tvrecv.tv_usec/1000;  /*�Ժ���Ϊ��λ����rtt*/
		/*��ʾ�����Ϣ*/
		printf("%d byte from %s: icmp_seq=%u ttl=%d time=%.3f ms\n", n,inet_ntoa(from.sin_addr),icmp->icmp_seq,ip->ip_ttl,rtt);
		return 0;
	}
	else
	{
		//printf("FUNCTION:%s	LINE:%d \n", __FUNCTION__, __LINE__);
		return -1;
	}
}



/*ping��socket��������*/
static int IcmpSocetCreat(void)
{
	int sockfd = -1;
	int size = 50*1024;
	int timeOut = 300;
	
	struct timeval timeVal;

	/*����ʹ��ICMP��ԭʼ�׽���,�����׽���ֻ��root��������*/
	if((sockfd=socket(AF_INET,SOCK_RAW,IPPROTO_ICMP) )<0)
	{
		printf("FUNCTION:%s	LINE:%d \n", __FUNCTION__, __LINE__);
		return -1;
	}

	//�趨���ճ�ʱʱ��
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
	
	/*�����׽��ֽ��ջ�������50K��������ҪΪ�˼�С���ջ���������ĵĿ�����,��������pingһ���㲥��ַ��ಥ��ַ,������������Ӧ��*/
	if(setsockopt(sockfd,SOL_SOCKET,SO_RCVBUF,&size,sizeof(size)) == -1)
	{
		printf("FUNCTION:%s	LINE:%d \n", __FUNCTION__, __LINE__);
		close(sockfd);
		return -1;
	}

	return sockfd;
}



/*********************************************
**��������			��������ping����ҪAPI������������������Ƿ�ͨ
**param [in]	��srcStr 	����������ʮ������ʽip��ַ
				  num 		ping�����Ĵ���
				timeout		ping��ʱ����				
**param[out]	����
**����ֵ			��0 �ɹ�		-2 ��ʱʧ��  -1����������������粻ͨ
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

	gettimeofday(&timeStart,NULL);  //��¼api���п�ʼʱ��  

	//����socket�ӿڣ�����IcmpӦ��
	ret = IcmpSocetCreat() ;
	if(ret < 0)
	{
		printf("creat socket error..\n");
		return -1;
	}
	sockfd = ret;
	
	//������תip��ʽ�ַ�
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

	

	/*��ȡmain�Ľ���id,��������ICMP�ı�־��*/
	pid=getpid();

	//��ѯ��ʽ�����ͽ��
	while(num > 0)
	{
		/*��������ICMP����*/
		if(icmp_send(pid, sockfd, dstAddr, sednum) == 0)
		{
			sednum++;	
		}
		usleep(100);
		/*��������ICMP����*/
		if(icmp_rec(pid, sockfd) == 0)
		{
			recnum++;
		}
		num--;

	}

	gettimeofday(&timeEnd,NULL);  //��¼api����ʱ��  
	Icmp_tv_sub(&timeEnd,&timeStart);  /*���պͷ��͵�ʱ���*/
	*times=timeEnd.tv_sec+(double)timeEnd.tv_usec/1000000;  /*����Ϊ��λ����times*/

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


























