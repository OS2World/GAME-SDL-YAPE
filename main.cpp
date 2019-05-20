/*
	YAPE - Yet Another Plus/4 Emulator

	The program emulates the Commodore 264 family of 8 bit microcomputers

	This program is free software, you are welcome to distribute it,
	and/or modify it under certain conditions. For more information,
	read 'Copying'.

	(c) 2000, 2001 Attila Grósz
*/

#include "main.h"
#include "interface.h"


// SDL stuff
SDL_Event		event;
static SDL_Surface     *screen, *image;
SDL_PixelFormat *fmt;
SDL_Palette		palette;
SDL_Color		*g_pal;
SDL_Rect		dest;

int				SDLresult;

// class pointer for the user interface
UI				*uinterface;

// color
Uint8	red, green, blue;
Uint8	g_screenpalette[5];
Uint8	g_scrpalindex = 0;
Uint8	p4col_calc[768];

Uint8	translatekey;

///////////////////
// Screen variables
Uint8	*pixel;
Sint32	g_winxoffset = 0, g_winyoffset = 0;
Sint32	winsizex, winsizey;

////////////////
// Supplementary
Uint32					g_TotFrames = 0, g_TotElapsed = 0;
Uint32					timeelapsed;
Uint32					prev_time = 0;
Uint32					now;

CPU						*machine = new CPU;
Uint32					mtab[SCR_VSIZE+1];
Uint32					i;
static char				textout[40];
static char				inipath[512] = "";
static char				inifile[512] = "";

Uint8					*screenptr;
bool					g_bActive = true;
bool					g_inDebug = false;
bool					g_FrameRate = true;
bool					g_50Hz = false;
bool					g_bSaveSettings = true;

//-----------------------------------------------------------------------------
// Name: ShowFrameRate()
//-----------------------------------------------------------------------------
inline void
ShowFrameRate()
{
	g_TotElapsed += timeelapsed;
	if ( ++g_TotFrames<50)
		//fprintf(stderr,"Time: %d, frames: %d, FPS: %3.0f\t", g_TotElapsed, g_TotFrames, (float) g_TotFrames /(float (g_TotElapsed/1000.0)+0.0001));
		return;

	sprintf(textout, "%3.0f FPS", (float) g_TotFrames /(float (g_TotElapsed/1000.0)+0.0001));
	machine->mem->texttoscreen(324,40,textout);
}


//-----------------------------------------------------------------------------
// Name: DebugInfo()
//-----------------------------------------------------------------------------
inline void
DebugInfo()
{
	sprintf(textout,"OPCODE: %2x ",machine->getcins());
	machine->mem->texttoscreen(-g_winxoffset,-g_winyoffset,textout);
	machine->mem->texttoscreen(-g_winxoffset,-g_winyoffset+8,"  PC  SR AC XR YR SP");
	sprintf(textout,";%4x %2x %2x %2x %2x %2x",machine->getPC(),machine->getST(),machine->getAC(),machine->getX(),machine->getY(), machine->getSP());
	machine->mem->texttoscreen(-g_winxoffset,-g_winyoffset+16,textout);
	sprintf(textout,"TAPE: %8d ",machine->mem->tap->TapeSoFar);
	machine->mem->texttoscreen(-g_winxoffset,-g_winyoffset+24,textout);
}

//-----------------------------------------------------------------------------
// Name: load_file()
// Desc: The Load File Procedure
//-----------------------------------------------------------------------------
bool load_file(char *fname)
{
	Uint8   *lpBufPtr;
	Uint16	loadaddr;
	FILE	*prg;
	Uint32	fsize;

	if ((prg = fopen(fname, "rb"))== NULL) {
    	return false;
	} else {
		// load PRG file
		fseek(prg, 0L, SEEK_END);
		fsize=ftell(prg);
		fseek(prg, 0L, SEEK_SET);
		lpBufPtr=(Uint8 *) malloc(fsize);
		fread(lpBufPtr,fsize,1,prg);
		// copy to memory
		loadaddr=lpBufPtr[0]|(lpBufPtr[1]<<8);
		for (i=0;i<fsize;i++)
			machine->mem->wrt(loadaddr+i,lpBufPtr[2+i]);

		machine->mem->wrt(0x2D,(loadaddr+fsize-2)&0xFF);
		machine->mem->wrt(0x2E,(loadaddr+fsize-2)>>8);
		machine->mem->wrt(0x2F,(loadaddr+fsize-2)&0xFF);
		machine->mem->wrt(0x30,(loadaddr+fsize-2)>>8);
		machine->mem->wrt(0x31,(loadaddr+fsize-2)&0xFF);
		machine->mem->wrt(0x32,(loadaddr+fsize-2)>>8);
		machine->mem->wrt(0x9D,(loadaddr+fsize-2)&0xFF);
		machine->mem->wrt(0x9E,(loadaddr+fsize-2)>>8);
		fclose(prg);
	};

	return true;
}
//-----------------------------------------------------------------------------
// Name: StartFile( )
// Desc: The Load PRG File Dialog Box Procedure
//-----------------------------------------------------------------------------
bool StartFile()
{
	machine->mem->wrt(0xEF,4);
	machine->mem->wrt(0x0527,82);
	machine->mem->wrt(0x0528,85);
	machine->mem->wrt(0x0529,78);
	machine->mem->wrt(0x052A,13);
	return true;
}

