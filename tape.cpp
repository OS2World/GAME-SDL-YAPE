/*
	YAPE - Yet Another Plus/4 Emulator

	The program emulates the Commodore 264 family of 8 bit microcomputers

	This program is free software, you are welcome to distribute it,
	and/or modify it under certain conditions. For more information,
	read 'Copying'.

	(c) 2000, 2001 Attila Grósz
*/

#include "tape.h"
#include "tedmem.h"
#include "cpu.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


unsigned char tap_header[]={
	'C','1','6','-','T','A','P','E','-','R','A','W',
	0x02, // version -> 1 - whole wave 2 - halfwave
	0x02, // 0 - C64, 1 - VIC20, 2 - C16/+4
	0x00, // Video standard ( 0= PAL, 1 =NTSC, 2 = NTSC2
	0x00, // empty
	// data length (4 byte) file size!!!!
	// data
};

typedef struct _MTAP {
	char string[12];
	unsigned char version;
	unsigned char platform;
	unsigned char video;
	unsigned char reserved;
} MTAPHEADER , * pMTAPHEADER;


FILE *tapfile;
MTAPHEADER tap_header_read;


TAP::TAP()
{
	TapeSoFar=0;
	TapeFileSize=0;
	pHeader=NULL;
	read_decode=&TAP::read_whole_wave;
	write_encode=&TAP::write_half_wave;
}

bool TAP::attach_tap()
{
	char tmpname[14];

	if (tapfile=fopen(tapefilename,"rb")) {
		// load TAP file into buffer
		fseek(tapfile, 0L, SEEK_END);
		TapeFileSize=ftell(tapfile);
		fseek(tapfile, 0L, SEEK_SET);
		// allocate and load file
		pHeader=(unsigned char *) malloc(TapeFileSize);
		fread(pHeader,TapeFileSize,1,tapfile);

		// initialise TAP image
		inwave=false;

		// setting platform
		tap_header_read.version=pHeader[0x0C];
		// allocating space for type...
		strncpy(tmpname,(const char *) pHeader,12);
		// determine the tYpe of tape file attached
		if (!strncmp(tmpname,"C16-TAPE-RAW",12)) {
			TapeSoFar=0x14; // offset to beginning of wave data
			if (tap_header_read.version==1)
				read_decode=&TAP::read_whole_wave;
			else
				read_decode=&TAP::read_half_wave;
		} else  // if no match for MTAP, assume it is a sample
			read_decode=&TAP::read_sample;
		// close the file, it's in the memory now...
		fclose(tapfile);
		mem->wrtDMA(1,mem->readDMA(1)|0x80); // this should be removed someday
		return true;
	}
	else
		return false;
}

bool TAP::create_tap()
{
	int filesize = 0x00;

	if (strlen(tapefilename)<5)
		return false;
	// initialise TAP image
	if (tapfile=fopen(tapefilename,"wb")) {
		// write the header
		fwrite(&tap_header,sizeof(tap_header),1,(FILE *)tapfile);
		fwrite(&filesize,4,1,(FILE *)tapfile);
		return true;
	}
	else
		return false;
}

bool TAP::detach_tap()
{
	if (mem->tapesave && tapfile!=NULL && TapeSoFar>0) {
		fseek(tapfile, 0x10L, SEEK_SET);
		fwrite(&TapeSoFar,sizeof(TapeSoFar),1,tapfile);
		fseek(tapfile, 0L, SEEK_END);
	}
	if (pHeader!=NULL) { // just to be sure....
		free(pHeader);
		pHeader=NULL;
	}
	if (tapfile!=NULL) {
		fclose(tapfile);
		tapfile=NULL;
	}
	mem->tapeload=false;
	mem->tapesave=false;
	TapeSoFar=0;
	return false;
}

void TAP::rewind()
{
	mem->tapeload=false;
	mem->tapesave=false;
	//sinus=false;
	inwave=false;
	TapeSoFar=0x14;
}

