#include "pt6302comm.h"
#include "stc11f02.h"

#define LEN_OF_PREFIX 2
#define BUFSIZE 16
#define UARTTIMEOUT 360

//#define UART_SEND
//#define NEC_SUPPORT
#define MCU_POWER_UART 

/**************************************************/
enum
{
	IR_READY,
	IR_HEAD,
	IR_USER_ID,
	IR_KEY_CODE,
	IR_RECVOK
};
#define CHECK_PERIOD(a, d, b, c)   ((uint16)((a) - (d)) > ((b) - (c)) && (uint16)((a) - (d)) <= ((b) + (c)))
#define NEC_CLOCKS_FOR_START    (97)          /* 13500 us*/
#define NEC_START_SKIP      (5)
#define NEC_CLOCKS_FOR_0    (8)          /* 1120 us*/
#define NEC_0_SKIP     (2)
#define NEC_CLOCKS_FOR_1    (16)         /*2240 us*/
#define NEC_1_SKIP      (3)
#define NEC_MAXCODECNT (16)
#define NEC_TIMEOUT (576)				//80ms


#ifdef NEC_SUPPORT
static uint8 g_nec_reccnt = 0;                
static uint8 g_stat_ir = IR_READY;
static uint16 g_curxtime = 0;
static uint16 g_lastxtime = 0;
static uint16 g_nec_usrid = 0;
static uint16 g_nec_keycode = 0;
static uint8 g_rctype = 0;
struct rc_t
{
	uint8 usrid;
	uint8 keycode_power;
};
static struct rc_t g_rc_t = {0xff, 0xff};
#endif


#define TH0_RELOAD_VALUE 128

enum
{
	SERIAL_STAT_PREFIX0 = 0,
	SERIAL_STAT_PREFIX1,
	SERIAL_STAT_PAYLOADLEN,
	SERIAL_STAT_PAYLOAD,
	SERIAL_STAT_CHECKSUM,
	SERIAL_STAT_RECVOK
};

BOOL g_uarttimeout = 0;
uint8 g_stat_recv = SERIAL_STAT_PREFIX0;
uint8 g_numofrecv = 0;
uint8 g_cntrecv = 0;
//uint8 g_getchar = 0;
uint8 g_checksum = 0;
uint16 g_jiffies = 0;
uint16 g_startrecvtime = 0;

uint8 g_wakesleep_recv_flag = 0;
BOOL g_issleep = 0;


#define _getcurtime(a) \
{\
	ET0 = 0;\
	a = g_jiffies;\
	ET0 = 1;\
}
const uint8 g_prefixbuf[LEN_OF_PREFIX + 1] = {0xA3, 0xFA, 0x9d};
uint8 g_payloadbuf[BUFSIZE + 1] = "ddddddddddddddddd";

void init();
static uint8 checksum(uint8* buf, uint8 len);
static BOOL istimeout(uint16, uint16);
void uartsendchar(uint8);
static void uartsendstr(uint8* str, uint8 len);
#if 1 
void main()
{
	init();
	while(1)
	{
		EA = 0;
#ifdef NEC_SUPPORT
		if(g_wakesleep_recv_flag)
		{
			pin_standby = ~pin_standby;
			pin_vcc = ~pin_vcc;
			P3_2 = ~P3_2;
			g_issleep = ~g_issleep;
			if(g_issleep)
			{
				pt6302_sendcmd(CMD_SHOW | 0x1, NULL, NULL);
			}
			else
			{
				pt6302_cleardcram();
				pt6302_clearadram();
				pt6302_sendcmd(CMD_SHOW, NULL, NULL);
			}
			g_wakesleep_recv_flag = FALSE;
			EA = 1;
			continue;
		}
#endif
		g_uarttimeout = istimeout(g_startrecvtime, UARTTIMEOUT);
		if(SERIAL_STAT_RECVOK == g_stat_recv)
		{
			if(g_checksum == (uint8)(g_prefixbuf[LEN_OF_PREFIX] + checksum(g_payloadbuf, g_payloadbuf[0])))
			{
				switch(g_payloadbuf[1] & 0xf0)
				{
					case CMD_DCRAM:
						pt6302_cleardcram();
						pt6302_sendcmd( g_payloadbuf[1], g_payloadbuf + 2, g_payloadbuf[0] - 1);
						break;
					case CMD_CGRAM:
						pt6302_sendcmd( g_payloadbuf[1], g_payloadbuf + 2, g_payloadbuf[0] - 1);
						break;
					case CMD_ADRAM:
						pt6302_sendcmd( g_payloadbuf[1], g_payloadbuf + 2, 1);
						break;
					case CMD_GOUTSET:
						pt6302_sendcmd( g_payloadbuf[1], NULL, NULL);
						break;
					case CMD_BRIGHTNESS:
						pt6302_sendcmd( g_payloadbuf[1], NULL, NULL);
						break;
					case CMD_NUM:
						pt6302_sendcmd( g_payloadbuf[1], NULL, NULL);
						break;
					case CMD_SHOW:
						pt6302_sendcmd( g_payloadbuf[1], NULL, NULL);
						break;
					case CMD_TEST:
						break;
					case CMD_MISC:
						switch(g_payloadbuf[1] & 0xf)
						{
#ifdef MCU_POWER_UART
							case 0:		  //STANDBY
								pin_standby = 1;
								pin_vcc = 0;
								P3_2 = 0;
								g_issleep = 1;
								pt6302_sendcmd(CMD_SHOW | 0x1, NULL, NULL);
								break;
							case 1:			//BOOTUP
								pin_standby = 0;
								pin_vcc = 1;
								P3_2 = 1;
								g_issleep = 0;
								pt6302_cleardcram();
								pt6302_clearadram();
								pt6302_sendcmd(CMD_SHOW, NULL, NULL);
								break;
#endif								
							case 2:		//init
								pt6302_init(g_payloadbuf[2], g_payloadbuf[3]);
#ifdef NEC_SUPPORT
								g_rc_t.usrid = g_payloadbuf[4];
								g_rc_t.keycode_power = g_payloadbuf[5];
#endif								
								break;
							case 3:		//clear
								pt6302_cleardcram();
								pt6302_clearadram();
								break;
							case 4:		//clear & show
								pt6302_cleardcram();
								pt6302_clearadram();
								pt6302_sendcmd(CMD_SHOW, NULL, NULL);
							default:
								break;
						}
						break;
					default:
						break;
				}
			}
			g_checksum = 0;
			g_uarttimeout = FALSE;
			g_stat_recv = SERIAL_STAT_PREFIX0;
		}
		EA = 1;
	}


}
#endif

