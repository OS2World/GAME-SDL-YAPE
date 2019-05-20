/*
	YAPE - Yet Another Plus/4 Emulator

	The program emulates the Commodore 264 family of 8 bit microcomputers

	This program is free software, you are welcome to distribute it,
	and/or modify it under certain conditions. For more information,
	read 'Copying'.

	(c) 2000, 2001 Attila Grósz
*/
#ifndef _CPU_H
#define _CPU_H

class MEM;

class CPU {
	private:
		unsigned char currins;
		unsigned char nextins;
		unsigned char farins;
		unsigned int  ptr;
		unsigned int  PC;
		unsigned char  SP;
						   // 76543210
		unsigned char  ST; // NV1BDIZC
		unsigned char  AC;
		unsigned char  X;
		unsigned char  Y;
		void ADC(unsigned char value);
		void SBC(unsigned char value);
		/*void push(unsigned char value);
		unsigned char pull();*/
		int cycle;
		bool startIRQ;
		int	IRQcount;

	public:
		CPU();
		~CPU();
		MEM *mem;
		void reset(void);
		void softreset(void);
		void debugreset(void);
		void setPC(unsigned int addr);
		unsigned int getPC() { return PC; };
		unsigned int getSP() { return SP; };
		unsigned int getST() { return ST; };
		unsigned int getAC() { return AC; };
		unsigned int getX() { return X; };
		unsigned int getY() { return Y; };
		unsigned int getcins() { return currins; };
		unsigned int getnextins() { return nextins; };
		unsigned int getptr() { return ptr; };
		int getcycle() { return cycle; };
		void process();
		void stopcycle();
		bool saveshot();
		bool loadshot();
		// breakpoint variables
		bool bp_active;
		bool bp_reached;
		unsigned int bp_address;
};

typedef struct INSTR
{
  char name[4];
  int  type;
  int  cycles;
  unsigned char ext;
} instructions;

/*
   types :   1 - one-unsigned char instruction - eg.: nop
             2 - two-unsigned char instruction eg.: lda #$20
             3 - three-unsigned char instruction eg.: jmp $2000
             4 - three-unsigned char indirect instruction eg.: jmp ($2000)
             5 - two-unsigned char zero slap eg.: lda $50
             6 - two-unsigned char zero slap X indexed eg.: lda $60,x
             7 - two-unsigned char zero slap Y indexed eg.: ldx $70,y
             8 - three-unsigned char X indexed eg.: sta $8000,x
             9 - three-unsigned char Y indexed eg.: sta $8000,y
            10 - two-unsigned char zero slap Y, indexed eg.: lda ($a0),Y
            11 - two-unsigned char zero slap X, indexed eg.: lda ($b0,X)
            12 - two-unsigned char relative jump eg.: beq $c000
    ext :   0 - regular assembler instruction mnemonic
    		1 - extended assembler instruction mnemonic
    		2 - not an assembly instruction
*/

const int typlen[13]={0,1,2,3,3,2,2,2,3,3,2,2,2};

