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
	char srcMac[6];		//Դmac��ַ���ˣ�������ϣ������ڴ�
	char dstMac[6];		//Ŀ��mac��ַ���ˣ�������ϣ������ڴ�
	char recType;	//0xff��������֡ 0x00���չ���֡ 0x1���տ���֡ 0x2��������֡
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

//����֡����
#define M_ASSOCIATION_REQUEST		0x00	//��������֡
#define M_ASSOCIATION_RESPONSE		0x10	//������Ӧ֡
#define M_REASSOCIATION_REQUEST		0x20	//��������֡
#define M_REASSOCIATION_RESPONSE	0x30	//������Ӧ��
#define M_PROBE_REQUEST				0x40	//̽������֡
#define M_PROBE_RESPONSE			0x50	//̽����Ӧ֡
#define M_BEACON					0x80	//�ű�beacon֡
#define M_ATIM						0x90	//֪ͨ����ָʾ��Ϣ֡
#define M_DISASSOCIATION			0xa0	//�������֡
#define M_AUTHENTICATION			0xb0	//�����֤֡
#define M_DEAUTHENTICATION			0xc0	//��������֤֡
#define D_DATE						0x08	//����֡	
#define D_DATE_NULL					0x48	//����֡��δ�������ݣ�
#define D_DATE_QOS					0x88	//nģʽ����֡
#define D_DATE_QOS_NULL				0xc8	//nģʽ�����ݴ�������֡
#define C_BLOCK_ACKREQ				0x84	//��ȷ������֡
#define C_BLOCK_ACK					0x94	//��ȷ��
#define C_PS_POLL					0xa4	//���ڽڵ�ģʽ
#define C_RTS						0xb4	//������֡������ԤԼ
#define C_CTS						0xc4	//������ͣ�ͬ��ԤԼ
#define C_ACK						0xd4	//ȷ��֡





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
	char src[6];// ����֡��֡�������ַ
	char dst[6]; // ����֡�����ս��ܵ�ַ
	char bssid[6];//AP��ַ
	uint16_t fragment_num:4;// 2
	uint16_t seq_frag_num:12;// 2
	uint16_t datalen;// 2 
}DateFrame_s;








#endif
