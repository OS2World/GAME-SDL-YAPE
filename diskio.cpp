/*
	YAPE - Yet Another Plus/4 Emulator

	The program emulates the Commodore 264 family of 8 bit microcomputers

	This program is free software, you are welcome to distribute it,
	and/or modify it under certain conditions. For more information,
	read 'Copying'.

	(c) 2000, 2001 Attila Grósz
*/
#include <stdio.h>
#include <string.h>
#include "diskio.h"
#include "archdep.h"

unsigned char	IEC_Data;         // 0xFEF0
unsigned char	IEC_Status;       // 0xFEF1
unsigned char	IEC_Handshake;    // 0xFEF2
unsigned char	IEC_LastCommand;
unsigned char	IEC_DriveStatus = IEC_IDLE;
unsigned char	IEC_InputBuffer[256];
unsigned long	IEC_InputBufferPointer=0;
unsigned char	IEC_OutputBuffer[65536];
unsigned int	IEC_OutputBufferPointer=0;
unsigned int	IEC_OutputBufferLength=0;
unsigned char	D64Image[174848];

bool			saving;
bool			endfilename;
char			OFName[18];
char			OFNamePtr = 0;
FILE			*ofile;

unsigned char	anyfile[256];
int				afctr;
int				D64_offset[D64_MAX_TRACKS];
bool			g_Log=false;
bool			g_anyfile;
int				g_DriveType8=1;
char			drivedir[MAX_PATH];		// current drive irectory

int D64_sectors_per_track[] =
{
	21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21,
	19, 19, 19, 19, 19, 19, 19,
	18, 18, 18, 18, 18, 18,
	17, 17, 17, 17, 17
};


int NumLen(int n)
{
    char buffer[20];

    sprintf(buffer,"%d",n);
    return strlen(buffer);
}

void Init(void)
{
	int i;

    D64_offset[0] = 0;
    for (i=1;i<=D64_MAX_TRACKS;i++) D64_offset[i] = D64_offset[i - 1]+D64_sectors_per_track[i - 1]*256;
}

int CalcOffset(int track, int sector)
{
    return D64_offset[track - 1] + sector * 256;
}

int LoadD64Image(char *filename)
{
    FILE *fp;

	fp=fopen(filename,"rb");
    if (fp)
    {
    	int result = fread(D64Image,1,174848,fp);
        fclose(fp);
        if (result) {
			g_DriveType8=2;
			Init();
			IEC_DriveStatus = IEC_IDLE;
			return 1;
		}
        else return 0;
    }
    else return 0;
}

void DetachD64Image(void)
{
	g_DriveType8=1;
}
void UpdateD64Image(char *filename)
{

}

