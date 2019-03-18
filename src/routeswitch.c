#include <string.h>
#include <stdio.h>
#include <sys/errno.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/file.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <net/route.h>
#include <linux/ethtool.h>
#include <linux/if_ether.h>
#include <linux/sockios.h>
#include <asm/types.h>                /* glibc 2 conflicts with linux/types.h */
#include <pthread.h>
#include <dirent.h>
#include <signal.h>
#include <unistd.h>



#define NET_WIFICONNECT 		0x01
#define NET_LINECONNECT 		0x02

static unsigned char g_linkstates = 0;
static unsigned char g_linkstatesold = 0;

enum NetcartStates {
	NETCARD_NO_CONNECT = 0,		//设备不连接
	NETCARD_CONNECT,					//设备连接
	NETCARD_BUFF,    					//种类数
};

typedef struct EthtoolValue
{
	unsigned int cmd;
	unsigned int data;
}EthtoolValue_s;


typedef struct CardInfo
{
	char eth_name[8];		//网卡接点
	int level;					//网卡优先级				
	int state;					//网路连接状态
}CardInfo_s;

CardInfo_s eth[2];	//eth0 eth2


static void ExecCmd(char *pCmd)
{
    printf("Exec Cmd:%s \r\n", pCmd);
    system(pCmd);
}

/*功能	：kill 掉指定名称的程序
 *参数	：proc_name 要杀掉的程序名
 *返回	：0 操作成功 <0 操作失败
 */