//-----------------------------------------------------------------------------

bool start_file(char *szFile )
{
	char			fileext[4];

	if (szFile[0]!='\0') {
		for (i=0;i<4;++i) fileext[i]=tolower(szFile[strlen(szFile)-3+i]);
		if (!strncmp(fileext,"d64",3)) {
			/*if (!LoadD64Image(szFile)) {
				return false;
			}*/
			machine->mem->wrt(0xEF,9);
			machine->mem->wrt(0x0527,68);
			machine->mem->wrt(0x0528,204);
			machine->mem->wrt(0x0529,34);
			machine->mem->wrt(0x052A,42);
			machine->mem->wrt(0x052B,13);
			machine->mem->wrt(0x052C,82);
			machine->mem->wrt(0x052D,85);
			machine->mem->wrt(0x052E,78);
			machine->mem->wrt(0x052F,13);
			return true;
		}
		if (!strcmp(fileext,"prg")) {
			load_file( szFile );
			StartFile();
			return true;
		}
		if (!strcmp(fileext,"tap")) {

			/*machine->mem->wrt(0xEF,3);
			machine->mem->wrt(0x0527,76);
			machine->mem->wrt(0x0528,207);
			machine->mem->wrt(0x0529,13);*/
			machine->mem->tap->detach_tap();
			strcpy(machine->mem->tap->tapefilename, szFile);
			machine->mem->tap->attach_tap();
			/*machine->mem->wrtDMA(0xFD10,machine->mem->readDMA(0xFD10)&~0x04);
			machine->mem->tapeload=true;*/
			return true;
		}
		return false;
    }
	return false;
}

//-----------------------------------------------------------------------------

void init_palette(void)
{
	Uint32 colorindex;
	double	Uc, Vc, Yc,  PI = 3.14159265 ;


    /* Allocate 256 color palette */
    int ncolors = 256;
    g_pal  = (SDL_Color *)malloc(ncolors*sizeof(SDL_Color));

	// calculate palette based on the HUE values
	memset(p4col_calc, 0, 768);
	for ( i=1; i<16; i++)
		for ( register int j = 0; j<8; j++) {
			if (i == 1)
				Uc = Vc = 0;
			else {
				Uc = sat[j] * ((float) cos( HUE[i] * PI / 180.0 ));
				Vc = sat[j] * ((float) sin( HUE[i] * PI / 180.0 ));
			}
			Yc = (luma[j+1] - 2.0)* 255.0 / (5.0 - 2.0); // 5V is the base voltage
			// RED, GREEN and BLUE component
			colorindex = (j)*16*3 + i*3;
			p4col_calc[ colorindex ] = p4col_calc[ 384 + colorindex ] =
				//(unsigned char) (Yc + 1.14 * Vc);
				(unsigned char) (Yc + 1.367 * Vc);
			p4col_calc[ colorindex + 1] = p4col_calc[ 384 + colorindex + 1] =
				//(unsigned char) (Yc - 0.396 * Uc - 0.581 * Vc );
				(unsigned char) (Yc - 0.336 * Uc - 0.698 * Vc );
			p4col_calc[ colorindex + 2] = p4col_calc[ 384 + colorindex + 2] =
				//(unsigned char) (Yc + 2.029 * Uc);
				(unsigned char) (Yc + 1.732 * Uc);
		}

	// creating multiplicator vectors for speedups
	for (i=0 ; i<SCR_VSIZE ; ++i)
		mtab[i]=i*SCR_HSIZE;

	for (i=0 ; i<256; ++i) {
		// Creating an 8 bit SDL_Color structure
		g_pal[i].b=p4col_calc[i*3+2];
		g_pal[i].g=p4col_calc[i*3+1];
		g_pal[i].r=p4col_calc[i*3];
		g_pal[i].unused=0;
	}

	palette.ncolors = 256;
	palette.colors = g_pal;

}