void InterpretInputBuffer_D64File(void)
{
    if (IEC_InputBufferPointer==0) // Length = 0 -> Command
    {
        IEC_OutputBufferPointer=0;
        IEC_OutputBufferLength = 0;
    } else {
        if (IEC_InputBufferPointer>2) {
            if (IEC_InputBuffer[1]==':')    // If filename starts with "0:"...
            {
                for (int i=0;i<254;i++)
                    IEC_InputBuffer[i]=IEC_InputBuffer[i+2];
            }
        }


        if (IEC_InputBuffer[0]=='$') // Directory
        {
           int i,j,k;
            int pos = 0;
            int BlocksFree = 0;
            int D64pos = 0;
            unsigned int addr = 0x1001;
            int dirtrack,dirsector;

            D64pos = CalcOffset(18,0);

            // Directory track and sector
            dirtrack = D64Image[D64pos];
            dirsector = D64Image[D64pos + 1];

            // Free blocks
            for (i=0;i<35;i++)
                BlocksFree += D64Image[D64pos + 4 + i*4];
            BlocksFree -= D64Image[D64pos + 4 + 17*4]; // Sub Track 18   (17!!!)

            IEC_OutputBuffer[pos++] = addr & 0xFF;
            IEC_OutputBuffer[pos++] = addr >> 8;
            addr +=30;
            IEC_OutputBuffer[pos++] = addr & 0xFF;
            IEC_OutputBuffer[pos++] = addr >> 8;
            IEC_OutputBuffer[pos++] = 0;
            IEC_OutputBuffer[pos++] = 0;
            IEC_OutputBuffer[pos++] = 0x12; // Inverse mode!
            IEC_OutputBuffer[pos++] = '\"'; // "

			memcpy((void *) &IEC_OutputBuffer[pos],(const void *) &D64Image[0x90+D64pos],16);
			pos+=16;
			D64pos+=16;

            IEC_OutputBuffer[pos++] = '\"'; // "
            IEC_OutputBuffer[pos++] = ' ';
            D64pos+=2;

            IEC_OutputBuffer[pos++] = D64Image[0x90+D64pos++];
            IEC_OutputBuffer[pos++] = D64Image[0x90+D64pos++];
            IEC_OutputBuffer[pos++] = ' ';
            D64pos++;

            IEC_OutputBuffer[pos++] = D64Image[0x90+D64pos++];
            IEC_OutputBuffer[pos++] = D64Image[0x90+D64pos++];

            IEC_OutputBuffer[pos++] = 0x00; // End of line

            // Let's generate the directory...
            while (dirtrack!=0x00 && dirsector!=0xFF) {
                D64pos = CalcOffset(dirtrack,dirsector);
                dirtrack = D64Image[D64pos];
                dirsector = D64Image[D64pos + 1];

                for (i=2; i<256; i+=32)
                {
                    if (D64Image[D64pos + i] !=0x00) //& 0x80)
                    {
                        int fileblocks = D64Image[D64pos + i + 28] + D64Image[D64pos + i + 29]*256;
                        addr+=31;
                        IEC_OutputBuffer[pos++] = addr & 0xff;
                        IEC_OutputBuffer[pos++] = addr >> 8;
                        IEC_OutputBuffer[pos++] = (unsigned char)(fileblocks) & 0xff;
                        IEC_OutputBuffer[pos++] = (unsigned char)(fileblocks) >> 8;

                        switch (NumLen(fileblocks))
                        {
                            case 1: IEC_OutputBuffer[pos++] = ' ';
                            case 2: IEC_OutputBuffer[pos++] = ' ';
                            case 3: IEC_OutputBuffer[pos++] = ' ';
                        }

                        IEC_OutputBuffer[pos++] = '\"'; // "

			            for (j=0;j<16 && D64Image[D64pos + i + j + 3]!=0xA0;j++)        // 0xA0 - Shift Space
                            IEC_OutputBuffer[pos++] = D64Image[D64pos + i + j + 3];

                        IEC_OutputBuffer[pos++] = '\"'; // "

                        for (k=j;k<16;k++)
                            IEC_OutputBuffer[pos++] = ' ';

                        // Damaged file...
                        if (D64Image[D64pos + i]&0x80)
                            IEC_OutputBuffer[pos++] = ' ';
                        else
                            IEC_OutputBuffer[pos++] = '*';

                        // File type...
                        switch (D64Image[D64pos + i] & 0x03)
                        {
                            case 0x00:
                                        IEC_OutputBuffer[pos++] = 'D';
                                        IEC_OutputBuffer[pos++] = 'E';
                                        IEC_OutputBuffer[pos++] = 'L';
                                        break;
                            case 0x01:
                                        IEC_OutputBuffer[pos++] = 'S';
                                        IEC_OutputBuffer[pos++] = 'E';
                                        IEC_OutputBuffer[pos++] = 'Q';
                                        break;
                            case 0x02:
                                        IEC_OutputBuffer[pos++] = 'P';
                                        IEC_OutputBuffer[pos++] = 'R';
                                        IEC_OutputBuffer[pos++] = 'G';
                                        break;
                            case 0x03:
                                        IEC_OutputBuffer[pos++] = 'U';
                                        IEC_OutputBuffer[pos++] = 'S';
                                        IEC_OutputBuffer[pos++] = 'R';
                                        break;
                            case 0x04:
                                        IEC_OutputBuffer[pos++] = 'R';
                                        IEC_OutputBuffer[pos++] = 'E';
                                        IEC_OutputBuffer[pos++] = 'L';
                                        break;
                            default:
                                        IEC_OutputBuffer[pos++] = '?';
                                        IEC_OutputBuffer[pos++] = '?';
                                        IEC_OutputBuffer[pos++] = '?';
                                        break;
                        }

                        // Protection...
                        if (D64Image[D64pos + i]&0x40)
                            IEC_OutputBuffer[pos++] = '<';
                        else
                            IEC_OutputBuffer[pos++] = ' ';

                        // Last filler spaces...
                        IEC_OutputBuffer[pos++] = ' ';
                        if (fileblocks>=100) IEC_OutputBuffer[pos++] = ' ';
                        if (fileblocks>=10) IEC_OutputBuffer[pos++] = ' ';

                        IEC_OutputBuffer[pos++] = 0x00; // End of line
                    }
                }
            }

            addr += 30;
            IEC_OutputBuffer[pos++] = addr & 0xff;
            IEC_OutputBuffer[pos++] = addr >> 8;
            IEC_OutputBuffer[pos++] = BlocksFree & 0xff;
            IEC_OutputBuffer[pos++] = BlocksFree >> 8;
			strncpy((char *) &IEC_OutputBuffer[pos],"BLOCKS FREE              ",25);
			pos+=25;

            IEC_OutputBuffer[pos++] = 0x00; // End of line

            IEC_OutputBuffer[pos++] = 0x00; // No more basic line pointer
            IEC_OutputBuffer[pos++] = 0x00; //

            IEC_OutputBufferLength = pos;
        }
        else    // Load File
        {
            int i,j,k;
            char filename[256];
            char fnbuffer[20];
            int D64pos = 0;
            int filepos = 0;
            int dirtrack,dirsector;
            int filetrack,filesector;
            int isfile = 0;
            int s = 0;
            int firstfile = 0;

            //strcpy(filename,(char *)(&IEC_InputBuffer[0]));

			// check for * in the filename
			afctr=0;
			i=0;
			do {
				if (IEC_InputBuffer[i]!='*') {
					filename[afctr++]=IEC_InputBuffer[i++];
					g_anyfile=false;
				} else {
					g_anyfile=true;
					filename[afctr]='\0';
					i++;
				}
			} while (IEC_InputBuffer[i]);
			filename[i]='\0';

            firstfile = IEC_InputBuffer[0]=='*'?1:0;
			if (!g_anyfile) afctr=16;

            IEC_OutputBufferLength = 0;

            // Directory track and sector
            D64pos = CalcOffset(18,0);
            dirtrack = D64Image[D64pos];
            dirsector = D64Image[D64pos + 1];

            // First let's find the file...
            while (isfile!=1 && dirtrack!=0x00 && dirsector!=0xFF) {
                D64pos = CalcOffset(dirtrack,dirsector);
                dirtrack = D64Image[D64pos];
                dirsector = D64Image[D64pos + 1];

                for (i=2; i<256; i+=32) {
                    if (D64Image[D64pos + i] != 0x00) {
			            for (j=0;j<afctr && D64Image[D64pos + i + j + 3]!=0xA0;j++)        // 0xA0 - Shift Space
                            fnbuffer[j] = D64Image[D64pos + i + j + 3];
                        fnbuffer[j] = 0x00;

                        if (strcmp(fnbuffer,filename)==0 || firstfile) {
                            isfile = 1;

                            // First track position from the directory entry...
                            filetrack = D64Image[D64pos + i + 1];
                            filesector = D64Image[D64pos + i + 2];

                            filepos = CalcOffset(filetrack,filesector);
                            filetrack = D64Image[filepos];
                            filesector = D64Image[filepos+1];

                            while (filetrack!=0) {
                                for (k=2;k<256;k++)
                                    IEC_OutputBuffer[s++] = D64Image[filepos+k];

                                filepos = CalcOffset(filetrack,filesector);
                                filetrack = D64Image[filepos];
                                filesector = D64Image[filepos+1];
                            }

                            if (filesector>0)
                            {
                                for (k=2;k<(filesector+2);k++)
                                    IEC_OutputBuffer[s++] = D64Image[filepos+k];
                            }

                            IEC_OutputBufferLength = s-1;
                            break;
                        }
                    }
                }

                // ?FILE NOT FOUND ERROR
                if (isfile==0)
                    IEC_Status=0x02;
                else
                    IEC_Status=0x00;
            }
        }
    }
}




