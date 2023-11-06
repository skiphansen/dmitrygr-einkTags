#ifndef _CHARS_H_
#define _CHARS_H_

#include <stdbool.h>
#include <stdint.h>


#define CHAR_WIDTH		8
#define CHAR_HEIGHT		16

struct CharCanvas {
	void* dst;
	uint32_t w, h, rowBytes, bpp;		//bpp must be apower of two <= 8
	bool flipV, flipH, msbFirst;
};

#define CHAR_COLOR_TRANSPARENT	0xffffffff

void charsDrawChar(const struct CharCanvas *canvas, uint8_t ch, int32_t x, int32_t y, uint32_t foreColor, uint32_t backColor, uint_fast8_t magnify);

#define CHAR_SIGNAL_PT1			(1)
#define CHAR_SIGNAL_PT2			(2)

#define CHAR_NO_SIGNAL_PT1		(3)
#define CHAR_NO_SIGNAL_PT2		(4)



#endif
