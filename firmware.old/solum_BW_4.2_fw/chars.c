#include "eeprom.h"
#include "chars.h"
#include "fw.h"

#define NUM_CHARS		256



static void charsPrvSetCanvasPixel(const struct CharCanvas *canvas, int32_t r, int32_t c, uint32_t color)
{
	uint32_t bitOfst, bitMask = (1 << canvas->bpp) - 1;
	uint8_t *dst = (uint8_t*)canvas->dst;
	
	if (r < 0 || r >= canvas->h)
		return;
	if (c < 0 || c >= canvas->w)
		return;
	
	if (canvas->flipV)
		r = canvas->h - r - 1;
	if (canvas->flipH)
		c = canvas->w - c - 1;
	
	dst += r * canvas->rowBytes;
	dst += c * canvas->bpp / 8;
	bitOfst = c * canvas->bpp % 8;
	
	if (canvas->msbFirst)
		bitOfst = 8 - bitOfst - canvas->bpp;
	
	*dst = ((*dst) & ~(bitMask << bitOfst)) | ((color & bitMask) << bitOfst);
}


void charsDrawChar(const struct CharCanvas *canvas, uint8_t chIn, int32_t x, int32_t y, uint32_t foreColor, uint32_t backColor, uint_fast8_t manifyIn)
{
	uint8_t imgInfoBuf[CHAR_WIDTH + 7 / 8 + 2];
	int32_t r, c, ch = chIn, mag = manifyIn;
	int32_t mr, mc;
	
	for (r = 0; r < CHAR_HEIGHT; r++) {
		
		//read char row (possibly over-read - no big deal)
		qspiRead(0, EEPROM_CHARS_IMGS_START + (CHAR_WIDTH * NUM_CHARS + 7) / 8 * r + (CHAR_WIDTH * ch) / 8, imgInfoBuf, sizeof(imgInfoBuf));
		
		for (c = 0; c < CHAR_WIDTH; c++) {
			
			uint32_t imgColumn = (CHAR_WIDTH * ch) % 8 + c, color;
			bool set;
			
			color = ((imgInfoBuf[imgColumn / 8] >> (7 - (imgColumn % 8))) & 1) ? foreColor : backColor;
			
			if (color == CHAR_COLOR_TRANSPARENT)
				continue;
			
			for (mr = 0; mr < mag; mr++) {
				
				int32_t er = r * mag + mr;
				
				for (mc = 0; mc < mag; mc++) {
					
					int32_t ec = c * mag + mc;
					
					charsPrvSetCanvasPixel(canvas, er + y, ec + x, color);
				}
			}
		}
	}
}


