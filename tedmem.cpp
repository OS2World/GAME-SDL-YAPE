/*<empty clipboard>
	YAPE - Yet Another Plus/4 Emulator

	The program emulates the Commodore 264 family of 8 bit microcomputers

	This program is free software, you are welcome to distribute it,
	and/or modify it under certain conditions. For more information,
	read 'Copying'.

	(c) 2000, 2001 Attila Grósz
*/
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <ctype.h>
#include "tedmem.h"
#include "sound.h"
#include "diskio.h"
#include "cpu.h"
#include "keyboard.h"
#include "tape.h"
#include "kernal.h"
#include "basic.h"
#include "3plus1.h"


#define TEXTMODE	0x00000000
#define GRAPHMODE	0x00000020
#define MULTICOLOR	0x00000010
#define EXTCOLOR	0x00000040
#define REVERSE		0x00000080
#define ILLEGAL		0x0000000F

/* Legend:
	p = Processor works
	* = Processor stop cycle, processor may write (STA, etc.)
	- = Empty
	S = TED fetches memory (0th line=character number, 7th=Char attribute)
	F = TED fetches FONT memory
	R = TED refreshes RAM

*/

// PAL SCREEN
const char DMA[2][115]
//    012345678901 -> 12x4 = 48
=  {{"-p-p-*-*-*-SFSFSFSFSFSFSFSFSFSFSFSFSFSFSFSFSFSFSFSFSFSFSFSFSFSFSFSFSFSFSFSFSFSFSFSFSFSFSFSFpRpRpRpRpRp-p-p-p-p-p-p"},
	{"pppp-*-*-*-SFSFSFSFSFSFSFSFSFSFSFSFSFSFSFSFSFSFSFSFSFSFSFSFSFSFSFSFSFSFSFSFSFSFSFSFSFSFSFSFpRpRpRpRpRppppppppppppp"}};

const char Normal[2][115]
=  {{"-p-p-p-p-p-pFpFpFpFpFpFpFpFpFpFpFpFpFpFpFpFpFpFpFpFpFpFpFpFpFpFpFpFpFpFpFpFpFpFpFpFpFpFpFpFpRpRpRpRpRp-p-p-p-p-p-p"},
	{"pppp-p-p-p-pFpFpFpFpFpFpFpFpFpFpFpFpFpFpFpFpFpFpFpFpFpFpFpFpFpFpFpFpFpFpFpFpFpFpFpFpFpFpFpFpRpRpRpRpRppppppppppppp"}};

const char Border[2][115]
=  {{"-p-p-p-p-p-p-p-p-p-p-p-p-p-p-p-p-p-p-p-p-p-p-p-p-p-p-p-p-p-p-p-p-p-p-p-p-p-p-p-p-p-p-p-p-p-pRpRpRpRpRp-p-p-p-p-p-p"},
	{"ppppppppppppppppppppppppppppppppppppppppppppppppppppppppppppppppppppppppppppppppppppppppppppRpRpRpRpRppppppppppppp"}};

#define LEFTEDGE 64 // 68

unsigned int			chr, charline, DMAline;
static int				x,y,tmp, row ;
static unsigned char	*charcol;
static unsigned char	*colorram, *grram, *gcindex;
unsigned int			cindex;

unsigned int offset = 64; // 84 68 60 52
unsigned int dmaoff = 16; // 20 20 12 4


MEM::MEM()
{
	register unsigned int	i;
	char					data[40];
	unsigned int			cmask;

	// clearing cartdridge ROMs
	for (i=0;i<4;++i) {
		memset(&(RomHi[i]),0,ROMSIZE);
		memset(&(RomLo[i]),0,ROMSIZE);
		memset(romlopath,0,sizeof(romlopath));
		memset(romhighpath,0,sizeof(romhighpath));
	};
	// default ROM sets
	strcpy(romlopath[0],"BASIC");
	strcpy(romhighpath[0],"KERNAL");

	for (i=0;i<RAMSIZE;(i>>1)<<1==i?Ram[i++]=0:Ram[i++]=0xFF); // init RAM

	// prepare multiplicator vectors
	for (i=0;i<SCR_VSIZE+1;i++)
		htab312[i]=i*SCR_HSIZE;

	memset(vtab320, 0, sizeof(vtab320));
	for (i=4;i<204;i++)
		vtab320[i]=((i-4)>>3)*320+((i-4)&0x07);

	memset(htab40,0,256);
	for (i=0;i<26;i++)
		htab40[i]=i*40;

	for (i=0;i<SCR_HSIZE+1;i++) {
		xcol[i]=i-LEFTEDGE;
		// bitmask for hires text speedup
		hccmask[i]=0x80>>(xcol[i]&0x07);
		cmask=(xcol[i]&0x0007)>>1;
		cmask=((~cmask)&0x03)<<1;
		// multicolor masks
		mccmask[i]=3<<cmask;
		mccmask2[i]=cmask;
		// the physical character column
		xcol[i]>>=3;
		// the horizontal raster register is offset to the electron beam...
		(i>=offset) ? rcolconv[i]=(i-offset)>>1 : rcolconv[i]=(456-offset+i)>>1; // was 68

		// ...and is also divided by 4
		// this is for the DMA table...
		(i>=dmaoff) ? xbeam[i]=(i-dmaoff)>>2 : xbeam[i]=(456-dmaoff+i)>>2;

		sprintf(data,"%d (%x) -> %d (%x) -> %d",i,i,rcolconv[i],rcolconv[i],xbeam[i]);
		//add_to_log(data);
	}
	// cursor coordinates
	for (i=0;i<1024;++i) {
		crclrx[i]=(i%40);
		crclry[i]=(i/40);
	}

	// 64 kbytes of memory allocated
	RAMMask=0xFFFF;

	// actual ram bank pointer default setting
	actram=Ram;
	// TED sound frequencies
	TEDfreq1=TEDfreq2=0;

	// setting screen memory pointer
	scrptr=screen;
	// pointer of the end of the screen memory
	endptr=scrptr+456*312;
	// setting the CPU to fast mode
	fastmode=1;
	DMAptr=Border[0];
	// initial position of the electron beam (upper left corner)
	irqline=charline=0;
	beamy=312-RASTEROFFSET;
	beamx=456-offset;
	soundon=false;
	scrblank= false;
	tapeload=tapesave=false;
	charrombank=charrambank=cset=colorram=Ram;
	scrattr=0;
	scrmode=&MEM::hi_text;
	timer1=timer2=timer3=0;

	// create an instance of the keyboard class
	keys = new KEYS;
	tap = new TAP;
	// setting the TAP::mem pointer to this MEM class
	tap->mem=this;

}

