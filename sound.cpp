/*
	YAPE - Yet Another Plus/4 Emulator

	The program emulates the Commodore 264 family of 8 bit microcomputers

	This program is free software, you are welcome to distribute it,
	and/or modify it under certain conditions. For more information,
	read 'Copying'.

	(c) 2000, 2001 Attila Grósz
*/

#include "sound.h"
#include "tedmem.h"
#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <math.h>

unsigned short digivalues[128] = {
        // 0x80-0x8F
        31,31,31,30,30,30,31,30,30,30,30,30,30,30,30,30,
        // 0x90-0x9F
        31, 562, 1528, 2485, 3462, 4454, 5450, 6464, 7334, 7335, 7336, 7337, 7336, 7334, 7336, 7336,
        // 0xA0-0xAF
        31, 542, 1508, 2465, 3443, 4434, 5429, 6443, 7314, 7312, 7313, 7315, 7314, 7313, 7315, 7315,
        // 0xB0-0xBF
        31, 1102, 3058, 5023, 7075, 9170, 11333, 13614, 15674, 15668, 15671, 15675, 15673, 15668, 15670, 15673,
        // 0xC0-0xCF
        30, 31, 31, 31, 31, 31, 31, 31, 31, 31, 30, 30, 30, 31, 31, 31,
        // 0xD0-0xDF
        31, 562, 1528, 2485, 3465, 4457, 5451, 6467, 7339, 7338, 7337, 7337, 7338, 7339, 7338, 7336,
        // 0xE0-0xEF
        31, 542, 1508, 2465, 3445, 4438, 5432, 6444, 7317, 7319, 7317, 7316, 7318, 7319, 7318, 7318,
        // 0xF0-0xFF
        30, 1103, 3061, 5025, 7076, 9176, 11341, 13621, 15676, 15679, 15680, 15681, 15677, 15677, 15680, 15681
        };


MEM *ted;

int             m_Sound;

int             m_MixingFreq;
int             m_BufferLength;

int             m_Volume;
int             m_Snd1Status;
int             m_Snd2Status;
int             m_SndNoiseStatus;
int             m_DAStatus;
unsigned short  m_Freq1;
unsigned short  m_Freq2;
int             m_NoiseCounter;

int             Side1, Side2;

int             Val1_1, Val1_2, Val2_1, Val2_2;

int             WaveCounter1_1;
int             WaveCounter1_2;
int             WaveCounter2_1;
int             WaveCounter2_2;
int             WaveCounter3;

int             WaveLen1_1;
int             WaveLen1_2;
int             WaveLen2_1;
int             WaveLen2_2;

char            noise[9][1024]; // 0-8
Uint8		buffer[4096];   // max buffer size
Uint16		TEDSample[256], TEDSample2[256];
Uint32		dwPlayPos = 0, dwBytesToFill;

// frequency lookup table
Uint8			digitable[256];
SDL_AudioSpec		*audiohwspec;


void audio_callback(void *userdata, Uint8 *stream, int len)
{
	register int i;

	for(i = 0; i < len; ++i)
      stream[i] = buffer[dwPlayPos++];
	//  if ( dwPlayPos >len  )
		dwPlayPos = 0;
    //}
}

void init_audio(class MEM *p)
{
    SDL_AudioSpec *desired, *obtained;
    int i;
    float j,k;

	m_MixingFreq = 44100;//44100 22050
	m_BufferLength = 1024; // 4096 in Windows version...
	for (i=128; i<256; ++i)
		digitable[i]=digivalues[i&0x7F]>>6;

	desired =(SDL_AudioSpec *) malloc(sizeof(SDL_AudioSpec));
	obtained=(SDL_AudioSpec *) malloc(sizeof(SDL_AudioSpec));

	desired->freq		= 44100;
	desired->format		= AUDIO_U8;
	desired->channels	= 1;
	desired->samples	= m_BufferLength;
	desired->callback	= audio_callback;
	desired->userdata	= NULL;

	if (SDL_OpenAudio( desired, obtained)) {
		fprintf(stderr,"SDL_OpenAudio failed!\n");
		return;
	} else {
		fprintf(stderr,"SDL_OpenAudio success!\n");
		if ( obtained == NULL ) {
			fprintf(stderr, "Great! We have our desired audio format!\n");
			audiohwspec = desired;
			free(obtained);
		} else {
			fprintf(stderr, "Oops! Failed to get desired audio format!\n");
			audiohwspec = obtained;
			free(desired);
		}
	}
 {
   char name[32];
  	printf("Using audio driver: %s\n", SDL_AudioDriverName(name, 32));
 }
	SDL_PauseAudio(0);
    Side1 = 0;
    Side2 = 0;
    WaveCounter1_1 = 0;
    WaveCounter1_2 = 0;
    WaveCounter2_1 = 0;
    WaveCounter2_2 = 0;
    WaveCounter3   = 0;
    m_NoiseCounter = 0;


    for ( i=0; i<1024; i++) {

       k = ((float) rand()*128)/(float) RAND_MAX;
       //fprintf(stderr,"%d\n ",RAND_MAX);
       j = (int) (k) - 64;
       //fprintf(stderr,"%d ",i);
       //fprintf(stderr,"%f ",j);
       //fprintf(stderr,"%f \n",k);
       noise[0][i] = 0;
       noise[1][i] = (char) (j*0.125);
       noise[2][i] = (char) (j*0.25);
       noise[3][i] = (char) (j*0.325);
       noise[4][i] = (char) (j*0.5);
       noise[5][i] = (char) (j*0.625);
       noise[6][i] = (char) (j*0.75);
       noise[7][i] = (char) (j*0.875);
        noise[8][i] = (char) j;
    }

    m_Sound = 1;

    //SDL_PauseAudio(1);
    ted = (MEM *) p;
}

