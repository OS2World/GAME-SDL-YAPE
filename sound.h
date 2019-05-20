#ifndef _SOUND_H
#define _SOUND_H

#include "SDL/SDL.h"


class MEM;
void init_audio(MEM *p);
void render_audio(void);
void close_audio();

/*
extern bool Initialise(void);
extern void SetTED(void *p);

extern void SetMixingFreq(int n) { m_MixingFreq = n; }
extern void SetBufferLength(int n) { m_BufferLength = n; }
extern int  GetBufferLength(void) { return m_BufferLength; }
extern Uint8 GetDigiValue(unsigned char n);
extern void InitTedSamples ( void );
*/
#endif