void InterpretInputBuffer_PCDir(void)
{
    if (IEC_InputBufferPointer==0) // Length = 0 -> Command
    {
        IEC_OutputBufferPointer=0;
        IEC_OutputBufferLength = 0;
    } else {
        if (IEC_InputBufferPointer>2)
            if (IEC_InputBuffer[1]==':')   // If filename starts with "0:"...
                for (int i=0;i<254;i++)
                    IEC_InputBuffer[i]=IEC_InputBuffer[i+2];

        // Change directory...
        int  chgdir = 0;
		char origdir[256];

		// keep the current directory as we must get back to it after reading the drive
		if (ad_get_curr_dir(origdir)) {
			ad_set_curr_dir("."); // drivedir
            chgdir = 1;
        }

		// check for * in the filename
		g_anyfile=false;
		afctr=0;
		int i=0;
		while (IEC_InputBuffer[i])
			if (IEC_InputBuffer[i]!='*') {
				anyfile[afctr++]=IEC_InputBuffer[i++];
			} else {
				g_anyfile=true;
				anyfile[afctr++]='*';
				anyfile[afctr++]='\0';
				i++;
			}

		char cfilename[256];
		void *handle;
        // Load first file...
		if (g_anyfile) {

            handle=ad_find_first_file( (char *) anyfile);

            if (handle!=NULL) {
	            //if (handle!=INVALID_HANDLE_VALUE) {
					strcpy( cfilename, (char *) ad_return_current_filename());
                    strcpy((char *)(&IEC_InputBuffer[0]), cfilename);
                    IEC_InputBuffer[strlen(cfilename)-4]=0x00; // Cut .prg extension
                //}
            }
        }

        if (IEC_InputBuffer[0]=='$') // Directory
        {
            int pos = 0;
            int BlocksFree = 644;
            unsigned short addr = 0x1001;

            IEC_OutputBuffer[pos++] = addr & 0xFF;
            IEC_OutputBuffer[pos++] = addr >> 8;
            addr +=30;
            IEC_OutputBuffer[pos++] = addr & 0xFF;
            IEC_OutputBuffer[pos++] = addr >> 8;
            IEC_OutputBuffer[pos++] = 0;
            IEC_OutputBuffer[pos++] = 0;
            IEC_OutputBuffer[pos++] = 0x12; // Inverse mode!
            IEC_OutputBuffer[pos++] = '\"'; // "

			strncpy((char *) &IEC_OutputBuffer[pos],"YAPE EMULATOR  ",15);
			pos+=15;

            IEC_OutputBuffer[pos++] = '\"'; // "
            IEC_OutputBuffer[pos++] = ' ';

            IEC_OutputBuffer[pos++] = '0';
            IEC_OutputBuffer[pos++] = '0';

            IEC_OutputBuffer[pos++] = ' ';

            IEC_OutputBuffer[pos++] = '2';
            IEC_OutputBuffer[pos++] = 'A';

            IEC_OutputBuffer[pos++] = 0x00; // End of line

            // Let's generate the file list...

            FILE *fp;
            int fsize;
            int i;

			handle = ad_find_first_file( "*.prg" );
            if (handle)
            {
	            //if (handle!=INVALID_HANDLE_VALUE)
	            {
		            for (;;)
		            {
                        // Let's get the length of the current file...
			    strcpy( cfilename , ad_return_current_filename());
	                    fp=fopen(cfilename,"rb");
                        if (fp)
                        {
                            fseek(fp, 0L, SEEK_END);
                            fsize=ftell(fp);
                            fseek(fp, 0L, SEEK_SET);

                            fclose(fp);
                        }
                        else fsize = -1;

                        if (fsize!=-1 && (strlen(cfilename)-4)<=16)
                        {
                            addr+=32;

                            IEC_OutputBuffer[pos++] = addr & 0xff;
                            IEC_OutputBuffer[pos++] = addr >> 8;
                            IEC_OutputBuffer[pos++] = (unsigned char)((fsize>>8)+1) & 0xff;
                            IEC_OutputBuffer[pos++] = (unsigned char)((fsize>>8)+1) >> 8;

                            switch (NumLen((fsize>>8)+1))
                            {
                                case 1: IEC_OutputBuffer[pos++] = ' ';
                                case 2: IEC_OutputBuffer[pos++] = ' ';
                                case 3: IEC_OutputBuffer[pos++] = ' ';
                            }

                            IEC_OutputBuffer[pos++] = '\"'; // "

                            // Make it CAPITAL
			                for ( i=0; cfilename[i]!=0; i++)
                                if (cfilename[i]>=97 && cfilename[i]<=122) cfilename[i]-=32;

			                for ( i=0; i<(strlen(cfilename)-4); i++)
                                IEC_OutputBuffer[pos++] = cfilename[i];

                            IEC_OutputBuffer[pos++] = '\"'; // "

                            for (i=(strlen(cfilename)-4);i<17;i++)
                                IEC_OutputBuffer[pos++] = ' ';

                            IEC_OutputBuffer[pos++] = 'P';
                            IEC_OutputBuffer[pos++] = 'R';
                            IEC_OutputBuffer[pos++] = 'G';

                            // Last filler spaces...

                            IEC_OutputBuffer[pos++] = ' ';
                            IEC_OutputBuffer[pos++] = ' ';
                            if (((fsize>>8)+1)>=100) IEC_OutputBuffer[pos++] = ' ';
                            if (((fsize>>8)+1)>=10) IEC_OutputBuffer[pos++] = ' ';

                            IEC_OutputBuffer[pos++] = 0x00; // End of line
                        }

                        if (ad_find_next_file()==0) break;
		            }
	            }
            }

            char currentdir[256];

            //GetCurrentDirectory(256,currentdir);
			//getcwd(currentdir, sizeof(currentdir));
            currentdir[3]=0;

            int nSectorsPerCluster, nBytesPerSector, nFreeClusters, nTotalClusters;
            /*if (GetDiskFreeSpace(currentdir, &nSectorsPerCluster, &nBytesPerSector, &nFreeClusters, &nTotalClusters))
            {
                BlocksFree = nFreeClusters * nSectorsPerCluster * nBytesPerSector;
            }
            else */
				BlocksFree = 0;

            if (BlocksFree>32768) BlocksFree=32768;

            addr += 30;
            IEC_OutputBuffer[pos++] = addr & 0xff;
            IEC_OutputBuffer[pos++] = addr >> 8;
            IEC_OutputBuffer[pos++] = BlocksFree & 0xff;
            IEC_OutputBuffer[pos++] = BlocksFree >> 8;
			strncpy((char *) &IEC_OutputBuffer[pos],"BLOCKS FREE              ",25);
			pos+=25;

            IEC_OutputBuffer[pos++] = 0x00; // End of line

            IEC_OutputBuffer[pos++] = 0x00; // No more basic line pointer
            IEC_OutputBuffer[pos++] = 0x00; //

            // Set end of buffer...
            IEC_OutputBufferLength = pos;
        }
        else
        {
            FILE *fp;
            int fsize;
	        int result;
            char filename[256];

            strcpy(filename,(char *)(&IEC_InputBuffer[0]));
            strcat(filename,".PRG");

	        fp=fopen(filename,"rb");

            if (fp) // LOADING
            {
                fseek(fp, 0L, SEEK_END);
                fsize=ftell(fp);
                fseek(fp, 0L, SEEK_SET);

    	        result=fread(IEC_OutputBuffer,1,fsize,fp);

                fclose(fp);

                IEC_OutputBufferLength = (unsigned short)fsize;

                IEC_Status=0x00;
            }
            else    // File not found error...
            {
                IEC_Status=0x02;
            }
        }

		// change back to the original directory
        /*if (chgdir==1)
			chdir(origdir);*/
            /*SetCurrentDirectory(origdir);*/

    }
}

