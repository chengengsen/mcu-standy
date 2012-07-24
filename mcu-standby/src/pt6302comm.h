#ifndef _PT6302COMM_H_
#define _PT6302COMM_H_
#include "STC11F02.H"
#include "types.h"

#define MAX_DISPLAY_NUM 12


#define CMD_DCRAM 0x10
#define CMD_CGRAM 0x20
#define CMD_ADRAM 0x30
#define CMD_GOUTSET 0x40
#define CMD_BRIGHTNESS 0x50
#define CMD_NUM 0x60
#define CMD_SHOW 0x70
#define CMD_TEST 0x80
#define CMD_MISC 0x90

#define pin_csb P1_2
#define pin_din P1_1
#define pin_clkb P1_3
#define pin_standby P1_7
#define pin_vcc P1_6
#define pin_rst P1_4

#if 0
void pt6302_reset();
void pt6302_powset(BOOL);
void pt6302_standbyset(BOOL);
#endif
void pt6302_sendcmd(uint8 cmd, uint8* buf , uint8 len);
void pt6302_init(uint8 arg1, uint8 arg2);
void pt6302_clearadram();
void pt6302_cleardcram();
#endif