int super_killer(const char *proc_name)
{
	FILE *fp;
	DIR *ptidr;
	char path[256];
	char buffer[256];
	int i= 0;
	int valid = 0;
	struct dirent * ptdirent;

	ptidr = opendir("/proc");
	if (ptidr == NULL)
	{
		printf("Open /proc failed, %s.\n",strerror(errno));
		//exit(-1);
		return -1;
	}

	while((ptdirent = readdir(ptidr)) != NULL)
	{
		//先检测名字合法性

		for (i = 0; ptdirent->d_name[i]; i ++)
		{
			if(ptdirent->d_name[i] >= '0' && ptdirent->d_name[i] <= '9')
			{
				valid = 1;
			}
		}
		if (valid) 
		{
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


static int IpGetConfig(void)
{
	FILE *fp = NULL;
	char cTemp[4] = {0};

	fp = fopen("/var/ipisconfig", "rb");
	if (NULL == fp)
	{
		printf("open file fail: %s\n", "/var/ipisconfig");
		return -1;
	}

	fgets(cTemp, sizeof(cTemp), fp);
	fclose(fp);
	return  atoi(cTemp);
}



/*功能	：检测wpa_supplicant是否成功连接上AP，成功标志为wpa_state=9
 *参数	：ethname 网卡名
 *返回值	：1连接上 ~1 没有连接上
 */
static int get_eth_status(const char * ethname)
{
	static FILE *fp = NULL;
	char ctmp[4] = {0};
	char cmdBuf[100] = {0};
	char buf[1000] = {0};

	snprintf(cmdBuf, 100, "cat /proc/net/dev |grep %s | grep -v grep", ethname);
	//printf("[%s]\n", cmdBuf);

	FILE *pp = popen(cmdBuf, "r");
	if(fgets(buf, sizeof(buf), pp) == NULL)
	{	
		pclose(pp);
		//printf("\033[33meth2 not exit..\033[0m\n");
		return -1; 	//模块不存在
	}
	//else
		//printf("\033[33meth2 exit..\033[0m\n");
	pclose(pp);
	

	if(fp == NULL)
	{
		fp = fopen("/var/tmp/wpa_state", "rb");
		if(fp == NULL)
		{
			return 0;//模块存在但是没有关联上
		}
	}
	if(fp != NULL)
	{
		fseek(fp, SEEK_SET, 0);
		fgets(ctmp, sizeof(ctmp), fp);
		if(atoi(ctmp) == 9)
		{
			return 1;//模块存在并且联接上
		}
		if(atoi(ctmp) == 5)
		{
			return 5;	
		}
		if(atoi(ctmp) == 3)
		{
			return 3;
		}
	}
	return 0;	
}



static int mii_diag(const char *ethname)
{
	int skfd;
	
	int result=0;
	EthtoolValue_s edata;
	struct ifreq ifr;
	char ctmp[4] = {0};
	FILE *fp;

	//printf("the ethname is [%s]\n", ethname);
	
	if (!strcmp(ethname,"eth0"))
	{
		if ((skfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) 
		{
			perror("Socket Failed\n");
			return -1;
		}
	
		memset(&ifr, 0, sizeof(ifr));
		strcpy(ifr.ifr_name, ethname);
	
		edata.cmd = ETHTOOL_GLINK;
		ifr.ifr_data = (char *)&edata;
	
		if (!(ioctl(skfd, SIOCETHTOOL, &ifr)<0)) 
		{
			if(edata.data)
			{
				result = 0;		
			}
			else
			{
				result = -1;		
			}
		} 
		else 
		{
			close(skfd);
			perror("Ioctl SIOCETHTOOL\n");
			return -2;//DRV_ERR_NOT_SUPPORTED;
		}
	
		close(skfd);
	}
	else if (!strcmp(ethname,"eth2"))
	{
		result = get_eth_status(ethname);
		if(result < 0)	
		{
		//	printf("no eth2 in /proc/net/dev\n");
			return -1;	//模块不存在
		}
		fp = fopen("/var/tmp/wpa_state", "rb");
		if(fp == NULL)
		{
		//	perror("no /var/tmp/wpa_state");
			return -1;//模块存在但是没有关联上
		}
		else
	   	{
			fseek(fp, SEEK_SET, 0);
			fgets(ctmp, sizeof(ctmp), fp);
			if(atoi(ctmp) == 9)
			{
				int ipconfig = 0;
				if(IpGetConfig() == 1)				
					result = 0;//模块存在并且联接上
				else
					result = -1;
			}
			else
		   	{
				result = -1;
			}
			fclose(fp);
		}
	}
	return result;

}


//检测网线连接状态,每隔5秒钟运行一次
int get_netconnectstates(void)
{
	int i = 0;
	int ret = 0;
	int state = 0;
	
	memset(&eth,0,sizeof(eth)/sizeof(eth[0]));	

	strcpy(eth[0].eth_name,"eth0");
	eth[0].level = 0;
	strcpy(eth[1].eth_name,"eth2");
	eth[1].level = 1;

	
	g_linkstates = 0;
	for(i=0;i<2;i++)
	{
		
		ret = mii_diag(eth[i].eth_name);
		if(ret < 0)
		{
			state = NETCARD_NO_CONNECT;
		}
		else
		{
			if(i == 0)
				g_linkstates |= NET_LINECONNECT;
			else
				g_linkstates |= NET_WIFICONNECT;
			state = NETCARD_CONNECT;
		}
		
		eth[i].state = state;
	}
	
	return 0;
}

static int GetFirstRoute(char *buf)
{
	FILE *fp;
	int nread;
	int dest = 0;
	int gway,flags, refcnt, use, metric,mask, mtu, win,irtt;
	fp = fopen("/proc/net/route","r");
	if(NULL==fp)
	{
		perror("fopen");
		return -1;
	}
	if (fscanf(fp, "%*[^\n]\n") < 0) {
		fclose(fp);
		return -1;
	}
        nread = fscanf(fp, "%63s%X%X%X%d%d%d%X%d%d%d\n",
                           buf, &dest, &gway, &flags, &refcnt, &use, &metric, &mask,
                           &mtu, &win, &irtt);
	if(nread!=11)
	{
		fclose(fp);
		return -1;
	}
	fclose(fp);
	return 0;
}



static int default_route_get(const char *pName, char *gateway)
{

	FILE *fp;
	char *p = NULL;
	char line[128] = {0};
	int ret = 0;
	
	char Flags[10] = {0};
	char gateip[4] = {0};
	
	fp = fopen("/proc/net/route","r");
	if(NULL==fp)
	{
		perror("fopen");
		return -1;
	}

	while(fgets(line, sizeof(line), fp))
	{
		
		p = strstr(line, pName);
		if(p == NULL)	
			continue;
		else
		{
			strncpy(Flags, p + 23, 4);
			strncpy(gateway, p + 14, 8);
			printf("the gateway is [%s]\n", gateway);
			sscanf(gateway, "%02hhx%02hhx%02hhx%02hhx", &gateip[3], &gateip[2], &gateip[1], &gateip[0]);
			printf("the gateip is [%d.%d.%d.%d \n", gateip[0], gateip[1], gateip[2], gateip[3]);
			//printf("the flags:[%s]\n", Flags);
			ret = atoi(Flags);
			if(ret == 3)	//默认路由
				break;
		}
	}
	
	fclose(fp);
	return ret;
}

static int del_route_handle(const char *pName)
{
	FILE *fp;
	char *p = NULL;
	char line[128] = {0};
	char Flags[20] = {0};
	char desIp[4] = {0};
	char maskIp[4] = {0};
	char Destination[20] = {0};
	char Netmask[20] = {0};
	char cmdBuf[100] = {0};
	int ret = 0;
	
	
	
	fp = fopen("/proc/net/route","r");
	if(NULL==fp)
	{
		perror("fopen");
		return -1;
	}

	while(fgets(line, sizeof(line), fp))
	{
		
		p = strstr(line, pName);
		if(p == NULL)	
			continue;
		else
		{
			strncpy(Flags, p + 23, 4);			
			//printf("the flags:[%s]\n", Flags);
			if(atoi(Flags) == 1)	//正常路由
			{
				strncpy(Destination, p + 5, 8);				
				sscanf(Destination, "%02hhx%02hhx%02hhx%02hhx", &desIp[3],&desIp[2], &desIp[1], &desIp[0]);
				snprintf(Destination, 20, "%d.%d.%d.%d", desIp[0], desIp[1], desIp[2], desIp[3]);
				//printf("the Destination:[%s]\n", Destination);
				
				strncpy(Netmask, p + 34, 8);
				sscanf(Netmask, "%02hhx%02hhx%02hhx%02hhx", &maskIp[3],&maskIp[2], &maskIp[1], &maskIp[0]);
				snprintf(Netmask, 20, "%d.%d.%d.%d", maskIp[0], maskIp[1], maskIp[2], maskIp[3]);
				//printf("the Netmask:[%s]\n", Destination);
				
				snprintf(cmdBuf, 100, "route del -net %s netmask %s dev %s", Destination, Netmask, pName);
				ExecCmd(cmdBuf);
			}
			else if(atoi(Flags) == 3)	//默认路由
			{
				snprintf(cmdBuf, 100, "route del default gw 0.0.0.0 dev %s", pName);
				ExecCmd(cmdBuf);
			}
			
		}
	}
	
	fclose(fp);
	

	return 0;
}






//主函数
int main(void)
{
	int i = 0;
	int flag = 0;
	char cmdBuf[100] = {0};

	ExecCmd("ifconfig eth0 up");
	
	while(1)
	{
		sleep(5);
		get_netconnectstates();
		
		//网络状态没有发生改变，退出继续下一次循环
		if( g_linkstates == g_linkstatesold )
		{
			//printf("glinkstate not change..\n");
			continue;
		}
		else
			printf("\033[33meth0_state:%d	eth2_state:%d	g_linkstate:%02x	g_linikstate_old:%02x\033[0m\n",
										eth[0].state, eth[1].state, g_linkstates, g_linkstatesold);

		if((eth[0].state == 0) && (eth[1].state == 0))
		{
			//删除有线和无线路由表
			del_route_handle(eth[0].eth_name);
			del_route_handle(eth[1].eth_name);
		}
		else if((eth[0].state == 0) && (eth[1].state == 1))
		{
			//删除有线路由表
			del_route_handle(eth[0].eth_name);

			//无线获取通过dhcp获取ip
			super_killer("udhcpc");
			snprintf(cmdBuf, 100, "udhcpc -a -i %s -s /usr/sbin/udhcpc.script &", eth[1].eth_name);
			ExecCmd(cmdBuf);
			printf("\033[33mRoute Switch To %s Done\033[0m\n", eth[1].eth_name);			
		}
		else if((eth[0].state == 1) && (eth[1].state == 0))
		{
			//删除无线路由表
			del_route_handle(eth[1].eth_name);

			//有线获取通过dhcp获取ip
			super_killer("udhcpc");
			snprintf(cmdBuf, 100, "udhcpc -a -i %s -s /usr/sbin/udhcpc.script &", eth[0].eth_name);
			ExecCmd(cmdBuf);
			printf("\033[33mRoute Switch To %s Done\033[0m\n", eth[0].eth_name);
		}
		else if((eth[0].state == 1) && (eth[1].state == 1))		//有线和无线都通时，默认路由为有线
		{
			//有线获取通过dhcp获取ip
			super_killer("udhcpc");
			snprintf(cmdBuf, 100, "udhcpc -a -i %s -s /usr/sbin/udhcpc.script &", eth[0].eth_name);
			ExecCmd(cmdBuf);
			printf("\033[33mRoute Switch To %s Done\033[0m\n", eth[0].eth_name);
			sleep(1);
			snprintf(cmdBuf, 100, "udhcpc -a -i %s -s /usr/sbin/udhcpc.script &", eth[1].eth_name);
			ExecCmd(cmdBuf);
			printf("\033[33mRoute Switch To %s Done\033[0m\n", eth[1].eth_name);
		}
		g_linkstatesold = g_linkstates;		
		
	}

	return 0;
}



