void InterpretInputBuffer(void)
{
    switch (g_DriveType8)
    {
        case 1: InterpretInputBuffer_PCDir();
                break;
        case 2: InterpretInputBuffer_D64File();
                break;
    }
}

unsigned char ReadParallelIEC(unsigned short addr)
{
    switch (g_DriveType8)
    {
		// None
		case 0 :    return 0x00;
					break;

        // PC Directory on 1551
        case 1 :
        case 2 :
                    switch (addr)
                    {
                        case 0xFEF0:    return IEC_Data;
                        case 0xFEF1:    return IEC_Status;
                        case 0xFEF2:    return IEC_Handshake;
                        default:        return 0x00;
                    }
                    break;
        default:    return 0x00;
    }
}

void WriteParallelIEC(unsigned short addr,unsigned char data)
{
	/*char stmp[50];
	sprintf(stmp,"Write : %4x - %2x",addr,data);
	add_to_log(stmp);*/

    switch (g_DriveType8) {
		// None
		case 0 :
					break;

        // PC Directory on 1551 or D64 file
        case 1 :
        case 2 :
                switch (addr) {
					case 0xFEF0:
                    switch (IEC_DriveStatus) {
						case IEC_IDLE: if (data==0x55) {    // Init DMA
											IEC_Data = 0x55;
											IEC_Status = 0x00;
											IEC_InputBufferPointer=0;
											for (int i=0;i<256;i++) IEC_InputBuffer[i]=0;
											break;
                                        }
                                        if (data==0x81) {    // device ID
											IEC_Handshake = 0x00;
                                            IEC_DriveStatus = IEC_DEVICEID;
                                            break;
                                         }
                                        if (data==0x82)     // Command coming (0xE0, 0xF0)
                                        {
                                            IEC_Handshake = 0x00;
                                            break;
                                        }
                                        if (data==0x83)     // Data (to drive)
                                        {
											IEC_Handshake = 0x00;
                                            IEC_DriveStatus = IEC_INPUT;
                                            break;
                                        }
                                        if (data==0x84) {    // Read Byte (to Plus4)
                                            IEC_Data = IEC_OutputBuffer[IEC_OutputBufferPointer++];    // Data to load
                                            IEC_Handshake = 0x00;
                                            if (IEC_OutputBufferPointer==IEC_OutputBufferLength) // End of buffer?
                                                IEC_Status = 0x03; // End of buffer.
                                            else
                                                IEC_Status = 0x00; // Not yet...
                                                 break;
                                            }
                                        // Commands after 0x82
                                        if (data==0xE0)     // Close
                                        {
	                                        IEC_LastCommand = data;
                                            IEC_Status = 0x00;
                                            IEC_Handshake = 0xFF;
                                            break;
										}
                                        if (data==0xE1)     // Close at save
                                        {
	                                        IEC_LastCommand = data;
                                            IEC_Status = 0x00;
                                            IEC_Handshake = 0xFF;

											if (ofile!=NULL)
												fclose(ofile);

											endfilename = false;
											saving = false;
                                            break;
										}
                                        if (data==0xF0)     // Open
										{
											IEC_LastCommand = data;
                                            IEC_Handshake = 0xFF;
                                            break;
                                        }
                                        if (data==0xF1)     // Open for save, filename comes
										{
											IEC_LastCommand = data;
                                            IEC_Handshake = 0xFF;
											OFNamePtr = 0;
											saving = true;
											endfilename = false;
                                            break;
                                        }
                                        break;
                                        if (data==0x61)     // Open for save, program data comes
										{
											IEC_LastCommand = data;
                                            IEC_Handshake = 0xFF;
											OFNamePtr = 0;
											saving = false;
											endfilename = true;
                                            break;
                                        }
                                        break;

							case IEC_INPUT:
										if (saving)
											OFName[OFNamePtr++] = data;
										if (endfilename)
											fwrite(&data,1,1,ofile);
                                        IEC_InputBuffer[IEC_InputBufferPointer++]=data;
                                        IEC_DriveStatus = IEC_IDLE;
                                        break;

                            case IEC_DEVICEID:
                                         if (data==0x20)     // Talk - 1551 gets data
                                         {
											IEC_Handshake = 0xFF;
											IEC_InputBufferPointer=0;
											IEC_DriveStatus = IEC_IDLE;
                                            for (int i=0;i<256;i++) IEC_InputBuffer[i]=0;
												break;
                                         }
										 if (data==0x40)     // Listen - 1551 gives data
                                         {
											IEC_Handshake = 0xFF;
                                            IEC_OutputBufferPointer=0;
                                            IEC_DriveStatus = IEC_IDLE;
                                            break;
                                         }
                                         if (data==0x3F)     // Untalk
                                         {
											InterpretInputBuffer();
                                            IEC_Handshake = 0xFF;
                                            IEC_DriveStatus = IEC_IDLE;
											// filename at save
											if (saving) {
												OFName[OFNamePtr++]='.';
												OFName[OFNamePtr++]='P';
												OFName[OFNamePtr++]='R';
												OFName[OFNamePtr++]='G';
												OFName[OFNamePtr]=0;
												if (OFName[1]==0x3A)
													strncpy(OFName,OFName+2,OFNamePtr-1);
												if (ofile = fopen(OFName,"wb"))
													endfilename = true;
												OFNamePtr = 0;
												saving = false;
											} else
												endfilename = false;
                                            break;
                                          }
                                          if (data==0x5F)     // Unlisten
                                          {
											IEC_Handshake = 0xFF;
                                            IEC_Status = 0x00;
                                            IEC_DriveStatus = IEC_IDLE;
                                            break;
                                          }
                                          break;
                                        }
                                        break;
                        case 0xFEF1:
                                        break;
                        case 0xFEF2:    if (data==0x00) IEC_Handshake = 0xFF;
                                        if (data==0x40) IEC_Handshake = 0x00;
                                        break;
                        default:
                                        break;
                    }
    }
}