inline void FrameUpdate(void)
{
	SDL_BlitSurface( image, NULL , screen, NULL);
	SDL_Flip ( screen );
    /*if ( SDL_MUSTLOCK(screen) ) {
        if ( SDL_LockSurface(screen) < 0 )
            exit(1);
    }
    Uint8 *sbuffer=(Uint8 *)image->pixels;
	Uint8 *dbuffer=(Uint8 *)screen->pixels;
    for ( register int i=0; i<312; ++i, sbuffer+=screen->pitch, dbuffer+=screen->pitch )
		memcpy( dbuffer, sbuffer, 456);

    if ( SDL_MUSTLOCK(screen) ) {
        SDL_UnlockSurface(screen);
    }*/
	//SDL_UpdateRect(screen,0,0,0,0);
}

//-----------------------------------------------------------------------------
// Name: SaveSettings
//-----------------------------------------------------------------------------
bool SaveSettings()
{
	FILE *ini;
	unsigned int rammask;

	if (ini=fopen(inifile,"wt")) {

		fprintf(ini, "[Yape configuration file]\n");
		//fprintf(ini,"Double sized window=%d\n",g_bDblSize);
		fprintf(ini,"Display frame rate=%d\n",g_FrameRate);
		fprintf(ini,"Display quick debug info=%d\n",g_inDebug);
		//fprintf(ini,"Use GDI for video=%d\n",g_UseGDI);
		//fprintf(ini,"Small border=%d\n",g_smallborder);
		fprintf(ini,"50 MHz timer=%d\n",g_50Hz);
		//fprintf(ini,"Relative speed index=%d\n",g_RelSpeedIndex);
		//fprintf(ini,"Sound=%d\n",g_bIsSound);
		fprintf(ini,"Joystick emulation=%d\n",machine->mem->joyemu);
		fprintf(ini,"Active joystick=%d\n",machine->mem->keys->activejoy);
		//fprintf(ini,"PC joystick=%d\n",PCjoyactive);
		rammask = machine->mem->getRamMask();
		fprintf(ini,"Ram mask=%x\n",rammask);
		fprintf(ini,"256KB RAM=%x\n",machine->mem->bigram);
		fprintf(ini,"Save settings on exit=%x\n",g_bSaveSettings);
		fprintf(ini,"Palette index=%d\n",g_scrpalindex);

		for (i = 0; i<4; i++) {
			fprintf(ini,"ROM C%d LOW=%s\n",i, machine->mem->romlopath[i]);
			fprintf(ini,"ROM C%d HIGH=%s\n",i, machine->mem->romhighpath[i]);
		}

		fclose(ini);
		return true;
	}
	return false;
}