void MEM::texttoscreen(int x,int y, char *scrtxt)
{
	register int i =0;

	while (scrtxt[i]!=0)
		chrtoscreen(x+i*8,y,scrtxt[i++]);
}

void MEM::chrtoscreen(int x,int y, char scrchr)
{
	register int j, k;
	unsigned char *charset = (unsigned char *) kernal+0x1000;

	if (isalpha(scrchr)) {
		scrchr=toupper(scrchr)-64;
		charset+=(scrchr<<3);
		for (j=0;j<8;j++)
			for (k=0;k<8;k++)
				(*(charset+j) & (0x80>>k)) ? screen[(y+j)*456+x+k]=0x00 : screen[(y+j)*456+x+k]=0x71;
		return;
	}
	charset+=(scrchr<<3);
	for (j=0;j<8;j++)
		for (k=0;k<8;k++)
			(*(charset+j) & (0x80>>k)) ? screen[(y+j)*456+x+k]=0x00 : screen[(y+j)*456+x+k]=0x71;
}

void MEM::loadroms()
{
	for (int i=0;i<4;i++) {
		loadhiromfromfile(i,romhighpath[i]);
		loadloromfromfile(i,romlopath[i]);
	}
	// this should not be HERE, but where else could I've put it??
	tap->rewind();
}

void MEM::loadloromfromfile(int nr, char fname[256])
{
	FILE			*img;
	unsigned char	*ibuf;

	if (fname[0]!='\0')
		if (img = fopen(fname, "rb")) {
		// load high ROM file
		ibuf=(unsigned char *) malloc(ROMSIZE);
		fread(ibuf,ROMSIZE,1,img);
		for (register int i=0;i<ROMSIZE;RomLo[nr][i]=ibuf[i],i++);
		fclose(img);
		free(ibuf);
	} else
		switch (nr) {
			case 0: memcpy(&(RomLo[0]),basic,ROMSIZE);
				break;
			case 1: if (!strncmp(fname,"3PLUS1LOW",9))
						memcpy(&(RomLo[1]),plus4lo,ROMSIZE);
					else
						memset(&(RomLo[1]),0,ROMSIZE);
				break;
			default : memset(&(RomLo[nr]),0,ROMSIZE);
		}
}

void MEM::loadhiromfromfile(int nr, char fname[256])
{
	FILE			*img;
	unsigned char	*ibuf;

	if (fname[0]!='\0')
		if (img = fopen(fname, "rb")) {
		// load high ROM file
		ibuf=(unsigned char *) malloc(ROMSIZE);
		fread(ibuf,ROMSIZE,1,img);
		for (register int i=0;i<ROMSIZE;RomHi[nr][i]=ibuf[i],i++);
		fclose(img);
		free(ibuf);
	} else
		switch (nr) {
			case 0: memcpy(&(RomHi[0]),kernal,ROMSIZE);
				break;
			case 1: if (!strncmp(fname,"3PLUS1HIGH",10))
						memcpy(&(RomHi[1]),plus4hi,ROMSIZE);
					else
						memset(&(RomHi[1]),0,ROMSIZE);
				break;
			default : memset(&(RomHi[nr]),0,ROMSIZE);
		}
}

