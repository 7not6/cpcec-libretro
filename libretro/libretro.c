#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

#include "libretro.h"

#ifdef HAVE_LIBCO
#define LIBCO_C 
#include "libco/libco.h"
#include "libco/libco.c"
cothread_t mainThread;
cothread_t emuThread;
#endif

#define PREFIX_CORE "libretro-"
#define SUFFIX_OPTION "_analog"
#define EMULATOR_CORENAME_STR PREFIX_CORE EMU_NAME
#define EMULATOR_OPTIONNAME_STR EMU_NAME SUFFIX_OPTION

FILE * inputFile;
int  INLOG=1;

#include "libretro-core.h"

extern int pre_main(const char *argv);

char RPATH[512];
char RETRO_DIR[512];
const char *retro_save_directory;
const char *retro_system_directory;
const char *retro_content_directory;
char retro_system_tos[512];

bool opt_analog;

int retrow=768;
int retroh=536;

#ifndef HAVE_LIBCO
extern void RetroLoop();
int CPULOOP=1;
#endif
int pauseg=0;

uint32_t *videoBuffer;

static retro_video_refresh_t video_cb;
static retro_environment_t environ_cb;
static retro_audio_sample_batch_t audio_batch_cb;
retro_audio_sample_t audio_cb;
retro_input_poll_t input_poll_cb;
retro_input_state_t input_state_cb;

int gui_poll_events(void);

void retro_set_video_refresh(retro_video_refresh_t cb) { video_cb = cb; }
void retro_set_audio_sample(retro_audio_sample_t cb) { audio_cb  =cb; }
void retro_set_audio_sample_batch(retro_audio_sample_batch_t cb) { audio_batch_cb = cb; }
void retro_set_input_poll(retro_input_poll_t cb) { input_poll_cb = cb; }
void retro_set_input_state(retro_input_state_t cb) { input_state_cb = cb; }

int AUDIOPAUSE=0;
int sdlinitok=0;

#if defined(_WIN32)
#include <time.h>
#include <sys/time.h>
#define PATHSEP '\\'
#else
#define PATHSEP '/'
#endif

void libretro_fillsample(int16_t *buf,int len){

	if(sdlinitok==0|| AUDIOPAUSE==1)return;
	len=len/4;
	for(int i=0;i<len;i++)
			audio_cb(*buf++,*buf++);	
}

char Key_Sate[512];
char old_Key_Sate[512],Ui_Sate[512];
    
