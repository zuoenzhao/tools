#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <net/if.h>
#include <netinet/in.h>
#include <assert.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <pthread.h>
#include <netinet/ip_icmp.h>
#include <sys/time.h>
#include <net/if_arp.h>
#include <netdb.h>
#include <setjmp.h>
#include <signal.h>
#include <dirent.h>
#include <errno.h>
#include <sys/prctl.h>
#include <linux/if_ether.h>
#include <netpacket/packet.h>

#include "monitor.h"
#include "radiotap-library-master/radiotap_iter.h"




#define WLAN_FILE			"/proc/net/dev"	




int IfconfigUp(const char *ethName)
{
	if (!ethName)
	{		
		return -1;
	}
		
	int sockfd;
	struct ifreq ifr;

	memset(&ifr, 0, sizeof(ifr));

	sockfd = socket(AF_INET, SOCK_DGRAM, 0);

	strcpy(ifr.ifr_name, ethName);

	if (ioctl(sockfd, SIOCGIFFLAGS, &ifr))
	{
		perror("ioctl SIOCGIFFLAGS");
		close(sockfd);
		return -1;
	}

	ifr.ifr_flags |= IFF_UP | IFF_RUNNING;

	if (ioctl(sockfd, SIOCSIFFLAGS, &ifr))
	{
		perror("ioctl SIOCSIFFLAGS");
		close(sockfd);
		return -1;
	}

	close(sockfd);

	return 0;
}


int WlanIsOK(const char *pName)
{
	int ret = -1;
	FILE *fp = NULL;
	char *p = NULL;
	char line[256] = {0};

	fp = fopen(WLAN_FILE, "r");
	if(NULL == fp)
	{
		perror("fopen error..\n");
		return ret;
	}

	while(fgets(line, sizeof(line), fp)) 
	{
		p = strstr(line, pName);
		if(p != NULL)
		{
			ret = 1;
			break;
		}
	}

	//ifconfig up 网卡
	if(ret)
	{
		if(IfconfigUp(pName) != 0)
		{
			printf("ifconfig %s up error\n", pName);
			ret = 0;
		}
		else
			printf("ifconfig %s up OK..\n", pName);
	}
		

	return ret;
}


int is_digit(char ch)
{
   if (ch >= '0' && ch <= '9')
		return 0;
	return -1;
}

int check_dir_name(const char *name)
{
	int i;

	for (i = 0; name[i]; i ++)
	{
		if (! is_digit(name[i])) 
		{
			return -1;
		}
	}
	return 0;
}

int super_killer(const char *proc_name)
{
	FILE *fp;
	DIR *ptidr;
	char path[256];
	char buffer[256];
	struct dirent * ptdirent;

	ptidr = opendir("/proc");
	if (ptidr == NULL)
	{
		printf("Open /proc failed, %s.\n",strerror(errno));
		//exit(-1);
		return -1;
	}

	while((ptdirent = readdir(ptidr)) != NULL) {
		if (check_dir_name(ptdirent->d_name)) {
			path[0] = 0;
			strcat(path, "/proc/");
			strcat(path, ptdirent->d_name);
			strcat(path, "/cmdline");
			//printf("path = %s\n", path);

			fp = fopen(path, "r");
			if (fp == NULL)
			{
				//printf("Open %s failed, %s.\n",path, strerror(errno));
				continue;
			}
			fgets(buffer, sizeof(buffer), fp);

			//printf("buffer = %s\n", buffer);
			if (! strcmp(buffer, proc_name)) 
			{
				if (kill(atoi(ptdirent->d_name), 9) == -1)
				{
					printf("%s kill failed, %s.\n",proc_name, strerror(errno));
					return -1;
				} 
				else 
				{
					printf("%s was killed.\n", proc_name);				
				}
			}
			fclose(fp);
		}
	}
	closedir(ptidr);
	return 0;	
}

int ManageToMonitor(void);
void SignalTermProc(int signal)
{	
 	MonitorToManage();
	printf("dertermind by shell %d\n",signal);
	exit(-1);
}