void render_audio(void)
{

    Uint32		result1,result2;
    Uint32		lockpos ;

    m_Volume = ted->TEDVolume;
    m_Freq1 = ted->TEDfreq1;
    m_Freq2 = ted->TEDfreq2;
    m_Snd1Status = ted->TEDChannel1;
    m_Snd2Status = ted->TEDChannel2;
    m_SndNoiseStatus = ted->TEDNoise;
    m_DAStatus = ted->TEDDA;

    // Channel 1 --------------------------------------
    if (m_Snd1Status) {
        Val1_1 = 0 + ( m_Volume *(64/9) );
        Val1_2 = 0 - ( m_Volume *(64/9) );
    } else
        Val1_1 = Val1_2 = 0;

    float freq1 = (float) (111860.781/(1024.0 - ((m_Freq1+1)&0x3FF ))*2);
    WaveLen1_1 = WaveLen1_2 = m_MixingFreq / freq1;
    // Correct big frequency jumps...
    if ((ceil((float)m_MixingFreq / freq1)-((float)m_MixingFreq / freq1))<0.5f) WaveLen1_2++;

    // Channel 2 --------------------------------------
    if (m_Snd2Status) {
        Val2_1 = 0 + ( m_Volume *(64/9) );
        Val2_2 = 0 - ( m_Volume *(64/9) );
    } else
        Val2_1 = Val2_2 = 0;

    float freq2 = (111860.781/(1024.0-((m_Freq2+1)&0x3FF ))*2);
    WaveLen2_1 = WaveLen2_2 = m_MixingFreq / freq2;
    // Correct big frequency jumps...
    if ((ceil((float)m_MixingFreq / freq2)-((float)m_MixingFreq / freq2))<0.5f) WaveLen2_2++;

    // Rendering...

///////////
    // Calculate the buffer...
    for (int i=0;i<m_BufferLength;i++) {
      if (!m_DAStatus) {// digi?
	  // Channel 1
          if (!Side1) {
	    result1 = Val1_1;
            if (WaveCounter1_1 == WaveLen1_1) result1>>=1; // smooth square
            WaveCounter1_1--;
            if (WaveCounter1_1<=0) {
	      result1>>=1; // smooth square
              WaveCounter1_2 = WaveLen1_2;
              if (WaveCounter1_2) Side1 = 1-Side1;
            }
          } else {
	    result1 = Val1_2;
            if (WaveCounter1_2 == WaveLen1_2) result1>>=1; // smooth square
            WaveCounter1_2--;
            if (WaveCounter1_2<=0) {
	      result1>>=1; // smooth square
              WaveCounter1_1 = WaveLen1_1;
              if (WaveCounter1_1) Side1 = 1-Side1;
            }
          }

          // Channel 2
          if (m_Snd2Status) {
	    if (!Side2) {
	      result2 = Val2_1;
              if (WaveCounter2_1 == WaveLen2_1) result2>>=1; // smooth square
              WaveCounter2_1--;
              if (WaveCounter2_1<=0) {
		result2>>=1; // smooth square
                WaveCounter2_2 = WaveLen2_2;
                if (WaveCounter2_2) Side2 = 1-Side2;
              }
            } else {
	      result2 = Val2_2;
              if (WaveCounter2_2 == WaveLen2_2) result2>>=1; // smooth square
              WaveCounter2_2--;
              if (WaveCounter2_2<=0) {
		result2>>=1; // smooth square
                WaveCounter2_1 = WaveLen2_1;
                if (WaveCounter2_1) Side2 = 1-Side2;
              }
            }
          } else
	    if (m_SndNoiseStatus) {
		WaveCounter3--;
		if (WaveCounter3<=0) {
		  WaveCounter3 = WaveLen2_1>>1; // Same freq as channel 2...
		  m_NoiseCounter++;
		  if (m_NoiseCounter>1023)
		    m_NoiseCounter=0;
		}
		result2 = noise[m_Volume][m_NoiseCounter];
	    } else
	      result2 = 0;
	  //SDL_LockAudio();
          buffer[i] = (128 + result1 + result2);
	  //SDL_UnlockAudio();
      } else {
	//SDL_LockAudio();
        buffer[i] = digitable[ted->Ram[0xFF11]];
	//SDL_UnlockAudio();
      }
    }   // for
}

void close_audio()
{
	if ( audiohwspec )
		free( audiohwspec );
	SDL_CloseAudio();
}
