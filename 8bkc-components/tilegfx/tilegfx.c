/*
 Simple graphics library for the PocketSprite based on 8x8 tiles.
*/

#include "tilegfx.h"
#include "8bkc-hal.h"
#include <stdlib.h>
#include <string.h>

static uint16_t *fb;
static int fb_w=0, fb_h=0;

//Render a tile that is not clipped by the screen extremities
static void render_tile_full(uint16_t *dest, const uint16_t *tile) {
	for (int y=0; y<8; y++) {
		memcpy(dest, tile, 8*2);
		tile+=8;
		dest+=fb_w;
	}
}

//Render a tile that is / may be clipped by the screen extremities
static void render_tile_part(uint16_t *dest, const uint16_t *tile, int xstart, int ystart, const tilegfx_rect_t *clip) {
	for (int y=0; y<8; y++) {
		if (y+ystart>=clip->x && y+ystart<clip->y+clip->h) {
			for (int x=0; x<8; x++) {
				if (x+xstart>=clip->y && x+xstart<clip->x+clip->w) dest[x]=tile[x];
			}
		}
		tile+=8;
		dest+=fb_w;
	}
}


void tilegfx_tile_map_render(const tilegfx_map_t *tiles, int offx, int offy, const tilegfx_rect_t *dest) {
	//Make sure to wrap around if offx/offy aren't in the tile map
	offx%=(tiles->w*8);
	offy%=(tiles->h*8);
	if (offx<0) offx+=tiles->w*8;
	if (offy<0) offy+=tiles->h*8;

	//Embiggen rendering field to start at the edges of all corner tiles.
	//We'll cut off the bits outside of dest when we get there.
	int sx=dest->x-(offx&7);
	int sy=dest->y-(offy&7);
	int ex=sx+((dest->w+(offx&7)+7)&~7);
	int ey=sy+((dest->h+(offy&7)+7)&~7);

	//x and y are the real onscreen coords that may fall outside the framebuffer.
	int tileposy=((offy/8)*tiles->w);
	uint16_t *p=fb+(fb_w*sy)+sx;
	for (int y=sy; y<ey; y+=8) {
		int tileposx=offx/8;
		uint16_t *pp=p;
		for (int x=sx; x<ex; x+=8) {
			if (x < dest->x || y < dest->y || x+7 >= dest->x+dest->w || y+7 >= dest->y+dest->h) {
				render_tile_part(pp, &tiles->gfx[tiles->tiles[tileposx+tileposy]*64], 
							x - dest->x, y - dest->y, dest);
			} else {
				render_tile_full(pp, &tiles->gfx[tiles->tiles[tileposx+tileposy]*64]);
			}
			tileposx++;
			if (tileposx >= tiles->w) tileposx=0; //wraparound
			pp+=8; //we filled these 8 columns
		}
		tileposy+=tiles->w; //skip to next row
		if (tileposy >= tiles->h*tiles->w) tileposy-=tiles->h*tiles->w; //wraparound
		p+=fb_w*8; //we filled these 8 lines
	}
}


void tilegfx_init(int doublesize) {
	if (doublesize) {
		fb_w=KC_SCREEN_W*2;
		fb_h=KC_SCREEN_H*2;
	} else {
		fb_w=KC_SCREEN_W;
		fb_h=KC_SCREEN_H;
	}
	fb=malloc(fb_w*fb_h*2);
}

//32-bit pixel:
// p1              p2
// rrrrrggggggbbbbbrrrrrggggggbbbbb
//We try to do subpixel rendering, and to this effect, for the subpixel itself:
//r=r1*0.75+r2*0.25
//g=g1*0.5+g2*0.5
//b=b1*0.25+b2*0.75
//Obviously, we also have to average the two lines of pixels together to keep the aspect ratio.

//rrrr.0ggg.gg0b.bbb0
//To take the lsb off, and with 0xf7de

//Takes the double-sized fb, scales it back to something that can actually be rendered.
void undo_x2_scaling() {
	uint32_t *fbw=(uint32_t*)fb; //fb but accessed 2 16-bit pixels at a time
	uint16_t *fbp=fb;
	for (int y=0; y<KC_SCREEN_H; y++) {
		for (int x=0; x<KC_SCREEN_W; x++) {
			uint32_t p=fbw[0];
			uint32_t p2=fbw[KC_SCREEN_W]; //one line down
			//Colors are stored with the bytes swapped. We need to swap them back.
			p=((p&0xFF00FF00)>>8)|((p&0x00ff00ff)<<8);
			p2=((p2&0xFF00FF00)>>8)|((p2&0x00ff00ff)<<8);
			p=((p&0xf7def7de)>>1)+((p2&0xf7def7de)>>1); //average both pixels
			
			//It's possible to do this using some code doing bitshifts and parallel
			//multiplies and stuff to get the most speed out of the CPU. This is not
			//that code. Pray to the GCC gods that this gets optimized into anything sane.
			int r=(((p>>27)&0x1F)*3+((p>>11)&0x1f)*1)/4;
			int g=(((p>>21)&0x3F)+((p>>5)&0x3F))/2;
			int b=(((p>>16)&0x1F)*1+((p>>0)&0x1f)*3)/4;
			uint16_t c=(r<<11)+(g<<5)+(b<<0);
			*fbp++=(c<<8)|(c>>8);
			fbw++;
		}
		fbw+=KC_SCREEN_W; //skip the next line; we already read that.
	}
}

void tilegfx_flush() {
	if (fb_w!=KC_SCREEN_W) {
		undo_x2_scaling();
	}
	kchal_send_fb(fb);
}