/* 6510,7510 assembler instructions */
const instructions ins[256] = { {"BRK",1,7,0},        // 00 - BRK
                          {"ORA",11,6,0},       // 01 - ORA ($B0,X)
                          {"ABS",1,2,1},        //*02 - ABS
                          {"ASO",11,8,1},       //*03 - ASO ($B0,X)
                          {"NO2",5,3,1},        //*04 - NO2 $50
                          {"ORA",5,3,0  },      // 05 - ORA $50
                          {"ASL",5,5,0 },     // 06 - ASL $50
                          {"ASO",5,5,1},       //*07 - ASO $50
                          {"PHP",1,3,0},      // 08
                          {"ORA",2,2,0},      // 09 - ORA #$20
                          {"ASL",1,2,0},      // 0A - ASL
                          {"ANN",2,2,1 },       //*0B - ANN #$20
                          {"NO3",3,4,1 },       //*0C - NO3 $3000
                          {"ORA",3,4,0  },     // 0D - ORA $3000
                          {"ASL",3,6,0  },      // 0E - ASL $3000
                          {"ASO",3,6,1 },       //*0F - ASO $3000

                          {"BPL",12,4,0 },      // 10 - BPL $C000 / add +1 if branch on same page, +2 othervise
                          {"ORA",10,6,0 },      // 11 - ORA ($A0),Y
                          {"ABS",1,2,1 },       //*12 - ABS
                          {"ASO",10,9,1},       //*13 - ASO ($A0),Y
                          {"NO2",6,4,1 },      //* 14 - NO2 $60,X
                          {"ORA",6,4,0},      // 15 - ORA $60,X
                          {"ASL",6,6,0  },      // 16 - ASL $60,X
                          {"ASO",6,6,1  },      //*17 - ASO $60,X
                          {"CLC",1,2,0   },     // 18 - CLC
                          {"ORA",9,5,0   },     // 19 - ORA $9000,Y / Add +1 on page crossing
                          {"NO1",1,2,1  },      //*1A - NO1
                          {"ASO",9,8,1  },      //*1B - ASO $9000,Y
                          {"NO3",8,5,1  },      //*1C - NO3 $8000,X
                          {"ORA",8,5,0   },     // 1D - ORA $8000,X / Add +1 on page crossing
                          {"ASL",8,7,0   },     // 1E - ASL $8000,X
                          {"ASO",8,8,1  },      //*1F - ASO $8000,X

                          {"JSR",3,6,0  } ,     // 20 - JSR $3000
                          {"AND",11,6,0 } ,    // 21 - AND ($B0,X)
                          {"ABS",1,2,1 } ,      //*22 - ABS
                          {"RAN",11,8,1},       //*23 - RAN ($B0,X)
                          {"BIT",5,3,0  },      // 24 - BIT $50
                          {"AND",5,3,0  },      // 25 - AND $50
                          {"ROL",5,5,0  },      // 26 - ROL $50
                          {"RAN",5,5,1 },       //*27 - RAN $50
                          {"PLP",1,4,0  },      // 28 - PLP
                          {"AND",2,2,0  },      // 29 - AND #$20
                          {"ROL",1,2,0  },      // 2A - ROL
                          {"ANN",2,2,1 },       //*2B - ANN #$20
                          {"BIT",3,4,0  },      // 2C - BIT $3000
                          {"AND",3,4,0  },      // 2D - AND $3000
                          {"ROL",3,6,0  },      // 2E - ROL $3000
                          {"RAN",3,6,1 },       //*2F - RAN $3000

                          {"BMI",12,4,0},       // 30 - BPL $C000 / add +1 if branch on same page, +2 othervise
                          {"AND",10,6,0},       // 31 - AND ($A0),Y
                          {"ABS",1,2,1},        //*32 - ABS
                          {"RAN",10,9,1},       //*33 - RAN ($A0),Y
                          {"NO2",6,4,1},        //*34 - NO2 $60,X
                          {"AND",6,4,0},        // 35 - AND $60,X
                          {"ROL",6,6,0},        // 36 - ROL $60,X
                          {"RAN",6,6,1},        //*37 - RAN $60,X
                          {"SEC",1,2,0},        // 38 - SEC
                          {"AND",9,5,0},        // 39 - AND $9000,Y / add +1 if page boundary is crossed
                          {"NO1",1,2,1},       //*3A - NO1
                          {"RAN",9,8,1},        //*3B - RAN $9000,Y
                          {"NO3",8,5,1},        //*3C - NO3 $8000,X
                          {"AND",8,5,0},        // 3D - AND $8000,X / add +1 if page boundary is crossed
                          {"ROL",8,7,0},        // 3E - ROL $8000,X
                          {"RAN",8,8,1},        //*3F - RAN $8000,X

                          {"RTI",1,6,0},        // 40 - RTI
                          {"EOR",11,6,0},       // 41 - EOR ($B0,X)
                          {"ABS",1,2,1},        //*42 - ABS
                          {"LSE",11,8,1},       //*43 - LSE ($B0,X)
                          {"NO2",5,3,1},        //*44 - NO2 $50
                          {"EOR",5,3,0},        // 45 - EOR $50
                          {"LSR",5,5,0},        // 46 - LSR $50
                          {"LSE",5,5,1},        //*47 - LSE $50
                          {"PHA",1,3,0},        // 48 - PHA
                          {"EOR",2,2,0},        // 49 - EOR #$20
                          {"LSR",1,2,0},        // 4A - LSR
                          {"ANL",2,2,1},        //*4B - ANL #$20
                          {"JMP",3,3,0},        // 4C - JMP $3000
                          {"EOR",3,4,0},        // 4D - EOR $3000
                          {"LSR",3,6,0},        // 4E - LSR $3000
                          {"LSE",3,6,1},        //*4F - LSE $3000

                          {"BVC",12,4,0},       // 50 - BVC $C000 / add +1 if branch on same page, +2 othervise
                          {"EOR",10,6,0},       // 51 - EOR ($A0),Y / add +1 if page boundary is crossed
                          {"ABS",1,2, 1},       //*52 - ABS
                          {"LSE",10,9,1},       //*53 - LSE ($A0),Y
                          {"NO2",6,4,1},        //*54 - NO2 $60,X
                          {"EOR",6,4,0},        // 55 - EOR $60,X
                          {"LSR",6,6,0},        // 56 - LSR $60,X
                          {"LSE",6,6,1},        //*57 - LSE $60,X
                          {"CLI",1,2,0},        // 58 - CLI
                          {"EOR",9,5,0},        // 59 - EOR $9000,Y / add +1 if page boundary is crossed
                          {"NO1",1,2,1},        //*5A - NO1
                          {"LSE",9,8,1},        //*5B - LSE $9000,Y
                          {"NO3",8,5,1},        //*5C - NO3 $8000,X
                          {"EOR",8,5,0},        // 5D - EOR $8000,X / add +1 if page boundary is crossed
                          {"LSR",8,7,0},        // 5E - LSR $8000,X
                          {"LSE",8,8,1},        //*5F - LSE $8000,X

                          {"RTS",1,6,0},        // 60 - RTS
                          {"ADC",11,6,0},       // 61 - ADC ($B0,X)
                          {"ABS",1,2,1},        //*62 - ABS
                          {"RAD",11,8,1},       //*63 - RAD ($B0,X)
                          {"NO2",5,3, 1},       //*64 - NO2 $50
                          {"ADC",5,3,0},        // 65 - ADC $50
                          {"ROR",5,5,0},        // 66 - ROR $50
                          {"RAD",5,5,1},        //*67 - RAD $50
                          {"PLA",1,4,0},        // 68 - PLA
                          {"ADC",2,2,0},        // 69 - ADC #$20
                          {"ROR",1,2,0},        // 6A - ROR
                          {"???",1,0,2},        // 6B
                          {"JMP",4,5,0},        // 6C - JMP ($4000)
                          {"ADC",3,4,0},        // 6D - ADC $3000
                          {"ROR",3,6,0},        // 6E - ROR $3000
                          {"RAD",3,6,1},        //*6F - RAD $3000

                          {"BVS",12,4,0},       // 70 - BVS $C000 / add +1 if branch on same page, +2 othervise
                          {"ADC",10,6,0},       // 71 - ADC ($A0),X / add +1 if page boundary is crossed
                          {"ABS",1,2, 1},       //*72 - ABS
                          {"RAD",10,9,1},       //*73 - RAD ($A0),Y
                          {"NO2",6,4,1},        //*74 - NO2 $60,X
                          {"ADC",6,4,0},        // 75 - ADC $60,X
                          {"ROR",6,6,0},        // 76 - ROR $60,X
                          {"RAD",6,6,1},        //*77 - RAD $60,X
                          {"SEI",1,2,0},        // 78 - SEI
                          {"ADC",9,5,0},        // 79 - ADC $9000,Y / add +1 if page boundary is crossed
                          {"NO1",1,2,1},        //*7A - NO1
                          {"RAD",9,8,1},        //*7B - RAD $9000,Y
                          {"NO3",8,5,1},        //*7C - NO3 $8000,X
                          {"ADC",8,5,0},        // 7D - ADC $8000,X / add +1 if page boundary is crossed
                          {"ROR",8,7,0},        // 7E - ROR $8000,X
                          {"RAD",8,8,1},        //*7F - RAD $8000,X

                          {"NO2",11,2,1},       //*80 - NO2 ($B0,X)
                          {"STA",11,6,0},       // 81 - STA ($B0,X)
                          {"NO2",11,2,1},       //*82 - NO2 ($B0,X)
                          {"AXX",11,8,1},       //*83 - AXX ($B0,X)
                          {"STY",5,3,0},        // 84 - STY $50
                          {"STA",5,3,0},        // 85 - STA $50
                          {"STX",5,3,0},        // 86 - STX $50
                          {"AXR",5,5,1},        //*87 - AXR $50
                          {"DEY",1,2,0},        // 88 - DEY
                          {"NO2",2,2,1},        //*89 - NO2 #$20
                          {"TXA",1,2,0},        // 8A - TXA
                          {"TAN",2,2,1},        //*8B - TAN #$20
                          {"STY",3,4,0},        // 8C - STY $3000
                          {"STA",3,4,0},        // 8D - STA $3000
                          {"STX",3,4,0},        // 8E - STX $3000
                          {"AAX",3,4,1},        //*8F - AAX $3000

                          {"BCC",12,4,0},       // 90 - BNE $C000 / add +1 if branch on same page, +2 othervise
                          {"STA",10,6,0},       // 91 - STA ($A0),Y
                          {"ABS",1,2,1},        //*92 - ABS
                          {"AXI",10,7,1},       //*93 - AXI ($A0),Y - hmm????????????????
                          {"STY",6,4,0},        // 94 - STY $60,X
                          {"STA",6,4,0},        // 95 - STA $60,X
                          {"STX",7,4,0},        // 96 - STX $70,Y
                          {"AXY",7,4,1},        //*97 - AXY $70,X
                          {"TYA",1,2,0},        // 98 - TYA
                          {"STA",9,5,0},        // 99 - STA $9000,Y
                          {"TXS",1,2,0},        // 9A - TXS
                          {"AXS",9,6,1},        //*9B - AXS $9000,Y
                          {"???",1,0,0},        // 9C
                          {"STA",8,5,0},        // 9D - STA $8000,X
                          {"???",1,0,2},        // 9E
                          {"???",1,0,2},        // 9F

                          {"LDY",2,2,0},        // A0 - LDY #$20
                          {"LDA",11,6,0},       // A1 - LDA ($B0,X)
                          {"LDX",2,2,0},        // A2 - LDX #$20
                          {"LDT",11,6,1},       //*A3 - LDT ($B0,X)
                          {"LDY",5,3,0},        // A4 - LDY $50
                          {"LDA",5,3,0},        // A5 - LDA $50
                          {"LDX",5,3,0},        // A6 - LDX $50
                          {"LDT",5,3,1},        //*A7 - LDT $50
                          {"TAY",1,2,0},        // A8 - TAY
                          {"LDA",2,2,0},        // A9 - LDA #$20
                          {"TAX",1,2,0},        // AA - TAX
                          {"ANX",2,2,1},        //*AB - ANX #$20
                          {"LDY",3,4,0},        // AC - LDY $3000
                          {"LDA",3,4,0},        // AD - LDA $3000
                          {"LDX",3,4,0},        // AE - LDX $3000
                          {"LDT",3,4,1},        //*AF - LDT $3000

                          {"BCS",12,4,0},       // B0 - BCS $C000 / add +1 if branch on same page, +2 othervise
                          {"LDA",10,6,0},       // B1 - LDA ($A0),Y / add +1 if page boundary is crossed
                          {"ABS",1,2,1},        //*B2 - ABS
                          {"LDT",10,6,1},       //*B3 - LDT ($A0),Y
                          {"LDY",6,4,0},        // B4 - LDY $60,X
                          {"LDA",6,4,0},        // B5 - LDA $60,X
                          {"LDX",7,4,0},        // B6 - LDX $70,Y
                          {"LDT",7,4,1},        //*B7 - LDT $70,X
                          {"CLV",1,2,0},        // B8 - CLV
                          {"LDA",9,5,0},        // B9 - LDA $9000,X / add +1 if page boundary is crossed
                          {"TSX",1,2,0},        // BA - TSX
                          {"TSA",8,5,1},        //*BB - TSA $8000,X or Y???
                          {"LDY",8,5,0},        // BC - LDY $8000,X / add +1 if page boundary is crossed
                          {"LDA",8,5,0},        // BD - LDA $8000,X / add +1 if page boundary is crossed
                          {"LDX",9,5,0},        // BE - LDX $9000,Y / add +1 if page boundary is crossed
                          {"LDT",9,5,1},        //*BF - LDT $9000,Y

                          {"CPY",2,2,0},        // C0 - CPY #$20
                          {"CMP",11,6,0},       // C1 - CMP ($B0,X)
                          {"NO2",11,2,1},       //*C2 - NO2 ($B0,X)
                          {"DEM",11,8,1},       //*C3 - DEM ($B0,X)
                          {"CPY",5,3,0},        // C4 - CPY $50
                          {"CMP",5,3,0},        // C5 - CMP $50
                          {"DEC",5,5,0},        // C6 - DEC $50
                          {"DEM",5,5,1},        //*C7 - DEM $50
                          {"INY",1,2,0},        // C8 - INY
                          {"CMP",2,2,0},        // C9 - CMP #$20
                          {"DEX",1,2,0},        // CA - DEX
                          {"XAS",2,2,1},        //*CB - XAS #$20
                          {"CPY",3,4,0},        // CC - CPY $3000
                          {"CMP",3,4,0},        // CD - CMP $3000
                          {"DEC",3,6,0},        // CE - DEC $3000
                          {"DEM",3,6,1},        //*CF - DEM $3000

                          {"BNE",12,4,0},       // D0 - BNE $C000 / add +1 if branch on same page, +2 othervise
                          {"CMP",10,6,0},       // D1 - CMP ($A0),Y / add +1 if page boundary is crossed
                          {"ABS",1,2,1},        //*D2 - ABS
                          {"DEM",10,9,1},       //*D3 - DEM ($A0),Y
                          {"NO2",6,4,1},        //*D4 - NO2 $60,X
                          {"CMP",6,4,0},        // D5 - CMP $60,X
                          {"DEC",6,6,0},        // D6 - DEC $60,X
                          {"DEM",7,6,1},        //*D7 - DEM $70,X or Y??
                          {"CLD",1,2,0},        // D8 - CLD
                          {"CMP",9,5,0},        // D9 - CMP $9000,Y / add +1 if page boundary is crossed
                          {"NO1",1,2,1},        //*DA - NO1
                          {"DEM",9,8,0},        // DB - DEM $9000,Y
                          {"NO3",8,5,1},        //*DC - NO3 $8000,X
                          {"CMP",8,5,0},        // DD - CMP $8000,X / add +1 if page boundary is crossed
                          {"DEC",8,7,0},        // DE - DEC $8000,X
                          {"DEM",8,8,1},        //*DF - DEM $8000,X

                          {"CPX",2,2,0},        // E0 - CPX #$20
                          {"SBC",11,6,0},       // E1 - SBC ($B0,X)
                          {"NO2",11,2,1},       //*E2 - NO2 ($B0,X)
                          {"INB",11,8,1},       //*E3 - INB ($B0,X)
                          {"CPX",5,3,0},        // E4 - CPX $50
                          {"SBC",5,3,0},        // E5 - SBC $50
                          {"INC",5,5,0},        // E6 - INC $50
                          {"INB",5,5,1},        //*E7 - INB $50
                          {"INX",1,2,0},        // E8 - INX
                          {"SBC",2,2,0},        // E9 - SBC #$20
                          {"NOP",1,2,0},        // EA - NOP
                          {"SBC",2,2,1},        //*EB - SBC #$20
                          {"CPX",3,4,0},        // EC - CPX $3000
                          {"SBC",3,4,0},        // ED - SBC $3000
                          {"INC",3,6,0},        // EE - INC $3000
                          {"INB",3,6,1},        //*EF - INB $3000

                          {"BEQ",12,4,0},       // F0 - BEQ $C000 / add +1 if branch on same page, +2 othervise
                          {"SBC",10,6,0},       // F1 - SBC ($A0),Y
                          {"ABS",1,2,1},        //*F2 - ABS
                          {"INB",10,9,1},       //*F3 - INB ($A0),Y
                          {"NO2",6,4,1},        //*F4 - NO2 $60,X
                          {"SBC",6,5,0},        // F5 - SBC $60,X / add +1 if page boundary is crossed
                          {"INC",6,6,0},        // F6 - INC $60,X
                          {"INB",6,6,1},        //*F7 - INB $60,X
                          {"SED",1,2,0},        // F8 - SED
                          {"SBC",9,5,0},        // F9 - SBC $9000,Y / +1 on boundary crossing
                          {"NO1",1,2,1},        //*FA - NO1
                          {"INB",9,8,1},        //*FB - INB $9000,Y
                          {"NO3",8,5,1},        //*FC - NO3 $8000,X
                          {"SBC",8,5,0},        // FD - SBC $8000,X / add +1 if page boundary is crossed
                          {"INC",8,7,0},        // FE - INC $8000,X
                          {"INB",8,8,1},        //*FF - INB $8000,X
                        };

#endif // _CPU_H