int WlanToolsGet(void)
{

	char toolFile[20][4] = {"/usr/bin/","/usr/sbin/", "/bin/", "/sbin/"};
	char toolName[30] = {0};
	int i = 0;
	int ret = 0;

	for(i = 0; i < 4; i++)
	{
		memset(toolName, 0, sizeof(toolName));
		sprintf(toolName,"%siw", toolFile[i]);
		if(access(toolName,F_OK)==0)
		{
			ret = 1;
			break;
		}
	}

	return 0;
}

int iw_sockets_open(void)
{
	static const int families[] = {AF_INET, AF_IPX, AF_AX25, AF_APPLETALK};
	unsigned int i = 0;
	int sock = -1;

	/* Try all families we support */
	for(i = 0; i < sizeof(families)/sizeof(int); ++i)
	{
		/* Try to open the socket, if success returns it */
		sock = socket(families[i], SOCK_DGRAM, 0);
		if(sock >= 0)
			return sock;
	}

	return -1;
}


int ManageToMonitor(void)
{
	int sockfd = -1;
	struct iwreq wrq;
	memset(&wrq, 0, sizeof(wrq));

	sockfd = iw_sockets_open();
	if(sockfd < 0)
	{
		printf("sock open error..\n");
	}

	strncpy(wrq.ifr_ifrn.ifrn_name, gWlan.ethName, IFNAMSIZ);

	if(ioctl(sockfd, SIOCGIWMODE, &wrq) < 0)
	{
		printf("ioctl get mode errer..\n");
		return -1;
	}

	if(wrq.u.mode == 6)
	{
		printf("\033[33mhad monitor success..\033[0m\n");
		close(sockfd);
		return 0;
	}

	wrq.u.mode = 6;
	if(ioctl(sockfd, SIOCSIWMODE, &wrq) < 0)
	{
		printf("ioctl set mode errer..\n");
		return -1;
	}

	printf("\033[33mmonitor success..\033[0m\n");
	
	close(sockfd);

	return 0;
}

int MonitorToManage(void)
{
	int sockfd = -1;
	struct iwreq wrq;
	memset(&wrq, 0, sizeof(wrq));

	sockfd = iw_sockets_open();
	if(sockfd < 0)
	{
		printf("sock open error..\n");
	}

	strncpy(wrq.ifr_ifrn.ifrn_name, gWlan.ethName, IFNAMSIZ);

	if(ioctl(sockfd, SIOCGIWMODE, &wrq) < 0)
	{
		printf("ioctl get mode errer..\n");
		return -1;
	}

	if(wrq.u.mode == 2)
	{
		printf("\033[33mhad managed success..\033[0m\n");
		close(sockfd);
		return 0;
	}

	wrq.u.mode = 2;
	if(ioctl(sockfd, SIOCSIWMODE, &wrq) < 0)
	{
		printf("ioctl set mode errer..\n");
		return -1;
	}

	printf("\033[33mmanaged success..\033[0m\n");
	
	close(sockfd);

	
	return 0;
}

int WlanSetChannel(int channel)
{
	int sockfd = -1;
	struct iwreq wrq;
	memset(&wrq, 0, sizeof(wrq));
	
	strncpy(wrq.ifr_ifrn.ifrn_name, gWlan.ethName, IFNAMSIZ);	
	wrq.u.freq.m = channel;
	wrq.u.freq.flags = IW_FREQ_FIXED;

	sockfd = iw_sockets_open();
	if(sockfd < 0)
	{
		printf("sock open error..\n");
	}

	if(ioctl(sockfd, SIOCSIWFREQ, &wrq) < 0)
	{
		printf("ioctl set channel errer..\n");
		return -1;
	}

	close(sockfd);

	return 0;
}


void FrameAllShow(char *frame, int n)
{
	#if 1
	int i = 0;
	for(i = 0; i< n; i++)
	{
		if(((i + 1)%16 == 0)||(i == n-1)) 
			printf("%02x\n", *(frame + i));
		else
			printf("%02x ", *(frame + i));

	}
	#endif

	return;
}

