/*
	YAPE - Yet Another Plus/4 Emulator

	The program emulates the Commodore 264 family of 8 bit microcomputers

	This program is free software, you are welcome to distribute it,
	and/or modify it under certain conditions. For more information,
	read 'Copying'.

	(c) 2000, 2001 Attila Grósz
*/

#include "SDL/SDL.h"
#include "tedmem.h"
#include "interface.h"
#include "archdep.h"
#include <string.h>
#include <stdlib.h>

#define COLOR( COL, SHADE) (SHADE<<4)|COL

static SDL_Surface *screen, *image;



UI::UI(class MEM *ted, SDL_Surface *buf1, SDL_Surface *buf2, char *path)
{
    display = (char *) (ted->screen);
	charset = (char *) ((ted->RomHi[0])+0x1400);
	strcpy(filepath,path);

	screen = buf1;
	image = buf2;
	clear (1, 5);
	show_menu((struct MENU *) main_menu);
	curr_sel = 0;
	curr_menu = (struct MENU *) main_menu;
	show_sel_bar( (struct MENU *) main_menu, curr_sel);
        wait_events();

}

void UI::screen_update(void)
{
	SDL_BlitSurface( image, NULL , screen, NULL);
	SDL_Flip ( screen );
}

void UI::clear(char color, char shade)
{
	for (register int i=0; i<SCR_HSIZE*SCR_VSIZE; ++i)
		display[i] = COLOR( color, shade );
	screen_update();
}

void UI::set_color(char foreground, char background)
{
	bgcol = background;
	fgcol = foreground;
}

void UI::texttoscreen(int x,int y, char *scrtxt)
{
	register int i =0;

	while (scrtxt[i]!=0)
		chrtoscreen(x+i*8,y,asc2pet(scrtxt[i++]));
}

void UI::chrtoscreen(int x,int y, char scrchr)
{
	register int j, k;
	char *cset;

	/*if (isalpha(scrchr)) {
		//scrchr=toupper(scrchr)-64;
		charset+=(scrchr<<3);
		for (j=0;j<8;j++)
			for (k=0;k<8;k++)
				(*(charset+j) & (0x80>>k)) ? display[(y+j)*456+x+k]=fgcol : display[(y+j)*456+x+k]=bgcol;
		return;
	}*/
	cset = charset + (scrchr<<3);
	for ( j=0; j<8; j++)
		for (k=0;k<8;k++)
			(*(cset+j) & (0x80>>k)) ? display[(y+j)*456+x+k]=fgcol : display[(y+j)*456+x+k]=bgcol;
}

void UI::hide_sel_bar(pMenu menu, int index)
{
	set_color( COLOR(0,0), COLOR(1,5) );
	texttoscreen(100,100+(index<<4),menu[index].name);
	screen_update();
}

void UI::show_sel_bar(pMenu menu, int index)
{
	set_color( COLOR(0,0), COLOR(1,7) );
	texttoscreen(100,100+(index<<4),menu[index].name);
	screen_update();
}

void UI::show_file_list()
{
	int nf = 0, res;
	char cfilename[256];
	char **filelist;

	strcpy( cfilename, "*.prg");

	set_color( COLOR(0,0), COLOR(1,5) );
	res = (int ) ad_find_first_file((char *) cfilename);
	while(res) {
		strcpy( cfilename, (char *) ad_return_current_filename());
		//texttoscreen(100,100+(nf<<3), cfilename);
		//filelist[nf++] = (char *) malloc(sizeof(cfilename));
		res = ad_find_next_file();
		if (res) ++nf;
	}
	screen_update();
}

void UI::show_menu(pMenu menu)
{
	int i, j;

	set_color( COLOR(0,0), COLOR(1,5) );
	for (i=0; i<3; ++i)
		texttoscreen(100,100+(i<<4),menu[i].name);
	screen_update();
}

int UI::wait_events()
{
	SDL_Event event;

	while (SDL_WaitEvent(&event)) {
		switch (event.type) {
	        case SDL_KEYDOWN:
				switch (event.key.keysym.sym) {
					case SDLK_ESCAPE :
					case SDLK_F8 :
						return 0;
					case SDLK_UP :
						hide_sel_bar( curr_menu, curr_sel);
						if (curr_sel>0)
							curr_sel-=1;
						show_sel_bar( curr_menu, curr_sel);
						break;
					case SDLK_DOWN :
						hide_sel_bar( curr_menu, curr_sel);
						if (curr_sel<2)
							curr_sel+=1;
						show_sel_bar( curr_menu, curr_sel);
						break;
					case SDLK_RETURN:
						clear (1, 5);
						show_file_list( );
						break;
					default :
						break;
				};
				break;
			case SDL_QUIT:
				SDL_Quit();
				exit(0);
		}
	}
	return 1;
}

char UI::pet2asc(char c, bool map_slash)
{
	if ((c >= 'A') && (c <= 'Z') || (c >= 'a') && (c <= 'z'))
		return c ^ 0x20;
	if ((c >= 0xc1) && (c <= 0xda))
		return c ^ 0x80;
	/*if ((c == '/'))
		return '\\';*/
	return c;
}

char UI::asc2pet(char c)
{
	if ((c >= 'A') && (c <= 'Z'))
		return c ;
	if ((c >= 'a') && (c <= 'z'))
		return c - 96;
	if ((c >= 0xc1) && (c <= 0xda))
		return c ^ 0x80;
	return c;
}

UI::~UI()
{
}
