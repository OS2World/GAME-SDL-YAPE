/*
	YAPE - Yet Another Plus/4 Emulator

	The program emulates the Commodore 264 family of 8 bit microcomputers

	This program is free software, you are welcome to distribute it,
	and/or modify it under certain conditions. For more information,
	read 'Copying'.

	(c) 2000, 2001 Attila Grósz
*/

#define TITLE   "YAPELIN 0.32"
#define NAME    "Yape for SDL 0.32.3"

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include "SDL/SDL.h"

#include "keyboard.h"
#include "cpu.h"
#include "tedmem.h"
#include "tape.h"
#include "sound.h"
#include "archdep.h"

// function prototypes
extern inline void FrameUpdate(void);