unsigned char MEM::read(unsigned int addr)
{
	if (RAMenable) {
		if (addr<0x1000)
			return Ram[addr];
		if (addr<0x4000)
			if (bramsm)
				return Ram[addr];
			else
				return *(actram+addr);
		if (addr<0xFD00)
			return *(actram+(addr & RAMMask));
		if (addr<0xFE00)
			return Ram[addr];
		if (addr<0xFF00)
			return ReadParallelIEC(addr);
		if (addr<0xFF40)
			switch (addr) {
				case 0xFF08 : if (joyemu)
								  return keys->feedjoy(Ram[0xFF08])&(keys->feedkey(Ram[0xFD30]));
							  else
								  return keys->feedkey(Ram[0xFD30]);
				case 0xFF0C : return (crsrpos>>8)&0x07;
				case 0xFF0D : return crsrpos&0xFF;
				case 0xFF00 : return Ram[0xFF00]=timer1&0xFF;
				case 0xFF01 : return Ram[0xFF01]=timer1>>8;
				case 0xFF02 : return Ram[0xFF02]=timer2&0xFF;
				case 0xFF03 : return Ram[0xFF03]=timer2>>8;
				case 0xFF04 : return Ram[0xFF04]=timer3&0xFF;
				case 0xFF05 : return Ram[0xFF05]=timer3>>8;
				case 0xFF09 : return Ram[0xFF09]|(0x21);
				case 0xFF0A : return Ram[0xFF0A]|(0xA0);
				case 0xFF15 : return Ram[0xFF15]|(0x80);	// The highest bit is not used and so always 1
				case 0xFF16 : return Ram[0xFF16]|(0x80);	// A few games (Rockman) used it...
				case 0xFF17 : return Ram[0xFF17]|(0x80);
				case 0xFF18 : return Ram[0xFF18]|(0x80);
				case 0xFF19 : return Ram[0xFF19]|(0x80);
				case 0xFF1D : return Ram[0xFF1D]=beamy&0xFF; /// setting the 1-8. bit of the rasterline counter
				case 0xFF1C : return Ram[0xFF1C]=(beamy>>8)|0xFE;
				case 0xFF1E : return Ram[0xFF1E]=rcolconv[beamx]; // raster column
				case 0xFF1A : return Ram[0xFF1A]=(htab40[cposy]>>8)&0xFF;
				case 0xFF1B : return Ram[0xFF1B]=(htab40[cposy])&0xFF;
				case 0xFF1F : return (0x80)|(crsrphase<<3)|charline;
				default     : return Ram[addr];
			}
		else
			return *(actram+(addr & RAMMask));
	} else {
		if (addr<0x1000)
			return Ram[addr];
		if (addr<0x4000)
			if (bramsm)
				return Ram[addr];
			else
				return *(actram+addr);
		if (addr<0x8000)
			return *(actram+(addr & RAMMask));
		if (addr<0xC000 )
			return *(actromlo+(addr & 0x7FFF));
		if (addr<0xFC00 )
			return *(actromhi+(addr & 0x3FFF));
		if (addr<0xFD00 )
			return RomHi[0][addr & 0x3FFF];
		if (addr<0xFE00)
			return Ram[addr];
		if (addr<0xFF00)
			return ReadParallelIEC(addr);
		if (addr<0xFF40)
			switch (addr) { // we repeat the code above for sake of speed
				case 0xFF08 : if (joyemu)
								  return keys->feedjoy(Ram[0xFF08])&(keys->feedkey(Ram[0xFD30]));
							  else
								  return keys->feedkey(Ram[0xFD30]);
				case 0xFF0C : return (crsrpos>>8)&0x07;
				case 0xFF0D : return crsrpos&0xFF;
				case 0xFF00 : return Ram[0xFF00]=timer1&0xFF;
				case 0xFF01 : return Ram[0xFF01]=timer1>>8;
				case 0xFF02 : return Ram[0xFF02]=timer2&0xFF;
				case 0xFF03 : return Ram[0xFF03]=timer2>>8;
				case 0xFF04 : return Ram[0xFF04]=timer3&0xFF;
				case 0xFF05 : return Ram[0xFF05]=timer3>>8;
				case 0xFF09 : return Ram[0xFF09]|(0x21);
				case 0xFF0A : return Ram[0xFF0A]|(0xA0);
				case 0xFF15 : return Ram[0xFF15]|(0x80);	// The highest bit is not used and so always 1
				case 0xFF16 : return Ram[0xFF16]|(0x80);	// A few games (Rockman) used it...
				case 0xFF17 : return Ram[0xFF17]|(0x80);
				case 0xFF18 : return Ram[0xFF18]|(0x80);
				case 0xFF19 : return Ram[0xFF19]|(0x80);
				case 0xFF1D : return Ram[0xFF1D]=beamy&0xFF; /// setting the 1-8. bit of the rasterline counter
				case 0xFF1C : return Ram[0xFF1C]=(beamy>>8)|0xFE;
							  // synch TED with the electron beam
							  // setting the 9. bit of the rasterline counter*/
				case 0xFF1E : return Ram[0xFF1E]=rcolconv[beamx]; // raster column
				case 0xFF1A : return Ram[0xFF1A]=(htab40[cposy]>>8)&0xFF;
				case 0xFF1B : return Ram[0xFF1B]=(htab40[cposy])&0xFF;
				case 0xFF1F : return (0x80)|(crsrphase<<3)|charline;
				default     : return Ram[addr];
			}
		return *(actromhi+(addr & 0x3FFF));
	}
}

