/*
	YAPE - Yet Another Plus/4 Emulator

	The program emulates the Commodore 264 family of 8 bit microcomputers

	This program is free software, you are welcome to distribute it,
	and/or modify it under certain conditions. For more information,
	read 'Copying'.

	(c) 2000, 2001 Attila Grósz
*/
#ifndef _TAPE_H
#define _TAPE_H

class MEM;
class TAP {
	private:
		int		TapeFileSize;
		unsigned char	*pHeader;

		int tapedelay;
		int origlength, halflength;
		// indicates if we started the TAP process
		bool inwave;
		char buf;
		// this indicates the second half of a whole sinus wave
		bool sinus;
		void read_whole_wave();
		void read_half_wave();
		void read_sample();
		void write_whole_wave();
		void write_half_wave();
		void (TAP::*read_decode)();
		void (TAP::*write_encode)();

	public:
		TAP::TAP();
		MEM *mem;
		char tapefilename[256];
		bool attach_tap();
		bool create_tap();
		bool detach_tap();
		void stop();
		void rewind();
		void read() { (this->*read_decode)(); };
		void write() { (this->*write_encode)(); };
		void changewave(bool wholewave);
		int	 TapeSoFar;
};


#endif // _TAPE_H