bool LoadSettings()
{
	FILE *ini;
	unsigned int rammask;
	bool tmpint;
	char first[6], fname[256];
	//SDL_Event	event;

	if (ini=fopen(inifile,"r")) {

		fscanf(ini,"%s configuration file]\n", first);
		if (strcmp(first, "[Yape"))
			return false;
		//fscanf(ini,"Double sized window=%d\n",&g_bDblSize);
		fscanf(ini,"Display frame rate=%d\n",&g_FrameRate);
		fscanf(ini,"Display quick debug info=%d\n",&g_inDebug);
		//fscanf(ini,"Use GDI for video=%d\n",&g_UseGDI);
		//fscanf(ini,"Small border=%d\n",&g_smallborder);
		fscanf(ini,"50 MHz timer=%d\n",&g_50Hz);
		//fscanf(ini,"Relative speed index=%d\n",&g_RelSpeedIndex);
		//fscanf(ini,"Sound=%d\n",&g_bIsSound);
		fscanf(ini,"Joystick emulation=%d\n",&tmpint);
		machine->mem->joyemu = tmpint;
		fscanf(ini,"Active joystick=%d\n",&(machine->mem->keys->activejoy));
		tmpint ? machine->mem->keys->joyinit() : machine->mem->keys->releasejoy();

		//fscanf(ini,"PC joystick=%d\n",&PCjoyactive);
		fscanf(ini,"Ram mask=%x\n",&rammask);
		machine->mem->setRamMask( rammask );
		fscanf(ini,"256KB RAM=%x\n",&tmpint);
		machine->mem->bigram = tmpint;
		fscanf(ini,"Save settings on exit=%x\n",&g_bSaveSettings);
		fscanf(ini,"Palette index=%d\n",&g_scrpalindex);

		/*g_bDblSize=!g_bDblSize;
		SendMessage(hWnd,WM_COMMAND,IDM_DBLSIZE,0L);*/
		/*g_FrameRate=!g_FrameRate;

		event.key.type  = SDL_KEYDOWN;
		event.key.state = SDL_RELEASED;

		SDL_PushEvent(&event);
		//SendMessage(hWnd,WM_COMMAND,IDM_FRAMERATE,0L);
		g_inDebug=!g_inDebug;
		/*SendMessage(hWnd,WM_COMMAND,IDM_DEBUG,0L);
		g_UseGDI=!g_UseGDI;
		SendMessage(hWnd,WM_COMMAND,IDM_GDI,0L);
		g_smallborder=!g_smallborder;
		SendMessage(hWnd,WM_COMMAND,IDM_SMALLBORDER,0L);
		SendMessage(hWnd,WM_COMMAND,g_SpeedMsg[g_RelSpeedIndex],0L);
		g_bIsSound=!g_bIsSound;
		SendMessage(hWnd,WM_COMMAND,IDM_SOUND,0L);
		machine->mem->joyemu=!machine->mem->joyemu;
		SendMessage(hWnd,WM_COMMAND,IDM_JOYEMU,0L);
		machine->mem->keys->activejoy=!machine->mem->keys->activejoy;
		SendMessage(hWnd,WM_COMMAND,IDM_SWAPJOY,0L);
		PCjoyactive=!PCjoyactive;
		SendMessage(hWnd,WM_COMMAND,IDM_PCJOY,0L);
		g_bSaveSettings=!g_bSaveSettings;
		SendMessage(hWnd,WM_COMMAND,IDM_SAVEONEXIT,0L);
		SendMessage(hWnd,WM_COMMAND,g_screenpalette[g_scrpalindex],0L);*/

		for (i = 0; i<4; i++) {
			if (fscanf(ini,"ROM C%d LOW=%s\n", &tmpint, fname))
				strcpy(machine->mem->romlopath[tmpint], fname);
			else
				strcpy(machine->mem->romlopath[tmpint], "");
			if (fscanf(ini,"ROM C%d HIGH=%s\n", &tmpint, fname))
				strcpy(machine->mem->romhighpath[tmpint], fname);
			else
				strcpy(machine->mem->romhighpath[tmpint], "");
		}

		fclose(ini);
		return true;
	}
	return false;
}

inline void PopupMsg(char *msg)
{
	Uint32 ix, len = strlen(msg);
	char dummy[40];

	ix = len;
	while( ix-->0)
		dummy[ix] = 32;
	dummy[len] = '\0';

	machine->mem->texttoscreen(220-(len<<2),144,dummy);
	machine->mem->texttoscreen(220-(len<<2),152,msg);
	machine->mem->texttoscreen(220-(len<<2),160,dummy);
	FrameUpdate();
	SDL_Delay(750);
}