void MEM::wrt(unsigned int addr, unsigned char value)
{
	if (addr<0x1000) {
		Ram[addr]=value;
		return;
	}
	if (addr<0x4000)
		if (bramsm) {
			Ram[addr]=value;
			return;
		} else {
			*(actram+addr)=value;
			return;
		}
	if (addr<0xFD00) {
		*(actram+(addr & RAMMask))=value;
		return;
	}
	if (addr<0xFE00) {
		// this has to be ironed out once... all 16 keyboard lines
		// change values together, and all are writable
		if (addr>>4==0xFD3) {
			Ram[addr]=value;
			return;
		}
		// ROM paging
		if (addr>>4==0xFDD) {
			actromlo=&(RomLo[addr&0x03][0]);
			actromhi=&(RomHi[(addr&0x0c)>>2][0]);
			return;
		}
		// 256 KB Hannes RAM extension handling
		if (addr==0xFD16 && bigram)
			if ((value&0x7F)==3) {
				actram=&(Ram[0]);
				bramsm=false;
			} else {
				actram=&(RamExt[value&0x03][0]);
				value&0x80 ? bramsm=true : bramsm=false;
			}
		Ram[addr]=value; // RS232, tape and keyboard communication
		return;
	}
	if (addr<0xFF00) {
		WriteParallelIEC(addr,value); // parallel IEC communication
		return;
	}
	if (addr<0xFF40) {
		switch (addr) {
			case 0xFF00 : t1on=false; // Timer1 disabled
						  t1start=(t1start & 0xFF00)|value;
						  timer1=(timer1 & 0xFF00)|value;
						  Ram[0xFF00]=value;
				break;
			case 0xFF01 : t1on=true; // Timer1 enabled
						  t1start=(t1start & 0xFF)|(value<<8);
						  timer1=(timer1 & 0x00FF)|(value<<8);
						  Ram[0xFF01]=value;
						  //timer1-=3;
				break;
			case 0xFF02 : t2on=false; // Timer2 disabled
						  timer2=(timer2 & 0xFF00)|value;
						  Ram[0xFF02]=value;
				break;
			case 0xFF03 : t2on=true; // Timer2 enabled
						  timer2=(timer2&0x00FF)|(value<<8);
						  Ram[0xFF03]=value;
				break;
			case 0xFF04 : t3on=false;  // Timer3 disabled
						  timer3=(timer3&0xFF00)|value;
						  Ram[0xFF04]=value;
				break;
			case 0xFF05 : t3on=true; // Timer3 enabled
						  timer3=(timer3&0x00FF)|(value<<8);
						  Ram[0xFF05]=value;
				break;
			case 0xFF06 : Ram[0xFF06]=value;
						  // check for flat screen (23 rows)
						  (Ram[0xFF06]&0x08) ? fltscr=4 : fltscr=0;
						  // get vertical offset of screen when smooth scroll
						  vshift=(Ram[0xFF06]&0x07)-3; // this actually freezes Yape at times
						  // check for extended mode
						  ecmode=Ram[0xFF06]&0x40;
						  if (ecmode) {
							  tmp=(0xF800)&RAMMask;
							  scrattr|=EXTCOLOR;
						  } else {
							  tmp=(0xFC00)&RAMMask;
							  scrattr&=~EXTCOLOR;
						  }
						  charrambank=Ram+((Ram[0xFF13]<<8)&tmp);
						  charrombank=&(RomHi[0][((Ram[0xFF13] & 0x3C)<<8)&tmp]);
						  // check for graphics mode (5th b14it)
						  grmode=Ram[0xFF06]&0x20;
						  scrattr=(scrattr&~GRAPHMODE)|grmode;
				break;
			case 0xFF07 : Ram[0xFF07]=value;
						  // check for narrow screen (38 columns)
						  nrwscr=Ram[0xFF07]&0x08;
						  scrleft=LEFTEDGE-nrwscr+8;
						  scrright=LEFTEDGE+320+nrwscr-7;
						  // get horizontal offset of screen when smooth scroll
						  hshift=Ram[0xFF07]&0x07;
						  // check for reversed mode
						  rvsmode=Ram[0xFF07]&0x80;
						  if (rvsmode) {
							  tmp=(0xF800)&RAMMask;
							  scrattr|=REVERSE;
						  } else {
							  tmp=(0xFC00)&RAMMask;
							  scrattr&=~REVERSE;
						  }
						  charrombank=&(RomHi[0][((Ram[0xFF13] & 0x3C)<<8)&tmp]);
						  charrambank=Ram+((Ram[0xFF13]<<8)&tmp);
						  (charrom) ? cset = charrombank : cset = charrambank;
						  // check for multicolor mode
   						  mcmode=Ram[0xFF07]&0x10;
						  scrattr=(scrattr&~MULTICOLOR)|mcmode;
				break;
			case 0xFF09 : // clear the interrupt requester bits
						  // by writing 1 into them (!!)
						  Ram[0xFF09]=(Ram[0xFF09]&0x7F)&(~value);
				break;
			case 0xFF0A : Ram[0xFF0A]=value;
						  // change the raster irq line
						  irqline=(irqline&0xFF)+((value&0x01)<<8);
				break;
			case 0xFF0B : Ram[0xFF0B]=value;
						  irqline=value+(irqline&0x0100);
				break;
			case 0xFF0C : crsrpos=((value<<8)+(crsrpos&0xFF))&0x3FF;
						  Ram[0xFF0C]=value;
				break;
			case 0xFF0D : crsrpos=value+(crsrpos&0xFF00);
						  Ram[0xFF0D]=value;
				break;
			case 0xFF0E : TEDfreq1=(TEDfreq1&0x300)|value;
						  Ram[0xFF0E]=value;
				break;
			case 0xFF0F : TEDfreq2=(TEDfreq2&0x300)|value;
						  Ram[0xFF0F]=value;
				break;
			case 0xFF10 : TEDfreq2=((value&0x3)<<8)|(TEDfreq2&0xFF);
						  Ram[0xFF10]=value;
				break;
			case 0xFF11 : Ram[0xFF11]=value;
						  TEDVolume=value&0x0F;
						  if (TEDVolume>8) TEDVolume=8;
						  TEDChannel1=value&0x10;
						  TEDChannel2=value&0x20;
						  TEDNoise=value&0x40 ;
						  TEDDA = value&0x80;
				break;
			case 0xFF12:  grbank=Ram+((value&0x38)<<10);
						  TEDfreq1=((value&0x3)<<8)|(TEDfreq1&0xFF);
						  // if the 2nd bit is set the chars are read from ROM
						  charrom=(value&0x04)>0;
						  if (charrom && Ram[0xFF13]<0x80)
							scrattr|=ILLEGAL;
						  else {
							scrattr&=~ILLEGAL;
							(charrom) ? cset = charrombank : cset = charrambank;
						  }
						  Ram[0xFF12]=value;
				break;
			case 0xFF13 : // the 0th bit is not writable, it indicates if the ROMs are on
						  Ram[0xFF13]=(value&0xFE)|(Ram[0xFF13]&0x01);
						  // bit 1 is the fast/slow mode switch
						  value&0x02 ? fastmode=0 : fastmode=1;
						  (ecmode || rvsmode) ? tmp=(0xF800)&RAMMask : tmp=(0xFC00)&RAMMask;
						  charbank = ((value)<<8)&tmp;
						  charrambank=Ram+charbank;
						  charrombank=&(RomHi[0][charbank & 0x3C00]);
						  if (charrom && value<0x80)
							scrattr|=ILLEGAL;
						  else {
							scrattr&=~ILLEGAL;
							(charrom) ? cset = charrombank : cset = charrambank;
						  }
				break;
			case 0xFF14 : Ram[0xFF14]=value;
						  colorbank=Ram+(((value&0xF8)<<8)&RAMMask);
				break;
			case 0xFF15 : mcol[0]=Ram[0xFF15]=value;
				break;
			case 0xFF16 : Ram[0xFF16]=value;
						  mcol[1]=Ram[0xFF16];
				break;
			case 0xFF17 : Ram[0xFF17]=value;
						  mcol[2]=Ram[0xFF17];
				break;
			case 0xFF19 : framecol=(value<<24)|(value<<16)|(value<<8)|value;
						  Ram[0xFF19]=value;
				break;
			case 0xFF1C : Ram[0xFF1C]=(value&0x01)|0xFE;
						  beamy=((value&0x01)<<8)|(beamy&0xFF);
						  charline=(beamy-4)&0x07;
						  cposy=(beamy-4)>>3;
				break;
			case 0xFF1D : Ram[0xFF1D]=value;
						  beamy=(beamy&0x0100)|value;
						  charline=beamy&0x07;
						  cposy=(beamy-4)>>3;
				break;
/*			case 0xFF1E : // FIXME!
						  beamx=((~value)<<1)&0xFF;
						  // value=(min(value,228))<<1;
						  beamx>=(456-offset) ? beamx=beamx-(456-offset) : beamx=beamx+offset;
						  // beamx>=372 ? beamx=value-372 : beamx=value+84;
						  if (beamx>451)
							  beamx=456;
						  if (scrptr>=endptr)
							  scrptr=screen;
						  x=beamx;
						  Ram[0xFF1E]=beamx;
				break;*/
			case 0xFF1F : charline=value&0x07;
						  /*if (vshift<4)
							(charline<DMAline) ? cposy = row + 1 : cposy = row;
						  else
							(charline<DMAline) ? cposy = row : cposy = row-1;*/
						  charline = ( charline + 3 - vshift )&0x07;
						  //if (DMAline-charline>0) cposy= row + 1;
						  //if (charline == 7) cposy += 1;
						  crsrphase=(crsrphase&0x1F)|(value>>3);
						  Ram[0xFF1F]=value;
				break;
			case 0xFF3E : Ram[0xFF13]|=0x01;
						  RAMenable=false;
				break;
			case 0xFF3F : Ram[0xFF13]&=0xFE;
						  RAMenable=true;
				break;
			default 	: Ram[addr]=value;
		};
		return;
	}
	else
		*(actram+(addr & RAMMask))=value;
}

