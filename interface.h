/*
	YAPE - Yet Another Plus/4 Emulator

	The program emulates the Commodore 264 family of 8 bit microcomputers

	This program is free software, you are welcome to distribute it,
	and/or modify it under certain conditions. For more information,
	read 'Copying'.

	(c) 2000, 2001 Attila Grósz
*/

#ifndef _INTERFACE_H
#define _INTERFACE_H

typedef struct MENU
{
	char *name;
	char *text;
	void *parent;
	void *menufunction;

} menu_t, *pMenu; //

const menu_t main_menu[3] = {
	{"Load PRG file", "...", 0, 0 },
	{"Options..."	, "...", 0, 0 },
	{"Quit YAPE"	, "F12", 0, 0 }
};

class MEM;

class UI {

	private:
		char *display; // pointer to the emulator screen
		char *charset; // pointer to the character ROM

		char bgcol; // background color
		char fgcol; // foreground color

		void screen_update(void);

		void clear(char color, char shade);  // clears the screen with given C= 264 color and shade
		void texttoscreen(int x,int y, char *scrtxt);
		void chrtoscreen(int x,int y, char scrchr);
		void set_color(char foreground, char background);
		int	 wait_events();

		void hide_sel_bar(pMenu menu, int index);
		void show_sel_bar(pMenu menu, int index);
		void show_menu(pMenu menu);
		void show_file_list();
		pMenu curr_menu;
		int  curr_sel;
		char filepath[512];

	public:
		UI(class MEM *ted, SDL_Surface *buf1, SDL_Surface *buf2, char *path);
		~UI();
		void load_prg(char *filename);
		char pet2asc(char c, bool map_slash);
		char asc2pet(char c);

};

/*typedef struct FILELIST {
	void	*prevfile;
	char	*currfile;
	void	*nextfile;
} filelist_t, *pFilelist;*/

#endif // _INTERFACE_H