int FreqToChannel(int freq)
{
	int channel = 0;

	if(freq < 4000)
	{
		if(freq == 2484)
		{
			channel = 14;
			goto Return;
		}
		else
		{
			channel = (freq - 2407)/5;
			goto Return;
		}
	}
	else if((freq >=5170) && (freq <= 5825))
	{
		freq = 34 + (freq - 5170)/5;
		goto Return;
	}
	else if((freq >=4915) && (freq <= 4980))
	{
		freq = 183 + (freq - 4915)/5;
		goto Return;
	}

Return:
	return channel;
}


static double mcsRate[4][8] = {
						{6.5, 13, 19.5, 26, 39, 52,58.5, 65}, 
						{7.2, 14.2, 21.7, 28.9, 43.3, 57.8, 65.0, 72.2},
						{13.5, 27.0, 40.5, 54.0, 81.0, 108.0, 121.5, 135.0},
						{15.0, 30.0, 45.0, 60.0, 90.0, 120.0, 135.0, 150.0}
						};

double McsToRate(int mcs, int flag)
{
	double rate = 0;
	
	if(flag == 0x0)
		rate = mcsRate[0][mcs];
	else if(flag == 0x1)
		rate = mcsRate[2][mcs];
	else if(flag == 0x4)
		rate = mcsRate[1][mcs];
	else if(flag == 0x5)
		rate = mcsRate[3][mcs];

	return rate;
}

int ModeToString(ModeInfo_e mode, char *out)
{
	if(mode == 0)
		strcpy(out, "[B]");
	else if(mode ==1)
		strcpy(out, "[G]");
	else if(mode == 2)
		strcpy(out, "[N]");
	else if(mode ==3)
		strcpy(out, "[A]");

	return 0;
}


static void DateFrameMacGet(char *pFrame, FrameInfo_s * frameInfo)
{
	
	int toDS = 0;
	int i = 0;
	char retry[4] = {0};

	toDS = *(pFrame + 1) & 0x3;

	#if 0
	printf("addr1[%02x:%02x:%02x:%02x:%02x:%02x]\n", (UINT8)*(pFrame + 4), (UINT8)*(pFrame + 5),(UINT8)*(pFrame + 6), 
											(UINT8)*(pFrame + 7),(UINT8)*(pFrame + 8), (UINT8)*(pFrame + 9));
	printf("addr1[%02x:%02x:%02x:%02x:%02x:%02x]\n", (UINT8)*(pFrame + 10), (UINT8)*(pFrame + 11),(UINT8)*(pFrame + 12),
											(UINT8)*(pFrame + 13),(UINT8)*(pFrame + 14), (UINT8)*(pFrame + 15));
	printf("addr1[%02x:%02x:%02x:%02x:%02x:%02x]\n", (UINT8)*(pFrame + 16), (UINT8)*(pFrame + 17),(UINT8)*(pFrame + 18), 
											(UINT8)*(pFrame + 19),(UINT8)*(pFrame + 20), (UINT8)*(pFrame + 21));
	#endif
	
	
	if(toDS == 0x0)
	{
		for(i = 0; i < 6; i++)
		{
			frameInfo->src[i] = *(pFrame + 10 + i);
			frameInfo->dst[i] = *(pFrame + 4 + i);
			frameInfo->bssid[i] = *(pFrame + 16 + i);
		}
	}
	else if(toDS == 0x1)
	{
		for(i = 0; i < 6; i++)
		{
			frameInfo->src[i] = *(pFrame + 10 + i);
			frameInfo->dst[i] = *(pFrame + 16 + i);
			frameInfo->bssid[i] = *(pFrame + 4 + i);
		}
	}
	else if(toDS == 0x2)
	{
		for(i = 0; i < 6; i++)
		{
			frameInfo->src[i] = *(pFrame + 16 + i);
			frameInfo->dst[i] = *(pFrame + 4 + i);
			frameInfo->bssid[i] = *(pFrame + 10 + i);
		}
	}
	
	

	if((*(pFrame + 1) & 0x8) == 0x8)
	{
		strncpy(retry, "R", 1);		
	}

	frameInfo->secNum = (*(pFrame + 22) >> 4)  | (*(pFrame + 23) << 4);

	printf("[%-25s] ", frameInfo->frameName);

	printf("src[%02x:%02x:%02x:%02x:%02x:%02x] dst[%02x:%02x:%02x:%02x:%02x:%02x] bssid[%02x:%02x:%02x:%02x:%02x:%02x] num:%05d%-3s ",
		(UINT8)frameInfo->src[0], (UINT8)frameInfo->src[1], (UINT8)frameInfo->src[2], (UINT8)frameInfo->src[3], (UINT8)frameInfo->src[4], (UINT8)frameInfo->src[5],  
		(UINT8)frameInfo->dst[0], (UINT8)frameInfo->dst[1], (UINT8)frameInfo->dst[2], (UINT8)frameInfo->dst[3], (UINT8)frameInfo->dst[4], (UINT8)frameInfo->dst[5],
		(UINT8)frameInfo->bssid[0], (UINT8)frameInfo->bssid[1], (UINT8)frameInfo->bssid[2], (UINT8)frameInfo->bssid[3], (UINT8)frameInfo->bssid[4], (UINT8)frameInfo->bssid[5],
						frameInfo->secNum, retry);

	return;
}