void MEM::dump(void *img)
{
	// this is ugly :-P
   	fwrite(Ram,RAMSIZE,1,(FILE *) img);
	fwrite(&RAMenable,sizeof(RAMenable),1,(FILE *) img);
	fwrite(&t1start,sizeof(t1start),1, (FILE *) img);
	fwrite(&t1on,sizeof(t1on),1, (FILE *) img);
	fwrite(&t2on,sizeof(t2on),1, (FILE *) img);
	fwrite(&t3on,sizeof(t3on),1, (FILE *) img);
	fwrite(&timer1,sizeof(timer1),1, (FILE *) img);
	fwrite(&timer2,sizeof(timer2),1, (FILE *) img);
	fwrite(&timer3,sizeof(timer3),1, (FILE *) img);
	fwrite(&beamx,sizeof(beamx),1, (FILE *) img);
	fwrite(&beamy,sizeof(beamy),1, (FILE *) img);
	//fwrite(&x,sizeof(x),1, (FILE *) img);
	fwrite(&irqline,sizeof(irqline),1, (FILE *) img);
	fwrite(&scrleft,sizeof(scrleft),1, (FILE *) img);
	fwrite(&scrright,sizeof(scrright),1, (FILE *) img);
	fwrite(&crsrpos,sizeof(crsrpos),1, (FILE *) img);
	fwrite(&scrattr,sizeof(scrattr),1, (FILE *) img);
	fwrite(&tmp,sizeof(tmp),1, (FILE *) img);
	fwrite(&nrwscr,sizeof(nrwscr),1, (FILE *) img);
	fwrite(&hshift,sizeof(hshift),1, (FILE *) img);
	fwrite(&vshift,sizeof(vshift),1, (FILE *) img);
	fwrite(&fltscr,sizeof(fltscr),1, (FILE *) img);
	fwrite(&mcol,sizeof(mcol),1, (FILE *) img);
	fwrite(&tapeload,sizeof(tapeload),1, (FILE *) img);
	fwrite(&tapesave,sizeof(tapesave),1, (FILE *) img);
	fwrite(chrbuf,40,1, (FILE *) img);
	fwrite(clrbuf,40,1, (FILE *) img);
	fwrite(&charrom,sizeof(charrom),1, (FILE *) img);
	fwrite(&charbank,sizeof(charbank),1, (FILE *) img);
	fwrite(&TEDfreq1,sizeof(TEDfreq1),1, (FILE *) img);
	fwrite(&TEDfreq2,sizeof(TEDfreq2),1, (FILE *) img);
	fwrite(&TEDVolume,sizeof(TEDVolume),1, (FILE *) img);
	fwrite(&TEDDA,sizeof(TEDDA),1, (FILE *) img);
	fwrite(&framecol,sizeof(framecol),1, (FILE *) img);
}

