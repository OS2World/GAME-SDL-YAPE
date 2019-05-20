/*
	YAPE - Yet Another Plus/4 Emulator

	The program emulates the Commodore 264 family of 8 bit microcomputers

	This program is free software, you are welcome to distribute it,
	and/or modify it under certain conditions. For more information,
	read 'Copying'.

	(c) 2000, 2001 Attila Grósz
*/
#include <stdio.h>
#include <stdlib.h>
#include "cpu.h"
#include "tedmem.h"

#define setZN(VALUE) 	(VALUE)		 ? ST&=0xFD : ST|=0x02; 	(VALUE)&0x80 ? ST|=0x80: ST&=0x7F;
#define push(VALUE) 	mem->Ram[0x0100|(SP--)]=VALUE
#define pull() 	mem->Ram[0x0100|SP]


CPU::CPU()
{

	mem = new MEM;
	mem->cpuptr=this;

	startIRQ = false;
	IRQcount = 1;
	bp_address = 0;
	bp_active = false;
	bp_reached = false;
	reset();
}

inline void CPU::ADC(unsigned char value)
{
	static unsigned int bin_ac;

	bin_ac=AC + value + (ST&0x01);

	if (ST&0x08) {

		static unsigned char AL, AH;

		AL=(AC&0x0F) + (value & 0x0F) + (ST&0x01);
		AH=(AC >> 4) + (value >> 4 ) ;
		// fix lower nybble
		if (AL>9) {
			AL+=6;
			AH++;
		}
		// zero bit depends on the normal ADC...
		(bin_ac)&0xFF ? ST&=0xFD : ST|=0x02;
		// negative flag also...
		( AH & 0x08 ) ? ST|=0x80 : ST&=0x7F;
		// V flag
		((((AH << 4) ^ AC) & 0x80) && !((AC ^ value) & 0x80)) ? ST|=0x40 : ST&=0xBF;
		// fix upper nybble
		if (AH>9)
			AH+=6;
		// set the Carry if the decimal add has an overflow
		(AH > 0x0f) ? ST|=0x01 : ST&=0xFE;
		// calculate new AC value
		AC = (AH<<4)|(AL & 0x0F);
	} else {

		(bin_ac>=0x100) ? ST|=0x01 : ST&=0xFE;
		(!((AC ^ value) & 0x80) && ((AC ^ bin_ac) & 0x80) ) ? ST|=0x40 : ST&=0xBF;
		AC=(unsigned char) bin_ac;
		setZN(AC);
	}
}

inline void CPU::SBC(unsigned char value)
{
	static unsigned int dec_ac, bin_ac;

	bin_ac = (AC - value - (1-(ST&0x01)));

	if (ST&0x08) { // if in decimal mode

		dec_ac = (AC & 0x0F) - (value & 0x0F) - (1-(ST&0x01));
		// Calculate the upper nybble.
		// fix upper nybble
		if (dec_ac&0x10)
			dec_ac = ((dec_ac-6)&0x0F) | ((AC&0xF0) - (value&0xF0) - 0x10);
		else
			dec_ac = (dec_ac&0x0F) | ((AC&0xF0) - (value&0xF0));

		if (dec_ac&0x100)
			dec_ac-= 0x60;

		// all flags depend on the normal ADC...
		(bin_ac<0x100) ? ST|=0x01 : ST&=0xFE ; // carry flag
		setZN( bin_ac & 0xFF );
		((AC^bin_ac)&0x80 && (AC^value)&0x80 ) ? ST|=0x40 : ST&=0xBF; // V flag

		AC=(unsigned char) dec_ac;

	} else {

		(bin_ac<0x100) ? ST|=0x01 : ST&=0xFE;
		(((AC ^ value) & 0x80) && ((AC ^ bin_ac) & 0x80) ) ? ST|=0x40 : ST&=0xBF;
		AC=(unsigned char) bin_ac;
		setZN(AC);
	}
}

bool CPU::saveshot()
{
	FILE *img;

	if ((img = fopen("SNAPSHOT.FRE", "wbt"))== NULL) {
   	   return false;
   	} else {
   		fwrite(&currins,sizeof(currins),1,img);
		fwrite(&nextins,sizeof(nextins),1,img);
		fwrite(&ptr,sizeof(ptr),1,img);
   		fwrite(&PC,sizeof(PC),1,img);
		fwrite(&cycle,sizeof(cycle),1,img);
   		fwrite(&SP,sizeof(SP),1,img);
  		fwrite(&ST,sizeof(ST),1,img);
   		fwrite(&AC,sizeof(AC),1,img);
   		fwrite(&X,sizeof(X),1,img);
   		fwrite(&Y,sizeof(Y),1,img);
		mem->dump(img);
   		fclose(img);
		return true;
	}
}

bool CPU::loadshot()
{
	FILE *img;

	if ((img = fopen("SNAPSHOT.FRE", "rb"))== NULL) {
   	   return false;
   	} else {
   		fread(&currins,sizeof(currins),1,img);
		fread(&nextins,sizeof(nextins),1,img);
		fread(&ptr,sizeof(ptr),1,img);
   		fread(&PC,sizeof(PC),1,img);
		fread(&cycle,sizeof(cycle),1,img);
   		fread(&SP,sizeof(SP),1,img);
  		fread(&ST,sizeof(ST),1,img);
   		fread(&AC,sizeof(AC),1,img);
   		fread(&X,sizeof(X),1,img);
   		fread(&Y,sizeof(Y),1,img);
		mem->memin(img);
   		fclose(img);
		return true;
	}
}

void CPU::reset()
{
	PC=AC=X=Y=0;
	SP=0xFF;
	ST=0x24;
	mem->loadroms();
	mem->RAMenable=true;
	for (int i=0;i<RAMSIZE;++i)
		if (i&0x0080)
			if (i&0x02)
				mem->wrtDMA(i,0xFF);
			else
				mem->wrtDMA(i,0);
		else
			if (i&0x02)
				mem->wrtDMA(i,0x00);
			else
				mem->wrtDMA(i,0xFF);
	mem->RAMenable=false;
	mem->wrt(0xFDD0,0);
	setPC(mem->read(0xFFFC)|(mem->read(0xFFFD)<<8));
}

void CPU::softreset()
{
	PC=AC=X=Y=0;
	SP=0xFF;
	ST=0x24;
	setPC(mem->read(0xFFFC)|(mem->read(0xFFFD)<<8));
}

void CPU::debugreset()
{
	ST=0x24;
	mem->loadroms();
	mem->RAMenable=false;
	setPC(mem->read(0xFFFC)|(mem->read(0xFFFD)<<8));
}

void CPU::setPC(unsigned int addr)
{
	PC=addr;
	cycle=1;
}