int TypeToString(char *frame, FrameInfo_s * frameInfo)
{
	switch(*frame)
	{
		case M_ASSOCIATION_REQUEST:
			strcpy(frameInfo->frameName, "M association request");
			break;
		case M_ASSOCIATION_RESPONSE:
			strcpy(frameInfo->frameName, "M association response");
			break;
		case M_REASSOCIATION_REQUEST:
			strcpy(frameInfo->frameName, "M reassociation request");
			break;
		case M_REASSOCIATION_RESPONSE:
			strcpy(frameInfo->frameName, "M reassociation response");
			break;
		case M_PROBE_REQUEST:
			strcpy(frameInfo->frameName, "M probe request");
			break;
		case M_PROBE_RESPONSE:
			strcpy(frameInfo->frameName, "M probe Response");
			break;
		case M_BEACON:
			strcpy(frameInfo->frameName, "M beacon");
			break;
		case M_ATIM:
			strcpy(frameInfo->frameName, "M ATIM");
			break;
		case M_DISASSOCIATION:
			strcpy(frameInfo->frameName, "M dissassociation");
			break;
		case M_AUTHENTICATION:
			strcpy(frameInfo->frameName, "M authentication");
			break;
		case M_DEAUTHENTICATION:
			strcpy(frameInfo->frameName, "M deauthentication");
			break;	
		case D_DATE:
			strcpy(frameInfo->frameName, "D date");
			DateFrameMacGet(frame, frameInfo);
			break;
		case D_DATE_NULL:
			strcpy(frameInfo->frameName, "D date Null");
			DateFrameMacGet(frame, frameInfo);
			break;
		case D_DATE_QOS:
			strcpy(frameInfo->frameName, "D QOS");
			DateFrameMacGet(frame, frameInfo);
			break;
		case D_DATE_QOS_NULL:
			strcpy(frameInfo->frameName, "D QOS Null");
			DateFrameMacGet(frame, frameInfo);
			break;
		case C_BLOCK_ACKREQ:
			strcpy(frameInfo->frameName, "C BA request");
			break;
		case C_BLOCK_ACK:
			strcpy(frameInfo->frameName, "C BA");
			break;
		case C_PS_POLL:
			strcpy(frameInfo->frameName, "C PS_Poll");
			break;
		case C_RTS:
			strcpy(frameInfo->frameName, "C RTS");
			break;
		case C_CTS:
			strcpy(frameInfo->frameName, "C CTS");
			break;
		case C_ACK:
			strcpy(frameInfo->frameName, "C ack");
			break;
	}

	printf("\n");

	return 0;
}


