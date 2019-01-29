#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <dirent.h>
#include <errno.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>
//#include <net/if.h>
#include <sys/time.h>
#include <time.h>
#include <sys/select.h>
#include <stdlib.h>
#include <sys/types.h>
#include <signal.h>
#include <linux/wireless.h>
#include <errno.h>
#include <pthread.h>
#include <fcntl.h>
#include <arpa/inet.h>  
#include <net/ethernet.h>  
#include <linux/if_packet.h>  
#include <netdb.h>  
#include <netinet/ether.h>  


typedef struct ArpPacket
{    
	struct ether_header  eh;    
	struct ether_arp arp;    
}ArpPacket_s;   


int GetHwAddr(const char *ethName, char *HwAddr)
{
	if (!ethName)
		return -1;

	int sockfd;
	struct ifreq ifr;

	memset(&ifr, 0, sizeof(ifr));

	sockfd = socket(AF_INET, SOCK_DGRAM, 0);

	strcpy(ifr.ifr_name, ethName);

	if (ioctl(sockfd, SIOCGIFHWADDR, &ifr))
	{
		printf("ioctl SIOCGIFHWADDR\n");
		return -1;
	}

	memcpy(HwAddr, ifr.ifr_hwaddr.sa_data, 8);
	//data_dump(HwAddr, 8);
	close(sockfd);

	return 0;
}


static int send_arp(int sockfd, struct sockaddr_ll *peer_addr,unsigned char* dst_ip)  
{
	int rtval;  
	ArpPacket_s frame;  
	char dst_mac[6] = {0xff,0xff,0xff,0xff,0xff,0xff};
	char src_mac[12] = {0};
	if (GetHwAddr("eth2", src_mac) != 0)
	{
		printf("Get HwAddr failed\n");
		return 0;
	}
	memset(&frame, 0x00, sizeof(ArpPacket_s));  

	memcpy(frame.eh.ether_dhost, dst_mac, 6);    
	memcpy(frame.eh.ether_shost, src_mac, 6);    
	frame.eh.ether_type = htons(ETH_P_ARP);      

	frame.arp.ea_hdr.ar_hrd = htons(ARPHRD_ETHER);  
		
	frame.arp.ea_hdr.ar_pro = htons(ETH_P_IP); 
	frame.arp.ea_hdr.ar_hln = 6;                
	frame.arp.ea_hdr.ar_pln = 4;                
	frame.arp.ea_hdr.ar_op = htons(ARPOP_REQUEST);  
	memcpy(frame.arp.arp_sha, src_mac, 6);
//	memcpy(frame.arp.arp_spa, src_ip, 4);
//	memset(frame.arp.arp_spa,0,4);
	memcpy(frame.arp.arp_tha, dst_mac, 6);
	memcpy(frame.arp.arp_tpa, dst_ip,4);

	rtval = sendto(sockfd, &frame, sizeof(ArpPacket_s), 0,(struct sockaddr*)peer_addr, sizeof(struct sockaddr_ll));    
	if (rtval < 0) {  
		printf("send arp error\n");
		return -1;  
	} 
	return 0;  
}
static int recv_arp(int sockfd, struct sockaddr_ll *peer_addr,unsigned char* src_ip)  
{  
	int i = 0;
	int rtval;  
	ArpPacket_s frame;  
	struct timeval tv_out;
	tv_out.tv_sec = 2;
	tv_out.tv_usec = 0;
	setsockopt(sockfd,SOL_SOCKET,SO_RCVTIMEO,&tv_out, sizeof(tv_out));
	memset(&frame, 0, sizeof(ArpPacket_s));  
	rtval = recvfrom(sockfd, &frame, sizeof(frame), 0,NULL, NULL);  
	if (htons(ARPOP_REPLY) == frame.arp.ea_hdr.ar_op ) {
		if(rtval > 0)
		{  
			if (memcmp(frame.arp.arp_spa, src_ip, 4) == 0)  
			{  
				fprintf(stdout, "IP address is common~\n");  
				for(i=0;i<sizeof(frame.eh.ether_shost);i++)
					fprintf(stdout, "%x ",frame.eh.ether_shost[i]);
				printf("\n");
				return 0;  
			}  
		}  
	}
	return -1;  
}


/************************************************************************
**describe:	检测同一局域网内ip是否冲突
**param[in]:	deviec名卡名	ip待检测ip	num发送arp包次数
**param[out]:	无
*************************************************************************/
int IsIpConflict(const char* device , const char* ip, int num)  
{ 
	int sockfd;  
	int rtval = -1;  
	
	struct sockaddr_ll peer_addr;  
	struct ifreq req;  
	unsigned char src_ip[4]={0};
	unsigned char dst_ip[4]={0};
	if(ip == NULL)
	{
		printf("ip can't be null\n");
		return -1;
	}
	sscanf(ip,"%hhu.%hhu.%hhu.%hhu",&src_ip[0],&src_ip[1],&src_ip[2],&src_ip[3]);
	memcpy(dst_ip,src_ip,4);											//发送地址和接收地址设置为相同，局域网内没有此ip存在，不会有回复
	printf("ip:[%hhu %hhu %hhu %hhu]\n",src_ip[0],src_ip[1],src_ip[2],src_ip[3]);
	

	sockfd = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ARP));  
	if (sockfd < 0) 
	{  
		printf("sock error\n");
	} 

	memset(&peer_addr, 0, sizeof(peer_addr));    
	peer_addr.sll_family = AF_PACKET;    
	
	bzero(&req, sizeof(struct ifreq));  
	strcpy(req.ifr_name, device);    
	if(ioctl(sockfd, SIOCGIFINDEX, &req) != 0){
		perror("ioctl()");    
	}
	peer_addr.sll_ifindex = req.ifr_ifindex;    
	peer_addr.sll_protocol = htons(ETH_P_ARP);  
	bind(sockfd, (struct sockaddr *) &peer_addr, sizeof(peer_addr));

	while(num > 0) 
	{ 
		rtval = send_arp(sockfd, &peer_addr,dst_ip);  
		if ( rtval < 0) 
		{  
			printf("send arp error\n");
		}  
		rtval = recv_arp(sockfd, &peer_addr,src_ip);  
		if (rtval == 0) 
		{  
			printf ("Get packet from peer and IP conflicts!\n");  
			return 1;
		} else if (rtval < 0)
		{  
			fprintf(stderr, "Recv arp IP not conflicts: %s\n", strerror(errno));  
		}
		num --;
		//printf("num:%d\n", num);
	}  
	close(sockfd);
	return 0;  
}


int main(int argc, char *argv[])
{
	if(argc < 3)
	{
		printf("invalid param\nipConflict [ethname] [ip] [num]\n");
		return -1;
	}
		

	printf("name[%s] ip[%s] num[%d]\n", argv[1], argv[2], atoi(argv[3]));

	IsIpConflict(argv[1], argv[2], atoi(argv[3]));


	return 0;
}