void MEM::memin(void *img)
{

	// this is ugly :-P
   	fread(Ram,RAMSIZE,1,(FILE *) img);

	fread(&RAMenable,sizeof(RAMenable),1,(FILE *) img);
	fread(&t1start,sizeof(t1start),1, (FILE *) img);
	fread(&t1on,sizeof(t1on),1, (FILE *) img);
	fread(&t2on,sizeof(t2on),1, (FILE *) img);
	fread(&t3on,sizeof(t3on),1, (FILE *) img);
	fread(&timer1,sizeof(timer1),1, (FILE *) img);
	fread(&timer2,sizeof(timer2),1, (FILE *) img);
	fread(&timer3,sizeof(timer3),1, (FILE *) img);
	fread(&beamx,sizeof(beamx),1, (FILE *) img);
	fread(&beamy,sizeof(beamy),1, (FILE *) img);
	//fread(&x,sizeof(x),1, (FILE *) img);
	fread(&irqline,sizeof(irqline),1, (FILE *) img);
	fread(&scrleft,sizeof(scrleft),1, (FILE *) img);
	fread(&scrright,sizeof(scrright),1, (FILE *) img);
	fread(&crsrpos,sizeof(crsrpos),1, (FILE *) img);
	fread(&scrattr,sizeof(scrattr),1, (FILE *) img);
	fread(&tmp,sizeof(tmp),1, (FILE *) img);
	fread(&nrwscr,sizeof(nrwscr),1, (FILE *) img);
	fread(&hshift,sizeof(hshift),1, (FILE *) img);
	fread(&vshift,sizeof(vshift),1, (FILE *) img);
	fread(&fltscr,sizeof(fltscr),1, (FILE *) img);
	fread(&mcol,sizeof(mcol),1, (FILE *) img);
	fread(&tapeload,sizeof(tapeload),1, (FILE *) img);
	fread(&tapesave,sizeof(tapesave),1, (FILE *) img);
	fread(chrbuf,40,1, (FILE *) img);
	fread(clrbuf,40,1, (FILE *) img);
	fread(&charrom,sizeof(charrom),1, (FILE *) img);
	fread(&charbank,sizeof(charbank),1, (FILE *) img);
	fread(&TEDfreq1,sizeof(TEDfreq1),1, (FILE *) img);
	fread(&TEDfreq2,sizeof(TEDfreq2),1, (FILE *) img);
	fread(&TEDVolume,sizeof(TEDVolume),1, (FILE *) img);
	fread(&TEDDA,sizeof(TEDDA),1, (FILE *) img);
	fread(&framecol,sizeof(framecol),1, (FILE *) img);

	beamy=0;
	beamx=0;
	scrptr=&(screen[0]);
	charrambank=Ram+charbank;
	charrombank=&(RomHi[0][charbank & 0x3C00]);
	(charrom) ? cset = charrombank : cset = charrambank;
}


// renders hires text with reverse (128 chars)
inline void MEM::hi_text()
{
	// number of pixels drawn in one cycle
	for (register int i=0;i<CYCSTEP;++i) {
		// get the actual physical character column
		charcol=clrbuf+*(xcol+x+i);
		chr=*(chrbuf+*(xcol+x+i));
		// check for reverse mode and behave accordingly
		if (cposy==*(crclry+crsrpos) && *(crclrx+crsrpos)==xcol[x+i] && crsrblinkon)
			(chr & 0x80) ?
				(*(cset+((chr&0x7F)<<3)+charline) & (hccmask[x+i])) ? *(scrptr+i)= *charcol : *(scrptr+i)= mcol[0]
				:
				(*(cset+(chr<<3)+charline) & (hccmask[x+i])) ? *(scrptr+i)= mcol[0] : *(scrptr+i)= *charcol;
		else
			if ((*charcol)&0x80 && !crsrblinkon)
				(chr & 0x80) ?	*(scrptr+i)= *charcol : *(scrptr+i)=mcol[0];
			else
				if (chr & 0x80)
					(*(cset+((chr&0x7F)<<3)+charline) & (hccmask[x+i])) ? *(scrptr+i)= mcol[0] : *(scrptr+i)= *charcol;
				else
					(*(cset+((chr)<<3)+charline) & (hccmask[x+i])) ? *(scrptr+i)= *charcol : *(scrptr+i)= mcol[0];
	}

}

// renders text without the reverse (all 256 chars)
void MEM::rv_text()
{
	// number of pixels drawn in one cycle
	for (register int i=0;i<CYCSTEP;++i) {
		// get the actual physical character column
		charcol=clrbuf+(*(xcol+x+i));
		chr=*(chrbuf+*(xcol+x+i));
		if (crsrblinkon && cposy==*(crclry+crsrpos) && *(crclrx+crsrpos)==*(xcol+x+i))
			if ((*charcol)&0x80)
				(*(cset+(chr<<3)+charline) & (*(hccmask+x+i))) ? *(scrptr+i)= *charcol : *(scrptr+i)= mcol[0];
			else
				(*(cset+(chr<<3)+charline) & (*(hccmask+x+i))) ? *(scrptr+i)= mcol[0] : *(scrptr+i)= *charcol;
		else
			if (!((*charcol)&0x80) || crsrblinkon)
				(*(cset+(chr<<3)+charline) & (*(hccmask+x+i))) ? *(scrptr+i)= *charcol : *(scrptr+i)= mcol[0];
			else
				*(scrptr+i)=mcol[0];
	}
}


