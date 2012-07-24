#include "stc11f02.h"
#include "types.h"
#include "pt6302comm.h"


#define CLK_DUTY 1
#define TDOFF 8
#define TCSH 16




static const uint8 g_clearbuf[MAX_DISPLAY_NUM] = 
		{' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' '};

static void delay(uint8);
static void sendbyte(uint8);
/*********************************************/
void pt6302_sendcmd(uint8 cmd, uint8* buf, uint8 len)
{
	uint8 i;
	pin_csb = HI_LEVEL;
	delay(1);
	pin_csb = LOW_LEVEL;
	sendbyte(cmd);
	for(i = 0; i < len; i++)
	{
		delay(TDOFF);
		sendbyte(buf[i]);
	}
	delay(TCSH);
	pin_csb = HI_LEVEL;
	delay(1);
}
#if 0
void pt6302_standbyset(BOOL i)
{
	pin_standby = i;
	delay(1);
}

void pt6302_reset()
{
	pin_rst = LOW_LEVEL;
	delay(1);
	pin_rst = HI_LEVEL;
//	delay(1);
}
void pt6302_powset(BOOL i)
{
	pin_vcc = i;
	delay(1);
}
#endif
static void sendbyte(uint8 dat)
{
	uint8 i = 8;
	do
	{
		pin_clkb = LOW_LEVEL;
		pin_din = (dat & 0x01) ? HI_LEVEL : LOW_LEVEL;
		delay(CLK_DUTY);
		pin_clkb = HI_LEVEL;
		delay(CLK_DUTY);
		dat = dat >> 1;
 	}while(--i);
 	pin_din = LOW_LEVEL;
}

static void delay(uint8 i)
{
	while(i--);
}

void pt6302_init(uint8 arg1, uint8 arg2)
{
	pt6302_sendcmd(CMD_GOUTSET, NULL, NULL);
//	pt6302_sendcmd(CMD_BRIGHTNESS | 0x7, NULL, NULL);
	pt6302_sendcmd(CMD_BRIGHTNESS | ((arg1 >> 4) & 0xf), NULL, NULL);
	pt6302_sendcmd(CMD_NUM | (arg1 & 0xf), NULL, NULL);
	pt6302_cleardcram();
	pt6302_clearadram();
	pt6302_sendcmd(CMD_SHOW | (arg2 & 0xf), NULL, NULL);	
}
void pt6302_cleardcram()
{
	pt6302_sendcmd(CMD_DCRAM, g_clearbuf, MAX_DISPLAY_NUM);
}
void pt6302_clearadram()
{
	pt6302_sendcmd(CMD_ADRAM, g_clearbuf, MAX_DISPLAY_NUM);
}