void TAP::read_whole_wave()
{
	mem->Ram[0xFD10]&=~0x04;
	// if tape motor is on and we have a TAP buffer
	if ((mem->Ram[1]&8)==0 && pHeader!=NULL) {
		// have we started the tape already?
		if (inwave) {
			// first or second half of sinus wave?
			if (tapedelay==origlength)
				mem->Ram[1]&=~0x10;
			if (tapedelay==halflength)
				mem->Ram[1]|=0x10;
			//tapedelay--;
			// have finished our sinus wave already?
			if (--tapedelay==0) {
				// fetch next byte from buffer
				origlength=tapedelay=((pHeader[TapeSoFar])<<3);
				// did we get a special byte?
				if (tapedelay==0) {
					tapedelay=pHeader[TapeSoFar+1];
					tapedelay+=pHeader[TapeSoFar+2]<<8;
					tapedelay+=pHeader[TapeSoFar+3]<<16;
					origlength=tapedelay;//<<=1;
					TapeSoFar+=3;
				}
				halflength=(tapedelay)>>1;
				if (++TapeSoFar>=TapeFileSize)
					mem->tapeload=false; // stop the tape
			}
		} else {
			inwave=true;
			origlength=tapedelay=(pHeader[TapeSoFar])<<3;
			if (tapedelay==0) {
				tapedelay=pHeader[TapeSoFar+1];
				tapedelay+=pHeader[TapeSoFar+2]<<8;
				tapedelay+=pHeader[TapeSoFar+3]<<16;
				origlength=tapedelay;//<<=1;
				TapeSoFar+=3;
			}
			halflength=tapedelay>>1;
			TapeSoFar++;
		}
	}
}

void TAP::read_half_wave()
{
	mem->Ram[0xFD10]&=~0x04;
	// if tape motor is on and we have a TAP buffer
	if ((mem->Ram[1]&8)==0 && pHeader!=NULL) {
		// have we started the tape already?
		if (inwave) {
			if (TapeSoFar>=TapeFileSize)
				mem->tapeload=false; // stop tape!
			// first or second half of sinus wave?
			if (!sinus && tapedelay==origlength)
				mem->Ram[1]&=~0x10;
			if (sinus && tapedelay==origlength)
				mem->Ram[1]|=0x10;
			//tapedelay--;
			// have finished our half sinus wave already?
			if (--tapedelay<0) {
				sinus=!sinus;
				TapeSoFar++;
				// fetch next byte from buffer
				origlength=tapedelay=((pHeader[TapeSoFar])<<3);
				// did we get a special byte?
				if (tapedelay==0) {
					tapedelay=pHeader[TapeSoFar+1];
					tapedelay+=pHeader[TapeSoFar+2]<<8;
					tapedelay+=pHeader[TapeSoFar+3]<<16;
					origlength=tapedelay;//=(tapedelay<<1);
					TapeSoFar+=3;
				}
			}
		} else {
			inwave=true;
			sinus=false;
			origlength=tapedelay=(pHeader[TapeSoFar])<<3;
			if (tapedelay==0) {
				tapedelay=pHeader[TapeSoFar+1];
				tapedelay+=pHeader[TapeSoFar+2]<<8;
				tapedelay+=pHeader[TapeSoFar+3]<<16;
				origlength=tapedelay;//=(tapedelay<<1);
				TapeSoFar+=3;
			}
		}
	}
}

void TAP::read_sample()
{
	mem->Ram[0xFD10]&=~0x04;
	// if tape motor is on and we have a TAP buffer
	if ((mem->Ram[1]&8)==0 && pHeader!=NULL) {
		// have we started the tape already?
		if (inwave) {
			// timing decrease
			tapedelay--;
			// have finished it?
			if (tapedelay<=0) {
				TapeSoFar++;
				if (TapeSoFar>=TapeFileSize)
					mem->tapeload=false; // stop tape!
				// fetch next byte from buffer
				origlength=pHeader[TapeSoFar];
				if (origlength>=0xC0)
					mem->Ram[1]&=~0x10;
				if (origlength<=0x3F)
					mem->Ram[1]|=0x10;
				// (114 cycles per line * 312 lines * 50 frames/sec) / (44100) / 2
				tapedelay=20;
			}
		} else {
			inwave=true;
			origlength=pHeader[TapeSoFar];
			tapedelay=20;
		}
	}
}

