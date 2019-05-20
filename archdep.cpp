/*
	YAPE - Yet Another Plus/4 Emulator

	The program emulates the Commodore 264 family of 8 bit microcomputers

	This program is free software, you are welcome to distribute it,
	and/or modify it under certain conditions. For more information,
	read 'Copying'.

	(c) 2000, 2001 Attila Grósz
*/

#include "archdep.h"

/* functions for Windows */
#ifdef WIN32

HANDLE				handle;
WIN32_FIND_DATA			rec;
char temp[512];

/* file management */
int ad_get_curr_dir(char *pathstring)
{
	return GetCurrentDirectory(sizeof(pathstring), pathstring);
}

int ad_set_curr_dir( char *path)
{
	return SetCurrentDirectory( path);
}

void *ad_find_first_file(char *filefilter)
{
	handle = FindFirstFile( filefilter, &rec);
	return (void*) handle;
}

char *ad_return_current_filename(void)
{
	return (char *) rec.cFileName;
}

int ad_find_next_file(void)
{
	return FindNextFile( handle, &rec);
}

int ad_makedirs(char *path)
{
  strcpy(temp,path);
  strcat(temp, "/yape");
  CreateDirectory(temp, NULL);

  return 1;
}

char *ad_getinifilename(char *tmpchr)
{
	strcpy( temp, tmpchr);
	strcat( temp, "..\\YAPE.INI");

	return temp;
}

/* end of Windows functions */
#else

#ifdef __OS2__

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <direct.h>

DIR *dirp;
struct	dirent *direntp;
char    temp[512];

/* file management */
int ad_get_curr_dir(char *pathstring)
{
  getcwd(pathstring, 256);
  return 1;
}

int ad_set_curr_dir( char *path)
{
  chdir(path);
  return 1;
}

void *ad_find_first_file(char *filefilter)
{
  dirp = opendir( filefilter );
  //fprintf( stderr, "reading from %s\n", homedir);
  if ( (direntp = readdir( dirp )) == NULL )
    closedir( dirp );

  return dirp;
}

char *ad_return_current_filename(void)
{
  return (char *) direntp->d_name;
}

int ad_find_next_file(void)
{
    if ( (direntp = readdir( dirp )) != NULL )
      return 1;
    else {
      closedir( dirp );
      return 0;
    }
}

int ad_makedirs(char *path)
{
  strcpy(temp,path);
  strcat(temp, "\\yape");
  mkdir(temp);

  return 1;
}

char *ad_getinifilename(char *tmpchr)
{
  strcpy( temp, tmpchr);
  strcat( temp, "..\\YAPE.INI");

  return temp;
}

/* end of OS/2 functions */
#else

#include <stdio.h>
#include <dirent.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>

#include <sys/stat.h>
//  #include <sys/types.h>
//  #include <sys/time.h>
//  #include <sys/resource.h>
//  #include <sys/wait.h>
//  #include <netinet/in.h>
//  #include <fcntl.h>
//  #include <string.h>

#define INVALID_HANDLE_VALUE 0x00

DIR		*dirp;
struct	dirent *direntp;
char    filter[4];
char    temp[512];

int ad_get_curr_dir(char *pathstring)
{
  //dirp = opendir( "." );
  //while ( (direntp = readdir( dirp )) != NULL )
  //  (void)printf( "%s\n", direntp->d_name );
  //(void)closedir( dirp );
  //return (0);
}

int ad_set_curr_dir( char *path)
{
  return 0;
}

void *ad_find_first_file(char *filefilter)
{
  char homedir[512];

  strcpy( homedir , getenv( "HOME" ));
  //strcpy( filter, filefilter);
  strcat( homedir, "/yape" );
  chdir ( homedir );
  dirp = opendir( homedir );
  //fprintf( stderr, "reading from %s\n", homedir);
  if ( (direntp = readdir( dirp )) == NULL )
    closedir( dirp );

}

char *ad_return_current_filename(void)
{
  return (char *) direntp->d_name;
}

int ad_find_next_file(void)
{

    if ( (direntp = readdir( dirp )) != NULL )
      return 1;
    else {
      closedir( dirp );
      return 0;
    }
}

int ad_makedirs(char *path)
{
  strcpy(temp,path);
  strcat(temp, "/yape");
  mkdir(temp, 0777);

  return 1;
}

char *ad_getinifilename(char *tmpchr)
{

	strcpy( temp, tmpchr);
	strcat( temp, "/yape/yape.conf");

	return temp;
}

#endif
#endif


/* print files in current directory in reverse order */
/*

#include <dirent.h>
main(){
    struct dirent **namelist;
    int n;

    n = scandir(".", &namelist, 0, alphasort);
    if (n < 0)
        perror("scandir");
    else {
        while(n--) {
            printf("%s\n", namelist[n]->d_name);
            free(namelist[n]);
        }
        free(namelist);
    }
}
*/
