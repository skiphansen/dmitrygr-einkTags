#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#define COMPRESSION_BITPACKED_3x5_to_7	0x62700357	//3 pixels of 5 possible colors in 7 bits
#define COMPRESSION_BITPACKED_5x3_to_8	0x62700538	//5 pixels of 3 possible colors in 8 bits
#define COMPRESSION_BITPACKED_3x6_to_8	0x62700368	//3 pixels of 6 possible colors in 8 bits


struct BitmapFileHeader {
	uint8_t sig[2];
	uint32_t fileSz;
	uint8_t rfu[4];
	uint32_t dataOfst;
	uint32_t headerSz;			//40
	int32_t width;
	int32_t height;
	uint16_t colorplanes;		//must be one
	uint16_t bpp;
	uint32_t compression;
	uint32_t dataLen;			//may be 0
	uint32_t pixelsPerMeterX;
	uint32_t pixelsPerMeterY;
	uint32_t numColors;			//if zero, assume 2^bpp
	uint32_t numImportantColors;
	
} __attribute__((packed));

struct BitmapClutEntry {
	uint8_t b, g, r, x;
} __attribute__((packed));

enum EinkClut {
	EinkClutTwoBlacks = 0,
	EinkClutTwoBlacksAndYellow,
	EinkClutTwoBlacksAndRed,
	EinkClutTwoBlacksAndYellowPacked,
	EinkClutTwoBlacksAndRedPacked,
	EinkClutFourBlacks,
	EinkClutThreeBlacksAndYellow,
	EinkClutThreeBlacksAndRed,
	EinkClutFourBlacksAndYellowPacked,
	EinkClutFourBlacksAndRedPacked,
	EinkClutFiveBlacksAndYellowPacked,
	EinkClutFiveBlacksAndRedPacked,
	EinkClutEightBlacks,
	EinkClutSevenBlacksAndYellow,
	EinkClutSevenBlacksAndRed,
	EinkClutSixteenBlacks,
};

void getBytes(void* dstP, unsigned len)
{
	char *dst = (char*)dstP;
	
	while (len--) {
		
		int ch;
		
		if ((ch = getchar()) == EOF) {
			fprintf(stderr, "input file ended prematurely\n");
			exit(-1);
		}
		
		if (dst)
			*dst++ = ch;
	}
}

void putBytes(const void* srcP, unsigned len)
{
	const char *src = (char*)srcP;
	
	while (len--)
		putchar(*src++);
}

static void usage(const char *self)
{
	fprintf(stderr, "USAGE: %s [-D] <CLUT TYPE> < in.bmp > out.img\n"
		"  Input bitmap should be a 24bpp color bitmap\n"
		"  -D requests dithering\n"
		"  Valid CLUT types:\n"
		"    \"1bpp\"     - Black & White\n"
		"    \"1bppY\"    - Black, White, & Yellow (as 3 clut entries, 2bpp)\n"
		"    \"1bppR\"    - Black, White, & Red  (as 3 clut entries, 2bpp)\n"
		"    \"3clrPkdY\" - Black, White, & Yellow, packed 5-3-8\n"
		"    \"3clrPkdR\" - Black, White, & Yellow, packed 5-3-8\n"
		"    \"2bpp\"     - 2-bit greyscale (4 blacks)\n"
		"    \"2bppY\"    - 2-bit greyscale with Yellow (3 blacks plus one yellow color)\n"
		"    \"2bppR\"    - 2-bit greyscale with Red (3 blacks plus one red color)\n"
		"    \"5clrPkdY\" - 4 greys + Yellow, packed 3-5-7\n"
		"    \"5clrPkdR\" - 4 greys + Red, packed 3-5-7\n"
		"    \"6clrPkdY\" - 5 greys + Yellow, packed 3-6-8\n"
		"    \"6clrPkdR\" - 5 greys + Red, packed 3-6-8\n"
		"    \"3bpp\"     - 3-bit greyscale (8 blacks)\n"
		"    \"3bppY\"    - 3-bit greyscale with Yellow (7 blacks plus one yellow color)\n"
		"    \"3bppR\"    - 3-bit greyscale with Red (7 blacks plus one red color)\n"
		"    \"4bpp\"     - 4-bit greyscale (16 blacks)\n"
		" RED should be #FF0000, Yellow should be #FFFF00\n"
		, self);
	
	exit(-1);
}

