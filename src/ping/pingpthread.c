
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

//���淢�Ͱ���״ֵ̬
typedef struct PingPacket{
	struct timeval tv_begin;     //����ʱ��
	struct timeval tv_end;       //���յ���ʱ��
	short seq;                   //���к�
	int flag;          //1����ʾ�Ѿ����͵���û�н��յ���Ӧ��0����ʾ���յ���Ӧ
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




/*����ʱ���time_sub*/
static struct timeval icmp_tvsub(struct timeval end, struct timeval begin)
{
	struct timeval tv;
	//�����ֵ
	tv.tv_sec = end.tv_sec - begin.tv_sec;
	tv.tv_usec = end.tv_usec - begin.tv_usec;
	//������յ�ʱ���usecֵС�ڷ���ʱ��usec,��uesc���λ
	if(tv.tv_usec < 0){
		tv.tv_sec --;
		tv.tv_usec += 1000000;
	}
 
	return tv;
}

/*icmp У���㷨*/
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


/*�ն��źŴ�����SIGINT*/
static void icmp_sigint(int signo)
{
	g_pingParam.alive = 0;
	gettimeofday(&g_pingParam.tv_end,NULL);
	g_pingParam.tv_interval = icmp_tvsub(g_pingParam.tv_end, g_pingParam.tv_begin);
 
	return;
}




/*���������еı�ʶ����, ���Һ��ʵİ���λ��, ��seqΪ1ʱ����ʾ���ҿհ�, ����ֵ��ʾ����seq��Ӧ�İ�*/
static PingPacket_s *icmp_findpacket(int seq)
{
   int i;
   PingPacket_s *found = NULL;
   //���Ұ���λ��
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

/*����ICMPͷ��У�飬����ICMP��ͷ*/
static void icmp_pack(struct icmp *icmph, int seq, struct timeval *tv, int length)
{
	unsigned char i = 0;
	//���ñ�ͷ
	icmph->icmp_type = ICMP_ECHO;   //ICMP��������
	icmph->icmp_code = 0;           //code��ֵΪ0
	icmph->icmp_cksum = 0;          //�Ƚ�cksum��ֵ��Ϊ0�������Ժ��cksum����
	icmph->icmp_seq = seq;          //���������к�
	icmph->icmp_id = g_pingParam.pid & 0xffff;  //��дPID
	for(i=0; i< length; i++)
		icmph->icmp_data[i] = i;   //����У���
	icmph->icmp_cksum = icmp_cksum((unsigned char*)icmph, length);
}

/*��ѹ���յ��İ�������ӡ��Ϣ*/
static int icmp_unpack(char *buf, int len)
{
	int iphdrlen;
	struct ip *ip = NULL;
	struct icmp *icmp = NULL;
	int rtt;
 
	ip = (struct ip *)buf;            //IP��ͷ
	iphdrlen = ip->ip_hl * 4;         //IPͷ������
	icmp = (struct icmp *)(buf+iphdrlen);  //ICMP�εĵ�ַ
	len -= iphdrlen;
	//�жϳ����Ƿ�ΪICMP��
	if(len < 8)
	{
		printf("ICMP packets\'s length is less than 8\n");
		return -1;
	}
	//ICMP����ΪICMP_ECHOREPLY����Ϊ�����̵�PID
	if((icmp->icmp_type == ICMP_ECHOREPLY) && (icmp->icmp_id == g_pingParam.pid))
	{
		struct timeval tv_interval,tv_recv,tv_send;
		//�ڷ��ͱ���в����Ѿ����͵İ�������seq
		PingPacket_s *packet = icmp_findpacket(icmp->icmp_seq);
		if(packet == NULL)
			return -1;
		packet->flag = 0;          //ȡ����־
		tv_send = packet->tv_begin;  //��ȡ�����ķ���ʱ��
 
		gettimeofday(&tv_recv,NULL);  //��ȡ��ʱ�䣬����ʱ���
		tv_interval = icmp_tvsub(tv_recv,tv_send);
		rtt = tv_interval.tv_sec * 1000 + tv_interval.tv_usec/1000;
		/*��ӡ�������
		  ICMP�εĳ���
		  ԴIP��ַ
		  �������к�
		  TTL
		  ʱ���
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



/*���ͱ���*/
static void *icmp_send(void *argv)
{
	unsigned char send_buff[72]; 
	//�������ʼ�������ݵ�ʱ��
	
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
		gettimeofday(&tv, NULL);     //��ǰ���ķ���ʱ��
		
		//�ڷ��Ͱ�״̬�������ҵ�һ������λ��
		PingPacket_s *packet = icmp_findpacket(-1);
		if(packet)
		{
			packet->seq = g_pingParam.sedNum;
			packet->flag = 1;
			gettimeofday(&packet->tv_begin,NULL);
		}

		//�������		
		icmp_pack((struct icmp *)send_buff,g_pingParam.sedNum,&tv, 64);
		
		//��������
		size = sendto(g_pingParam.socket, send_buff,64,0,(struct sockaddr *)&g_pingParam.dest, sizeof(g_pingParam.dest));
		if(size < 0){
			perror("sendto error");
			continue;
		}		
		g_pingParam.sedNum ++;
		
		//ÿ��1s����һ��ICMP���������
		usleep(1000*100);
	}
	return NULL;
}

/**********����pingĿ�������Ļظ�***********/
static void * icmp_recv(void *argv)
{
	//��ѯ�ȴ�ʱ��
	struct timeval tv;
	tv.tv_usec = 200;
	tv.tv_sec = 0;
	fd_set readfd;
	char recv_buff[2*1024];

	
	
	//��û���źŷ���һֱ��������
	while(g_pingParam.alive)
	{
		
		int ret = 0;
		FD_ZERO(&readfd);
		FD_SET(g_pingParam.socket,&readfd);
		ret = select(g_pingParam.socket+1,&readfd,NULL,NULL,&tv);
		
		switch(ret)
		{
			case -1:
				//������
				printf("select error\n");
				break;
			case 0:
				//��ʱ
				//printf("select timeout\n");
				break;
			default :
				{				
					//��������
				
					memset(recv_buff, 0, sizeof(recv_buff));
					int size = recv(g_pingParam.socket,recv_buff,sizeof(recv_buff),0);
					if(errno == EINTR){
						perror("recvfrom error");
						continue;
					}
					
                                        //���
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



/*��ʱ����ʱ������*/
static void Alarmhandler(int signo)
{
	longjmp(AlarmBuf, 1);
}

/*������תIp����*/
static int IsHostName(void)
{
	int ret = -1;	
	char buf[1024] = {0};
	
	struct sockaddr_in dest_addr;
	struct hostent hostinfo,*phost;

	if (0 == inet_aton(g_pingParam.dstStr, &dest_addr.sin_addr))                                                                   
	{
		//��ʼ����ʱ��
		if(signal(SIGALRM, Alarmhandler) == SIG_ERR)
		{
			printf("signal(SIGALRM) Error\n");
			exit(-1);
		}
		//��ʱ����ת������
		if(setjmp(AlarmBuf) != 0)
		{
			printf("alarm timeout...\n");
			ret = -2;
			goto error;
		}
		//���ö�ʱ������ʱ��������������ȡip��ַAPI
	  	alarm(g_pingParam.timeout/1000);

		//��ʼת����ַ
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

/*ping��socket��������*/
static int PingSocetCreat(void)
{
	int sockfd = -1;
	int size = 128*1024;

	/*����ʹ��ICMP��ԭʼ�׽���,�����׽���ֻ��root��������*/
	if((sockfd=socket(AF_INET,SOCK_RAW,IPPROTO_ICMP) )<0)
	{
		printf("FUNCTION:%s	LINE:%d \n", __FUNCTION__, __LINE__);
		return -1;
	}
	
	/*�����׽��ֽ��ջ�������50K��������ҪΪ�˼�С���ջ���������ĵĿ�����,��������pingһ���㲥��ַ��ಥ��ַ,������������Ӧ��*/
	if(setsockopt(sockfd,SOL_SOCKET,SO_RCVBUF,&size,sizeof(size)) == -1)
	{
		printf("FUNCTION:%s	LINE:%d \n", __FUNCTION__, __LINE__);
		close(sockfd);
		return -1;
	}

	g_pingParam.socket = sockfd;


	return 0;
}


//��������ڣ�������ping���ܲ��
int MyPingPthread(const char *IP, int Num, int timeOut)
{
	int ret =0;	
	
	memset(pingpacket, 0, sizeof(pingpacket));
	memset(&g_pingParam, 0, sizeof(g_pingParam));
	
	strncpy(g_pingParam.dstStr, IP, strlen(IP));
	g_pingParam.nums = Num;
	g_pingParam.timeout = timeOut;
	
	//�ȼ����������ַ����IP��ַ
	ret = IsHostName();
	if( ret != 0)
	{
		printf("IsHostName error..\n");
		return ret;
	}

	//����socket����ping����
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
	
	//��ȡ�ź�SIGINT,��icmp_sigint�ҽ���
	signal(SIGINT,icmp_sigint);
	g_pingParam.alive = 1;

	//�������ͺͽ����߳�
	pthread_t send_id = -1, recv_id = -1;    
	ret = pthread_create(&send_id, NULL, icmp_send, NULL); //����
	if(ret <  0)
	{
		return -1;
	}
	ret = pthread_create(&recv_id, NULL, icmp_recv, NULL); //����
	if(ret < 0)
	{
		return -1;
	}
	
    //�ȴ��߳̽���
	pthread_join(send_id, NULL);
	pthread_join(recv_id, NULL);
	
    //������ӡͳ�ƽ��
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



























