/*
	YAPE - Yet Another Plus/4 Emulator

	The program emulates the Commodore 264 family of 8 bit microcomputers

	This program is free software, you are welcome to distribute it,
	and/or modify it under certain conditions. For more information,
	read 'Copying'.

	(c) 2000, 2001 Attila Grósz
*/

/*
This file contains the architecture dependent function prototypes
*/

#ifndef _ARCHDEP_H
#define _ARCHDEP_H

#ifdef WIN32
#include <windows.h>
#else
#ifdef __OS2__
#define MAX_PATH 256
#else
#include <unistd.h>
#define MAX_PATH 256
#endif

int		ad_get_curr_dir(char *pathstring);
int		ad_set_curr_dir(char *path);
void	*ad_find_first_file(char *filefilter);
char	*ad_return_current_filename(void);
int		ad_find_next_file(void);

int		ad_makedirs(char *temp);
char	*ad_getinifilename(char *tmpchr);

#endif
#endif