void TAP::write_half_wave()
{
	if ((mem->Ram[1]&8)==0 && tapfile!=NULL) {
		if (inwave) {
			tapedelay++;
			// if CST_WRITE line high, 2nd half of sinus wave starts
			if (!sinus && mem->Ram[1]&0x02) {
				sinus=true;
				if ((tapedelay>>3)<256) {
					buf=tapedelay>>3;
					fwrite(&buf,1,1,tapfile);
					TapeSoFar++;
					tapedelay=0;
				} else
					do {
						// signal byte first
						buf=0;
						fwrite(&buf,1,1,tapfile);
						// only half of the cycles needed is written to the file
						buf=(tapedelay)&0xFF;
						fwrite(&buf,1,1,tapfile);
						// this should be 8 but we shift by 1 anyway...
						buf=(tapedelay>>8)&0xFF;
						fwrite(&buf,1,1,tapfile);
						// this should be 16 but we shift by 1 anyway...
						buf=(tapedelay>>16)&0xFF;
						fwrite(&buf,1,1,tapfile);
						// advance in tape file
						TapeSoFar+=4;
						// divide by 2^24
						tapedelay>>=24;
					} while (tapedelay); // repeat this until we've finished our long pulse...
			} else
			if (sinus && !(mem->Ram[1]&0x02)) {
				sinus=false;
				if ((tapedelay>>3)<256) {
					buf=tapedelay>>3;
					fwrite(&buf,1,1,tapfile);
					TapeSoFar++;
					tapedelay=0;
				} else
					do {
						// signal byte first
						buf=0;
						fwrite(&buf,1,1,tapfile);
						// only half of the cycles needed is written to the file
						buf=(tapedelay)&0xFF;
						fwrite(&buf,1,1,tapfile);
						// this should be 8 but we shift by 1 anyway...
						buf=(tapedelay>>8)&0xFF;
						fwrite(&buf,1,1,tapfile);
						// this should be 16 but we shift by 1 anyway...
						buf=(tapedelay>>16)&0xFF;
						fwrite(&buf,1,1,tapfile);
						// advance in tape file
						TapeSoFar+=4;
						// divide by 2^24
						tapedelay>>=24;
					} while (tapedelay); // repeat this until we've finished our long pulse...
					//tapedelay=0;
			}
		} else {
			sinus=false;
			inwave=true;
			tapedelay=0;
		}
	}
}

void TAP::write_whole_wave()
{
	if ((mem->Ram[1]&8)==0 && tapfile!=NULL) {
		if (inwave) {
			tapedelay++;
			// if CST_WRITE line high, 2nd half of sinus wave starts
			if (mem->Ram[1]&0x02)
				sinus=true;
			else
				if (sinus) {
					if ((tapedelay>>3)<256) {
						buf=tapedelay>>3;
						fwrite(&buf,1,1,tapfile);
						TapeSoFar++;
					} else
						do {
						// signal byte first
						buf=0;
						fwrite(&buf,1,1,tapfile);
						// only half of the cycles needed is written to the file
						buf=(tapedelay)&0xFF;
						fwrite(&buf,1,1,tapfile);
						// this should be 8 but we shift by 1 anyway...
						buf=(tapedelay>>8)&0xFF;
						fwrite(&buf,1,1,tapfile);
						// this should be 16 but we shift by 1 anyway...
						buf=(tapedelay>>16)&0xFF;
						fwrite(&buf,1,1,tapfile);
						// advance in tape file
						TapeSoFar+=4;
						// divide by 2^24
						tapedelay>>=24;
					} while (tapedelay); // repeat this until we've finished our long pulse...
					sinus=false;
					tapedelay=0;
				}
		} else {
			sinus=false;
			inwave=true;
			tapedelay=0;
		}
	}
}

void TAP::changewave(bool wholewave)
{
	if (!wholewave) {
		write_encode=&TAP::write_half_wave;
		tap_header[12]=2;
	} else {
		write_encode=&TAP::write_whole_wave;
		tap_header[12]=1;
	}
}

