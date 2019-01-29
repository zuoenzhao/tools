#include <stdio.h>
#include <sys/ioctl.h>
#include <sys/types.h>          /* See NOTES */
#include <sys/socket.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <net/route.h>
#include <unistd.h>
#include <time.h> 
#include <stdlib.h> 



static int GetIpAddr(const char *ethName, char *ip)
{
	int sockfd = -1;
	struct ifreq struReq;
	
	memset(&struReq, 0x00, sizeof(struct ifreq));
	strncpy(struReq.ifr_name, ethName, sizeof(struReq.ifr_name));
	
	sockfd = socket(PF_INET, SOCK_STREAM, 0);	
    if(sockfd < 0)
    {
        perror("socket   error");     
        return -1;     
    }

	if (-1 == ioctl(sockfd, SIOCGIFADDR, &struReq))
	{
		printf("ioctl error!\n");
		close(sockfd);
		return -1;
	}	
	strcpy(ip, inet_ntoa(((struct sockaddr_in *)&(struReq.ifr_addr))->sin_addr));

	close(sockfd);

	return 0;
}

static int SetIpAddr(const char *ethName, const char *ip)
{
	int fd = -1;
	struct ifreq ifr; 
    struct sockaddr_in *sin;

	fd = socket(AF_INET, SOCK_DGRAM, 0);
    if(fd < 0)
    {
        perror("socket   error");     
        return -1;     
    }
    memset(&ifr,0,sizeof(ifr)); 
    strcpy(ifr.ifr_name,ethName); 
    sin = (struct sockaddr_in*)&ifr.ifr_addr;     
    sin->sin_family = AF_INET;     
   
	if(inet_aton(ip,&(sin->sin_addr)) < 0)   
	{     
		perror("inet_aton   error");     
		close(fd);
		return -1;     
	}    

	if(ioctl(fd,SIOCSIFADDR,&ifr) < 0)   
	{     
		perror("ioctl   SIOCSIFADDR   error");     
		close(fd);
		return -1;     
	}

	close(fd);

	return 0;
}

static int GetGatewayAddr(const char *ethName, char *gateway)
{
	FILE *fp;
	int ret = 0;
	char filename[] = "/proc/net/route";
	char line[256] = {0};
	char gate[10] = {0};
	char gateW[4] = {0};
	char *p = NULL;

	if ((fp=fopen(filename,"r"))== NULL)
	{
		perror("fopen");
		return -1;
	}

	while (fgets(line, sizeof(line), fp))
	{
	
		p = strstr(line, ethName);
		if(p != NULL)
		{
			p += 14;
			strncpy(gate, p, 8);
			sscanf(gate, "%2hhx%2hhx%2hhx%2hhx", &gateW[3], &gateW[2], &gateW[1], &gateW[0]);
			sprintf(gateway, "%d.%d.%d.%d", gateW[0], gateW[1],gateW[2],gateW[3]);
			//printf("[%s]\n", gateway);
			if(strcmp(gateway, "0.0.0.0") != 0)
				ret = 1;
			break;

		}
		
	}

	fclose(fp);

	return ret;
}