//-----------------------------------------------------------------------------
// Name: SaveBitmap()
// Desc: Saves the SDL surface to Windows bitmap file named as yapeXXXX.bmp
//-----------------------------------------------------------------------------
int
SaveBitmap(  )
{
	bool			success = true;
    char			bmpname[16];
	FILE			*fp;
	int				ix = 0;

    // finding the last yapeXXXX.bmp image
    while (success) {
        sprintf( bmpname, "yape%.4d.bmp", ix);
        fp=fopen( bmpname,"rb");
        if (fp)
            fclose(fp);
		else
			success=false;
        ix++;
    };
	//fprintf( stderr, "%s\n", bmpname);
	return SDL_SaveBMP( screen, bmpname);
}
//-----------------------------------------------------------------------------
// Name: poll_events()
// Desc: polls SDL events if there's any in the message queue
//-----------------------------------------------------------------------------
inline void poll_events(void)
{
  if ( SDL_PollEvent(&event) ) {
    printf("event!\n");
        switch (event.type) {
	        case SDL_KEYDOWN:

            /*printf("The %d,%d,%s key was pressed!\n",event.key.keysym.sym,event.key.keysym.scancode,
                   SDL_GetKeyName(event.key.keysym.sym));*/

				switch (event.key.keysym.mod) {
					case KMOD_LALT :
						switch (event.key.keysym.sym) {
							case SDLK_j :
								machine->mem->joyemu=!machine->mem->joyemu;
								if (machine->mem->joyemu) {
									sprintf(textout , " JOYSTICK EMULATION IS ON ");
									machine->mem->keys->joyinit();
								} else {
									sprintf(textout , " JOYSTICK EMULATION IS OFF ");
									machine->mem->keys->releasejoy();
								};
								PopupMsg(textout);
								break;
							case SDLK_i :
								machine->mem->keys->swapjoy();
								sprintf(textout, "ACTIVE JOY IS : %d", machine->mem->keys->activejoy);
								PopupMsg(textout);
								break;
							case SDLK_s :
								g_50Hz = ! g_50Hz ;
								if (g_50Hz)
									sprintf(textout , " 50 HZ TIMER IS ON ");
								else
									sprintf(textout , " 50 HZ TIMER IS OFF ");
								PopupMsg(textout);
								// restart counter
								g_TotFrames = 0;
								g_TotElapsed = 0;
								timeelapsed = 0;
								prev_time = now = SDL_GetTicks();
								break;
							case SDLK_g :
								g_TotFrames = 0;
								g_TotElapsed = 0;
								g_FrameRate = !g_FrameRate;
								break;
							case SDLK_RETURN :
								SDL_WM_ToggleFullScreen( screen );
								break;
						};
						return;
				}
				switch (event.key.keysym.sym) {

					case SDLK_PAUSE :
						if (g_bActive)
							PopupMsg(" PAUSED ");
						g_bActive=!g_bActive;
						break;
					case SDLK_F5 :
						machine->mem->wrtDMA(0xFD10,machine->mem->readDMA(0xFD10)&~0x04);
						machine->mem->wrtDMA(1,machine->mem->readDMA(1)&0xF7);
						machine->mem->tapeload=true;
						break;
					case SDLK_F6 :
						machine->mem->wrtDMA(0xFD10,machine->mem->readDMA(0xFD10)|0x04);
						machine->mem->wrtDMA(1,machine->mem->readDMA(1)|0x08);
						machine->mem->tapeload=false;
						machine->mem->tapesave=false;
						break;
					case SDLK_F7:
						//SDL_SaveBMP( screen , "yape.bmp");
						if (!SaveBitmap( ))
							fprintf( stderr, "Screenshot saved.\n");
						break;
					case SDLK_F8:
						uinterface = new UI(machine->mem, screen, image, inipath);
						delete uinterface;
						break;
					case SDLK_F9 :
						g_inDebug=!g_inDebug;
						break;
					case SDLK_F10 :
						if (SaveSettings())
						  fprintf( stderr, "Settings saved to %s.\n", inifile);
						else
						  fprintf( stderr, "Oops! Could not save settings... %s\n", inifile);
						break;
					case SDLK_F11 :
						g_bActive = false;
						break;
					case SDLK_F12 :
					case SDLK_ESCAPE :
						// Save settings if required
						if (g_bSaveSettings)
							SaveSettings();
						exit(0);
					default :
						machine->mem->keys->pushkey(event.key.keysym.sym&0x1FF);
				}
				break;

	        case SDL_KEYUP:
				switch (event.key.keysym.sym) {
					case SDLK_F11 :
						g_bActive = true;
						switch (event.key.keysym.mod) {
							case KMOD_LSHIFT :
							case KMOD_RSHIFT :
								machine->reset();
								break;
							case KMOD_LCTRL :
							case KMOD_RCTRL :
								machine->debugreset();
								break;
							default :
								machine->softreset();
						}
						break;
					default:
						machine->mem->keys->releasekey(event.key.keysym.sym&0x1FF);
				}
				break;
			/*case SDL_VIDEORESIZE :
				resize_window(event.resize.w, event.resize.h);
				break;*/
            case SDL_QUIT:
                exit(0);
        }
    }
}