static uint8 checksum(uint8* buf, uint8 len)
{
	uint8 i;
	uint8 sum;
	for(i = 0, sum = 0; i < len + 1; i++)
		sum += buf[i];
	return(sum);
}
void init()
{
	EA = 0;					//disables grobal interrupts
	P1M1 = 0x2;
	
	TMOD = 0x22;				//timer1:8bit reloaded; timer0:8bit reloaded
	TH1 = 255;				//57600 baud rate
	TL1 = 255;
	TH0 = 128;				//timer0 interrupt freq: 7200Hz
	TL0 = 128;
	
	SCON = 0xd0;				//mode #3, 9-bit uart enables serial reception
//	PCON |= POR_;
	PCON |= (SMOD_ + POR_);	//double baud rate, enable FE.
#ifdef NEC_SUPPORT
	IE1 = 1;					//enable external interrupt 1
	IT1 = 1;					//external interrupt falling edge triggered
#endif
	pt6302_init(0x74, 0x00);
	pt6302_sendcmd(CMD_DCRAM | 0x2, "Init ok", 7);
	
	pin_standby = 0;
	pin_vcc = 1;
	P3_2 = 1;
	g_issleep = 0;

	
	ES = 1;					//enables serial interrupt
	ET0 = 1;					//enables timer0 interrupt
#ifdef NEC_SUPPORT
	EX1 = 1;					//enables external interrupt 1
#endif
	EA = 1;					//enables grobal interrupts

	TR1 = 1;
	TR0 = 1;
}
void timer0irq() interrupt TF0_VECTOR using 1
{
	ET0 = 0;
	g_jiffies++;
	ET0 = 1;
}
#if 1
void serialrecv() interrupt SIO_VECTOR using 2
{	
	EA = 0;
	RI = 0;
	if(g_uarttimeout)
	{
		g_stat_recv = SERIAL_STAT_PREFIX0;
		g_uarttimeout = FALSE;
		g_numofrecv = 0;
		g_cntrecv = 0;
	}
	switch(g_stat_recv)
	{
		case SERIAL_STAT_PREFIX0:
			if(SBUF == g_prefixbuf[0])
			{
				g_stat_recv = SERIAL_STAT_PREFIX1;
				_getcurtime(g_startrecvtime);
			}
			break;
		case SERIAL_STAT_PREFIX1:
			if(SBUF == g_prefixbuf[1])
				g_stat_recv = SERIAL_STAT_PAYLOADLEN;
			else g_stat_recv = SERIAL_STAT_PREFIX0;
			break;
		case SERIAL_STAT_PAYLOADLEN:
			g_numofrecv = SBUF;
			if(BUFSIZE < g_numofrecv)
			{
				g_stat_recv = SERIAL_STAT_PREFIX0;				
				break;
			}
			g_stat_recv = SERIAL_STAT_PAYLOAD;
			g_cntrecv = 1;
			g_payloadbuf[0] = g_numofrecv;
			break;

		case SERIAL_STAT_PAYLOAD:
			g_payloadbuf[g_cntrecv++] = SBUF;
			if(!(--g_numofrecv))
				g_stat_recv = SERIAL_STAT_CHECKSUM;				
			break;
		case SERIAL_STAT_CHECKSUM:
			g_checksum = SBUF;
			g_cntrecv = 0;
			g_stat_recv = SERIAL_STAT_RECVOK;
			break;			
		default:
			break;
	}
	EA = 1;
}
#else
void serialrecv() interrupt SIO_VECTOR using 2
{
	ES = 0;
	RI = 0;
	if(g_cntrecv == 0)
	{
		g_payloadbuf[0] = SBUF;
		g_cntrecv = 1;
	}
	RI = 1;
}
#endif
#ifdef UART_SEND
static void uartsendchar(uint8 value)
{
	SBUF = value;
	while (TI == 0);
	TI = 0;
}
static void uartsendstr(uint8* str, uint8 len)
{
	uint8 i;
	for(i = 0; i < len; i++)
		uartsendchar(str[i]);
}
#endif
static BOOL istimeout(uint16 starttime, uint16 timeout)
{
	uint16 t;
	_getcurtime(t);
	return(t - starttime > timeout ? TRUE : FALSE);
}