void CPU::process()
{
	if (!startIRQ && mem->Ram[0xFF09]&0x80 && !(ST&0x04)) {// is IRQ occured and allowed?
		startIRQ = true;
		IRQcount = 8;
	}
	if (startIRQ)
		(--IRQcount);
	if (cycle==1) {		// in the first cycle the CPU fetches the opcode
		if (IRQcount<1 && startIRQ) {
		//if (!(ST&0x04) && mem->Ram[0xFF09]&0x80) {
			push(PC>>8);
			push(PC&0xFF);
			push(ST);

			PC=mem->read(0xFFFE)|(mem->read(0xFFFF)<<8);
			ST|=0x04;
			startIRQ = false;
		}
		currins=mem->read(PC);				// fetch opcode
		nextins=mem->read(PC+1);			// prefetch next opcode/operand
		ptr=nextins|(mem->read(PC+2)<<8);	// prefetch next word (for speed)
		cycle++;							// increment the CPU cycle counter
		if ( bp_active && PC == bp_address ) {
			bp_reached = true;
			PC = (PC - 1 ) & 0xFFFF;
			return;
		}
		//++PC;
		PC=(++PC)&0xFFFF;
	}
	else

	switch (currins){
		case 0xea : // NOP
			switch(cycle++) {
				case 2:	cycle=1;
						break;
			};
			break;

		case 0x18 : // CLC
			switch(cycle++) {
				case 2: ST&=0xFE;
						cycle=1;
						break;
			};
			break;

		case 0x38 : // SEC
			switch(cycle++) {
				case 2: ST|=0x01;
						cycle=1;
						break;
			};
			break;

		case 0x58 : // CLI
			switch(cycle++) {
				case 2: ST&=0xFB;
						cycle=1;
						break;
			};
			break;

		case 0x78 : // SEI
			switch(cycle++) {
				case 2: ST|=0x04;
						cycle=1;
						startIRQ = false;
						IRQcount = 1;
						break;
			};
			break;

		case 0xb8 : // CLV
			switch(cycle++) {
				case 2: ST&=0xBF;
						cycle=1;
						break;
			};
			break;

		case 0xD8 : // CLD
			switch(cycle++) {
				case 2: ST&=0xF7;
						cycle=1;
						break;
			};
			break;

		case 0xF8 : // SED
			switch(cycle++) {
				case 2: ST|=0x08;
						cycle=1;
						break;
			};
			break;

		// branch functions (conditional jumps )

		case 0x10 : // BPL
			switch(cycle++) {
				case 2: PC++;
						if ((ST&0x80))
							cycle=1;
						break;
				case 3: if ((PC&0xFF)+((signed char) nextins)<0x100) {
							PC+=(signed char) nextins;
							cycle=1;
						};
						break;
				case 4: PC+=(signed char) nextins;
						cycle=1;
						break;
			};
			break;

		case 0x30 : // BMI
			switch(cycle++) {
				case 2: PC++;
						if (!(ST&0x80))
							cycle=1;
						break;
				case 3: if ((PC&0xFF)+((signed char) nextins)<0x100) {
							PC+=(signed char) nextins;
							cycle=1;
						};
						break;
				case 4: PC+=(signed char) nextins;
						cycle=1;
						break;
			};
			break;

		case 0x50 : // BVC
			switch(cycle++) {
				case 2: PC++;
						if (ST&0x40)
							cycle=1;
						break;
				case 3: if ((PC&0xFF)+((signed char) nextins)<0x100) {
							PC+=(signed char) nextins;
							cycle=1;
						};
						break;
				case 4: PC+=(signed char) nextins;
						cycle=1;
						break;
			};
			break;

		case 0x70 : // BVS
			switch(cycle++) {
				case 2: PC++;
						if (!(ST&0x40))
							cycle=1;
						break;
				case 3: if ((PC&0xFF)+((signed char) nextins)<0x100) {
							PC+=(signed char) nextins;
							cycle=1;
						};
						break;
				case 4: PC+=(signed char) nextins;
						cycle=1;
						break;
			};
			break;

		case 0x90 : // BCC
			switch(cycle++) {
				case 2: PC++;
						if (ST&0x01)
							cycle=1;
						break;
				case 3: if ((PC&0xFF)+((signed char) nextins)<0x100) {
							PC+=(signed char) nextins;
							cycle=1;
						};
						break;
				case 4: PC+=(signed char) nextins;
						cycle=1;
						break;
			};
			break;

		case 0xB0 : // BCS
			switch(cycle++) {
				case 2: PC++;
						if (!(ST&0x01))
							cycle=1;
						break;
				case 3: if ((PC&0xFF)+((signed char) nextins)<0x100) {
							PC+=(signed char) nextins;
							cycle=1;
						};
						break;
				case 4: PC+=(signed char) nextins;
						cycle=1;
						break;
			};
			break;

		case 0xD0 : // BNE
			switch(cycle++) {
				case 2: PC++;
						if (ST&0x02)
							cycle=1;
						break;
				case 3: if ((PC&0xFF)+((signed char) nextins)<0x100) {
							PC+=(signed char) nextins;
							cycle=1;
						};
						break;
				case 4: PC+=(signed char) nextins;
						cycle=1;
						break;
			};
			break;

		case 0xF0 : // BEQ
			switch(cycle++) {
				case 2: PC++;
						if (!(ST&0x02))
							cycle=1;
						break;
				case 3: if ((PC&0xFF)+((signed char) nextins)<0x100) {
							PC+=(signed char) nextins;
							cycle=1;
						};
						break;
				case 4: PC+=(signed char) nextins;
						cycle=1;
						break;
			};
			break;

		// manipulating index registers

		case 0x88 : // DEY
			switch(cycle++) {
				case 2: --Y;
						setZN(Y);
						cycle=1;
						break;
			};
			break;

		case 0xC8 : // INY
			switch(cycle++) {
				case 2: ++Y;
						setZN(Y);
						cycle=1;
						break;
			};
			break;

		case 0xCA : // DEX
			switch(cycle++) {
				case 2: --X;
						setZN(X);
						cycle=1;
						break;
			};
			break;

		case 0xE8 : // INX
			switch(cycle++) {
				case 2: ++X;
						setZN(X);
						cycle=1;
						break;
			};
			break;

		case 0x00 : // BRK
			switch (cycle++) {
				case 2: ST|=0x10;
						PC++;
						break;
				case 3: push(PC>>8);
						break;
				case 4: push(PC&0xFF);
						break;
				case 5: push(ST);
						ST|=0x04;
						break;
				case 6: PC=mem->read(0xFFFE);
						break;
				case 7: PC|=mem->read(0xFFFF)<<8;
						cycle=1;
						break;
			};
			break;

		case 0x40 : // RTI
			switch (cycle++) {
				case 2: break;
				case 3: break;
				case 4: SP++;
						// the B flag is always cleared?
						ST=pull()&0xEF;
						break;
				case 5: SP++;
						PC=pull();
						break;
				case 6: SP++;
						PC|=pull()<<8;
						//ST&=0xFB;
						cycle=1;
						break;
			};
			break;

		case 0x60 : // RTS
			switch (cycle++) {
				case 2: break;
				case 3: SP++;
						PC=pull();
						break;
				case 4: SP++;
						PC|=pull()<<8;
						break;
				case 5: PC++;
						break;
				case 6: cycle=1;
						break;
			};
			break;

		// stack operations

		case 0x08 : // PHP
			switch (cycle++) {
				case 2: break;
				case 3: // the PHP always pushes the status with the B flag set... but
						// we dont use this....
						push(ST);
						cycle=1;
						break;
			};
			break;

		case 0x28 : // PLP
			switch (cycle++) {
				case 2: break;
				case 3: SP++;
						break;
				case 4: ST=pull()&0xEF;
						cycle=1;
						break;
			};
			break;

		case 0x48 : // PHA
			switch (cycle++) {
				case 2:	break;
				case 3: push(AC);
						cycle=1;
						break;
			};
			break;

		case 0x68 : // PLA
			switch (cycle++) {
				case 2: break;
				case 3: SP++;
						break;
				case 4: AC=pull();
						setZN(AC);
						cycle=1;
						break;
			};
			break;

		// inter-register operations

		case 0x8A : // TXA
			switch (cycle++) {
				case 2: AC=X;
						setZN(AC);
						cycle=1;
						break;
			};
			break;

		case 0xAA : // TAX
			switch (cycle++) {
				case 2: X=AC;
						setZN(X);
						cycle=1;
						break;
			};
			break;

		case 0x98 : // TYA
			switch (cycle++) {
				case 2: AC=Y;
						setZN(AC);
						cycle=1;
						break;
			};
			break;

		case 0xA8 : // TAY
			switch (cycle++) {
				case 2: Y=AC;
						setZN(Y);
						cycle=1;
						break;
			};
			break;

		case 0x9A : // TXS
			switch (cycle++) {
				case 2: SP=X;
						cycle=1;
						break;
			};
			break;

		case 0xBA : // TSX
			switch (cycle++) {
				case 2: X=SP;
						setZN(X);
						cycle=1;
						break;
			};
			break;

		// subroutine & unconditional jump

		case 0x20 : // JSR
			switch (cycle++) {
				case 2: PC++;
						break;
				case 3: // unknown
					break;
				case 4: push(PC>>8);
					break;
				case 5: push(PC&0xFF);
					break;
				case 6: PC=ptr;
						cycle=1;
						break;
			};
			break;

		case 0x4C : // JMP $0000
			switch (cycle++) {
				case 2: //PC++;
					break;
				case 3: PC=ptr;
						cycle=1;
						break;
			};
			break;

		case 0x6C : // JMP ($0000)
			switch (cycle++) {
				case 2: //PC++;
						break;
				case 3: break;
				case 4: break;
				case 5: PC=mem->read(ptr)|(mem->read(ptr+1)<<8);
						cycle=1;
						break;
			};
			break;

 		// Accumulator operations with immediate addressing

		case 0x09 : // ORA #$00
			switch (cycle++) {
				case 2: PC++;
						AC|=nextins;
						setZN(AC);
						cycle=1;
						break;
			};
			break;

		case 0x29 : // AND #$00
			switch (cycle++) {
				case 2: PC++;
						AC&=nextins;
						setZN(AC);
						cycle=1;
						break;
			};
			break;

		case 0x49 : // EOR #$00
			switch (cycle++) {
				case 2: PC++;
						AC^=nextins;
						setZN(AC);
						cycle=1;
						break;
			};
			break;

		case 0x69 : // ADC #$00
			switch (cycle++) {
				case 2: PC++;
						ADC(nextins);
						cycle=1;
						break;
			};
			break;

		case 0xC9 : // CMP #$00
			switch (cycle++) {
				case 2: PC++;
						(AC<nextins) ? ST&=0xFE : ST|=0x01 ;
						nextins=AC-nextins;
						setZN(nextins);
						cycle=1;
						break;
			};
			break;

		case 0xE9 : // SBC #$00
			switch (cycle++) {
				case 2: PC++;
						SBC(nextins);
						cycle=1;
						break;
			};
			break;

		// rotations

		case 0x0A : // ASL
			switch (cycle++) {
				case 2: AC&0x80 ? ST|=0x01 : ST&=0xFE; // the Carry flag
						AC<<=1;
						setZN(AC);
						cycle=1;
						break;
			};
			break;

		case 0x2A : // ROL
			switch (cycle++) {
				case 2: nextins=(AC<<1)|(ST&0x01);
						AC&0x80 ? ST|=0x01 : ST&=0xFE; // the Carry flag
						AC=nextins;
						setZN(AC);
						cycle=1;
						break;
			};
			break;

		case 0x4A : // LSR
			switch (cycle++) {
				case 2: AC&0x01 ? ST|=0x01 : ST&=0xFE; // the Carry flag
						AC=AC>>1;
						setZN(AC);
						cycle=1;
						break;
			};
			break;

		case 0x6A : // ROR
			switch (cycle++) {
				case 2: nextins=(AC>>1)|((ST&0x01)<<7);
						AC&0x01 ? ST|=0x01 : ST&=0xFE; // the Carry flag
						AC=nextins;
						setZN(AC);
						cycle=1;
						break;
			};
			break;

		// loads

		case 0xA0 : // LDY
			switch (cycle++) {
				case 2: PC++;
						Y=nextins;
						setZN(Y);
						cycle=1;
						break;
			};
			break;

		case 0xA2 : // LDX
			switch (cycle++) {
				case 2: PC++;
						X=nextins;
						setZN(X);
						cycle=1;
						break;
			};
			break;

		case 0xA9 : // LDA
			switch (cycle++) {
				case 2: PC++;
						AC=nextins;
						setZN(AC);
						cycle=1;
						break;
			};
			break;

		// comparisons with immediate addressing

		case 0xC0 : // CPY
			switch (cycle++) {
				case 2: PC++;
						(Y>=nextins) ? ST|=0x01 : ST&=0xFE;
						nextins=Y-nextins;
						setZN(nextins);
						cycle=1;
						break;
			};
			break;

		case 0xE0 : // CPX
			switch (cycle++) {
				case 2: PC++;
						(X>=nextins) ? ST|=0x01 : ST&=0xFE;
						nextins=X-nextins;
						setZN(nextins);
						cycle=1;
						break;
			};
			break;

		// BIT tests with accumulator

		case 0x24 : // BIT $00
			switch (cycle++) {
				case 2: PC++;
						nextins=mem->read(nextins);
					break;
				case 3: (nextins&0x80) ? ST|=0x80 : ST&=0x7F;
						(nextins&0x40) ? ST|=0x40 : ST&=0xBF;
						(AC&nextins)  ? ST&=0xFD : ST|=0x02;
						cycle=1;
						break;
			};
			break;

		case 0x2C : // BIT $0000
			switch (cycle++) {
				case 2: PC++;
					break;
				case 3: nextins=mem->read(ptr);
						PC++;
					break;
				case 4: (nextins&0x80) ? ST|=0x80 : ST&=0x7F;
						(nextins&0x40) ? ST|=0x40 : ST&=0xBF;
						(AC&nextins)  ? ST&=0xFD : ST|=0x02;
						cycle=1;
						break;
			};
			break;

		// Read instructions with absolute addressing

		case 0x0D : // ORA $0000
			switch (cycle++) {
				case 2: PC++;
						break;
				case 3: PC++;
						break;
				case 4: AC|=mem->read(ptr);
						setZN(AC);
						cycle=1;
						break;
			};
			break;

		case 0x2D : // AND $0000
			switch (cycle++) {
				case 2: PC++;
						break;
				case 3: PC++;
						break;
				case 4: AC&=mem->read(ptr);
						setZN(AC);
						cycle=1;
						break;
			};
			break;

		case 0x4D : // EOR $0000
			switch (cycle++) {
				case 2: PC++;
						break;
				case 3: PC++;
						break;
				case 4: AC^=mem->read(ptr);
						setZN(AC);
						cycle=1;
						break;
			};
			break;

		case 0x6D : // ADC $0000
			switch (cycle++) {
				case 2: PC++;
						break;
				case 3: PC++;
						break;
				case 4: ADC(mem->read(ptr));
						cycle=1;
						break;
			};
			break;

		case 0x99 : // STA $0000,Y
			switch (cycle++) {
				case 2: PC++;
						break;
				case 3: PC++;
						break;
				case 4: ptr+=Y;
						break;
				case 5: mem->wrt(ptr,AC);
						cycle=1;
						break;
			};
			break;

		case 0xAC : // LDY $0000
			switch (cycle++) {
				case 2: PC++;
						break;
				case 3: PC++;
						break;
				case 4: Y=mem->read(ptr);
						setZN(Y);
						cycle=1;
						break;
			};
			break;

		case 0xCC : // CPY $0000
			switch (cycle++) {
				case 2: PC++;
						break;
				case 3: PC++;
						nextins=mem->read(ptr);
						break;
				case 4: (Y>=nextins) ? ST|=0x01 : ST&=0xFE;
						nextins=Y-nextins;
						setZN(nextins);
						cycle=1;
						break;
			};
			break;

		case 0xEC : // CPX $0000
			switch (cycle++) {
				case 2: PC++;
						break;
				case 3: PC++;
						nextins=mem->read(ptr);
						break;
				case 4: (X>=nextins) ? ST|=0x01 : ST&=0xFE;
						nextins=X-nextins;
						setZN(nextins);
						cycle=1;
						break;
			};
			break;

		case 0xAD : // LDA $0000
			switch (cycle++) {
				case 2: PC++;
						break;
				case 3: PC++;
						break;
				case 4: AC=mem->read(ptr);
						setZN(AC);
						cycle=1;
						break;
			};
			break;

		case 0xCD : // CMP $0000
			switch (cycle++) {
				case 2: PC++;
						break;
				case 3: nextins=mem->read(ptr);
						PC++;
						break;
				case 4: (AC>=nextins) ? ST|=0x01 : ST&=0xFE;
						nextins=AC-nextins;
						setZN(nextins);
						cycle=1;
						break;
			};
			break;

		case 0xED : // SBC $0000
			switch (cycle++) {
				case 2: PC++;
						break;
				case 3: PC++;
						break;
				case 4: SBC(mem->read(ptr));
						cycle=1;
						break;
			};
			break;

		case 0x0E : // ASL $0000
			switch (cycle++) {
				case 2: PC++;
						break;
				case 3: PC++;
						break;
				case 4: nextins=mem->read(ptr);
						break;
				case 5:	mem->wrt(ptr,nextins);
						(nextins&0x80) ? ST|=0x01 : ST&=0xFE; // the Carry flag
						nextins<<=1;
						break;
				case 6:	mem->wrt(ptr,nextins);
						setZN(nextins);
						cycle=1;
						break;
			};
			break;

		case 0x1E : // ASL $0000,X
			switch (cycle++) {
				case 2: PC++;
						break;
				case 3: PC++;
						ptr+=X;
						break;
				case 4: nextins=mem->read(ptr);
						break;
				case 5:	break;
				case 6:	mem->wrt(ptr,nextins);
						(nextins&0x80) ? ST|=0x01 : ST&=0xFE; // the Carry flag
						nextins<<=1;
						break;
				case 7: mem->wrt(ptr,nextins);
						setZN(nextins);
						cycle=1;
						break;
			};
			break;

		case 0x2E : // ROL $0000
			switch (cycle++) {
				case 2: PC++;
						break;
				case 3: PC++;
						break;
				case 4: farins=mem->read(ptr);
						nextins=(farins<<1)|(ST&0x01);
						break;
				case 5:	mem->wrt(ptr,farins);
						(farins&0x80) ? ST|=0x01 : ST&=0xFE; // the Carry flag
						break;
				case 6:	mem->wrt(ptr,nextins);
						setZN(nextins);
						cycle=1;
						break;
			};
			break;

		case 0x3E : // ROL $0000,X
			switch (cycle++) {
				case 2: PC++;
						break;
				case 3: PC++;
						ptr+=X;
						break;
				case 4: farins=mem->read(ptr);
						break;
				case 5:	nextins=(farins<<1)|(ST&0x01);
						(farins&0x80) ? ST|=0x01 : ST&=0xFE; // the Carry flag
						break;
				case 6:	mem->wrt(ptr,farins);
						break;
				case 7: mem->wrt(ptr,nextins);
						setZN(nextins);
						cycle=1;
						break;
			};
			break;

		case 0x4E : // LSR $0000
			switch (cycle++) {
				case 2: PC++;
						break;
				case 3: PC++;
						break;
				case 4: nextins=mem->read(ptr);
						(nextins&0x01) ? ST|=0x01 : ST&=0xFE;; // the Carry flag
						break;
				case 5:	mem->wrt(ptr,nextins);
						nextins=nextins>>1;
						break;
				case 6:	mem->wrt(ptr,nextins);
						setZN(nextins);
						cycle=1;
						break;
			};
			break;

		case 0x5E : // LSR $0000,X
			switch (cycle++) {
				case 2: PC++;
						break;
				case 3: PC++;
						ptr+=X;
						break;
				case 4: nextins=mem->read(ptr);
						break;
				case 5:	(nextins&0x01) ? ST|=0x01 : ST&=0xFE; // the Carry flag
						break;
				case 6:	mem->wrt(ptr,nextins);
						nextins>>=1;
						break;
				case 7: mem->wrt(ptr,nextins);
						setZN(nextins);
						cycle=1;
						break;
			};
			break;

		case 0x6E : // ROR $0000
			switch (cycle++) {
				case 2: PC++;
						break;
				case 3: PC++;
						break;
				case 4: farins=mem->read(ptr);
						break;
				case 5: mem->wrt(ptr,farins);
						nextins=(farins>>1)|((ST&0x01)<<7);
						(farins&0x01) ? ST|=0x01 : ST&=0xFE; // the Carry flag
						break;
				case 6: mem->wrt(ptr,nextins);
						setZN(nextins);
						cycle=1;
						break;
			};
			break;

		case 0x7E : // ROR $0000,X
			switch (cycle++) {
				case 2: PC++;
						break;
				case 3: PC++;
						ptr+=X;
						break;
				case 4: farins=mem->read(ptr);
						break;
				case 5: nextins=(farins>>1)|((ST&0x01)<<7);
						(farins&0x01) ? ST|=0x01 : ST&=0xFE; // the Carry flag
						break;
				case 6: mem->wrt(ptr,farins);
						break;
				case 7: mem->wrt(ptr,nextins);
						setZN(nextins);
						cycle=1;
						break;
			};
			break;

		case 0xAE : // LDX $0000
			switch (cycle++) {
				case 2: PC++;
						break;
				case 3: PC++;
						break;
				case 4: X=mem->read(ptr);
						setZN(X);
						cycle=1;
						break;
			};
			break;

		case 0xCE : // DEC $0000
			switch (cycle++) {
				case 2: PC++;
						break;
				case 3: PC++;
						break;
				case 4: nextins=mem->read(ptr);
						break;
				case 5: mem->wrt(ptr,nextins);
						--nextins;
						break;
				case 6: mem->wrt(ptr,nextins);
						setZN(nextins);
						cycle=1;
						break;
			};
			break;

		case 0xDE : // DEC $0000,X
			switch (cycle++) {
				case 2: PC++;
						break;
				case 3: PC++;
						ptr+=X;
						break;
				case 4: nextins=mem->read(ptr);
						break;
				case 5: break;
				case 6: mem->wrt(ptr,nextins);
						--nextins;
						break;
				case 7: mem->wrt(ptr,nextins);
						setZN(nextins);
						cycle=1;
						break;
			};
			break;

		case 0xEE : // INC $0000
			switch (cycle++) {
				case 2: PC++;
						break;
				case 3: PC++;
						break;
				case 4: nextins=mem->read(ptr);
						break;
				case 5: mem->wrt(ptr,nextins);
						++nextins;
						break;
				case 6: mem->wrt(ptr,nextins);
						setZN(nextins);
						cycle=1;
						break;
			};
			break;

		case 0xFE : // INC $0000,X
			switch (cycle++) {
				case 2: PC++;
						break;
				case 3: PC++;
						ptr+=X;
						break;
				case 4: nextins=mem->read(ptr);
						break;
				case 5: break;
				case 6: mem->wrt(ptr,nextins);
						++nextins;
						break;
				case 7: mem->wrt(ptr,nextins);
						setZN(nextins);
						cycle=1;
						break;
			};
			break;

		// zero page indexed with X or Y

		case 0x94 : // STY $00,X
			switch (cycle++) {
				case 2: PC++;
						break;
				case 3: nextins+=X;
						break;
				case 4: mem->wrt(nextins,Y);
						cycle=1;
						break;
			};
			break;

		case 0x95 : // STA $00,X
			switch (cycle++) {
				case 2: PC++;
						break;
				case 3: nextins+=X;
						break;
				case 4: mem->wrt(nextins,AC);
						cycle=1;
						break;
			};
			break;

		case 0x96 : // STX $00,Y
			switch (cycle++) {
				case 2: PC++;
						break;
				case 3: nextins+=Y;
						break;
				case 4: mem->wrt(nextins,X);
						cycle=1;
						break;
			};
			break;

		case 0xB4 : // LDY $00,X
			switch (cycle++) {
				case 2: PC++;
						break;
				case 3: nextins+=X;
						break;
				case 4: Y=mem->read(nextins);
						setZN(Y);
						cycle=1;
						break;
			};
			break;

		case 0xB5 : // LDA $00,X
			switch (cycle++) {
				case 2: PC++;
						break;
				case 3: nextins+=X;
						break;
				case 4: AC=mem->read(nextins);
						setZN(AC);
						cycle=1;
						break;
			};
			break;

		case 0xB6 : // LDX $00,Y
			switch (cycle++) {
				case 2: PC++;
						break;
				case 3: nextins+=Y;
						break;
				case 4: X=mem->read(nextins);
						setZN(X);
						cycle=1;
						break;
			};
			break;

		case 0xD5 : // CMP $00,X
			switch (cycle++) {
				case 2: PC++;
						break;
				case 3: nextins+=X;
						nextins=mem->read(nextins);
						break;
				case 4: (AC>=nextins) ? ST|=0x01 : ST&=0xFE;
						nextins=AC-nextins;
						setZN(nextins);
						cycle=1;
						break;
			};
			break;

		case 0x15 : // ORA $00,X
			switch (cycle++) {
				case 2: PC++;
						nextins+=X;
						break;
				case 3: AC|=(mem->read(nextins));
						break;
				case 4: setZN(AC);
						cycle=1;
						break;
			};
			break;

		case 0x35 : // AND $00,X
			switch (cycle++) {
				case 2: PC++;
						nextins+=X;
						break;
				case 3: AC&=(mem->read(nextins));
						break;
				case 4: setZN(AC);
						cycle=1;
						break;
			};
			break;

		case 0x16 : // ASL $00,X
			switch (cycle++) {
				case 2: PC++;
						break;
				case 3: nextins+=X;
						break;
				case 4: farins=mem->read(nextins);
						break;
				case 5: mem->wrt(nextins,farins);
						(farins)&0x80 ? ST|=0x01 : ST&=0xFE;
						farins<<=1;
						break;
				case 6: mem->wrt(nextins,farins);
						setZN(farins);
						cycle=1;
						break;
			};
			break;

		case 0x36 : // ROL $00,X
			switch (cycle++) {
				case 2: PC++;
						break;
				case 3: nextins+=X;
						ptr=nextins;
						break;
				case 4: farins=mem->read(ptr);
						break;
				case 5: mem->wrt(ptr,farins);
						nextins=(farins<<1)|((ST&0x01));
						farins&0x80 ? ST|=0x01 : ST&=0xFE;
						break;
				case 6: mem->wrt(ptr,nextins);
						setZN(nextins);
						cycle=1;
						break;
			};
			break;

		// absolute addressing indexed with X or Y

		case 0x19 : // ORA $0000,Y
			switch (cycle++) {
				case 2: PC++;
						break;
				case 3: ptr+=Y;
						PC++;
						break;
				case 4: if (nextins+Y<0x100) {
							AC|=mem->read(ptr);
							setZN(AC);
							cycle=1;
						}
						break;
				case 5: AC|=mem->read(ptr);
						setZN(AC);
						cycle=1;
						break;
			};
			break;

		case 0x39 : // AND $0000,Y
			switch (cycle++) {
				case 2: PC++;
						break;
				case 3: ptr+=Y;
						PC++;
						break;
				case 4: if (nextins+Y<0x100) {
							AC&=mem->read(ptr);
							setZN(AC);
							cycle=1;
						}
						break;
				case 5: AC&=mem->read(ptr);
						setZN(AC);
						cycle=1;
						break;
			};
			break;

		case 0x59 : // EOR $0000,Y
			switch (cycle++) {
				case 2: PC++;
						break;
				case 3: ptr+=Y;
						PC++;
						break;
				case 4: if (nextins+Y<0x100) {
							AC^=mem->read(ptr);
							setZN(AC);
							cycle=1;
						}
						break;
				case 5: AC^=mem->read(ptr);
						setZN(AC);
						cycle=1;
						break;
			};
			break;

		case 0x79 : // ADC $0000,Y
			switch (cycle++) {
				case 2: PC++;
						break;
				case 3: ptr+=Y;
						PC++;
						break;
				case 4: if (nextins+Y<0x100) {
							ADC(mem->read(ptr));
							cycle=1;
						}
						break;
				case 5: ADC(mem->read(ptr));
						cycle=1;
						break;
			};
			break;

		case 0xB9 : // LDA $0000,Y
			switch (cycle++) {
				case 2: PC++;
						break;
				case 3: ptr+=Y;
						PC++;
						break;
				case 4: if (nextins+Y<0x100) {
							AC=mem->read(ptr);
							setZN(AC);
							cycle=1;
						}
						break;
				case 5: AC=mem->read(ptr);
						setZN(AC);
						cycle=1;
						break;
			};
			break;

		case 0x1D : // ORA $0000,X
			switch (cycle++) {
				case 2: PC++;
						break;
				case 3: ptr+=X;
						PC++;
						break;
				case 4: if (nextins+X<0x100) {
							AC|=mem->read(ptr);
							setZN(AC);
							cycle=1;
						};
						break;
				case 5: AC|=mem->read(ptr);
						setZN(AC);
						cycle=1;
						break;
			};
			break;

		case 0x3D : // AND $0000,X
			switch (cycle++) {
				case 2: PC++;
						break;
				case 3: ptr+=X;
						PC++;
						break;
				case 4: if (nextins+X<0x100) {
							AC&=mem->read(ptr);
							setZN(AC);
							cycle=1;
						};
						break;
				case 5: AC&=mem->read(ptr);
						setZN(AC);
						cycle=1;
						break;
			};
			break;

		case 0x5D : // EOR $0000,X
			switch (cycle++) {
				case 2: PC++;
						break;
				case 3: ptr+=X;
						PC++;
						break;
				case 4: if (nextins+X<0x100) {
							AC^=mem->read(ptr);
							setZN(AC);
							cycle=1;
						};
						break;
				case 5: AC^=mem->read(ptr);
						setZN(AC);
						cycle=1;
						break;
			};
			break;

		case 0x7D : // ADC $0000,X
			switch (cycle++) {
				case 2: PC++;
						break;
				case 3: ptr+=X;
						PC++;
						break;
				case 4: if (nextins+X<0x100) {
							ADC(mem->read(ptr));
							cycle=1;
						};
						break;
				case 5: ADC(mem->read(ptr));
						cycle=1;
						break;
			};
			break;

		case 0xBC : // LDY $0000,X
			switch (cycle++) {
				case 2: PC++;
						break;
				case 3: ptr+=X;
						PC++;
						break;
				case 4: if (nextins+X<0x100) {
							Y=mem->read(ptr);
							setZN(Y);
							cycle=1;
						};
						break;
				case 5: Y=mem->read(ptr);
						setZN(Y);
						cycle=1;
						break;
			};
			break;

		case 0xBD : // LDA $0000,X
			switch (cycle++) {
				case 2: PC++;
						break;
				case 3: ptr+=X;
						PC++;
						break;
				case 4: if (nextins+X<0x100) {
							AC=mem->read(ptr);
							setZN(AC);
							cycle=1;
						};
						break;
				case 5: AC=mem->read(ptr);
						setZN(AC);
						cycle=1;
						break;
			};
			break;

		case 0xBE : // LDX $0000,Y
			switch (cycle++) {
				case 2: PC++;
						break;
				case 3: ptr+=Y;
						PC++;
						break;
				case 4: if (nextins+Y<0x100) {
							X=mem->read(ptr);
							setZN(X);
							cycle=1;
						};
						break;
				case 5: X=mem->read(ptr);
						setZN(X);
						cycle=1;
						break;
			};
			break;

		case 0xD9 : // CMP $0000,Y
			switch (cycle++) {
				case 2: PC++;
						break;
				case 3: farins=mem->read(ptr+Y);
						PC++;
						break;
				case 4: (AC>=farins) ? ST|=0x01 : ST&=0xFE;
						if (nextins+Y<0x100){
							farins=AC-farins;
							setZN(farins);
							cycle=1;
						};
						break;
				case 5: farins=AC-farins;
						setZN(farins);
						cycle=1;
						break;
			};
			break;

		case 0xF9 : // SBC $0000,Y - ujabb
			switch (cycle++) {
				case 2: PC++;
						break;
				case 3: ptr+=Y;
						PC++;
						break;
				case 4: if (nextins+Y<0x100){
							SBC(mem->read(ptr));
							cycle=1;
						};
						break;
				case 5: SBC(mem->read(ptr));
						cycle=1;
						break;
			};
			break;

		case 0xDD : // CMP $0000,X
			switch (cycle++) {
				case 2: PC++;
						break;
				case 3: farins=mem->read(ptr+X);
						PC++;
						break;
				case 4: (AC>=farins) ? ST|=0x01 : ST&=0xFE;
						if (nextins+X<0x100) {
							farins=AC-farins;
							setZN(farins);
							cycle=1;
						};
						break;
				case 5: farins=AC-farins;
						setZN(farins);
						cycle=1;
						break;
			};
			break;

		case 0xFD : // SBC $0000,X - ujabb
			switch (cycle++) {
				case 2: PC++;
						break;
				case 3: ptr+=X;
						PC++;
						break;
				case 4: if (nextins+X<0x100) {
							SBC(mem->read(ptr));
							cycle=1;
						};
						break;
				case 5: SBC(mem->read(ptr));
						cycle=1;
						break;
			};
			break;

		// zero page operations

		case 0xA4 : // LDY $00
			switch (cycle++) {
				case 2: PC++;
						break;
				case 3: Y=mem->read(nextins);
						setZN(Y);
						cycle=1;
						break;
			};
			break;

		case 0xC4 : // CPY $00
			switch (cycle++) {
				case 2: PC++;
						nextins=mem->read(nextins);
						break;
				case 3: (Y>=nextins) ? ST|=0x01 : ST&=0xFE;
						nextins=Y-nextins;
						setZN(nextins);
						cycle=1;
						break;
			};
			break;

		case 0x05 : // ORA $00
			switch (cycle++) {
				case 2: PC++;
						break;
				case 3: AC|=mem->read(nextins);
						break;
				case 4:	setZN(AC);
						cycle=1;
						break;
			};
			break;

		case 0x55 : // EOR $00,X
			switch (cycle++) {
				case 2: PC++;
						break;
				case 3: nextins=mem->read(nextins+X);
						break;
				case 4: AC^=nextins;
						setZN(AC);
						cycle=1;
						break;
			};
			break;

		case 0x65 : // ADC $00
			switch (cycle++) {
				case 2: PC++;
						break;
				case 3: ADC(mem->read(nextins));
						cycle=1;
						break;
			};
			break;

		case 0x75 : // ADC $00,X
			switch (cycle++) {
				case 2: PC++;
						break;
				case 3: nextins+=X;
						break;
				case 4: ADC(mem->read(nextins));
						cycle=1;
						break;
			};
			break;

		case 0xA5 : // LDA $00
			switch (cycle++) {
				case 2: PC++;
						break;
				case 3: AC=mem->read(nextins);
						setZN(AC);
						cycle=1;
						break;
			};
			break;

		case 0xC5 : // CMP $00
			switch (cycle++) {
				case 2: PC++;
						nextins=mem->read(nextins);
						break;
				case 3: (AC>=nextins) ? ST|=0x01 : ST&=0xFE;
						nextins=AC-nextins;
						setZN(nextins);
						cycle=1;
						break;
			};
			break;

		case 0xE5 : // SBC $00
			switch (cycle++) {
				case 2: PC++;
						break;
				case 3: nextins=mem->read(nextins);
						SBC(nextins);
						cycle=1;
						break;
			};
			break;

		case 0xF5 : // SBC $00,X
			switch (cycle++) {
				case 2: PC++;
						break;
				case 3: nextins=mem->read(nextins+X);
						break;
				case 4: SBC(nextins);
						cycle=1;
						break;
			};
			break;

		case 0x06 : // ASL $00
			switch (cycle++) {
				case 2: PC++;
						break;
				case 3: farins=mem->read(nextins);
						farins&0x80 ? ST|=0x01 : ST&=0xFE;
						break;
				case 4: mem->wrt(nextins,farins);
						farins<<=1;
						break;
				case 5: mem->wrt(nextins,farins);
						setZN(farins);
						cycle=1;
						break;
			};
			break;

		case 0x26 : // ROL $00
			switch (cycle++) {
				case 2: PC++;
						ptr=nextins;
						break;
				case 3: farins=mem->read(ptr);
						nextins=(farins<<1)|(ST&0x01);
						break;
				case 4: mem->wrt(ptr,farins);
						farins&0x80 ? ST|=0x01 : ST&=0xFE; // the Carry flag
						break;
				case 5: mem->wrt(ptr,nextins);
						setZN(nextins);
						cycle=1;
						break;
			};
			break;

		case 0x25 : // AND $00
			switch (cycle++) {
				case 2: PC++;
						break;
				case 3: AC&=mem->read(nextins);
						setZN(AC);
						cycle=1;
						break;
			};
			break;

		case 0x45 : // EOR $00
			switch (cycle++) {
				case 2: PC++;
						break;
				case 3: nextins=mem->read(nextins);
						AC^=nextins;
						setZN(AC);
						cycle=1;
						break;
			};
			break;

		case 0x46 : // LSR $00
			switch (cycle++) {
				case 2: PC++;
						break;
				case 3: farins=mem->read(nextins);
						farins&0x01 ? ST|=0x01 : ST&=0xFE;
						break;
				case 4: mem->wrt(nextins,farins);
						farins>>=1;
						break;
				case 5: mem->wrt(nextins,farins);
						setZN(farins);
						cycle=1;
						break;
			};
			break;

		case 0x56 : // LSR $00,X
			switch (cycle++) {
				case 2: PC++;
						break;
				case 3: nextins+=X;
						break;
				case 4: farins=mem->read(nextins);
						farins&0x01 ? ST|=0x01 : ST&=0xFE;
						break;
				case 5: mem->wrt(nextins,farins);
						farins>>=1;
						break;
				case 6: mem->wrt(nextins,farins);
						setZN(farins);
						cycle=1;
						break;
			};
			break;

		case 0x66 : // ROR $00
			switch (cycle++) {
				case 2: PC++;
						break;
				case 3: ptr=nextins;
						farins=mem->read(ptr);
						nextins=(farins>>1)|((ST&0x01)<<7);
						break;
				case 4: mem->wrt(ptr,farins);
						farins&0x01 ? ST|=0x01 : ST&=0xFE; // the Carry flag
						break;
				case 5: mem->wrt(ptr,nextins);
						setZN(nextins);
						cycle=1;
						break;
			};
			break;

		case 0x76 : // ROR $00,X
			switch (cycle++) {
				case 2: PC++;
						break;
				case 3: nextins+=X;
						ptr=nextins;
						break;
				case 4: farins=mem->read(ptr);
						nextins=(farins>>1)|((ST&0x01)<<7);
						break;
				case 5: mem->wrt(ptr,farins);
						farins&0x01 ? ST|=0x01 : ST&=0xFE; // the Carry flag
						break;
				case 6: mem->wrt(ptr,nextins);
						setZN(nextins);
						cycle=1;
						break;
			};
			break;

		case 0xA6 : // LDX $00
			switch (cycle++) {
				case 2: PC++;
						break;
				case 3: X=mem->read(nextins);
						setZN(X);
						cycle=1;
						break;
			};
			break;

		case 0xC6 : // DEC $00
			switch (cycle++) {
				case 2: PC++;
						break;
				case 3: farins=mem->read(nextins);
						break;
				case 4: mem->wrt(nextins,farins);
						--farins;
						break;
				case 5: mem->wrt(nextins,farins);
						setZN(farins);
						cycle=1;
						break;
			};
			break;

		case 0xE4 : // CPX $00
			switch (cycle++) {
				case 2: PC++;
						nextins=mem->read(nextins);
						break;
				case 3: (X>=nextins) ? ST|=0x01 : ST&=0xFE;
						nextins=X-nextins;
						setZN(nextins);
						cycle=1;
						break;
			};
			break;

		case 0xE6 : // INC $00
			switch (cycle++) {
				case 2: PC++;
						break;
				case 3: farins=mem->read(nextins);
						break;
				case 4: mem->wrt(nextins,farins);
						++farins;
						break;
				case 5: mem->wrt(nextins,farins);
						setZN(farins);
						cycle=1;
						break;
			};
			break;

		case 0xD6 : // DEC $00,X
			switch (cycle++) {
				case 2: PC++;
						break;
				case 3: nextins+=X;
						break;
				case 4: farins=mem->read(nextins);
						break;
				case 5: mem->wrt(nextins,farins);
						--farins;
						break;
				case 6: mem->wrt(nextins,farins);
						setZN(farins);
						cycle=1;
						break;
			};
			break;

		case 0xF6 : // INC $00,X
			switch (cycle++) {
				case 2: PC++;
						break;
				case 3: nextins+=X;
						break;
				case 4: farins=mem->read(nextins);
						break;
				case 5: mem->wrt(nextins,farins);
						++farins;
						break;
				case 6: mem->wrt(nextins,farins);
						setZN(farins);
						cycle=1;
						break;
			};
			break;

		// indexed indirect

		case 0x01 : // ORA ($00,X)
			switch (cycle++) {
				case 2: PC++;
						break;
				case 3: nextins+=X;
						break;
				case 4: ptr=mem->read(nextins);
						break;
				case 5: ptr|=(mem->read(nextins+1)<<8);
						break;
				case 6: AC|=mem->read(ptr);
						setZN(AC);
						cycle=1;
						break;
			};
			break;

		case 0x21 : // AND ($00,X)
			switch (cycle++) {
				case 2: PC++;
						break;
				case 3: nextins+=X;
						break;
				case 4: ptr=mem->read(nextins);
						break;
				case 5: ptr|=(mem->read(nextins+1)<<8);
						break;
				case 6: AC&=mem->read(ptr);
						setZN(AC);
						cycle=1;
						break;
			};
			break;

		case 0x41 : // EOR ($00,X)
			switch (cycle++) {
				case 2: PC++;
						break;
				case 3: nextins+=X;
						break;
				case 4: ptr=mem->read(nextins);
						break;
				case 5: ptr|=(mem->read(nextins+1)<<8);
						break;
				case 6: AC^=mem->read(ptr);
						setZN(AC);
						cycle=1;
						break;
			};
			break;

		case 0x61 : // ADC ($00,X)
			switch (cycle++) {
				case 2: PC++;
						break;
				case 3: nextins+=X;
						break;
				case 4: ptr=mem->read(nextins);
						break;
				case 5: ptr|=(mem->read(nextins+1)<<8);
						break;
				case 6: ADC(mem->read(ptr));
						cycle=1;
						break;
			};
			break;

		case 0x81 : // STA ($00,X)
			switch (cycle++) {
				case 2: PC++;
						break;
				case 3: nextins+=X;
						break;
				case 4: ptr=mem->read(nextins);
						break;
				case 5: ptr|=(mem->read(nextins+1)<<8);
						break;
				case 6: mem->wrt(ptr,AC);
						cycle=1;
						break;
			};
			break;

		case 0xA1 : // LDA ($00,X)
			switch (cycle++) {
				case 2: PC++;
						break;
				case 3: nextins+=X;
						break;
				case 4: ptr=mem->read(nextins);
						break;
				case 5: ptr|=(mem->read(nextins+1)<<8);
						break;
				case 6: AC=mem->read(ptr);
						setZN(AC);
						cycle=1;
						break;
			};
			break;

		case 0xC1 : // CMP ($00,X)
			switch (cycle++) {
				case 2: PC++;
						break;
				case 3: nextins+=X;
						break;
				case 4: ptr=mem->read(nextins);
						break;
				case 5: ptr|=(mem->read(nextins+1)<<8);
						break;
				case 6: (AC>=mem->read(ptr)) ? ST|=0x01 : ST&=0xFE;
						nextins=AC-mem->read(ptr);
						setZN(nextins);
						cycle=1;
						break;
			};
			break;

		case 0xE1 : // SBC ($00,X)
			switch (cycle++) {
				case 2: PC++;
						break;
				case 3: nextins+=X;
						break;
				case 4: ptr=mem->read(nextins);
						break;
				case 5: ptr|=(mem->read(nextins+1)<<8);
						break;
				case 6: SBC(mem->read(ptr));
						cycle=1;
						break;
			};
			break;

		// indirect indexed

		case 0x11 : // ORA ($00),Y
			switch (cycle++) {
				case 2: PC++;
						break;
				case 3: ptr=mem->read(nextins);
						break;
				case 4: ptr|=mem->read(nextins+1)<<8;
						break;
				case 5: if ((ptr&0x00FF)+Y<0x100) {
							AC|=mem->read(ptr+Y);
							cycle=1;
							setZN(AC);
						}
						break;
				case 6: AC|=mem->read(ptr+Y);
						setZN(AC);
						cycle=1;
						break;
			};
			break;

		case 0x31 : // AND ($00),Y
			switch (cycle++) {
				case 2: PC++;
						break;
				case 3: ptr=mem->read(nextins);
						break;
				case 4: ptr|=mem->read(nextins+1)<<8;
						break;
				case 5: if ((ptr&0x00FF)+Y<0x100) {
							AC&=mem->read(ptr+Y);
							cycle=1;
							setZN(AC);
						}
						break;
				case 6: AC&=mem->read(ptr+Y);
						setZN(AC);
						cycle=1;
						break;
			};
			break;

		case 0x51 : // EOR ($00),Y
			switch (cycle++) {
				case 2: PC++;
						break;
				case 3: ptr=mem->read(nextins);
						break;
				case 4: ptr|=mem->read(nextins+1)<<8;
						break;
				case 5: if ((ptr&0x00FF)+Y<0x100) {
							AC^=mem->read(ptr+Y);
							cycle=1;
							setZN(AC);
						}
						break;
				case 6: AC^=mem->read(ptr+Y);
						setZN(AC);
						cycle=1;
						break;
			};
			break;

		case 0x71 : // ADC ($00),Y
			switch (cycle++) {
				case 2: PC++;
						break;
				case 3: ptr=mem->read(nextins);
						break;
				case 4: ptr|=mem->read(nextins+1)<<8;
						break;
				case 5: if ((ptr&0x00FF)+Y<0x100) {
							ADC(mem->read(ptr+Y));
							cycle=1;
						};
						break;
				case 6: ADC(mem->read(ptr+Y));
						cycle=1;
						break;
			};
			break;

		case 0x91 : // STA ($00),Y
			switch (cycle++) {
				case 2: PC++;
						break;
				case 3: ptr=mem->read(nextins);
						break;
				case 4: ptr|=mem->read(nextins+1)<<8;
						break;
				case 5: if ((ptr&0x00FF)+Y<0x100) {
							mem->wrt(ptr+Y,AC);
							cycle=1;
						}
						break;
				case 6: mem->wrt(ptr+Y,AC);
						cycle=1;
						break;
			};
			break;

		case 0xB1 : // LDA ($00),Y
			switch (cycle++) {
				case 2: PC++;
						break;
				case 3: ptr=mem->read(nextins);
						break;
				case 4: ptr|=mem->read(nextins+1)<<8;
						break;
				case 5: if ((ptr&0x00FF)+Y<0x100) {
							AC=mem->read(ptr+Y);
							cycle=1;
							setZN(AC);
						}
						break;
				case 6: AC=mem->read(ptr+Y);
						setZN(AC);
						cycle=1;
						break;
			};
			break;

		case 0xD1 : // CMP ($00),Y
			switch (cycle++) {
				case 2: PC++;
						break;
				case 3: ptr=mem->read(nextins);
						break;
				case 4: ptr|=mem->read(nextins+1)<<8;
						break;
				case 5: (AC>=mem->read(ptr+Y)) ? ST|=0x01 : ST&=0xFE;
						if ((ptr&0x00FF)+Y<0x100) {
							nextins=AC-mem->read(ptr+Y);
							cycle=1;
							setZN(nextins);
						}
						break;
				case 6: nextins=AC-mem->read(ptr+Y);
						setZN(nextins);
						cycle=1;
						break;

			};
			break;

		case 0xF1 : // SBC ($00),Y
			switch (cycle++) {
				case 2: PC++;
						break;
				case 3: ptr=mem->read(nextins);
						break;
				case 4: ptr|=mem->read(nextins+1)<<8;
						break;
				case 5: if ((ptr&0x00FF)+Y<0x100) {
							SBC(mem->read(ptr+Y));
							cycle=1;
						};
						break;
				case 6: SBC(mem->read(ptr+Y));
						cycle=1;
						break;
			};
			break;

		// storage functions

		case 0x84 : // STY $00
			switch (cycle++) {
				case 2: PC++;
						break;
				case 3: mem->wrt(nextins,Y);
						cycle=1;
						break;
			};
			break;

		case 0x85 : // STA $00
			switch (cycle++) {
				case 2: PC++;
						break;
				case 3: mem->wrt(nextins,AC);
						cycle=1;
						break;
			};
			break;

		case 0x86 : // STX $00
			switch (cycle++) {
				case 2: PC++;
						break;
				case 3: mem->wrt(nextins,X);
						cycle=1;
						break;
			};
			break;

		case 0x8C : // STY $0000
			switch (cycle++) {
				case 2: PC++;
						break;
				case 3: PC++;
						break;
				case 4: mem->wrt(ptr,Y);
						cycle=1;
						break;
			};
			break;

		case 0x8D : // STA $0000
			switch (cycle++) {
				case 2: PC++;
						break;
				case 3: PC++;
						break;
				case 4: mem->wrt(ptr,AC);
						cycle=1;
						break;
			};
			break;

		case 0x8E : // STX $0000
			switch (cycle++) {
				case 2: PC++;
						break;
				case 3: PC++;
						break;
				case 4: mem->wrt(ptr,X);
						cycle=1;
						break;
			};
			break;

		case 0x9D : // STA $0000,X
			switch (cycle++) {
				case 2: PC++;
						break;
				case 3: PC++;
						break;
				case 4: ptr+=X;
						break;
				case 5: mem->wrt(ptr,AC);
						cycle=1;
						break;
			};
			break;

		//----------------
		// illegal opcodes
		//----------------

		case 0x03 : // ASO/SLO ($00,X) : A <- (M << 1) \/ A
			switch (cycle++) {
				case 2: PC++;
						break;
				case 3: nextins+=X;
						break;
				case 4: ptr=mem->read(nextins);
						break;
				case 5: ptr|=(mem->read(nextins+1)<<8);
						break;
				case 6: mem->wrt(ptr,mem->read(ptr));
						farins=mem->read(ptr);
						(farins)&0x80 ? ST|=0x01 : ST&=0xFE;
						farins<<=1;
						break;
				case 7: mem->wrt(ptr,farins);
						break;
				case 8: AC|=farins;
						setZN(AC);
						cycle=1;
						break;
			};
			break;

		case 0x04 : // NOP $00
			switch (cycle++) {
				case 2: PC++;
						break;
				case 3: cycle=1;
						break;
			};
			break;

		case 0x07 : // ASO/SLO $00		: A <- (M << 1) \/ A
			switch (cycle++) {
				case 2: PC++;
						ptr=nextins;
						break;
				case 3: mem->wrt(ptr,mem->read(ptr));
						nextins=mem->read(ptr);
						(nextins)&0x80 ? ST|=0x01 : ST&=0xFE;
						nextins<<=1;
						break;
				case 4: mem->wrt(ptr,nextins);
						break;
				case 5: AC|=nextins;
						setZN(AC);
						cycle=1;
						break;
			};
			break;

		case 0x0B : // ANN/ANC #$00
			switch (cycle++) {
				case 2: PC++;
						AC&=nextins;
						if (AC&0x80) {
							ST|=0x01;
							ST|=0x80;
						} else {
							ST&=0xFE;
							ST&=0x7F;
						};
						cycle=1;
						break;
			};
			break;

		case 0x0C : // NOP $0000
			switch (cycle++) {
				case 2: PC++;
						break;
				case 3: PC++;
						break;
				case 4: cycle=1;
						break;
			};
			break;

		case 0x0F : // ASO/SLO $0000		: A <- (M << 1) \/ A
			switch (cycle++) {
				case 2: PC++;
						break;
				case 3: mem->wrt(ptr,mem->read(ptr));
						PC++;
						break;
				case 4: nextins=mem->read(ptr);
						(nextins)&0x80 ? ST|=0x01 : ST&=0xFE;
						nextins<<=1;
						break;
				case 5: mem->wrt(ptr,nextins);
						break;
				case 6: AC|=nextins;
						setZN(AC);
						cycle=1;
						break;
			};
			break;

		case 0x13 : // ASO/SLO ($00),Y		: A <- (M << 1) \/ A
			switch (cycle++) {
				case 2: PC++;
						break;
				case 3: ptr=mem->read(nextins);
						break;
				case 4: ptr|=(mem->read(nextins+1)<<8);
						break;
				case 5: mem->wrt(ptr+Y,mem->read(ptr+Y));
						farins=mem->read(ptr+Y);
						(farins)&0x80 ? ST|=0x01 : ST&=0xFE;
						farins<<=1;
						break;
				case 6: mem->wrt(ptr+Y,farins);
						break;
				case 7: break;
				case 8: if (nextins+Y<0x100) {
							AC|=farins;
							cycle=1;
							setZN(AC);
						}
						break;
				case 9: AC|=farins;
						setZN(AC);
						cycle=1;
						break;
			};
			break;

		case 0x14 : // NOP $00,X
			switch (cycle++) {
				case 2: PC++;
						break;
				case 3: break;
				case 4: cycle=1;
						break;
			};
			break;

		case 0x17 : // ASO/SLO $00,X		: A <- (M << 1) \/ A
			switch (cycle++) {
				case 2: PC++;
						nextins+=X;
						ptr=nextins;
						break;
				case 3: mem->wrt(ptr,mem->read(ptr));
						farins=mem->read(ptr);
						(farins)&0x80 ? ST|=0x01 : ST&=0xFE;
						farins<<=1;
						break;
				case 4: mem->wrt(ptr,farins);
						break;
				case 5: AC|=farins;
						break;
				case 6: setZN(AC);
						cycle=1;
						break;
			};
			break;

		case 0x1A : // NOP
			switch (cycle++) {
				case 2: cycle=1;
						break;
			};
			break;

		case 0x1B : // ASO/SLO $0000,Y
			switch (cycle++) {
				case 2: PC++;
						break;
				case 3: ptr+=Y;
						PC++;
						break;
				case 4: mem->wrt(ptr,mem->read(ptr));
						break;
				case 5:	farins=mem->read(ptr);
						(farins)&0x80 ? ST|=0x01 : ST&=0xFE;
						farins<<=1;
						break;
				case 6:	mem->wrt(ptr,farins);
						break;
				case 7: if (nextins+Y<0x100) {
							AC|=farins;
							setZN(AC);
							cycle=1;
						}
						break;
				case 8: AC|=farins;
						setZN(AC);
						cycle=1;
						break;
			};
			break;

		case 0x1C : // NOP $0000,X
			switch (cycle++) {
				case 2: PC++;
						break;
				case 3: PC++;
						break;
				case 4: if (nextins+X<0x100)
							cycle=1;
						break;
				case 5: cycle=1;
						break;
			};
			break;

		case 0x1F : // ASO/SLO $0000,X
			switch (cycle++) {
				case 2: PC++;
						break;
				case 3: PC++;
						ptr+=X;
						break;
				case 4: farins=mem->read(ptr);
						mem->wrt(ptr,farins);
						break;
				case 5:	farins=mem->read(ptr);
						(farins)&0x80 ? ST|=0x01 : ST&=0xFE;
						farins<<=1;
						mem->wrt(ptr,farins);
						break;
				case 7: if (nextins+X<0x100) {
							AC|=farins;
							setZN(AC);
							cycle=1;
						}
						break;
				case 8: AC|=farins;
						setZN(AC);
						cycle=1;
						break;
			};
			break;

		case 0x23 : // RAN/RLA ($00,X) - ROL memory, AND accu, result into acc
			switch (cycle++) {
				case 2: PC++;
						break;
				case 3: nextins+=X;
						break;
				case 4: ptr=mem->read(nextins);
						break;
				case 5: ptr|=(mem->read(nextins+1)<<8);
						farins=mem->read(ptr);
						break;
				case 6: nextins=(farins<<1)|(ST&0x01);
						mem->wrt(ptr,nextins);
						break;
				case 7:	(farins&0x80) ? ST|=0x01 : ST&=0xFE; // the Carry flag
						break;
				case 8: AC&=nextins;
						setZN(AC);
						cycle=1;
						break;
			};
			break;

		case 0x27 : // RAN/RLA $00 -		A <- (M << 1) /\ (A)
			switch (cycle++) {
				case 2: PC++;
						ptr=nextins;
						break;
				case 3: farins=mem->read(ptr);
						nextins=(farins<<1)|(ST&0x01);
						break;
				case 4: mem->wrt(ptr,nextins);
						(farins&0x80) ? ST|=0x01 : ST&=0xFE; // the Carry flag
						break;
				case 5: AC&=nextins;
						setZN(AC);
						cycle=1;
						break;
			};
			break;

		case 0x2B : // ANN/ANC #$00
			switch (cycle++) {
				case 2: PC++;
						AC&=nextins;
						if (AC&0x80) {
							ST|=0x01;
							ST|=0x80;
						} else {
							ST&=0xFE;
							ST&=0x7F;
						};
						cycle=1;
						break;
			};
			break;

		case 0x2F : // RAN/RLA $0000
			switch (cycle++) {
				case 2: PC++;
						break;
				case 3: PC++;
						break;
				case 4: farins=mem->read(ptr);
						mem->wrt(ptr, farins);
						nextins=(farins<<1)|(ST&0x01);
						break;
				case 5: mem->wrt(ptr,nextins);
						(farins&0x80) ? ST|=0x01 : ST&=0xFE; // the Carry flag
						if ((ptr&0xFF)<0x100) {
							AC&=nextins;
							setZN(AC);
							cycle=1;
						}
						break;
				case 6: AC&=nextins;
						setZN(AC);
						cycle=1;
						break;
			};
			break;

		case 0x33 : // RAN/RLA ($00),Y -	A <- (M << 1) /\ (A) - not 100%
			switch (cycle++) {
				case 2: PC++;
						break;
				case 3: ptr=mem->read(nextins);
						break;
				case 4: ptr|=mem->read(nextins+1)<<8;
						break;
				case 5: farins=mem->read(ptr+Y);
						nextins=(farins<<1)|(ST&0x01);
						break;
				case 6: mem->wrt(ptr+Y,nextins);
						break;
				case 7: (farins&0x80) ? ST|=0x01 : ST&=0xFE; // the Carry flag
						break;
				case 8: if ((ptr&0x00FF)+Y<0x100) {
							AC&=nextins;
							cycle=1;
							setZN(AC);
						}
						break;
				case 9: AC&=nextins;
						setZN(AC);
						cycle=1;
						break;
			};
			break;

		case 0x34 : // NOP $00,X
			switch (cycle++) {
				case 2: PC++;
						break;
				case 3: break;
				case 4: cycle=1;
						break;
			};
			break;

		case 0x37 : // RAN/RLA $00,X -			A <- (M << 1) /\ (A)
			switch (cycle++) {
				case 2: PC++;
						break;
				case 3: nextins+=X;
						ptr=nextins;
						break;
				case 4: farins=mem->read(ptr);
						nextins=(farins<<1)|(ST&0x01);
						break;
				case 5: mem->wrt(ptr,nextins);
						(farins&0x80) ? ST|=0x01 : ST&=0xFE; // the Carry flag
						break;
				case 6: AC&=nextins;
						setZN(AC);
						cycle=1;
						break;
			};
			break;

		case 0x3A : // NOP
			switch (cycle++) {
				case 2: cycle=1;
						break;
			};
			break;

		case 0x3B : // RAN/RLA $0000,Y -	A <- (M << 1) /\ (A)
			switch (cycle++) {
				case 2: PC++;
						break;
				case 3: PC++;
						break;
				case 4: farins=mem->read(ptr+Y);
						mem->wrt(ptr+Y,farins);
						break;
				case 5:	nextins=(farins<<1)|(ST&0x01);
						mem->wrt(ptr+Y,nextins);
						break;
				case 6:	(farins&0x80) ? ST|=0x01 : ST&=0xFE; // the Carry flag
						break;
				case 7: if ((ptr&0xFF)+Y<0x100) {
							AC&=nextins;
							setZN(AC);
							cycle=1;
						}
						break;
				case 8: AC&=nextins;
						setZN(AC);
						cycle=1;
						break;
			};
			break;

		case 0x3C : // NOP $0000,X
			switch (cycle++) {
				case 2: PC++;
						break;
				case 3: ptr+=X;
						PC++;
						break;
				case 4: if (nextins+X<0x100)
							cycle=1;
						break;
				case 5: cycle=1;
						break;
			};
			break;

		case 0x3F : // RAN/RLA $0000,X - not implemented only fakely
			switch (cycle++) {
				case 2: PC++;
						break;
				case 3: ptr+=X;
						PC++;
						break;
				case 4: farins=mem->read(ptr);
						mem->wrt(ptr,farins);
						break;
				case 5: nextins=(farins<<1)|(ST&0x01);
						mem->wrt(ptr,nextins);
						(farins&0x80) ? ST|=0x01 : ST&=0xFE; // the Carry flag
						break;
				case 6: AC&=nextins;
						setZN(AC);
						break;
				case 7: if (ptr&0xFF<0x100) {
							cycle=1;
						}
						break;
				case 8: cycle=1;
						break;
			};
			break;

		case 0x43 : // LSE/SRE ($00,X) -	A <- (M >> 1) ^ A
			switch (cycle++) {
				case 2: PC++;
						break;
				case 3: nextins+=X;
						break;
				case 4: ptr=mem->read(nextins);
						break;
				case 5: ptr|=(mem->read(nextins+1)<<8);
						break;
				case 6: nextins=mem->read(ptr)>>1;
						break;
				case 7: mem->wrt(ptr,nextins);
						break;
				case 8: AC^=nextins;
						setZN(AC);
						cycle=1;
						break;
			};
			break;

		case 0x44 : // NOP $00
			switch (cycle++) {
				case 2: PC++;
						break;
				case 3: cycle=1;
						break;
			};
			break;

		case 0x47 : // LSE/SRE $00 -		A <- (M >> 1) \-/ A
			switch (cycle++) {
				case 2: PC++;
						ptr=nextins;
						break;
				case 3: nextins=mem->read(ptr);
						nextins&0x01 ? ST|=0x01 : ST&=0xFE;
						nextins>>=1;
						mem->wrt(ptr,nextins);
						break;
				case 4: AC^=nextins;
						break;
				case 5: setZN(AC);
						cycle=1;
						break;
			};
			break;

		case 0x4B : // ANL/ASR #$00
			switch (cycle++) {
				case 2: PC++;
						AC=(AC&nextins);
						(AC&0x01) ? ST|=0x01 : ST&=0xFE;
						AC>>=1;
						setZN(AC);
						cycle=1;
						break;
			};
			break;

		case 0x4F : // LSE/SRE $0000 - A <- (M >> 1) \-/ A
			switch (cycle++) {
				case 2: PC++;
						break;
				case 3: PC++;
						break;
				case 4: nextins=mem->read(ptr);
						nextins&0x01 ? ST|=0x01 : ST&=0xFE;
						nextins>>=1;
						break;
				case 5: mem->wrt(ptr,nextins);
						break;
				case 6: AC^=nextins;
						setZN(AC);
						cycle=1;
						break;
			};
			break;

		case 0x53 : // LSE/SRE ($00),Y -	A <- (M >> 1) \-/ A	 - not 100%
			switch (cycle++) {
				case 2: PC++;
						break;
				case 3: ptr=mem->read(nextins);
						break;
				case 4: ptr|=mem->read(nextins+1)<<8;
						break;
				case 5: nextins=mem->read(ptr+Y);
						nextins&0x01 ? ST|=0x01 : ST&=0xFE;
						nextins>>=1;
						break;
				case 6: mem->wrt(ptr+Y,nextins);
						break;
				case 7: break;
				case 8: if ((ptr&0x00FF)+Y<0x100) {
							AC^=nextins;
							cycle=1;
							setZN(AC);
						}
						break;
				case 9: AC^=nextins;
						setZN(AC);
						cycle=1;
						break;
			};
			break;

		case 0x54 : // NOP $00,X
			switch (cycle++) {
				case 2: break;
				case 3: PC++;
						break;
				case 4: cycle=1;
						break;
			};
			break;

		case 0x57 : // LSE/SRE $00,X -			A <- (M >> 1) \-/ A	 - not 100%
			switch (cycle++) {
				case 2: PC++;
						break;
				case 3: nextins+=X;
						break;
				case 4: ptr=nextins;
						nextins=mem->read(ptr);
						nextins&0x01 ? ST|=0x01 : ST&=0xFE;
						nextins>>=1;
						break;
				case 5: mem->wrt(ptr,nextins);
						break;
				case 6: AC^=nextins;
						setZN(AC);
						cycle=1;
						break;
			};
			break;

		case 0x5A : // NOP
			switch (cycle++) {
				case 2: cycle=1;
						break;
			};
			break;

		case 0x5B : // LSE/SRE $0000,Y -	A <- (M >> 1) \-/ A
			switch (cycle++) {
				case 2: PC++;
						break;
				case 3: ptr+=Y;
						PC++;
						break;
				case 4: farins=mem->read(ptr);
						farins&0x01 ? ST|=0x01 : ST&=0xFE;
						farins>>=1;
						break;
				case 5: mem->wrt(ptr,farins);
						break;
				case 6: break;
				case 7: if (nextins+Y<0x100) {
							AC^=farins;
							setZN(AC);
							cycle=1;
						}
						break;
				case 8: AC^=farins;
						setZN(AC);
						cycle=1;
						break;
			}
			break;

		case 0x5C : // NOP $0000,X
			switch (cycle++) {
				case 2: PC++;
						break;
				case 3: PC++;
						break;
				case 4: if (nextins+X<0x100)
							cycle=1;
						break;
				case 5: cycle=1;
						break;
			};
			break;

		case 0x5F : // LSE/SRE $0000,X -	A <- (M >> 1) \-/ A	 - not 100%
			switch (cycle++) {
				case 2: PC++;
						break;
				case 3: PC++;
						ptr+=X;
						break;
				case 4: farins=mem->read(ptr);
						farins&0x01 ? ST|=0x01 : ST&=0xFE;
						farins>>=1;
						break;
				case 5: mem->wrt(ptr,farins);
						break;
				case 6: break;
				case 7: if (nextins+X<0x100) {
							AC^=farins;
							setZN(AC);
							cycle=1;
						}
						break;
				case 8: AC^=farins;
						setZN(AC);
						cycle=1;
						break;
			};
			break;

		case 0x63 : // RAD/RRA ($00,X) - ROR memory, ADC accu, result into accu FAKE!!!!!!!!
			switch (cycle++) {
				case 2: PC++;
						break;
				case 3: nextins+=X;
						break;
				case 4: ptr=mem->read(nextins);
						break;
				case 5: ptr|=(mem->read(nextins+1)<<8);
						nextins=mem->read(ptr);
						farins=(nextins>>1)|((ST&0x01)<<7);
						break;
				case 6: nextins&0x01 ? ST|=0x01 : ST&=0xFE;
						mem->wrt(ptr,farins);
						//ADC(farins);
						break;
				case 7: AC+=farins;
						setZN(AC);
						break;
				case 8: cycle=1;
						break;
			};
			break;

		case 0x64 : // NOP $00
			switch (cycle++) {
				case 2: PC++;
						break;
				case 3: cycle=1;
						break;
			};
			break;

		case 0x67 : // RAD/RRA $00
			switch (cycle++) {
				case 2: PC++;
						ptr=nextins;
						break;
				case 3: nextins=mem->read(ptr);
						farins=(nextins>>1)|((ST&0x01)<<7);
						break;
				case 4: nextins&0x01 ? ST|=0x01 : ST&=0xFE;
						mem->wrt(ptr,farins);
						break;
				case 5: //ADC(farins);
						AC+=farins;
						setZN(AC);
						cycle=1;
						break;
			};
			break;

		// Barbarian 3 won't like it....
		// this instruction is not good at all!!!
		case 0x6B : // ARR $00
			switch (cycle++) {
				case 2: PC++;
						AC&=nextins;
						break;
				case 3: AC>>=1;
						setZN(AC);
						cycle=1;
						break;
			};
			break;

		case 0x6F : // RAD/RRA $0000
			switch (cycle++) {
				case 2: PC++;
						break;
				case 3: PC++;
						break;
				case 4: nextins=mem->read(ptr);
						mem->wrt(ptr,nextins);
						farins=(nextins>>1)|((ST&0x01)<<7);
						break;
				case 5: nextins&0x01 ? ST|=0x01 : ST&=0xFE;
						mem->wrt(ptr,farins);
						//ADC(farins);
						break;
				case 6: AC+=farins;
						setZN(AC);
						cycle=1;
						break;
			};
			break;

		case 0x73 : // RAD/RRA ($00),Y -	A <- (M >> 1) + (A) + C - not good yet!!!!!!
			switch (cycle++) {
				case 2: PC++;
						break;
				case 3: ptr=mem->read(nextins);
						break;
				case 4: ptr|=mem->read(nextins+1)<<8;
						break;
				case 5: nextins=mem->read(ptr+Y);
						farins=(nextins>>1)|((ST&0x01)<<7);
						break;
				case 6: nextins&0x01 ? ST|=0x01 : ST&=0xFE;
						mem->wrt(ptr+Y,farins);
						//ADC(farins);
						break;
				case 7: AC+=farins;
						setZN(AC);
						break;
				case 8: if ((ptr&0x00FF)+Y<0x100)
							cycle=1;
						break;
				case 9: cycle=1;
						break;
			};
			break;

		case 0x74 : // NOP $00,X
			switch (cycle++) {
				case 2: PC++;
						break;
				case 3: break;
				case 4: cycle=1;
						break;
			};
			break;

		case 0x77 : // RAD/RRA $00,X -	A <- (M >> 1) + (A) + C  - not good yet!!!!!!
			switch (cycle++) {
				case 2: PC++;
						break;
				case 3: nextins+=X;
						ptr=nextins;
						break;
				case 4: nextins=mem->read(ptr);
						farins=(nextins>>1)|((ST&0x01)<<7);
						break;
				case 5: nextins&0x01 ? ST|=0x01 : ST&=0xFE;
						mem->wrt(ptr,farins);
						//ADC(farins);
						break;
				case 6: AC+=farins;
						setZN(AC);
						cycle=1;
						break;
			};
			break;

		case 0x7A : // NOP
			switch (cycle++) {
				case 2: cycle=1;
						break;
			};
			break;

		case 0x7B : // RAD/RRA $0000,Y -	A <- (M >> 1) + (A) + C   - not good yet!!!!!!
			switch (cycle++) {
				case 2: PC++;
						break;
				case 3: ptr+=Y;
						PC++;
						break;
				case 4: nextins=mem->read(ptr);
						farins=(nextins>>1)|((ST&0x01)<<7);
						break;
				case 5: nextins&0x01 ? ST|=0x01 : ST&=0xFE;
						mem->wrt(ptr,farins);
						//ADC(farins);
						break;
				case 6: AC+=farins;
						setZN(AC);
						break;
				case 7: if (nextins+Y<0x100)
							cycle=1;
						break;
				case 8: cycle=1;
						break;
			}
			break;

		case 0x7C : // NOP $0000,X
			switch (cycle++) {
				case 2: PC++;
						break;
				case 3: PC++;
						break;
				case 4: if (nextins+X<0x100)
							cycle=1;
						break;
				case 5: cycle=1;
						break;
			};
			break;

		case 0x7F : // RAD/RRA $8000,X
			switch (cycle++) {
				case 2: PC++;
						break;
				case 3: ptr+=X;
						PC++;
						break;
				case 4: nextins=mem->read(ptr);
						mem->wrt(ptr, nextins);
						farins=(nextins>>1)|((ST&0x01)<<7);
						break;
				case 5: nextins&0x01 ? ST|=0x01 : ST&=0xFE;
						mem->wrt(ptr,farins);
						break;
				case 6: AC+=farins;
						setZN(AC);
						break;
				case 7: if (nextins+X<0x100)
							cycle=1;
						break;
				case 8: cycle=1;
						break;
			}
			break;

		case 0x80 : // NOP #$00
			switch (cycle++) {
				case 2: PC++;
						cycle=1;
						break;
			};
			break;

		case 0x82 : // NOP #$00
			switch (cycle++) {
				case 2: PC++;
						cycle=1;
						break;
			};
			break;

		case 0x83 : // AXX/SAX ($00,X) -	M <- (A) /\ (X)
			switch (cycle++) {
				case 2: PC++;
						break;
				case 3: nextins+=X;
						break;
				case 4: ptr=mem->read(nextins);
						break;
				case 5: ptr|=(mem->read(nextins+1)<<8);
						break;
				case 6: nextins=AC&X;
						break;
				case 7: mem->wrt(ptr,nextins);
						break;
				case 8: //setZN(nextins);
						cycle=1;
						break;
			};
			break;

		case 0x87 : // AXR/SAX $00 - M <- (A) /\ (X)
			switch (cycle++) {
				case 2: PC++;
						break;
				case 3: farins=AC&X;
						mem->wrt(nextins,farins);
						//setZN(farins);
						cycle=1;
						break;
			};
			break;

		case 0x89 : // NOP #$00
			switch (cycle++) {
				case 2: PC++;
						cycle=1;
						break;
			};
			break;

		case 0x8B : // TAN/ANE $00 -	M <-[(A)/\$EE] \/ (X)/\(M)
			switch (cycle++) {
				case 2: PC++;
						//mem->wrt(nextins,(AC&0xEE)|(X&mem->read(nextins)));
						AC=X&nextins&(AC|0xEE);
						setZN(nextins);
						cycle=1;
						break;
			};
			break;

		case 0x8F : // AAX/SAX $0000
			switch (cycle++) {
				case 2: PC++;
						break;
				case 3: nextins=AC&X;
						PC++;
						break;
				case 4: mem->wrt(ptr,nextins);
						setZN(nextins);
						cycle=1;
						break;
			};
			break;

		case 0x93 : // AXI/SHA ($00),Y		(A) /\ (X) /\ (PCH+1) vagy 0000,X ki tudja???
			switch (cycle++) {
				case 2: PC++;
						break;
				case 3: ptr=mem->read(nextins);
						break;
				case 4: ptr|=mem->read(nextins+1)<<8;
						break;
				case 5: break;
				case 6: if ((ptr&0x00FF)+Y<0x100) {
							mem->wrt(ptr+Y,AC&X&mem->read(ptr+Y+1)+1); //check this!
							cycle=1;
						}
						break;
				case 7: mem->wrt(ptr+Y,AC&X&mem->read(ptr+Y+1)+1);
						cycle=1;
						break;
			};
			break;

		case 0x97 : // AXY/SAX $00,Y
			switch (cycle++) {
				case 2: PC++;
						break;
				case 3: nextins+=Y;
						ptr=nextins;
						break;
				case 4: nextins=AC&X;
						mem->wrt(ptr,nextins);
						//setZN(nextins);
						cycle=1;
						break;
			};
			break;

		case 0x9B : // AXS/SHS $0000,Y	- X <- (A) /\ (X), S <- (X) _plus_  M <- (X) /\ (PCH+1)
			switch (cycle++) {
				case 2: PC++;
						break;
				case 3: PC++;
						ptr+=Y;
						break;
				case 4: SP=AC&X;
						break;
				case 5: if (nextins+Y<0x100) {
							mem->wrt(ptr,AC&X&mem->read(ptr+1)+1);
							cycle=1;
						}
						break;
				case 6: mem->wrt(ptr,AC&X&mem->read(ptr+1)+1);
						cycle=1;
						break;
			};
			break;

		// huh...????
		case 0x9F : // AXI/SHA $00		(A) /\ (X) /\ (PCH+1) vagy 0000,X ki tudja???
			switch (cycle++) {
				case 2: PC++;
						break;
				case 3: nextins+=Y;
						break;
				case 4: mem->wrt(nextins,AC&X&((PC>>4)+1));
						cycle=1;
						break;
			};
			break;

		case 0xA3 : // LDT/LAX ($00,X)
			switch (cycle++) {
				case 2: PC++;
						break;
				case 3: nextins+=X;
						break;
				case 4: ptr=mem->read(nextins);
						break;
				case 5: ptr|=mem->read(nextins+1)<<8;
						break;
				case 6: X=AC=mem->read(ptr);
						setZN(X);
						cycle=1;
						break;
			};
			break;

		case 0xA7 : // LDT/LAX $00
			switch (cycle++) {
				case 2: PC++;
						break;
				case 3: ptr=nextins;
						break;
				case 4: X=AC=mem->read(ptr);
						setZN(X);
						cycle=1;
						break;
			};
			break;

		case 0xAB : // ANX/LXA #$00
			switch (cycle++) {
				case 2: PC++;
						X=AC=(nextins&(AC|0xEE));
						setZN(AC);
						//((X&0x08) & (nextins&0x08)) ? X|=0x08 : X&=0xF7;
						//((AC&0x08) & (nextins&0x08)) ? AC|=0x08 : AC&=0xF7;
						cycle=1;
						break;
			};
			break;

		case 0xAF : // LDT/LAX $0000
			switch (cycle++) {
				case 2: PC++;
						break;
				case 3: PC++;
						break;
				case 4: X=AC=mem->read(ptr);
						setZN(X);
						cycle=1;
						break;
			};
			break;

		case 0xB3 : // LDT/LAX ($00),Y
			switch (cycle++) {
				case 2: PC++;
						break;
				case 3: ptr=mem->read(nextins);
						break;
				case 4: ptr|=mem->read(nextins+1)<<8;
						break;
				case 5: if ((ptr&0x00FF)+Y<0x100) {
							X=AC=mem->read(ptr+Y);
							setZN(X);
							cycle=1;
						}
						break;
				case 6: X=AC=mem->read(ptr+Y);
						setZN(X);
						cycle=1;
						break;
			};
			break;

		case 0xB7 : // LDT/LAX $00,X
			switch (cycle++) {
				case 2: PC++;
						nextins+=X;
						break;
				case 3: X=AC=mem->read(nextins);
						break;
				case 4: setZN(X);
						cycle=1;
						break;
			};
			break;

		case 0xBB : // LAE/TSA Stack-Pointer AND with memory, TSX, TXA
			switch (cycle++) {
				case 2: PC++;
						break;
				case 3: PC++;
						ptr+=Y;
						break;
				case 4: if (nextins+Y<0x100) {
							SP&=mem->read(ptr);
							AC=X=SP;
							setZN(X);
							cycle=1;
						}
						break;
				case 5: SP&=mem->read(ptr);
						AC=X=SP;
						setZN(X);
						cycle=1;
						break;
			};
			break;

		case 0xBF : // LDT/LAX $0000,Y
			switch (cycle++) {
				case 2: PC++;
						break;
				case 3: ptr+=Y;
						PC++;
						break;
				case 4: if (nextins+Y<0x100) {
							AC=X=mem->read(ptr);
							setZN(AC);
							cycle=1;
						}
						break;
				case 5: AC=X=mem->read(ptr);
						setZN(AC);
						cycle=1;
						break;
			};
			break;

		case 0xC2 : // NOP #$00
			switch (cycle++) {
				case 2: PC++;
						cycle=1;
						break;
			};
			break;

		case 0xC3 : // DEM/DCP ($00,X)
			switch (cycle++) {
				case 2: PC++;
						break;
				case 3: nextins+=X;
						break;
				case 4: ptr=mem->read(nextins);
						break;
				case 5: ptr|=(mem->read(nextins+1)<<8);
						break;
				case 6: nextins=mem->read(ptr)-1;
						mem->wrt(ptr,nextins);
						(AC>=nextins) ? ST|=0x01: ST&=0xFE;
						break;
				case 7: if ((ptr&0xFF)+X<0x100) {
							farins=AC-nextins;
							setZN(farins);
							cycle=1;
						}
						break;
				case 8: farins=AC-nextins;
						setZN(farins);
						cycle=1;
						break;
			};
			break;

		case 0xC7 : // DEM/DCP $00
			switch (cycle++) {
				case 2: PC++;
						ptr=nextins;
						break;
				case 3: mem->wrt(ptr,mem->read(ptr)-1);
						break;
				case 4: nextins=mem->read(ptr);
						(AC>=nextins) ? ST|=0x01 : ST&=0xFE;
						break;
				case 5:	nextins=AC-nextins;
						setZN(nextins);
						cycle=1;
						break;
			};
			break;

		case 0xCB : // XAS/SBX -	X <- (X)/\(A) - M
			switch (cycle++) {
				case 2: PC++;
						X=(X&AC);
						(X>=nextins) ? ST|=0x01 : ST&=0xFE;
						nextins=(X-nextins);
						setZN(nextins);
						cycle=1;
						break;
			};
			break;

		case 0xCF : // DEM/DCP $0000
			switch (cycle++) {
				case 2: PC++;
						break;
				case 3: PC++;
						break;
				case 4: farins=mem->read(ptr)-1;
						break;
				case 5: mem->wrt(ptr,farins);
						break;
				case 6: (AC>=farins) ? ST|=0x01 : ST&=0xFE;
						farins=AC-farins;
						setZN(farins);
						cycle=1;
						break;
			};
			break;

		case 0xD3 : // DEM/DCP ($00),Y - DEC memory, CMP memory
			switch (cycle++) {
				case 2: PC++;
						break;
				case 3: ptr=mem->read(nextins);
						break;
				case 4: ptr|=mem->read(nextins+1)<<8;
						break;
				case 5: farins=mem->read(ptr+Y)-1;
						break;
				case 6: mem->wrt(ptr+Y,farins);
						break;
				case 7: (AC>=farins) ? ST|=0x01 : ST&=0xFE;
						break;
				case 8: if ((ptr&0x00FF)+Y<0x100) {
							nextins=AC-farins;
							setZN(nextins);
							cycle=1;
						}
						break;
				case 9: nextins=AC-farins;
						setZN(nextins);
						cycle=1;
						break;
			};
			break;

		case 0xD4 : // NOP #$00
			switch (cycle++) {
				case 2: PC++;
						break;
				case 3: break;
				case 4: cycle=1;
						break;
			};
			break;

		case 0xD7 : // DEM/DCP $00,Y -	M <- (M)-1, (A-M) -> NZC
			switch (cycle++) {
				case 2: PC++;
						break;
				case 3: nextins+=Y;
						ptr=nextins;
						break;
				case 4: nextins=mem->read(ptr)-1;
						mem->wrt(ptr,nextins);
						break;
				case 5: (AC>=nextins) ? ST|=0x01 : ST&=0xFE;
						nextins=AC-nextins;
						break;
				case 6: setZN(nextins);
						cycle=1;
						break;
			};
			break;

		case 0xDA : // NOP
			switch (cycle++) {
				case 2: cycle=1;
						break;
			};
			break;

		case 0xDB : // DEM/DCP $0000,Y : M <- (M)-1, (A-M) -> NZC
			switch (cycle++) {
				case 2: PC++;
						break;
				case 3: ptr+=Y;
						PC++;
						break;
				case 4: farins=mem->read(ptr)-1;
						break;
				case 5: mem->wrt(ptr,farins);
						break;
				case 6: (AC>=farins) ? ST|=0x01 : ST&=0xFE;
						farins=AC-farins;
						setZN(farins);
						break;
				case 7: if (nextins+Y<0x100)
							cycle=1;
						break;
				case 8: cycle=1;
						break;
			};
			break;

		case 0xDC : // NOP $0000,X
			switch (cycle++) {
				case 2: PC++;
						break;
				case 3: PC++;
						break;
				case 4: if (nextins+X<0x100)
							cycle=1;
						break;
				case 5: cycle=1;
						break;
			};
			break;

		case 0xDF : // DEM/DCP $0000,X : M <- (M)-1, (A-M) -> NZC
			switch (cycle++) {
				case 2: PC++;
						break;
				case 3: ptr+=X;
						PC++;
						break;
				case 4: farins=mem->read(ptr)-1;
						break;
				case 5: mem->wrt(ptr,farins);
						break;
				case 6: (AC>=farins) ? ST|=0x01 : ST&=0xFE;
						farins=AC-farins;
						setZN(farins);
						break;
				case 7: if (nextins+X<0x100)
							cycle=1;
						break;
				case 8: cycle=1;
						break;
			};
			break;

		case 0xE2 : // NOP ($B0,X)
			switch (cycle++) {
				case 2: PC++;
						cycle=1;
						break;
			};
			break;

		case 0xE3 : // INB/ISB ($00,X) -	M <- (M) + 1, A <- (A) - M - C
			switch (cycle++) {
				case 2: PC++;
						break;
				case 3: nextins+=X;
						break;
				case 4: ptr=mem->read(nextins);
						break;
				case 5: ptr|=(mem->read(nextins+1)<<8);
						break;
				case 6: nextins=mem->read(ptr)+1;
						mem->wrt(ptr,nextins);
						break;
				case 7: AC-=nextins;
						setZN(AC);
						break;
				case 8: cycle=1;
						break;
			};
			break;

		case 0xE7 : // INB/ISB $00 -	M <- (M) + 1, A <- (A) - M - C
			switch (cycle++) {
				case 2: PC++;
						ptr=nextins;
						break;
				case 3: nextins=mem->read(ptr)+1;
						mem->wrt(ptr,nextins);
						(AC>=nextins) ? ST|=0x01 : ST&=0xFE;
						break;
				case 4: //SBC(nextins);
						AC-=nextins;
						setZN(AC);
						cycle=1;
						break;
			};
			break;

		case 0xEB : // SBC #$00 - illegal version
			switch (cycle++) {
				case 2: PC++;
						SBC(nextins);
						cycle=1;
						break;
			};
			break;

		case 0xEF : // INB/ISB $0000
			switch (cycle++) {
				case 2: PC++;
						break;
				case 3: PC++;
						break;
				case 4: farins=mem->read(ptr)+1;
						(AC>=farins) ? ST|=0x01 : ST&=0xFE;
						break;
				case 5: mem->wrt(ptr,farins);
						break;
				case 6: AC-=farins;
						setZN(AC);
						cycle=1;
						break;
			};
			break;

		case 0xF3 : // INB/ISB ($00),Y - increase and subtract from AC
			switch (cycle++) {
				case 2: PC++;
						break;
				case 3: ptr=mem->read(nextins);
						break;
				case 4: ptr|=mem->read(nextins+1)<<8;
						break;
				case 5: farins=mem->read(ptr+Y)+1;
						(AC>=farins) ? ST|=0x01 : ST&=0xFE;
						break;
				case 6: mem->wrt(ptr+Y,farins);
						break;
				case 7: break;
				case 8: if (nextins+Y<0x100) {
							//SBC(nextins);
							AC-=farins;
							setZN(AC);
							cycle=1;
						}
						break;
				case 9: //SBC(nextins);
						AC-=farins;
						setZN(AC);
						cycle=1;
						break;
			};
			break;

		case 0xF4 : // NOP $60,X
			switch (cycle++) {
				case 2: PC++;
						break;
				case 3: break;
				case 4: cycle=1;
						break;
			};
			break;

		case 0xF7 : // INB/ISB $00,X -	M <- (M) + 1, A <- (A) - M - C
			switch (cycle++) {
				case 2: PC++;
						break;
				case 3: nextins+=X;
						break;
				case 4: ptr=nextins;
						break;
				case 5: nextins=mem->read(ptr)+1;
						(AC>=nextins) ? ST|=0x01 : ST&=0xFE;
						mem->wrt(ptr,nextins);
						break;
				case 6: AC-=nextins;
						setZN(AC);
						cycle=1;
						break;
			};
			break;

		case 0xFA : // NOP
			switch (cycle++) {
				case 2: cycle=1;
						break;
			};
			break;

		case 0xFB : // INB/ISB $0000,Y - increase and subtract from AC
			switch (cycle++) {
				case 2: PC++;
						break;
				case 3: ptr+=Y;
						PC++;
						break;
				case 4: farins=mem->read(ptr)+1;
						(AC>=farins) ? ST|=0x01 : ST&=0xFE;
						break;
				case 5: mem->wrt(ptr,farins);
						break;
				case 6: break;
				case 7: if (nextins+Y<0x100) {
							AC-=farins;
							setZN(AC);
							cycle=1;
						}
						break;
				case 8: AC-=farins;
						setZN(AC);
						cycle=1;
						break;
			};
			break;

		case 0xFC : // NOP $0000,X
			switch (cycle++) {
				case 2: PC++;
						break;
				case 3: PC++;
						break;
				case 4: if (nextins+X<0x100)
							cycle=1;
						break;
				case 5: cycle=1;
						break;
			};
			break;

		case 0xFF : // INB/ISB $0000,X - increase and subtract from AC
			switch (cycle++) {
				case 2: PC++;
						break;
				case 3: ptr+=X;
						PC++;
						break;
				case 4: farins=mem->read(ptr)+1;
						break;
				case 5: mem->wrt(ptr,farins);
						break;
				case 6: (AC>=farins) ? ST|=0x01 : ST&=0xFE;
						break;
				case 7: if (nextins+X<0x100) {
							AC-=farins;
							setZN(AC);
							cycle=1;
						}
						break;
				case 8: AC-=farins;
						setZN(AC);
						cycle=1;
						break;
			};
			break;

		case 0x02 : // ABS, JAM, KIL or whatever - jams the machine
		case 0x12 : // ABS, JAM, KIL or whatever - jams the machine
		case 0x22 : // ABS/JAM or what?
		case 0x32 : // ABS/JAM or what?
		case 0x42 : // ABS, JAM, KIL or whatever - jams the machine
		case 0x52 : // ABS, JAM, KIL or whatever - jams the machine
		case 0x62 : // ABS, JAM, KIL or whatever - jams the machine
		case 0x72 : // ABS, JAM, KIL or whatever - jams the machine
		case 0x92 : // ABS, JAM, KIL or whatever - jams the machine
		case 0xB2 : // ABS, JAM, KIL or whatever - jams the machine
		case 0xD2 : // ABS, JAM, KIL or whatever - jams the machine
		case 0xF2 : // ABS, JAM, KIL or whatever - jams the machine
			switch (cycle++) {
				case 2: break;
			};
			break;

//		default : cycle=1;
		};
};

