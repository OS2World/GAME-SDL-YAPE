/*
	YAPE - Yet Another Plus/4 Emulator

	The program emulates the Commodore 264 family of 8 bit microcomputers

	This program is free software, you are welcome to distribute it,
	and/or modify it under certain conditions. For more information,
	read 'Copying'.

	(c) 2000, 2001 Attila Grósz
*/
#ifndef _KEYBOARD_H
#define _KEYBOARD_H

class KEYS {
	private:
		unsigned char keybuffer[256];
		unsigned char joybuffer[256];
		unsigned char key_trans(unsigned char r);
		unsigned char joy_trans(unsigned char r);
	public:
		KEYS();
		~KEYS();
		void pushkey(unsigned int code);
		void releasekey(unsigned int code);
		unsigned char feedkey(unsigned char latch);
		unsigned char feedjoy(unsigned char latch);
		void joyinit(void);
		void swapjoy(void);
		void releasejoy(void);
		void empty(void);
		int activejoy;
		unsigned char joytrans[512];
};

#endif // _KEYBOARD_H