// "illegal" mode: when $FF13 points to an illegal ROM address
//  the current data on the bus is displayed
void MEM::illegalbank()
{
	char tmpbyte = (char) cpuptr->getcins();
	for (register int i=0;i<CYCSTEP;++i) {
		charcol=clrbuf+(*(xcol+x+i));
		chr=*(chrbuf+*(xcol+x+i));
		if (cposy==*(crclry+crsrpos) && *(crclrx+crsrpos)==xcol[x+i] && crsrblinkon)
			(chr & 0x80) ?
				(tmpbyte & (hccmask[x+i])) ? *(scrptr+i)= *charcol : *(scrptr+i)= mcol[0]
				:
				(tmpbyte & (hccmask[x+i])) ? *(scrptr+i)= mcol[0] : *(scrptr+i)= *charcol;
		else
			if ((*charcol)&0x80 && !crsrblinkon)
				(chr & 0x80) ?	*(scrptr+i)= *charcol : *(scrptr+i)=mcol[0];
			else
				if (chr & 0x80)
					(tmpbyte & (hccmask[x+i])) ? *(scrptr+i)= mcol[0] : *(scrptr+i)= *charcol;
				else
					(tmpbyte & (hccmask[x+i])) ? *(scrptr+i)= *charcol : *(scrptr+i)= mcol[0];
	}
}

// when multi and extended color modes are all on the screen is blank
void MEM::mcec()
{
	for (register int i=0;i<CYCSTEP;++i)
		*(scrptr+i)=0;
}

// renders multicolor text
void MEM::mc_text()
{
	// number of pixels drawn in one cycle
	for (register int i=0;i<CYCSTEP;++i) {
		// get the actual physical character column
		charcol=clrbuf+(*(xcol+x+i));
		chr=*(chrbuf+*(xcol+x+i));
		if (!rvsmode) chr&=0x7F;
		if ((*charcol)&0x08) { // if character is multicolored
			mcol[3]=(*charcol)&0xF7;
			*(scrptr+i)= mcol[ (*(cset+(chr<<3)+charline) & (mccmask[x+i])) >> mccmask2[x+i] ];
		} else // this is a normally colored character
			(*(cset+(chr<<3)+charline) & (hccmask[x+i])) ? *(scrptr+i)= *charcol : *(scrptr+i)= mcol[0];
	}
}

// renders extended color text
void MEM::ec_text()
{
	// number of pixels drawn in one cycle
	for (register int i=0;i<CYCSTEP;++i) {
		// get the actual physical character column
		charcol=clrbuf+(*(xcol+x+i));
		chr=*(chrbuf+*(xcol+x+i));
		cindex=((chr&0x3F)<<3)+charline;
		chr=(chr&0xC0)>>6;
		(cset[cindex] & (hccmask[x+i])) ? *(scrptr+i)= *charcol : *(scrptr+i)= mcol[chr];
	}
}

// renders hires bitmap graphics
void MEM::hi_bitmap()
{
	// number of pixels drawn in one cycle
	for (register int i=0;i<CYCSTEP;++i) {
		// get the actual physical character column
		gcindex=colorram+*(xcol+x+i);
		hcol[0]=(*(gcindex+0x0400)&0x0F)+((*gcindex)&0xF0);
		hcol[1]=((*(gcindex+0x0400)&0xF0)>>4)+(((*gcindex)&0x0F)<<4);
		gcindex=grram+((*(xcol+x+i))<<3);
		(*(gcindex) & (*(hccmask+x+i))) ? *(scrptr+i)= hcol[1] : *(scrptr+i)= hcol[0];
	}
}

