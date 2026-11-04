
#ifndef _libretrocore_h
#define _libretrocore_h

#include "libco/libco.h"
extern cothread_t mainThread;
extern cothread_t emuThread;

extern FILE * inputFile;
extern int INLOG;
extern long GetTicks(void);
extern int gui_poll_events(void);

extern int mbt[16],ombt[16];
extern char RETRO_DIR[512],Key_Sate[512],old_Key_Sate[512],Ui_Sate[512];
extern void libretro_fillsample(int16_t *buf,int len);
extern int slowdown;

#include <stdlib.h>
#include <stdint.h>
extern uint32_t *videoBuffer;
typedef int8_t Sint8;
typedef uint8_t Uint8;
typedef int16_t Sint16;
typedef uint16_t Uint16;
typedef int32_t Sint32;
typedef uint32_t Uint32;
typedef struct Rect
{
    int x, y;
    int w, h;
} Rect;

//#define PCSX2 1
#ifdef PCSX2
#define DBGFILE "host:/csfcecdebug.txt"
#else
#define DBGFILE "./csfcecdebug.txt"
#endif

#define BAR_HELPER(fmt, ...) \
do { \
  if(INLOG!=0){ \
  fprintf(inputFile,fmt "\n%s", __VA_ARGS__); \
  	fflush( inputFile ); \
    fclose( inputFile ); \
    inputFile = fopen( DBGFILE, "a" ); \
    if ( inputFile == NULL ) INLOG=0; \
  } \
} while (0);

#define LOGI(...) BAR_HELPER(__VA_ARGS__, "")

#endif /* _libretrocore_h */

