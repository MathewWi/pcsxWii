/* DEBUG.c - DEBUG interface
   by Mike Slegeir for Mupen64-GC
 */

#include <gccore.h>
#include <string.h>
#include <stdio.h>
#include <fat.h>
#include <stdlib.h>
#include <unistd.h>
#include <ogc/lwp_watchdog.h>
#include "DEBUG.h"
#include "TEXT.h"
//#include "usb.h"

char text[DEBUG_TEXT_HEIGHT][DEBUG_TEXT_WIDTH];
static char printToSD = 1;

#ifdef SHOW_DEBUG
char txtbuffer[1024];
long long texttimes[DEBUG_TEXT_HEIGHT];
static void check_heap_space(void){
	sprintf(txtbuffer,"%dKB MEM1 available", SYS_GetArena1Size()/1024);
	sprintf(txtbuffer,"%dKB MEM2 available", SYS_GetArena2Size()/1024);
	DEBUG_print(txtbuffer,DBG_MEMFREEINFO);

}
#endif

void DEBUG_update(void) {
	#ifdef SHOW_DEBUG
	int i;
	long long nowTick = gettime();
	for(i=0; i<DEBUG_TEXT_HEIGHT; i++){
		if(diff_sec(texttimes[i],nowTick)>=DEBUG_STRING_LIFE) 
		{
			memset(text[i],0,DEBUG_TEXT_WIDTH);
		}
	}
	check_heap_space();
	#endif
}

static int flushed = 0;
static int writtenbefore = 0;
static int amountwritten = 0;
//char *dump_filename = "dev0:\\PSXISOS\\debug.txt";
static const char *dump_filename = "/PSXISOS/debug.txt";
static FILE* fdebug = NULL;
void DEBUG_print(const char* string, int pos){

	#ifdef SHOW_DEBUG
		if(pos == DBG_USBGECKO) {
			#ifdef PRINTGECKO
			if(!flushed){
				usb_flush(1);
				flushed = 1;
			}
			int size = strlen(string);
			usb_sendbuffer_safe(1, &size,4);
			usb_sendbuffer_safe(1, string,size);
			#endif
		}
		else if(pos == DBG_SDGECKOOPEN) {
#ifdef SDPRINT
			if(!f && printToSD)
				fdebug = fopen( dump_filename, "wb" );
#endif
		}
		else if(pos == DBG_SDGECKOAPPEND) {
#ifdef SDPRINT
			if(!fdebug && printToSD)
				fdebug = fopen( dump_filename, "ab" );
#endif
		}
		else if(pos == DBG_SDGECKOCLOSE) {
#ifdef SDPRINT
			if(fdebug)
			{
				fclose(fdebug);
				fdebug = NULL;
			}
#endif
		}
		else if(pos == DBG_SDGECKOPRINT) {			
#ifdef SDPRINT
			if(!f || (printToSD == 0))
				return;
			fwrite(string, 1, strlen(string), f);
#endif
		}
		else {
			memset(text[pos],0,DEBUG_TEXT_WIDTH);
			strncpy(text[pos], string, DEBUG_TEXT_WIDTH);
			memset(text[DEBUG_TEXT_WIDTH-1],0,1);
			texttimes[pos] = gettime();
		}
	#endif
	
}


#define MAX_STATS 20
static unsigned int stats_buffer[MAX_STATS];
static unsigned int avge_counter[MAX_STATS];
void DEBUG_stats(int stats_id, char *info, unsigned int stats_type, unsigned int adjustment_value) 
{
	#ifdef SHOW_DEBUG
	switch(stats_type)
	{
		case STAT_TYPE_ACCUM:	//accumulate
			stats_buffer[stats_id] += adjustment_value;
			break;
		case STAT_TYPE_AVGE:	//average
			avge_counter[stats_id] += 1;
			stats_buffer[stats_id] += adjustment_value;
			break;
		case STAT_TYPE_CLEAR:
			if(stats_type & STAT_TYPE_AVGE)
				avge_counter[stats_id] = 0;
			stats_buffer[stats_id] = 0;
			break;
		
	}
	unsigned int value = stats_buffer[stats_id];
	if(stats_type == STAT_TYPE_AVGE) value /= avge_counter[stats_id];
	
	sprintf(txtbuffer,"%s [ %i ]", info, value);
	DEBUG_print(txtbuffer,DBG_STATSBASE+stats_id);
	#endif
}

