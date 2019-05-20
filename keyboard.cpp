/*
	YAPE - Yet Another Plus/4 Emulator

	The program emulates the Commodore 264 family of 8 bit microcomputers

	This program is free software, you are welcome to distribute it,
	and/or modify it under certain conditions. For more information,
	read 'Copying'.

	(c) 2000, 2001 Attila Grósz
*/

#include "keyboard.h"
#include <memory.h>

/*
 Values		: C16 keyboard matrix positions
 Positions	: SDL key scan codes
 Eg. C16 has the "return" at matrix postition 8
	 SDL has it at position 13 (0x0D), ie. the 13. position has the value 8
*/

unsigned char sdltrans[512]=
	{
//      0
	  255,255, 255, 255  ,255 ,255 ,255, 255,  0,  63, 255,  255, 255, 8, 255, 255, // 15
      255, 29,   5,  30  ,  6 ,255 ,255, 255,  255, 255, 255,  255, 255, 255, 255, 255, // 31
       39,255, 255, 255  ,255 ,255 ,255, 22,  255, 255, 255,  255,  61,  62,  37,  56, // 47
       38,  7,  31, 1    , 25 ,  2 , 26,   3,   27,   4, 255,  45, 255,  46, 255, 255,
//      64
      255,255, 255, 255  ,255 ,255  ,255, 255,  255, 255, 255,  255, 255, 255, 255, 255,
      255,255, 255, 255  ,255 ,255  ,255, 255,  255, 255, 255,   54,  14,  53, 255, 255, // 95
       28, 17,  35,  34  , 18 , 49  , 42,  19,  43,  12,  20,    44,   21,  36,  60,  52, // 111
       13, 55,  10,  41  , 50 , 51  , 59,   9,   58, 11,  33,   255,  255, 255, 255, 255,
//      128
     };

const unsigned int joystick[5]={
	273,275,274,276,305 }; // PC keycodes up, right, down, left and fire

const unsigned int joykeys[2][5]=
	{ {2, 26, 10, 18, 50 }, // keys for joystick 1 and 2 up, right, down, left and fire
	{1, 25, 9, 17, 57 } };

const unsigned int origkeys[5]={
	29, 30, 5, 6, 23};

//--------------------------------------------------------------

KEYS::KEYS() {
	register int i;

	memset(sdltrans+128, 0xFF,256);
	sdltrans[273] = 29; // UP
	sdltrans[274] = 5; // DOWN
	sdltrans[275] = 30; // RIGHT
	sdltrans[276] = 6; // LEFT
	sdltrans[278] = 15; // HOME
	sdltrans[279] = 16; // POUND
	sdltrans[282] = 32; // F1
	sdltrans[283] = 40; // F2
	sdltrans[284] = 48; // F3
	sdltrans[285] = 24; // F4
	sdltrans[304] = 57; // SHIFT
	sdltrans[305] = 23; // RIGHT CTRL = CTRL
	sdltrans[306] = 23; // LEFT CTRL = CTRL
	sdltrans[307] = 47; // RIGHT ALT = COMMODORE

	memset(joytrans,0xFF,512);
	activejoy=0;
	for (i=0;i<5;++i)
		joytrans[joystick[i]]=joykeys[activejoy][i];

	empty();
}

void KEYS::empty(void)
{
	memset(keybuffer,0,256);
	memset(joybuffer,0,256);
}

void KEYS::pushkey(unsigned int code)
{
	*(keybuffer+sdltrans[code])=0x80;
	*(joybuffer+joytrans[code])=0x80;
}

void KEYS::releasekey(unsigned int code)
{
	*(keybuffer+sdltrans[code])=0;
	*(joybuffer+joytrans[code])=0;
}

unsigned char KEYS::key_trans(unsigned char r)
{
	return ~((keybuffer[r]>>7)|(keybuffer[r+8]>>6)|(keybuffer[r+16]>>5)|(keybuffer[r+24]>>4)|(keybuffer[r+32]>>3)|(keybuffer[r+40]>>2)|(keybuffer[r+48]>>1)|(keybuffer[r+56]>>0));
}

unsigned char KEYS::feedkey(unsigned char latch)
{
	static unsigned char tmp;

	tmp=0xFF;

	if ((latch&0x01)==0) tmp&=key_trans(0);
	if ((latch&0x02)==0) tmp&=key_trans(1);
	if ((latch&0x04)==0) tmp&=key_trans(2);
	if ((latch&0x08)==0) tmp&=key_trans(3);
	if ((latch&0x10)==0) tmp&=key_trans(4);
	if ((latch&0x20)==0) tmp&=key_trans(5);
	if ((latch&0x40)==0) tmp&=key_trans(6);
	if ((latch&0x80)==0) tmp&=key_trans(7);

	return tmp;
}

unsigned char KEYS::joy_trans(unsigned char r)
{
	return ~((joybuffer[r]>>7)|(joybuffer[r+8]>>6)|(joybuffer[r+16]>>5)|(joybuffer[r+24]>>4)|(joybuffer[r+32]>>3)|(joybuffer[r+40]>>2)|(joybuffer[r+48]>>1)|(joybuffer[r+56]>>0));
}

unsigned char KEYS::feedjoy(unsigned char latch)
{
	static unsigned char tmp;

	tmp=0xFF;

	if ((latch&0x01)==0)
		tmp&=joy_trans(2);
	if ((latch&0x02)==0)
		tmp&=joy_trans(1);
	if ((latch&0x04)==0)
		tmp&=joy_trans(2); // Joy2 is wired two times...

	return tmp;
}

void KEYS::joyinit(void)
{
	register int i;

	for (i=0;i<5;++i) {
		joytrans[joystick[i]]=joykeys[activejoy][i];
		sdltrans[joystick[i]]=0xFF;
	}
}

void KEYS::swapjoy()
{
	register int i;

	activejoy=1-activejoy;
	for (i=0;i<5;++i)
		joytrans[joystick[i]]=joykeys[activejoy][i];
}

void KEYS::releasejoy()
{
	register int i;

	for (i=0;i<5;++i)
		sdltrans[joystick[i]]=origkeys[i];
}

KEYS::~KEYS() {

}




