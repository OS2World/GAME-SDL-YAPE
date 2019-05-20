/*
	YAPE - Yet Another Plus/4 Emulator

	The program emulates the Commodore 264 family of 8 bit microcomputers

	This program is free software, you are welcome to distribute it,
	and/or modify it under certain conditions. For more information,
	read 'Copying'.

	(c) 2000, 2001 Attila Grósz
*/
#ifndef _DISKIO_H
#define _DISKIO_H

#define IEC_IDLE        0
#define IEC_INPUT       1
#define IEC_DEVICEID    2
#define IEC_OUTPUT      3

#define D64_MAX_TRACKS  35


// Prototypes
void			InterpretInputBuffer_PCDir(void);
void			InterpretInputBuffer_D64File(void);
void			InterpretInputBuffer(void);
unsigned char	ReadParallelIEC(unsigned short addr);
void			WriteParallelIEC(unsigned short addr,unsigned char data);
int				CalcOffset(int track, int sector);
int				LoadD64Image(char *filename);
void			DetachD64Image(void);
void			diskio_set_drive_dir(void *hWnd);

#endif _DISKIO_H