#define IEEE80211_CHAN_A (IEEE80211_CHAN_5GHZ | IEEE80211_CHAN_OFDM)
#define IEEE80211_CHAN_G (IEEE80211_CHAN_2GHZ | IEEE80211_CHAN_OFDM)
#define IEEE80211_CHAN_B (IEEE80211_CHAN_2GHZ |IEEE80211_CHAN_CCK)
#define IEEE80211_CHAN_N (IEEE80211_CHAN_DYN)
 
static void print_radiotap_namespace(struct ieee80211_radiotap_iterator *iter, FrameInfo_s * frameInfo)
{
    int signal = 0;
	int channel = 0;
	int mode = 0;

	int mcs = 0;
	double rate = 0;
    uint32_t phy_freq = 0;
    
 
    switch (iter->this_arg_index)
    {
	    case IEEE80211_RADIOTAP_TSFT:
	        //printf("\tTSFT: %llu\n", le64toh(*(unsigned long long *)iter->this_arg));
	        break;
	 
	    case IEEE80211_RADIOTAP_FLAGS:
	        //printf("\tflags: %02x\n", *iter->this_arg);
	        break;
	 
	    case IEEE80211_RADIOTAP_RATE:
	    	frameInfo->rate = (double)*iter->this_arg/2;
	        //printf("\trate: %.2f Mbit/s\n", (double)*iter->this_arg/2);	        	
	        break;
	 
	    case IEEE80211_RADIOTAP_CHANNEL:
	        phy_freq = le16toh(*(uint16_t*)iter->this_arg); // 信道
	        iter->this_arg = iter->this_arg + 2; // 通道信息如2G、5G，等
	        int x = le16toh(*(uint16_t*)iter->this_arg);
	      	channel = FreqToChannel(phy_freq);
	        //printf("\tfreq: %d channel:%d ", phy_freq, channel);
	 
	        if ((x & IEEE80211_CHAN_A) == IEEE80211_CHAN_A) {
	        	mode = aMode;
	            //printf("mode: A\n");	            	
	        } else if ((x & IEEE80211_CHAN_G) == IEEE80211_CHAN_G) {
	        	mode = gMode;
	            //printf("mode: G\n");
	        } else if ((x & IEEE80211_CHAN_B) == IEEE80211_CHAN_B) {
	        	mode = bMode;
	            //printf("mode: B\n");
	        }else if((x & IEEE80211_CHAN_N) == IEEE80211_CHAN_N) {
	        	mode = nMode;
	        	//printf("mode: N\n");
	        }
	        frameInfo->mode = mode;
	        frameInfo->channel = channel;
	        break;
	 
	    case IEEE80211_RADIOTAP_DBM_ANTSIGNAL:
	        signal = *(signed char*)iter->this_arg;
	        frameInfo->rssi = signal;
	        //printf("\tsignal: %d dBm\n", signal);
	        break;
	 
	    case IEEE80211_RADIOTAP_RX_FLAGS:
	        //printf("\tRX flags: %#.4x\n", le16toh(*(uint16_t *)iter->this_arg));
	        break;
	 
	    case IEEE80211_RADIOTAP_ANTENNA:
	        //printf("\tantenna: %x\n", *iter->this_arg);
	        break;

		case IEEE80211_RADIOTAP_MCS:
			mcs = *(iter->this_arg +1) & 0x7;
			rate = McsToRate(*(iter->this_arg + 2), *(iter->this_arg + 1));
			frameInfo->rate = rate;
			//printf("\trate: %.2f Mbit/s\n", rate);
			break;
	 
	    case IEEE80211_RADIOTAP_RTS_RETRIES:
	    case IEEE80211_RADIOTAP_DATA_RETRIES:
	    case IEEE80211_RADIOTAP_FHSS:
	    case IEEE80211_RADIOTAP_DBM_ANTNOISE:
	    case IEEE80211_RADIOTAP_LOCK_QUALITY:
	    case IEEE80211_RADIOTAP_TX_ATTENUATION:
	    case IEEE80211_RADIOTAP_DB_TX_ATTENUATION:
	    case IEEE80211_RADIOTAP_DBM_TX_POWER:
	    case IEEE80211_RADIOTAP_DB_ANTSIGNAL:
	    case IEEE80211_RADIOTAP_DB_ANTNOISE:
	    case IEEE80211_RADIOTAP_TX_FLAGS:
	        break;
	 
	    default:
	        //printf("\tBOGUS DATA\n");
	        break;
    }
}