static int SetGatewayAddr(const char *ethName, const char *gateway)
{
	struct rtentry rt;
	struct sockaddr_in *sin;
	struct ifreq ifr;  
	int sockfd;

	
	/* Create a socket to the INET kernel. */
	if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
	{
		perror("Socket Failed\n");
		return -1;
	}


	
	

	memset(&ifr,0,sizeof(ifr));   
    strcpy(ifr.ifr_name,ethName);   
    sin = (struct sockaddr_in*)&ifr.ifr_addr;       

	/*delete old route */
	memset(&rt, 0, sizeof(struct rtentry));
	
	sin = (struct sockaddr_in *)&(rt.rt_dst);
	sin->sin_family = AF_INET;
	sin->sin_port = 0;
	sin->sin_addr.s_addr = INADDR_ANY;
	rt.rt_flags = RTF_UP | RTF_MODIFIED;
	rt.rt_flags |= RTF_GATEWAY;
	

	rt.rt_dev = (void*)ethName;
	ioctl(sockfd, SIOCDELRT, &rt);
		
	/*add new route*/
	memset(&rt, 0, sizeof(struct rtentry));  
	memset(sin, 0, sizeof(struct sockaddr_in));  
	sin->sin_family = AF_INET;  
	sin->sin_port = 0;  
	if(inet_aton(gateway, &sin->sin_addr)<0)  
	{  
		printf ( "inet_aton error\n" );  
	}  
	
	memcpy ( &rt.rt_gateway, sin, sizeof(struct sockaddr_in));
	((struct sockaddr_in *)&rt.rt_dst)->sin_family=AF_INET;
	((struct sockaddr_in *)&rt.rt_genmask)->sin_family=AF_INET;
	rt.rt_flags = RTF_GATEWAY;
	
	if (ioctl(sockfd, SIOCADDRT, &rt) < 0)
	{
		perror("SetGateWay SIOCADDRT Failed\n");
		close(sockfd);
		return -1;
	}

	/* Close the socket. */
	close(sockfd);
	
	return 0;
}


static int GetNetmaskAddr(const char *ethName, char *netmask)
{
	int sockfd = -1;
	struct ifreq struReq;
	
	memset(&struReq, 0x00, sizeof(struct ifreq));
	strncpy(struReq.ifr_name, ethName, sizeof(struReq.ifr_name));
	
	sockfd = socket(PF_INET, SOCK_STREAM, 0);	
    if(sockfd < 0)
    {
        perror("socket   error");     
        return -1;     
    }

	if (-1 == ioctl(sockfd, SIOCGIFNETMASK, &struReq))
	{
		printf("ioctl error!\n");
		close(sockfd);
		return -1;
	}	
	strcpy(netmask, inet_ntoa(((struct sockaddr_in *)&(struReq.ifr_addr))->sin_addr));

	close(sockfd);
	
	return 0;
}

static int SetNetmaskAddr(const char *ethName, const char *netmask)
{
	
	int fd = -1;
	struct ifreq ifr; 
	struct sockaddr_in *sin;

	fd = socket(AF_INET, SOCK_DGRAM, 0);
	if(fd < 0)
	{
		perror("socket	 error");	  
		return -1;	   
	}
	memset(&ifr,0,sizeof(ifr)); 
	strcpy(ifr.ifr_name,ethName); 
	sin = (struct sockaddr_in*)&ifr.ifr_addr;	  
	sin->sin_family = AF_INET;	   
   
	if(inet_aton(netmask,&(sin->sin_addr)) < 0)	 
	{	  
		perror("inet_aton	error");	 
		close(fd);
		return -1;	   
	}	 

	if(ioctl(fd,SIOCSIFNETMASK,&ifr) < 0)	 
	{	  
		perror("ioctl	SIOCSIFNETMASK   error");	   
		close(fd);
		return -1;	   
	}

	close(fd);

	return 0;
}