static uint32_t repackPackedVals(uint32_t val, uint32_t pixelsPerPackedUnit, uint32_t packedMultiplyVal)
{
	uint32_t ret = 0, i;
	
	for (i = 0; i < pixelsPerPackedUnit; i++) {
		
		ret = ret * packedMultiplyVal + val % packedMultiplyVal;
		val /= packedMultiplyVal;
	}
	
	return ret;
}

int main(int argc, char **argv)
{
	uint32_t c, rowBytesOut, rowBytesIn, outBpp, i, numRows, pixelsPerPackedUnit = 1, packedMultiplyVal = 0x01000000, packedOutBpp = 0, compressionFormat = 0;
	uint32_t numGrays, extraColor = 0;
	struct BitmapFileHeader hdr;
	const char *self = argv[0];
	enum EinkClut clutType;
	uint8_t clut[256][3];
	bool dither = false;
	int skipBytes;
	
	if (argc != 2 && argc != 3)
		usage(self);
	
	if (argc == 3) {
		if (strcmp(argv[1], "-D"))
			usage(self);
		dither = true;
		argv++;
	}
	if (!strcmp(argv[1], "1bpp"))
		clutType = EinkClutTwoBlacks;
	else if (!strcmp(argv[1], "1bppY"))
		clutType = EinkClutTwoBlacksAndYellow;
	else if (!strcmp(argv[1], "1bppR"))
		clutType = EinkClutTwoBlacksAndRed;
	else if (!strcmp(argv[1], "3clrPkdY"))
		clutType = EinkClutTwoBlacksAndYellowPacked;
	else if (!strcmp(argv[1], "3clrPkdR"))
		clutType = EinkClutTwoBlacksAndRedPacked;
	else if (!strcmp(argv[1], "2bpp"))
		clutType = EinkClutFourBlacks;
	else if (!strcmp(argv[1], "2bppY"))
		clutType = EinkClutThreeBlacksAndYellow;
	else if (!strcmp(argv[1], "2bppR"))
		clutType = EinkClutThreeBlacksAndRed;
	else if (!strcmp(argv[1], "5clrPkdY"))
		clutType = EinkClutFourBlacksAndYellowPacked;
	else if (!strcmp(argv[1], "5clrPkdR"))
		clutType = EinkClutFourBlacksAndRedPacked;
	else if (!strcmp(argv[1], "6clrPkdY"))
		clutType = EinkClutFiveBlacksAndYellowPacked;
	else if (!strcmp(argv[1], "6clrPkdR"))
		clutType = EinkClutFiveBlacksAndRedPacked;
	else if (!strcmp(argv[1], "3bpp"))
		clutType = EinkClutEightBlacks;
	else if (!strcmp(argv[1], "3bppY"))
		clutType = EinkClutSevenBlacksAndYellow;
	else if (!strcmp(argv[1], "3bppR"))
		clutType = EinkClutSevenBlacksAndRed;
	else if (!strcmp(argv[1], "4bpp"))
		clutType = EinkClutSixteenBlacks;
	else
		usage(self);
	
	getBytes(&hdr, sizeof(hdr));
	
	if (hdr.sig[0] != 'B' || hdr.sig[1] != 'M' || hdr.headerSz < 40 || hdr.colorplanes != 1 || hdr.bpp != 24 || hdr.compression) {
		fprintf(stderr, "BITMAP HEADER INVALID\n");
		return -1;
	}
	
	switch (clutType) {
		case EinkClutTwoBlacks:
			numGrays = 2;
			outBpp = 1;
			break;
		
		case EinkClutTwoBlacksAndYellow:
			extraColor = 0xffff00;
			numGrays = 2;
			outBpp = 2;
			break;
		
		case EinkClutTwoBlacksAndRed:
			extraColor = 0xff0000;
			numGrays = 2;
			outBpp = 2;
			break;
		
		case EinkClutTwoBlacksAndYellowPacked:
			numGrays = 2,
			extraColor = 0xffff00;
			outBpp = 2;				//for clut purposes, we have 2..3 entries so a 2-bit clut
			packedOutBpp = 8;		//packing is in 8-bit quantities
			pixelsPerPackedUnit = 5;
			packedMultiplyVal = 3;
			compressionFormat = COMPRESSION_BITPACKED_5x3_to_8;
			break;
		
		case EinkClutTwoBlacksAndRedPacked:
			numGrays = 2,
			extraColor = 0xff0000;
			outBpp = 2;				//for clut purposes, we have 2..3 entries so a 2-bit clut
			packedOutBpp = 8;		//packing is in 8-bit quantities
			pixelsPerPackedUnit = 5;
			packedMultiplyVal = 3;
			compressionFormat = COMPRESSION_BITPACKED_5x3_to_8;
			break;
		
		case EinkClutFourBlacks:
			numGrays = 4;
			outBpp = 2;
			break;
		
		case EinkClutThreeBlacksAndYellow:
			numGrays = 3;
			extraColor = 0xffff00;
			outBpp = 2;
			break;
		
		case EinkClutThreeBlacksAndRed:
			numGrays = 3;
			extraColor = 0xff0000;
			outBpp = 2;
			break;
		
		case EinkClutFourBlacksAndYellowPacked:
			numGrays = 4,
			extraColor = 0xffff00;
			outBpp = 3;				//for clut purposes, we have 5..8 entries so a 3-bit clut
			packedOutBpp = 7;		//packing is in 7-bit quantities
			pixelsPerPackedUnit = 3;
			packedMultiplyVal = 5;
			compressionFormat = COMPRESSION_BITPACKED_3x5_to_7;
			break;
		
		case EinkClutFourBlacksAndRedPacked:
			numGrays = 4,
			extraColor = 0xff0000;
			outBpp = 3;				//for clut purposes, we have 5..8 entries so a 3-bit clut
			packedOutBpp = 7;		//packing is in 7-bit quantities
			pixelsPerPackedUnit = 3;
			packedMultiplyVal = 5;
			compressionFormat = COMPRESSION_BITPACKED_3x5_to_7;
			break;
		
		case EinkClutFiveBlacksAndYellowPacked:
			numGrays = 5,
			extraColor = 0xffff00;
			outBpp = 3;				//for clut purposes, we have 5..8 entries so a 3-bit clut
			packedOutBpp = 8;		//packing is in 8-bit quantities
			pixelsPerPackedUnit = 3;
			packedMultiplyVal = 6;
			compressionFormat = COMPRESSION_BITPACKED_3x6_to_8;
			break;
		
		case EinkClutFiveBlacksAndRedPacked:
			numGrays = 5,
			extraColor = 0xff0000;
			outBpp = 3;				//for clut purposes, we have 5..8 entries so a 3-bit clut
			packedOutBpp = 8;		//packing is in 8-bit quantities
			pixelsPerPackedUnit = 3;
			packedMultiplyVal = 6;
			compressionFormat = COMPRESSION_BITPACKED_3x6_to_8;
			break;
		
		case EinkClutEightBlacks:
			numGrays = 8;
			outBpp = 3;
			break;
		
		case EinkClutSevenBlacksAndYellow:
			numGrays = 7;
			extraColor = 0xffff00;
			outBpp = 3;
			break;
		
		case EinkClutSevenBlacksAndRed:
			numGrays = 7;
			extraColor = 0xff0000;
			outBpp = 3;
			break;
		
		case EinkClutSixteenBlacks:
			numGrays = 16;
			outBpp = 4;
			break;
	}
	
	if (!packedOutBpp)
		packedOutBpp = outBpp;
	
	skipBytes = hdr.dataOfst - sizeof(hdr);
	if (skipBytes < 0) {
		fprintf(stderr, "file header was too short!\n");
		exit(-1);
	}
	getBytes(NULL, skipBytes);
	
	rowBytesIn = (hdr.width * hdr.bpp + 31) / 32 * 4;
	
	//first sort out how many pixel packages we'll have and round up
	rowBytesOut = ((hdr.width + pixelsPerPackedUnit - 1) / pixelsPerPackedUnit) * packedOutBpp;
	
	//the convert that to row bytes (round up to nearest multiple of 4 bytes)
	rowBytesOut = (rowBytesOut + 31) / 32 * 4;
	
	
	numRows = hdr.height < 0 ? -hdr.height : hdr.height;
	hdr.bpp = outBpp;
	hdr.numColors = 1 << outBpp;
	hdr.numImportantColors = 1 << outBpp;
	hdr.dataOfst = sizeof(struct BitmapFileHeader) + 4 * hdr.numColors;
	hdr.dataLen = numRows * rowBytesOut;
	hdr.fileSz = hdr.dataOfst + hdr.dataLen;
	hdr.headerSz = 40 /* do not ask */;
	hdr.compression = compressionFormat;
	
	putBytes(&hdr, sizeof(hdr));
	
	
	//emit & record grey clut entries
	for (i = 0; i < numGrays; i++) {
		
		uint32_t val = 255 * i / (numGrays - 1);
		
		putchar(val);
		putchar(val);
		putchar(val);
		putchar(val);
		
		clut[i][0] = val;
		clut[i][1] = val;
		clut[i][2] = val;
	}
	
	//if there is a color CLUT entry, emit that
	if (extraColor) {
		
		putchar((extraColor >> 0) & 0xff);	//B
		putchar((extraColor >> 8) & 0xff);	//G
		putchar((extraColor >> 16) & 0xff);	//R
		putchar(0x00);						//A
		
		clut[i][0] = (extraColor >> 0) & 0xff;
		clut[i][1] = (extraColor >> 8) & 0xff;
		clut[i][2] = (extraColor >> 16) & 0xff;
	}
	
	//pad clut to size
	for(i = numGrays + (extraColor ? 1 : 0); i < hdr.numColors; i++) {
		
		putchar(0x00);
		putchar(0x00);
		putchar(0x00);
		putchar(0x00);
	}
	
	while (numRows--) {
		
		uint32_t pixelValsPackedSoFar = 0, numPixelsPackedSoFar = 0, valSoFar = 0, bytesIn = 0, bytesOut = 0, bitsSoFar = 0;
		
		for (c = 0; c < hdr.width; c++, bytesIn += 3) {
			int64_t bestDist = 0x7fffffffffffffffll;
			uint8_t rgb[3], bestIdx = 0;
			int32_t ditherFudge = 0;
			
			getBytes(rgb, sizeof(rgb));
			
			if (dither)
				ditherFudge = (rand() % 255 - 127) / (int)numGrays;
			
			for (i = 0; i < hdr.numColors; i++) {
				int64_t dist = 0;
				
				dist += (rgb[0] - clut[i][0] + ditherFudge) * (rgb[0] - clut[i][0] + ditherFudge) * 4750ll;
				dist += (rgb[1] - clut[i][1] + ditherFudge) * (rgb[1] - clut[i][1] + ditherFudge) * 47055ll;
				dist += (rgb[2] - clut[i][2] + ditherFudge) * (rgb[2] - clut[i][2] + ditherFudge) * 13988ll;
				
				if (dist < bestDist) {
					bestDist = dist;
					bestIdx = i;
				}
			}
			
			//pack pixels as needed
			pixelValsPackedSoFar = pixelValsPackedSoFar * packedMultiplyVal + bestIdx;
			if (++numPixelsPackedSoFar != pixelsPerPackedUnit)
				continue;
			
			numPixelsPackedSoFar = 0;
			
			//it is easier to display when low val is firts pixel. currently last pixel is low - reverse this
			pixelValsPackedSoFar = repackPackedVals(pixelValsPackedSoFar, pixelsPerPackedUnit, packedMultiplyVal);
			
			valSoFar = (valSoFar << packedOutBpp) | pixelValsPackedSoFar;
			pixelValsPackedSoFar = 0;
			bitsSoFar += packedOutBpp;
			
			if (bitsSoFar >= 8) {
				putchar(valSoFar >> (bitsSoFar -= 8));
				valSoFar &= (1 << bitsSoFar) - 1;
				bytesOut++;
			}
		}
		
		//see if we have unfinished pixel packages to write
		if (numPixelsPackedSoFar) {
			while (numPixelsPackedSoFar++ != pixelsPerPackedUnit)
				pixelValsPackedSoFar *= packedMultiplyVal;
			
			//it is easier to display when low val is firts pixel. currently last pixel is low - reverse this
			pixelValsPackedSoFar = repackPackedVals(pixelValsPackedSoFar, pixelsPerPackedUnit, packedMultiplyVal);
			
			valSoFar = (valSoFar << packedOutBpp) | pixelValsPackedSoFar;
			pixelValsPackedSoFar = 0;
			bitsSoFar += packedOutBpp;
			
			if (bitsSoFar >= 8) {
				putchar(valSoFar >> (bitsSoFar -= 8));
				valSoFar &= (1 << bitsSoFar) - 1;
				bytesOut++;
			}
		}
		
		if (bitsSoFar) {
			valSoFar <<= 8 - bitsSoFar;	//left-align it as is expected
			putchar(valSoFar);
			bytesOut++;
		}
		
		while (bytesIn++ < rowBytesIn)	
			getchar();
		while (bytesOut++ < rowBytesOut)
			putchar(0);
	}
	fprintf(stderr, "SUCCESS\n");
	return 0;
}