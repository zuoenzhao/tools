#ifndef _MONITOR_H_
#define _MONITOR_H_


typedef unsigned char  uint8_t;
typedef unsigned short  uint16_t;
typedef unsigned char  U8_T;
typedef unsigned short  UINT16;
typedef unsigned char UINT8;
typedef unsigned int UINT32;


/* Modes as human readable strings */
const char * const iw_operation_mode[] = { "Auto",
					"Ad-Hoc",
					"Managed",
					"Master",
					"Repeater",
					"Secondary",
					"Monitor",
					"Unknown/bug" };


					

/* Frequency flags */
#define IW_FREQ_AUTO		0x00	/* Let the driver decides */
#define IW_FREQ_FIXED		0x01	/* Force a specific value */

#define SIOCSIWFREQ	0x8B04		/* set channel/frequency (Hz) */
#define SIOCGIWFREQ	0x8B05		/* get channel/frequency (Hz) */
#define SIOCSIWMODE	0x8B06		/* set operation mode */
#define SIOCGIWMODE	0x8B07		/* get operation mode */


/*	channel:11  	m=11 		e=0	i=0 flag=0(force) 1(aoto)
	freq:2.462G 	m=246200000 e=1	i=0 
	freq:2462M		m=246200000 e=1	i=0 
	freq:2462000k	m=246200000 e=1	i=0 */
struct	iw_freq
{
	__s32		m;		/* Mantissa */
	__s16		e;		/* Exponent */
	__u8		i;		/* List index (when in range struct) */
	__u8		flags;		/* Flags (fixed/auto) */
};					

/* ------------------------ IOCTL REQUEST ------------------------ */
/*
 * The structure to exchange data for ioctl.
 * This structure is the same as 'struct ifreq', but (re)defined for
 * convenience...
 *
 * Note that it should fit on the same memory footprint !
 * You should check this when increasing the above structures (16 octets)
 * 16 octets = 128 bits. Warning, pointers might be 64 bits wide...
 */
struct	iwreq 
{
	union
	{
		char	ifrn_name[IFNAMSIZ];	/* if name, e.g. "eth0" */
	} ifr_ifrn;

	/* Data part */
	union
	{
		/* Config - generic */
		char		name[IFNAMSIZ];		/* Name : used to verify the presence of  wireless extensions. * Name of the protocol/provider... */
		struct iw_freq	freq;	/* frequency or channel :* 0-1000 = channel  * > 1000 = frequency in Hz */
		__u32		mode;		/* Operation mode */	
	}u;
};




typedef struct WlanInfo
{
	char ethName[10];
	char srcMac[6];		//源mac地址过滤，如果符合，存入内存
	char dstMac[6];		//目的mac地址过滤，如果符合，存入内存
	char recType;	//0xff接收三种帧 0x00接收管理帧 0x1接收控制帧 0x2接收数据帧
	int run;
	int channelChange;
	int frameTotal;
	struct timeval begin;
	struct timeval now;
}WlanInfo_s;

WlanInfo_s gWlan;

typedef enum ModeInfo
{
	bMode,
	gMode,
	nMode,
	aMode,	
}ModeInfo_e;

//无线帧类型
#define M_ASSOCIATION_REQUEST		0x00	//连接请求帧
#define M_ASSOCIATION_RESPONSE		0x10	//连接响应帧
#define M_REASSOCIATION_REQUEST		0x20	//重连请求帧
#define M_REASSOCIATION_RESPONSE	0x30	//重连响应镇
#define M_PROBE_REQUEST				0x40	//探测请求帧
#define M_PROBE_RESPONSE			0x50	//探测响应帧
#define M_BEACON					0x80	//信标beacon帧
#define M_ATIM						0x90	//通知传输指示消息帧
#define M_DISASSOCIATION			0xa0	//解除连接帧
#define M_AUTHENTICATION			0xb0	//身份验证帧
#define M_DEAUTHENTICATION			0xc0	//解除身份验证帧
#define D_DATE						0x08	//数据帧	
#define D_DATE_NULL					0x48	//数据帧（未传送数据）
#define D_DATE_QOS					0x88	//n模式数据帧
#define D_DATE_QOS_NULL				0xc8	//n模式无数据传输数据帧
#define C_BLOCK_ACKREQ				0x84	//块确认请求帧
#define C_BLOCK_ACK					0x94	//块确认
#define C_PS_POLL					0xa4	//用于节电模式
#define C_RTS						0xb4	//请求发送帧，申请预约
#define C_CTS						0xc4	//清除发送，同意预约
#define C_ACK						0xd4	//确认帧





typedef struct FrameInfo
{
	double rate;
	char type;	
	int rssi;
	int channel;
	int secNum;
	ModeInfo_e mode;
	char frameName[25];
	char src[6];
	char dst[6];
	char bssid[6];
}FrameInfo_s;



typedef struct Frame
{
	uint8_t frame_type;// 1
	uint8_t frame_ctrl_flags;// 1
	uint16_t duration;// 2
	char src[6];// 数据帧发帧的最初地址
	char dst[6]; // 数据帧的最终接受地址
	char bssid[6];//AP地址
	uint16_t fragment_num:4;// 2
	uint16_t seq_frag_num:12;// 2
	uint16_t datalen;// 2 
}DateFrame_s;








#endif