int FrameToInfo(char *frame, int len,  FrameInfo_s * frameInfo)
{
	struct ieee80211_radiotap_iterator iter;
    int err;
    int j; 
     
    err = ieee80211_radiotap_iterator_init(&iter, (struct ieee80211_radiotap_header *)frame, *(frame + 2), NULL);
    if (err) 
    {
        printf("not valid radiotap...\n");
        return -1;
    }

    j = 0;

    /**
     * 遍历时，this_arg_index表示当前索引(如IEEE80211_RADIOTAP_TSFT等)，
     * this_arg表示当前索引的值，this_arg_size表示值的大小。只有flag为true时才会进一步解析。
     */
    while (!(err = ieee80211_radiotap_iterator_next(&iter))) 
    {
        //printf("next[%d]: index: %d size: %d\n", j, iter.this_arg_index, iter.this_arg_size);
        if (iter.is_radiotap_ns) 
        { // 表示是radiotap的命名空间
            print_radiotap_namespace(&iter, frameInfo);
        }

        j++;
    }

    //printf("==================================\n");
   



	return 0;
}

int WlanFrameShow(char *frame, int n)
{
	int i = 0;
	double rate = 0;
	int rssi = 0;
	double time= 0;
	char mode[4] = {0};
	char frametype = 0;
	struct timeval timeNow;

	if(gWlan.recType != 0xff)
	{
		frametype = *(frame + *(frame + 2)) >> 2;
		if((frametype & 0x3) != gWlan.recType)
			return 0;
	}
	
	memset(&timeNow, 0, sizeof(timeNow));
	gettimeofday(&timeNow,NULL);
	time = (timeNow.tv_sec-gWlan.begin.tv_sec) + (double)(timeNow.tv_usec- gWlan.begin.tv_usec)/1000000;

	//无线帧全部内容打印，调试对照用
	//FrameAllShow(frame, n);	
	//FrameAllShow(frame + *(frame + 2), n - *(frame + 2));

	//获取MPDU数据前面的帧头部信息并打印
	FrameInfo_s frameInfo;
	memset(&frameInfo, 0, sizeof(frameInfo));
	FrameToInfo(frame, n, &frameInfo);	
	//printf("%ld %ld\n", (long int)(timeNow.tv_sec-gWlan.begin.tv_sec), (long int)(timeNow.tv_usec-gWlan.begin.tv_usec));
	ModeToString(frameInfo.mode, mode);
	printf("%06d time:%lf channel:%-3d rssi:%-4d rate:%4.1lf%-5s ", gWlan.frameTotal, time, frameInfo.channel, frameInfo.rssi, frameInfo.rate, mode);

	//MPDU数据的处理

	TypeToString(frame + *(frame + 2), &frameInfo);
													


	return 0;
}