#ifdef NEC_SUPPORT
void EX0InterruptHandler_IR() interrupt  IE1_VECTOR using 3
{
	uint16 tmp;
	EA = 0;
	g_curxtime = g_jiffies;
	if(NEC_TIMEOUT < (g_curxtime - g_lastxtime))
	{
		g_stat_ir = IR_READY;
	}
	switch(g_stat_ir)
	{
		case IR_READY:
			g_lastxtime = g_jiffies;
			g_stat_ir = IR_HEAD;
			break;
		case IR_HEAD:
			if(CHECK_PERIOD(g_curxtime, g_lastxtime, NEC_CLOCKS_FOR_START, NEC_START_SKIP))
			{
				g_lastxtime = g_curxtime;
				g_nec_reccnt = 0;
				g_nec_usrid = 0;
				g_stat_ir = IR_USER_ID;
			}else
			{
				g_lastxtime = 0;
				g_stat_ir = IR_READY;
			}
			break;
		case IR_USER_ID:	
			if(CHECK_PERIOD(g_curxtime, g_lastxtime, NEC_CLOCKS_FOR_0, NEC_0_SKIP))
			{
				g_lastxtime = g_curxtime;
				g_nec_reccnt++;
			}else
				if(CHECK_PERIOD(g_curxtime, g_lastxtime, NEC_CLOCKS_FOR_1, NEC_1_SKIP))
				{
					g_lastxtime = g_curxtime;
					tmp = 1 << g_nec_reccnt++;
					g_nec_usrid |= tmp;
				}else
				{
					g_lastxtime = 0;
					g_stat_ir = IR_READY;
				}
			if(NEC_MAXCODECNT <= g_nec_reccnt)
			{
				g_lastxtime = g_curxtime;
				g_nec_reccnt = 0;
				g_nec_keycode= 0;
				g_stat_ir = IR_KEY_CODE;
			}
			break;
		case IR_KEY_CODE:	
			if(CHECK_PERIOD(g_curxtime, g_lastxtime, NEC_CLOCKS_FOR_0, NEC_0_SKIP))
			{
				g_lastxtime = g_curxtime;
				g_nec_reccnt++;
			}else
				if(CHECK_PERIOD(g_curxtime, g_lastxtime, NEC_CLOCKS_FOR_1, NEC_1_SKIP))
				{
					g_lastxtime = g_curxtime;
					tmp = 1 << g_nec_reccnt++;
					g_nec_keycode |= tmp;
				}else
				{
					g_lastxtime = 0;
					g_stat_ir = IR_READY;
				}
			if(NEC_MAXCODECNT <= g_nec_reccnt)
			{
				g_lastxtime = g_curxtime;
				g_nec_reccnt = 0;
				g_stat_ir = IR_RECVOK;
			}			
			break;
		default:
			break;
	}
	if(IR_RECVOK == g_stat_ir)
	{
		if((0xff == (g_nec_usrid & 0xff) + (g_nec_usrid >> 8)) &&
			(0xff == (g_nec_keycode & 0xff) + (g_nec_keycode >> 8)) )
		{
			if((g_rc_t.usrid== (uint8)(g_nec_usrid & 0xff)) &&
				(g_rc_t.keycode_power == (uint8)(g_nec_keycode & 0xff)))
			{
				g_wakesleep_recv_flag = TRUE;
			}
		}
		g_stat_ir = IR_READY;
	}
	EA = 1;
}
#endif