int mbt[16]={0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
int ombt[16]={0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
//   RETRO      B    Y    SLT  STA  UP   DWN  LEFT RGT  A    X    L    R    L2   R2   L3   R3
//   INDEX      0    1    2    3    4    5    6    7    8    9    10   11   12   13   14   15

#ifdef PS2
#include <sys/types.h>
#include <sys/time.h>
#include <time.h>
#endif

#ifdef HAVE_LIBCO
long frame=0;
unsigned long  Ktime=0 , LastFPSTime=0;
//NO SURE FIND BETTER WAY TO COME BACK IN MAIN THREAD IN HATARI GUI
long GetTicks(void)
{ // in MSec
#ifndef _ANDROID_

#ifdef __CELLOS_LV2__

   //#warning "GetTick PS3\n"

   unsigned long	ticks_micro;
   uint64_t secs;
   uint64_t nsecs;

   sys_time_get_current_time(&secs, &nsecs);
   ticks_micro =  secs * 1000000UL + (nsecs / 1000);

   return ticks_micro/1000;
#else
   struct timeval tv;
   gettimeofday (&tv, NULL);
   return (tv.tv_sec*1000000 + tv.tv_usec)/1000;
#endif

#else

   struct timespec now;
   clock_gettime(CLOCK_MONOTONIC, &now);
   return (now.tv_sec*1000000 + now.tv_nsec/1000)/1000;
#endif

} 

int slowdown=0;

int gui_poll_events(void)
{
   Ktime = GetTicks();

   if(Ktime - LastFPSTime >= 1000/50)
   { 
   
	  slowdown=0;

      frame++; 
      LastFPSTime = Ktime;		
      co_switch(mainThread);
      return 1;
   }

   slowdown++;
   
   return 0;
}

static void retro_wrap_emulator()
{    
    	
LOGI("WRAP EMU THD\n");
   pre_main(RPATH);


LOGI("EXIT EMU THD\n");
   pauseg=-1;sdlinitok=0;
retro_deinit();
   //environ_cb(RETRO_ENVIRONMENT_SHUTDOWN, 0); 

   // Were done here
   co_switch(mainThread);

   // Dead emulator, but libco says not to return
   while(true)
   {
      LOGI("Running a dead emulator.");
      co_switch(mainThread);
   }

}
#endif

//Args for experimental_cmdline
static char ARGUV[64][1024];
static unsigned char ARGUC=0;

// Args for Core
static char XARGV[64][1024];
static const char* xargv_cmd[64];
int PARAMCOUNT=0;

extern int bmain(int argc, char *argv[]);

void parse_cmdline( const char *argv );

void Add_Option(const char* option)
{
   static int first=0;

   if(first==0)
   {
      PARAMCOUNT=0;	
      first++;
   }

   sprintf(XARGV[PARAMCOUNT++],"%s\0",option);
}

int pre_main(const char *argv)
{
   int i;
   bool Only1Arg;

   parse_cmdline(argv); 

   Only1Arg = (strcmp(ARGUV[0],EMU_NAME) == 0) ? 0 : 1;
    	
   for (i = 0; i<64; i++)
      xargv_cmd[i] = NULL;


   if(Only1Arg)
   {  
      int cfgload=0;

      Add_Option(EMU_NAME);

      if (strlen(RPATH) >= strlen("cfg")){
	if(!strcasecmp(&RPATH[strlen(RPATH)-strlen("cfg")], "cfg")){
			Add_Option("-c");
			cfgload=1;
	}
      }

      if(cfgload==0) {
		//Add_Option("-verbose");
		//Add_Option(retro_system_tos);
		//Add_Option("-8");
      }

      Add_Option(RPATH);
   }
   else
   { // Pass all cmdline args
      for(i = 0; i < ARGUC; i++)
         Add_Option(ARGUV[i]);
   }

   for (i = 0; i < PARAMCOUNT; i++)
   {
      xargv_cmd[i] = (char*)(XARGV[i]);
      LOGI("%2d  %s\n",i,XARGV[i]);
   }
    	
   bmain(PARAMCOUNT,( char **)xargv_cmd); 

   xargv_cmd[PARAMCOUNT - 2] = NULL;

   return 0;
}

void parse_cmdline(const char *argv)
{
	char *p,*p2,*start_of_word;
	int c,c2;
	static char buffer[512*4];
	enum states { DULL, IN_WORD, IN_STRING } state = DULL;
	
	strcpy(buffer,argv);
	strcat(buffer," \0");

	for (p = buffer; *p != '\0'; p++)
   {
      c = (unsigned char) *p; /* convert to unsigned char for is* functions */
      switch (state)
      {
         case DULL: /* not in a word, not in a double quoted string */
            if (isspace(c)) /* still not in a word, so ignore this char */
               continue;
            /* not a space -- if it's a double quote we go to IN_STRING, else to IN_WORD */
            if (c == '"')
            {
               state = IN_STRING;
               start_of_word = p + 1; /* word starts at *next* char, not this one */
               continue;
            }
            state = IN_WORD;
            start_of_word = p; /* word starts here */
            continue;
         case IN_STRING:
            /* we're in a double quoted string, so keep going until we hit a close " */
            if (c == '"')
            {
               /* word goes from start_of_word to p-1 */
               //... do something with the word ...
               for (c2 = 0,p2 = start_of_word; p2 < p; p2++, c2++)
                  ARGUV[ARGUC][c2] = (unsigned char) *p2;
               ARGUC++; 

               state = DULL; /* back to "not in word, not in string" state */
            }
            continue; /* either still IN_STRING or we handled the end above */
         case IN_WORD:
            /* we're in a word, so keep going until we get to a space */
            if (isspace(c))
            {
               /* word goes from start_of_word to p-1 */
               //... do something with the word ...
               for (c2 = 0,p2 = start_of_word; p2 <p; p2++,c2++)
                  ARGUV[ARGUC][c2] = (unsigned char) *p2;
               ARGUC++; 

               state = DULL; /* back to "not in word, not in string" state */
            }
            continue; /* either still IN_WORD or we handled the end above */
      }	
   }
}
void texture_init(){
        memset(videoBuffer, 0, sizeof(videoBuffer));
} 

void retro_set_environment(retro_environment_t cb)
{
   environ_cb = cb;

   struct retro_variable variables[] = {
      {
         EMULATOR_OPTIONNAME_STR,"Use Analog; OFF|ON",
      },
      { NULL, NULL },
   };

   cb(RETRO_ENVIRONMENT_SET_VARIABLES, variables);
}

static void update_variables(void)
{
   struct retro_variable var = {0};

 
   var.key = EMULATOR_OPTIONNAME_STR;
   var.value = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      fprintf(stderr, "value: %s\n", var.value);
      if (strcmp(var.value, "OFF") == 0)
         opt_analog = false;
      if (strcmp(var.value, "ON") == 0)
         opt_analog = true;

        fprintf(stderr, "[%s]: Analog: %s.\n",EMULATOR_CORENAME_STR,opt_analog?"ON":"OFF");
   }

}

void update_input()
{
	
	input_poll_cb();
	
	for(int i=0;i<16;i++)
		mbt[i]= input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, i);
		
	for(int i=0;i<322;i++)
        Key_Sate[i]=input_state_cb(0, RETRO_DEVICE_KEYBOARD, 0,i) ? 0x80: 0;	
        
   if(slowdown==0)memcpy(Ui_Sate,Key_Sate , sizeof(Key_Sate) );       
   else memset(Ui_Sate,0, sizeof(Key_Sate) );     	
   
}