void *recvdata(void *arg)
{
	int n = 0;
	int chn = 0;
	
	//wifi_frame_t *frame;
	
	int saddr_size;
	struct sockaddr saddr;	
	char ifName[IFNAMSIZ];           
	struct ifreq ifr;                
	struct sockaddr_ll sll;         
	int error_counter = 0;           
    struct  timeval tv;
    char recvbuf[1024*4] = {0};
	
	saddr_size = sizeof saddr;

	/* set normal mode name */
	strcpy(ifName, gWlan.ethName);      
      
    
    int sockfd = socket( PF_PACKET , SOCK_RAW , htons(ETH_P_ALL)) ;
    if(sockfd < 0){
        perror("Please use Sudo / or Socket Error");
        return NULL;
    }

    //Wait for device created , if created ok then  find the interface index 
    memset( &ifr, 0, sizeof( ifr ) );
    strncpy( ifr.ifr_name, ifName, sizeof( ifr.ifr_name ) - 1 );
	while (1) 
	{
		sleep (1);
		if(ioctl( sockfd , SIOCGIFINDEX, &ifr ) == 0)
			break;
		printf("Interface %s not created yet !: \n",  ifName);
		error_counter++;
		if (error_counter>6){
			printf("SIOCGIFINDEX Error!\n");
			close(sockfd);
			return (void*)-1;
		}
	}

	memset( &sll, 0, sizeof( sll ) );
	sll.sll_family	 = AF_PACKET;
	sll.sll_ifindex  = ifr.ifr_ifindex;
	sll.sll_protocol	= htons(ETH_P_ALL);

	// Bind to device : Method 2 
	if( bind(  sockfd , (struct sockaddr *) &sll, sizeof( sll ) ) < 0 )
	{	
		printf("Interface %s: \n",ifName);
		perror( "bind(ETH_P_ALL) failed" );
		return (void*)-1;
	}

	tv.tv_sec = 0;
	tv.tv_usec = 300*1000;
	setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

	gettimeofday(&gWlan.begin, NULL);
    while(gWlan.run)
	{
		if(gWlan.channelChange)
		{
			gettimeofday(&gWlan.now, NULL);
		
		    if(((gWlan.now.tv_sec - gWlan.begin.tv_sec) * 1000 + (gWlan.now.tv_usec - gWlan.begin.tv_usec) / 1000) > 300)  // 150714 100 //连续150ms没有收到有效数据切换频道 1-13个频道
			{
				gettimeofday(&gWlan.begin, NULL);
				chn++;
				if(chn > 13)
					chn = 0;
				
				WlanSetChannel(chn);
				printf("set channel %d.begin=%ldms,now=%ldms,last=%dms \n",chn, gWlan.begin.tv_sec*1000+gWlan.begin.tv_usec/1000, gWlan.now.tv_sec*1000 + gWlan.now.tv_usec/1000,300);
			}

		}
	        
		
	    //Receive a packet
	    memset(recvbuf, 0, sizeof(recvbuf));
        n = recvfrom(sockfd , recvbuf , sizeof(recvbuf) , 0 , &saddr , (socklen_t*)&saddr_size);
        if(n <0 ){
			if (errno == EAGAIN ||errno == EWOULDBLOCK){
				continue;
			}else {
    	    	printf("Recvfrom error , failed to get packets\n");
       		    return (void*)-1;
			}
        }
		gWlan.frameTotal++;
		WlanFrameShow(recvbuf, n);
	
	}
	close(sockfd);
	return NULL;
}



int MonitorMain(const char *pName)
{
	int ret = -1;
	//全局变量结构体清零
	memset(&gWlan, 0, sizeof(gWlan));
	
	strncpy(gWlan.ethName, pName, sizeof(gWlan.ethName));
	gWlan.recType = 0x2;
	
	//检查网络接口是否存在
	if(WlanIsOK(pName) != 1)
	{
		printf("wlan interface [%s] not exist\n", pName);
		return -1;
	}

	//杀掉已经运行网络工具
	super_killer("wpa_supplicant");
	super_killer("udhcpc");
	super_killer("hostapd");
	super_killer("udhcpd");

	//调试ctrl+c退出处理
	signal(SIGINT, SignalTermProc);
	gWlan.run = 1;

	ManageToMonitor();
	WlanSetChannel(11);

	//创建接收线程
	pthread_t recv_pid;
	pthread_attr_t attr;
	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
	ret = pthread_create(&recv_pid, &attr, recvdata, NULL);                             
	if(ret != 0) 
	{
		perror("can't create thread recvdata");
		return -1;
	}
	pthread_detach(recv_pid);

	//循环等待线程结束
	while(1)
	{
		sleep(3);
	}

	


	return 0;
}



int main(int argc, char *argv[])
{
	if(argc < 2)
	{
		printf("invalid param\nmonitor [ethname]\n");
		return -1;
	}

	MonitorMain(argv[1]);

	

	return 0;
}
