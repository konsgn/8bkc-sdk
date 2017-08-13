
#include <stdlib.h>
#include <stdio.h>

#include "rom/ets_sys.h"
#include "ssd1331.h"
#include "fb.h"
#include "lcd.h"
#include <string.h>
#include "menu.h"

#include "esp_task_wdt.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

#if 0

static uint16_t *frontbuff, *backbuff;
static volatile uint16_t *toRender=NULL;
static volatile uint16_t *overlay=NULL;
struct fb fb;
static SemaphoreHandle_t renderSem;

static bool doShutdown=false;

void vid_preinit()
{
}

void vid_init()
{
	frontbuff=malloc(160*144*2);
	backbuff=malloc(160*144*2);
	fb.w = 160;
	fb.h = 144;
	fb.pelsize = 2;
	fb.pitch = 160*2;
	fb.ptr = (unsigned char*)frontbuff;
	fb.enabled = 1;
	fb.dirty = 0;

	fb.indexed = 0;
	fb.cc[0].r = fb.cc[2].r = 3;
	fb.cc[1].r = 2;
	fb.cc[0].l = 11;
	fb.cc[1].l = 5;
	fb.cc[2].l = 0;
	
	gbfemtoMenuInit();
	memset(frontbuff, 0, 160*144*2);
	memset(backbuff, 0, 160*144*2);

	ets_printf("Video inited.\n");
}


void vid_close()
{
	doShutdown=true;
	xSemaphoreGive(renderSem);
}

void vid_settitle(char *title)
{
}

void vid_setpal(int i, int r, int g, int b)
{
}

extern int patcachemiss, patcachehit, frames;


void vid_begin()
{
//	vram_dirty(); //use this to find out a viable size for patcache
	frames++;
	patcachemiss=0;
	patcachehit=0;
	esp_task_wdt_feed();
}

void vid_end()
{
	overlay=NULL;
	toRender=(uint16_t*)fb.ptr;
	xSemaphoreGive(renderSem);
	if (fb.ptr == (unsigned char*)frontbuff ) {
		fb.ptr = (unsigned char*)backbuff;
	} else {
		fb.ptr = (unsigned char*)frontbuff;
	}
//	printf("Pcm %d pch %d\n", patcachemiss, patcachehit);
}

uint32_t *vidGetOverlayBuf() {
	return (uint32_t*)fb.ptr;
}

void vidRenderOverlay() {
	overlay=(uint16_t*)fb.ptr;
	if (fb.ptr == (unsigned char*)frontbuff ) toRender=(uint16_t*)backbuff; else toRender=(uint16_t*)frontbuff;
}

void kb_init()
{
}

void kb_close()
{
}

void kb_poll()
{
}

void ev_poll()
{
	kb_poll();
}


uint16_t oledfb[96*64];

int getAvgPix(uint16_t* bufs, int pitch, int x, int y) {
	int col;
	//Red power LED is simulated by a clump of red pixels in the margin
	if (x<-8 && y>35 && y<45) return 0xf800;

	if (x<0 || x>=160) return 0;
	//16-bit: E79C
	//15-bit: 739C
	col=(bufs[x+(y*(pitch>>1))]&0xE79C)>>2;
	col+=(bufs[(x+1)+(y*(pitch>>1))]&0xE79C)>>2;
	col+=(bufs[x+((y+1)*(pitch>>1))]&0xE79C)>>2;
	col+=(bufs[(x+1)+((y+1)*(pitch>>1))]&0xE79C)>>2;
	return col&0xffff;
}


int addOverlayPixel(uint16_t p, uint32_t ov) {
	int or, og, ob, a;
	int br, bg, bb;
	int r,g,b;
	br=((p>>11)&0x1f)<<3;
	bg=((p>>5)&0x3f)<<2;
	bb=((p>>0)&0x1f)<<3;

	a=(ov>>24)&0xff;
	//hack: Always show background darker
	a=(a/2)+128;

	ob=(ov>>16)&0xff;
	og=(ov>>8)&0xff;
	or=(ov>>0)&0xff;

	r=(br*(256-a))+(or*a);
	g=(bg*(256-a))+(og*a);
	b=(bb*(256-a))+(ob*a);

	return ((r>>(3+8))<<11)+((g>>(2+8))<<5)+((b>>(3+8))<<0);
}

//This thread runs on core 1.
void gnuboy_esp32_videohandler() {
	int x, y;
	renderSem=xSemaphoreCreateBinary();
	uint16_t *oledfbptr;
	uint16_t c;
	uint32_t *ovl;
	volatile uint16_t *rendering;
	printf("Video thread running\n");
	memset(oledfb, 0, sizeof(oledfb));
	while(!doShutdown) {
		if (toRender==NULL) xSemaphoreTake(renderSem, portMAX_DELAY);
		rendering=toRender;
		ovl=(uint32_t*)overlay;
		oledfbptr=oledfb;
		int hc=0;
		for (y=0; y<64; y++) {
			if (((y+1)&14)==0) hc++;
			for (x=0; x<96; x++) {
				c=getAvgPix((uint16_t*)rendering, 160*2, (x*2)-16, (y*2)+hc+2);
				if (ovl) c=addOverlayPixel(c, *ovl++);
				*oledfbptr++=(c>>8)+((c&0xff)<<8);
			}
		}
		ssd1331SendFB(oledfb);
	}
	ssd1331PowerDown();
	vTaskDelete(NULL);
}





#endif