// renders multicolor bitmap graphics
void MEM::mc_bitmap()
{
	// number of pixels drawn in one cycle
	for (register int i=0;i<CYCSTEP;++i) {
		// get the actual physical character column
		gcindex=colorram+*(xcol+x+i);
		mcol[1]=((*(gcindex+0x0400)&0xF0)>>4)+(((*gcindex)&0x0F)<<4);
		mcol[2]=(*(gcindex+0x0400)&0x0F)+((*gcindex)&0xF0);
		//mcol3 is defined at the beginning of the line
		gcindex=grram+((*(xcol+x+i))<<3);
		*(scrptr+i)= mcol[ (*gcindex & (*(mccmask+x+i))) >> *(mccmask2+x+i)];
	}
}
	/*
Lets see first what happens if the timer high byte write happens on
an even cycle (even cycles are what the CPU can get in slow mode having
$ff1e values like $c4 $c8 etc...)

         Write
         is
         here
Timer 1   $ff $ff $fe $fe $fd $fd $fc $fc $fb $fb
Timer 2   $ff $ff $ff $fe $fe $fd $fd $fc $fc $fb
Timer 3   $ff $fe $fe $fd $fd $fc $fc $fb $fb $fa
           e    o   e   o   e   o   e   o   e   o

Now lets see when the write happened on an odd cycle (cycles the CPU
can get only in fast mode)

        Write
        is
        here
Timer 1  $ff $fe $fe $fd $fd $fc $fc $fb $fb $fa
Timer 2  $ff $ff $fe $fe $fd $fd $fc $fc $fb $fb
Timer 3  $ff $ff $fe $fe $fd $fd $fc $fc $fb $fb
          o   e   o   e   o   e   o   e   o   e

So timer 1 decrementation happens on odd-even boundary, and there is
no delay.
Timer 2 decrementation happens on even-odd boundary, and there is a
delay if the write was on an even cycle.
Timer 3 decrementation happens also on even-odd boundary and there is
no delay.
*/
// main loop of the whole emulation as the TED feeds the CPU with clock cycles
void MEM::ted_process(void)
{
	if ((beamx&0x04)) {	// perform these only in every second cycle
		if (t1on && --timer1==0) { // Timer1 permitted decreased and zero
			timer1=t1start;
			Ram[0xFF0A]&0x08 ? Ram[0xFF09]|=0x88 : Ram[0xFF09]|=0x08; // interrupt
		}
		if (t2on && --timer2==0) {// Timer2 permitted
			timer2=0xFFFF;
			Ram[0xFF0A]&0x10 ? Ram[0xFF09]|=0x90 : Ram[0xFF09]|=0x10; // interrupt
		}
		if (t3on && --timer3==0) {// Timer3 permitted
			timer3=0xFFFF;
			Ram[0xFF0A]&0x40 ? Ram[0xFF09]|=0xC0 : Ram[0xFF09]|=0x40; // interrupt
		}
		if (tapeload)
			tap->read();
		if (tapesave)
			tap->write();
	}

	if (beamx==456) {	// the beam reached a new line?
		beamx=0;

		if (++beamy==260) { // end of the physical screen?
			// frame ready...
			render_ok=true;
			// reset screen pointer ("TV" electron beam)
			scrptr=screen;
			// cursor phase counter in TED register $1F
			crsrblinkon=((crsrphase++)&0x10)!=0;
			render_audio();
		}

		else if (beamy<204 && scrblank) {

			// calculate the left side beginning of the visible screen
			x=scrleft-hshift;
			// get the actual physical character row
			y=beamy-vshift;

			// getting the line within the character
			/*if (++DMAline == 8) {
				DMAline = 0;
				row++;
			}
			charline = ( DMAline + 11 - vshift )&0x07;
			if (vshift<4)
				(charline<DMAline) ? cposy = row + 1 : cposy = row;
			else
				(charline<DMAline) ? cposy = row : cposy = row-1;*/
			cposy=(y-4)>>3;
			charline=(y-4)&0x07;

			switch (charline) {
				case 0:
					// check the place of the colorram
					colorram=colorbank+htab40[cposy];
					// buffering characters in the 0th charline
					memcpy(chrbuf,colorram+0x400,40);
					memcpy(clrbuf,colorram,40);
					DMAptr=DMA[fastmode];
					break;
				case 7:
					// actually color memory DMA fetching should occur here...
					DMAptr=DMA[fastmode];
					break;
				default:
					DMAptr=Normal[fastmode];
					break;
			}
			// set the appropriate renderer for the current line
			switch (scrattr) {
				default :
					scrmode=&MEM::hi_text;
					break;
				case REVERSE :
					scrmode=&MEM::rv_text;
					break;
				case MULTICOLOR|REVERSE :
				case MULTICOLOR :
					scrmode=&MEM::mc_text;
					mcol[1]=Ram[0xFF16];
					mcol[2]=Ram[0xFF17];
					break;
				case EXTCOLOR|REVERSE :
				case EXTCOLOR :
					scrmode=&MEM::ec_text;
					mcol[1]=Ram[0xFF16];
					mcol[2]=Ram[0xFF17];
					mcol[3]=Ram[0xFF18];
					break;
				case GRAPHMODE|REVERSE :
				case GRAPHMODE :
					scrmode=&MEM::hi_bitmap;
					grram=grbank+vtab320[y];
					break;
				case EXTCOLOR|MULTICOLOR :
				case GRAPHMODE|EXTCOLOR :
				case GRAPHMODE|MULTICOLOR|EXTCOLOR :
				case REVERSE|MULTICOLOR|EXTCOLOR :
				case GRAPHMODE|MULTICOLOR|EXTCOLOR|REVERSE :
					scrmode=&MEM::mcec;
					break;
				case GRAPHMODE|MULTICOLOR :
				case GRAPHMODE|MULTICOLOR|REVERSE :
					scrmode=&MEM::mc_bitmap;
					grram=grbank+vtab320[y];
					mcol[3]=Ram[0xFF16];
					break;
			}
			if (scrattr&ILLEGAL==ILLEGAL)
					scrmode = &MEM::illegalbank;
		}
		else {
			if (beamy>311) {
				beamy=0;
				// check for screen blank
				scrblank=(Ram[0xFF06]&0x10)>0;
				/*row = cposy = -1;
				charline = 4;*/
			}
			DMAptr=Border[fastmode];
		}

		// is there raster interrupt?
		if (beamy == irqline )
			Ram[0xFF0A]&0x02 ? Ram[0xFF09]|=0x82 : Ram[0xFF09]|=0x02;
	}

	// check where the electron beam is
	if (scrblank && beamy>(7-fltscr) && beamy<(200+fltscr) && beamx>scrleft && beamx<scrright) {// drawing the visible part of the screen
		(this->*scrmode)(); // call the relevant rendering function
		scrptr+=CYCSTEP;
		beamx+=CYCSTEP;
		x+=CYCSTEP;
	} else {
		// we are on the border area, so use the frame color
		memcpy(scrptr,&framecol,4);
		scrptr+=4;
		beamx+=4;
	}

	switch (*(DMAptr+xbeam[beamx])) {
		case '*' :
			cpuptr->stopcycle();
			break;
		case 'p' :
			cpuptr->process();
			break;
		default : ;
	}
	if (scrptr>endptr)
		scrptr=screen;
};

MEM::~MEM()
{
	delete keys;
	delete tap;
};
//--------------------------------------------------------------