// in these cycles, only write operations are allowed for the CPU
void CPU::stopcycle()
{
	switch (currins) {

		case 0x0E : // ASL $0000
			switch (cycle) {
				case 5:	mem->wrt(ptr,nextins);
						(nextins&0x80) ? ST|=0x01 : ST&=0xFE; // the Carry flag
						nextins<<=1;
						++cycle;
						break;
				case 6:	mem->wrt(ptr,nextins);
						setZN(nextins);
						cycle=1;
						break;
			};
			break;

		case 0x1E : // ASL $0000,X
			switch (cycle) {
				case 6:	mem->wrt(ptr,nextins);
						(nextins&0x80) ? ST|=0x01 : ST&=0xFE; // the Carry flag
						nextins<<=1;
						++cycle;
						break;
				case 7: mem->wrt(ptr,nextins);
						setZN(nextins);
						cycle=1;
						break;
			};
			break;

		case 0x2E : // ROL $0000
			switch (cycle) {
				case 5:	mem->wrt(ptr,farins);
						(farins&0x80) ? ST|=0x01 : ST&=0xFE; // the Carry flag
						++cycle;
						break;
				case 6:	mem->wrt(ptr,nextins);
						setZN(nextins);
						cycle=1;
						break;
			};
			break;

		case 0x3E : // ROL $0000,X
			switch (cycle) {
				case 6:	mem->wrt(ptr,farins);
						++cycle;
						break;
				case 7: mem->wrt(ptr,nextins);
						setZN(nextins);
						cycle=1;
						break;
			};
			break;

		case 0x4E : // LSR $0000
			switch (cycle) {
				case 5:	mem->wrt(ptr,nextins);
						nextins=nextins>>1;
						++cycle;
						break;
				case 6:	mem->wrt(ptr,nextins);
						setZN(nextins);
						cycle=1;
						break;
			};
			break;

		case 0x5E : // LSR $0000,X
			switch (cycle) {
				case 6:	mem->wrt(ptr,nextins);
						nextins>>=1;
						++cycle;
						break;
				case 7: mem->wrt(ptr,nextins);
						setZN(nextins);
						cycle=1;
						break;
			};
			break;

		case 0x6E : // ROR $0000
			switch (cycle) {
				case 5: mem->wrt(ptr,farins);
						nextins=(farins>>1)|((ST&0x01)<<7);
						(farins&0x01) ? ST|=0x01 : ST&=0xFE; // the Carry flag
						++cycle;
						break;
				case 6: mem->wrt(ptr,nextins);
						setZN(nextins);
						cycle=1;
						break;
			};
			break;

		case 0x7E : // ROR $0000,X
			switch (cycle) {
				case 6: mem->wrt(ptr,farins);
						++cycle;
						break;
				case 7: mem->wrt(ptr,nextins);
						setZN(nextins);
						cycle=1;
						break;
			};
			break;

		case 0x06 : // ASL $00
			switch (cycle) {
				case 4: mem->wrt(nextins,farins);
						farins<<=1;
						++cycle;
						break;
				case 5: mem->wrt(nextins,farins);
						setZN(farins);
						cycle=1;
						break;
			};
			break;

		case 0x16 : // ASL $00,X
			switch (cycle) {
				case 5: mem->wrt(nextins,farins);
						(farins)&0x80 ? ST|=0x01 : ST&=0xFE;
						farins<<=1;
						++cycle;
						break;
				case 6: mem->wrt(nextins,farins);
						setZN(farins);
						cycle=1;
						break;
			};
			break;

		case 0x36 : // ROL $00,X
			switch (cycle) {
				case 5: mem->wrt(ptr,farins);
						nextins=(farins<<1)|((ST&0x01));
						farins&0x80 ? ST|=0x01 : ST&=0xFE;
						++cycle;
						break;
				case 6: mem->wrt(ptr,nextins);
						setZN(nextins);
						cycle=1;
						break;
			};
			break;

		case 0x46 : // LSR $00
			switch (cycle) {
				case 4: mem->wrt(nextins,farins);
						farins>>=1;
						++cycle;
						break;
				case 5: mem->wrt(nextins,farins);
						setZN(farins);
						cycle=1;
						break;
			};
			break;

		case 0x26 : // ROL $00
			switch (cycle) {
				case 4: mem->wrt(ptr,farins);
						farins&0x80 ? ST|=0x01 : ST&=0xFE; // the Carry flag
						++cycle;
						break;
				case 5: mem->wrt(ptr,nextins);
						setZN(nextins);
						cycle=1;
						break;
			};
			break;

		case 0x56 : // LSR $00,X
			switch (cycle) {
				case 5: mem->wrt(nextins,farins);
						farins>>=1;
						++cycle;
						break;
				case 6: mem->wrt(nextins,farins);
						setZN(farins);
						cycle=1;
						break;
			};
			break;

		case 0x66 : // ROR $00
			switch (cycle) {
				case 4: mem->wrt(ptr,farins);
						farins&0x01 ? ST|=0x01 : ST&=0xFE; // the Carry flag
						++cycle;
						break;
				case 5: mem->wrt(ptr,nextins);
						setZN(nextins);
						cycle=1;
						break;
			};
			break;

		case 0x76 : // ROR $00,X
			switch (cycle) {
				case 5: mem->wrt(ptr,farins);
						farins&0x01 ? ST|=0x01 : ST&=0xFE; // the Carry flag
						break;
				case 6: mem->wrt(ptr,nextins);
						setZN(nextins);
						cycle=1;
						break;
			};
			break;

		// indexed indirect

		case 0x81 : // STA ($00,X)
			switch (cycle) {
				case 6: mem->wrt(ptr,AC);
						cycle=1;
						break;
			};
			break;

		case 0x84 : // STY $00
			switch (cycle) {
				case 3: mem->wrt(nextins,Y);
						cycle=1;
						break;
			};
			break;

		case 0x85 : // STA $00
			switch (cycle) {
				case 3: mem->wrt(nextins,AC);
						cycle=1;
						break;
			};
			break;

		case 0x86 : // STX $00
			switch (cycle) {
				case 3: mem->wrt(nextins,X);
						cycle=1;
						break;
			};
			break;

		case 0x8C : // STY $0000
			switch (cycle) {
				case 4: mem->wrt(ptr,Y);
						cycle=1;
						break;
			};
			break;

		case 0x8D : // STA $0000
			switch (cycle) {
				case 4: mem->wrt(ptr,AC);
						cycle=1;
						break;
			};
			break;

		case 0x8E : // STX $0000
			switch (cycle) {
				case 4: mem->wrt(ptr,X);
						cycle=1;
						break;
			};
			break;

		// indirect indexed

		case 0x91 : // STA ($00),Y
			switch (cycle) {
				case 5: if ((ptr&0x00FF)+Y<0x100) {
							mem->wrt(ptr+Y,AC);
							cycle=0;
						}
						++cycle;
						break;
				case 6: mem->wrt(ptr+Y,AC);
						cycle=1;
						break;
			};
			break;

		// zero page indexed with X or Y

		case 0x94 : // STY $00,X
			switch (cycle) {
				case 4: mem->wrt(nextins,Y);
						cycle=1;
						break;
			};
			break;

		case 0x95 : // STA $00,X
			switch (cycle) {
				case 4: mem->wrt(nextins,AC);
						cycle=1;
						break;
			};
			break;

		case 0x96 : // STX $00,Y
			switch (cycle) {
				case 4: mem->wrt(nextins,X);
						cycle=1;
						break;
			};
			break;

		case 0x99 : // STA $0000,Y
			switch (cycle) {
				case 5: mem->wrt(ptr,AC);
						cycle=1;
						break;
			};
			break;

		case 0x9D : // STA $0000,X
			switch (cycle) {
				case 5: mem->wrt(ptr,AC);
						cycle=1;
						break;
			};
			break;

		case 0xC6 : // DEC $00
			switch (cycle) {
				case 4: mem->wrt(nextins,farins);
						--farins;
						++cycle;
						break;
				case 5: mem->wrt(nextins,farins);
						setZN(farins);
						cycle=1;
						break;
			};
			break;

		case 0xCE : // DEC $0000
			switch (cycle) {
				case 5: mem->wrt(ptr,nextins);
						--nextins;
						++cycle;
						break;
				case 6: mem->wrt(ptr,nextins);
						setZN(nextins);
						cycle=1;
						break;
			};
			break;

		case 0xD6 : // DEC $00,X
			switch (cycle) {
				case 5: mem->wrt(nextins,farins);
						--farins;
						++cycle;
						break;
				case 6: mem->wrt(nextins,farins);
						setZN(farins);
						cycle=1;
						break;
			};
			break;

		case 0xDE : // DEC $0000,X
			switch (cycle) {
				case 6: mem->wrt(ptr,nextins);
						--nextins;
						++cycle;
						break;
				case 7: mem->wrt(ptr,nextins);
						setZN(nextins);
						cycle=1;
						break;
			};
			break;

		case 0xE6 : // INC $00
			switch (cycle) {
				case 4: mem->wrt(nextins,farins);
						++farins;
						++cycle;
						break;
				case 5: mem->wrt(nextins,farins);
						setZN(farins);
						cycle=1;
						break;
			};
			break;

		case 0xEE : // INC $0000
			switch (cycle) {
				case 5: mem->wrt(ptr,nextins);
						++nextins;
						++cycle;
						break;
				case 6: mem->wrt(ptr,nextins);
						setZN(nextins);
						cycle=1;
						break;
			};
			break;

		case 0xF6 : // INC $00,X
			switch (cycle) {
				case 5: mem->wrt(nextins,farins);
						++farins;
						++cycle;
						break;
				case 6: mem->wrt(nextins,farins);
						setZN(farins);
						cycle=1;
						break;
			};
			break;

		case 0xFE : // INC $0000,X
			switch (cycle) {
				case 6: mem->wrt(ptr,nextins);
						++nextins;
						++cycle;
						break;
				case 7: mem->wrt(ptr,nextins);
						setZN(nextins);
						cycle=1;
						break;
			};
			break;

	}
}

CPU::~CPU()
{
	delete mem;
}