#if 0
static void keyboard_cb(bool down, unsigned keycode, uint32_t character, uint16_t mod)
{
}
#endif

/************************************
 * libretro implementation
 ************************************/

void retro_get_system_info(struct retro_system_info *info)
{
   	memset(info, 0, sizeof(*info));
	info->library_name = EMU_NAME;
	info->library_version = "0.1";
	info->need_fullpath = true;
	info->valid_extensions = "*";
}


void retro_get_system_av_info(struct retro_system_av_info *info)
{
//FIXME handle vice PAL/NTSC
   struct retro_game_geometry geom = { retrow, retroh,retrow, retroh ,4.0 / 3.0 };
   struct retro_system_timing timing = { 50.0, 44100.0 };

   info->geometry = geom;
   info->timing   = timing;
   LOGI("GETAV INFO\n");
}

void retro_set_controller_port_device(unsigned port, unsigned device)
{
    (void)port;
    (void)device;
}

size_t retro_serialize_size(void)
{
	return 0;
}

bool retro_serialize(void *data, size_t size)
{
    return false;
}

bool retro_unserialize(const void *data, size_t size)
{
    return false;
}

void retro_cheat_reset(void)
{}

void retro_cheat_set(unsigned index, bool enabled, const char *code)
{
    (void)index;
    (void)enabled;
    (void)code;
}

bool retro_load_game(const struct retro_game_info *info)
{    
	const char *full_path;

    full_path = info->path;

	strcpy(RPATH,full_path);

	LOGI("LOAD EMU\n");

    	return true;
}

bool retro_load_game_special(unsigned game_type, const struct retro_game_info *info, size_t num_info)
{
    (void)game_type;
    (void)info;
    (void)num_info;
    return false;
}

void retro_unload_game(void)
{
     pauseg=0;
}

unsigned retro_get_region(void)
{
    return RETRO_REGION_NTSC;
}

unsigned retro_api_version(void)
{
    return RETRO_API_VERSION;
}

void *retro_get_memory_data(unsigned id)
{
    return NULL;
}

size_t retro_get_memory_size(unsigned id)
{
    return 0;
}