static int GetHwAddr(const char *ethName, char *hw)
{
	int sockfd = -1;
	char mac[6] = {0};
	struct ifreq struReq;
	
	memset(&struReq, 0x00, sizeof(struct ifreq));
	strncpy(struReq.ifr_name, ethName, sizeof(struReq.ifr_name));
	
	sockfd = socket(PF_INET, SOCK_STREAM, 0);	
    if(sockfd < 0)
    {
        perror("socket   error");     
        return -1;     
    }

	if (-1 == ioctl(sockfd, SIOCGIFHWADDR, &struReq))
	{
		printf("ioctl error!\n");
		return -1;
	}
	strcpy(mac, struReq.ifr_hwaddr.sa_data);
	snprintf(hw, 18, "%02x:%02x:%02x:%02x:%02x:%02x", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

	close(sockfd);
	
	return 0;
}

static int if_updown(const char *ifname, int flag)
{
    int fd, rtn;
    struct ifreq ifr;        

    if (!ifname) 
    {
        return -1;
    }

    fd = socket(AF_INET, SOCK_DGRAM, 0 );
    if ( fd < 0 ) {
        perror("socket");
        return -1;
    }
    
    ifr.ifr_addr.sa_family = AF_INET;
    strncpy(ifr.ifr_name, (const char *)ifname, IFNAMSIZ - 1 );

    if ( (rtn = ioctl(fd, SIOCGIFFLAGS, &ifr) ) == 0 ) 
    {
        if ( flag == 0 )
            ifr.ifr_flags &= ~IFF_UP;
        else if ( flag == 1 ) 
            ifr.ifr_flags |= IFF_UP;
        
    }

    if ( (rtn = ioctl(fd, SIOCSIFFLAGS, &ifr) ) != 0) 
    {
        perror("SIOCSIFFLAGS");
    }

    close(fd);

    return rtn;
}

static int SetHwAddr(const char *ethName, const char *hw)
{
	int fd, rtn;
	char mac[6] = {0};
    struct ifreq ifr;

	sscanf(hw, "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx", &mac[0], &mac[1], &mac[2], &mac[3], &mac[4], &mac[5]);

    if( !ethName || !hw ) 
    {
        return -1;
    }
    fd = socket(AF_INET, SOCK_DGRAM, 0 );
    if ( fd < 0 ) 
    {
        perror("socket");
        return -1;
    }
    ifr.ifr_addr.sa_family = ARPHRD_ETHER;
    strncpy(ifr.ifr_name, (const char *)ethName, IFNAMSIZ - 1 );
    memcpy((unsigned char *)ifr.ifr_hwaddr.sa_data, mac, 6);
    
    if ( (rtn = ioctl(fd, SIOCSIFHWADDR, &ifr) ) != 0 )
    {
        perror("SIOCSIFHWADDR");
    }
    close(fd);
    return rtn;

}



int GetEthAddr(const char *ethName, int type, char* out)
{
	
	char show_cmd[][10] = {"ip", "gateway", "netmask", "gw"};

	switch(type)
	{
		case 0:
			GetIpAddr(ethName, out);
			break;
		case 1:
			GetGatewayAddr(ethName, out);
			break;
		case 2:			
			GetNetmaskAddr(ethName, out);			
			break;
		case 3:
			GetHwAddr(ethName, out);
			break;
		default:
			printf("invalid param\ntype: 0/1/2/3\n");
			break;

	}

	printf("get %-10s success:[%s]\n", show_cmd[type], out);

	return 0;

}


int SetEthAddr(const char *ethName, int type, const char *str)
{
	char show_cmd[][10] = {"ip", "gateway", "netmask", "gw"};
	printf("name[%s] type[%d] str[%s]\n", ethName, type, str);

	switch(type)
	{
		case 0:
			SetIpAddr(ethName, str);
			break;
		case 1:
			printf("000\n");
			SetGatewayAddr(ethName, str);
			break;
		case 2:			
			SetNetmaskAddr(ethName, str);			
			break;
		case 3:
			if_updown(ethName, 0);
			SetHwAddr(ethName, str);
			if_updown(ethName, 1);
			break;
		default:
			printf("invalid param\ntype: 0/1/2/3\n");
			break;

	}

	printf("set %-10s success:[%s]\n", show_cmd[type], str);
	

	return 0;
}



int main(int argc, char *argv[])
{
	//if(argc < 2)
		//printf("invalid param\nparam:[ethname] [type 0/1/2/3]\n");
	
	//char ip[20] = {0};
	//GetEthAddr(argv[1], atoi(argv[2]), ip);
	SetEthAddr(argv[1], atoi(argv[2]), argv[3]);
	
	
	return 0;
}