inline Uint32 time_elapsed(void)
{
	prev_time = now;
	now = SDL_GetTicks();
    //now = SDL_GetTicks();
    /*if ( next_time <= now ) {
        next_time = now+20;
        return(0);
    }*/
    return( now - prev_time);
}


// main program
int main(int argc, char *argv[])
{
	Uint16		delay = 0;
	char		drvnamebuf[16];

	// initialise SDL...
    if ( SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) < 0 ) { //  SDL_INIT_AUDIO|
        fprintf(stderr, "Unable to init SDL: %s\n", SDL_GetError());
        exit(1);
    }
	atexit(SDL_Quit);

	// check the video driver we have
	SDL_VideoDriverName( drvnamebuf, 16);
	fprintf(stderr, "Using driver : %s\n", drvnamebuf);

	// set the window title and icon
	SDL_WM_SetCaption(NAME, "Yapelin");
	SDL_WM_SetIcon(SDL_LoadBMP("Yapelin.bmp"), NULL);

	// set the appropriate video mode
	screen = SDL_SetVideoMode(456, 312, 8,
		SDL_HWPALETTE|SDL_SRCCOLORKEY|SDL_HWSURFACE|SDL_DOUBLEBUF  ); // SDL_DOUBLEBUF|SDL_FULLSCREEN|SDL_RESIZABLE
	if ( screen == NULL) {
        fprintf(stderr, "Unable to set video mode\n");
        exit(1);
    }

	// set the window range to be updated
    dest.x = 0;
    dest.y = 0;
    dest.w = screen->w;
    dest.h = screen->h;

	// calculate and initialise palette
	init_palette();

	init_audio(machine->mem);

	// set colors to the screen buffer
	SDLresult = SDL_SetColors(screen, g_pal, 0, 256);

	// set the backbuffer to the same format
	image = SDL_DisplayFormat(screen);

	// set the colors for the backbuffer too
	if (image->format->palette!= NULL ) {
        SDL_SetColors(screen,
                      image->format->palette->colors, 0,
                      image->format->palette->ncolors);
    }

	// change the pointer to the pixeldata of the backbuffer
	image->pixels = machine->mem->screen;

	if (SDL_EnableKeyRepeat(0, SDL_DEFAULT_REPEAT_INTERVAL))
		fprintf(stderr,"Oops... could not set keyboard repeat rate.\n");

	if (getenv("HOME")) {
		strcpy( inipath, getenv("HOME"));
		fprintf(stderr,"Home directory is %s\n",inipath);
		ad_makedirs( inipath );
	} else {
		strcpy( inipath, "");
		fprintf(stderr,"No home directory, using current directory\n");
	}

	strcpy( inifile, ad_getinifilename( inipath ) );

	if (LoadSettings()) {
		fprintf(stderr,"Settings loaded successfully.\n");
		machine->reset();
	} else
		fprintf(stderr,"Error loading settings or no .ini file present...\n");

    //-------------------------------------------------------------------------
	// if we have a command line parameter
	if (argv[1]!='\0') {
		printf("Parameter 1 :%s\n", argv[1]);
		// do some frames
		while (g_TotFrames<6) {
			machine->mem->ted_process();
			if (machine->mem->render_ok && g_bActive) {
				FrameUpdate();
				machine->mem->render_ok=false;
				g_TotFrames++;
			}
		}
		// and then try to load the parameter as file
		load_file(argv[1]);
		start_file(argv[1]);
	}
	//--------------------------------------------------------------

	/*SDL_Event filter = (SDL_QUIT|SDL_KEYDOWN|SDL_KEYUP);
	SDL_SetEventFilter( filter );*/

	now = SDL_GetTicks();
	while (true) {
		// hook into the emulation loop if active
		if (g_bActive)
			machine->mem->ted_process();
		if (machine->mem->render_ok || !(--delay))
			poll_events();
		// if a new frame is ready
		if (machine->mem->render_ok) {
			// setting frame ready indicator to false
			machine->mem->render_ok=false;
			if (g_inDebug)
				DebugInfo();
			timeelapsed = time_elapsed() ;
			if (g_50Hz && timeelapsed<20)
				SDL_Delay(20-timeelapsed);
			if (g_FrameRate)
				ShowFrameRate();
			// frame update
			FrameUpdate();
		}
	}

	// close audio
	close_audio();
	// release the palette
	if (g_pal)
		free(g_pal);
	return 0;
}