void retro_init(void)
{
   const char *system_dir = NULL;

	if (strncmp(EMU_NAME,"msxec", 5) == 0 || strncmp(EMU_NAME,"zxsec", 5) == 0) {	
		retrow=(40<<4);
		retroh=(30<<4);	
	}
	else {		
		retrow=(48<<4);
		retroh=(67<<3);		
	}
	
    inputFile = fopen( DBGFILE, "wt" );
   
	if ( inputFile == NULL ) {
       INLOG=0;           
    }
    
   memset(Key_Sate,0,512);
   memset(old_Key_Sate,0,512);
    
   if (environ_cb(RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY, &system_dir) && system_dir)
   {
      // if defined, use the system directory			
      retro_system_directory=system_dir;		
   }		   

   const char *content_dir = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_CONTENT_DIRECTORY, &content_dir) && content_dir)
   {
      // if defined, use the system directory			
      retro_content_directory=content_dir;		
   }			

   const char *save_dir = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY, &save_dir) && save_dir)
   {
      // If save directory is defined use it, otherwise use system directory
      retro_save_directory = *save_dir ? save_dir : retro_system_directory;      
   }
   else
   {
      // make retro_save_directory the same in case RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY is not implemented by the frontend
      retro_save_directory=retro_system_directory;
   }

   if(retro_system_directory==NULL)sprintf(RETRO_DIR, "%s%c\0",".",PATHSEP);
   else sprintf(RETRO_DIR, "%s%c\0", retro_system_directory,PATHSEP);

   //sprintf(retro_system_tos, "%s/tos.img\0",RETRO_DIR);

   LOGI("Retro SYSTEM_DIRECTORY %s\n",retro_system_directory);
   LOGI("Retro SAVE_DIRECTORY %s\n",retro_save_directory);
   LOGI("Retro CONTENT_DIRECTORY %s\n",retro_content_directory);
   
   fprintf(inputFile,"Retro SYSTEM_DIRECTORY %s\n",retro_system_directory);
   fprintf(inputFile,"Retro SAVE_DIRECTORY %s\n",retro_save_directory);
   fprintf(inputFile,"Retro CONTENT_DIRECTORY %s\n",retro_content_directory);

    enum retro_pixel_format fmt = RETRO_PIXEL_FORMAT_XRGB8888;
    if (!environ_cb(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &fmt))
    {
        fprintf(stderr, "Pixel format XRGB8888 not supported by platform, cannot use.\n");
        exit(0);
    }

   struct retro_input_descriptor inputDescriptors[] = {
		{ 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_A, "A" },
		{ 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_B, "B" },
		{ 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_X, "X" },
		{ 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_Y, "Y" },
		{ 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_SELECT, "Select" },
		{ 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_START, "Start" },
		{ 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_RIGHT, "Right" },
		{ 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_LEFT, "Left" },
		{ 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_UP, "Up" },
		{ 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_DOWN, "Down" },
		{ 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R, "R" },
		{ 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L, "L" },
		{ 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R2, "R2" },
		{ 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L2, "L2" },
		{ 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R3, "R3" },
		{ 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L3, "L3" }
	};
	environ_cb(RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS, &inputDescriptors);

/*
    struct retro_keyboard_callback cbk = { keyboard_cb };
    environ_cb(RETRO_ENVIRONMENT_SET_KEYBOARD_CALLBACK, &cbk);
*/
  	update_variables();

    
#ifdef HAVE_LIBCO
   if(!emuThread && !mainThread)
   {
      mainThread = co_active();
      emuThread = co_create(65536*sizeof(void*), retro_wrap_emulator);
   }
#endif

}

//extern bool bQuitProgram;
 extern int session_event;

void retro_deinit(void)
{ 
 	
sdlinitok==0;
#ifdef HAVE_LIBCO
   session_event=0x0080;
   co_switch(emuThread);
LOGI("retro_deinit emu %x \n",session_event);
  // main_exit();
   co_switch(mainThread);
LOGI("retro_deinit main %x\n",session_event);
   if(emuThread)
   {	 
      co_delete(emuThread);
      emuThread = 0;
   }
#else
	//Quit_Hatari();
#endif
        LOGI("Retro DeInit\n");
        
    if(INLOG==1){
       	fprintf(inputFile, "End log %s core 2.\n",EMULATOR_CORENAME_STR);
      	fclose( inputFile );
 	}
 	
}

void retro_reset(void)
{
}

void retro_run(void)
{
	bool updated = false;
   
	if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE, &updated) && updated)
      		update_variables(); 
      		
#ifndef HAVE_LIBCO
        static bool firstcall=true;
	if(firstcall){
		firstcall=false;

		pre_main(RPATH);
		sdlinitok=1;
	}
    	
	RetroLoop();
	//LOGI("frame\n");	
#else
    static bool firstcall=true;
	if(firstcall){
		firstcall=false;
		sdlinitok=1;	    	
	}
#endif
	update_input();

    video_cb(videoBuffer, retrow, retroh, retrow << 2);
#ifdef HAVE_LIBCO
 
   co_switch(emuThread);
#endif
}

