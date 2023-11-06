#include <string.h>
#include <stdarg.h>
#include "printf.h"

typedef void (*PrintfPutchar)(char ch, void *userData);


int putchar(int c)
{
	prPutchar(c);
	
	return (unsigned)(unsigned char)c;
}

static uint_fast8_t prvDiv10(uint64_t *valP)
{
	uint64_t val = *valP;
	uint32_t retHi, retMid, retLo, retChar;
	
	retHi = val >> 32;
	retMid = retHi % 10;
	retHi /= 10;
	
	retMid = (retMid << 16) + (uint16_t)(val >> 16);
	retLo = retMid % 10;
	retMid /= 10;
	
	retLo = (retLo << 16) + (uint16_t)val;
	retChar = retLo % 10;
	retLo /= 10;
	
	val = retHi;
	val <<= 16;
	val += retMid;
	val <<= 16;
	val += retLo;
	
	*valP = val;
	
	return retChar;
}

static uint32_t StrPrvPrintfEx_number(PrintfPutchar putcharF, void *putcharD, uint64_t number, bool baseTen, bool zeroExtend, bool isSigned, uint32_t padToLength)
{
	char buf[64];
	uint32_t idx = sizeof(buf) - 1;
	uint32_t numPrinted = 0;
	bool neg = false;
	uint32_t chr, i;
	
	
	if (padToLength > 63)
		padToLength = 63;
	
	buf[idx--] = 0;	//terminate
	
	if (isSigned) {
		
		if (((int64_t)number) < 0) {
			
			neg = true;
			number = -number;
		}
	}
	
	do {
		
		if (baseTen)
			chr = prvDiv10(&number);
		else {
			
			chr = number & 0x0f;
			number = number >> 4;
		}
		buf[idx--] = (chr >= 10)?(chr + 'A' - 10):(chr + '0');
		
		numPrinted++;
		
	} while (number);
	
	if (neg) {
	
		buf[idx--] = '-';
		numPrinted++;
	}
	
	if (padToLength > numPrinted)
		padToLength -= numPrinted;
	else
		padToLength = 0;
	
	while (padToLength--) {
		
		buf[idx--] = zeroExtend ? '0' : ' ';
		numPrinted++;
	}
	
	idx++;
	for (i = 0; i < numPrinted; i++)
		putcharF((buf + idx)[i], putcharD);
	
	return numPrinted;
}

static uint32_t StrVPrintf_StrLen_withMax(const char* s, uint32_t max)
{
	uint32_t len = 0;
	
	while ((*s++) && (len < max))
		len++;
	
	return len;
}

static int prvPrintf(PrintfPutchar putcharF, void *putcharD, const char* fmtStr, va_list vl)
{	
	uint32_t nDone = 0;
	uint64_t val64;
	char c, t;
	
	while((c = *fmtStr++) != 0){
		
		if (c == '%') {
			
			bool zeroExtend = false, useLong = false, useVeryLong = false, isSigned = false, baseTen = false;
			uint32_t padToLength = 0, len, i;
			const char* str;
			
more_fmt:
			switch (c = *fmtStr++) {
				
				case '%':
				default:	
					putcharF(c, putcharD);
					nDone++;
					break;
				
				case 'c':
					
					t = va_arg(vl,unsigned int);
					putcharF(t, putcharD);
					nDone++;
					break;
				
				case 's':
					
					str = va_arg(vl,char*);
					if (!str)
						str = "(null)";
					if (padToLength)
						len = StrVPrintf_StrLen_withMax(str, padToLength);
					else
						padToLength = len = strlen(str);
					
					for (i = len; i < padToLength; i++)
						putcharF(' ', putcharD);
					for (i = 0; i < len; i++)
						putcharF(*str++, putcharD);
					nDone += padToLength;
					break;
				
				case '0':
					
					if (!zeroExtend && !padToLength) {
						
						zeroExtend = true;
						goto more_fmt;
					}
				
				case '1':
				case '2':
				case '3':
				case '4':
				case '5':
				case '6':
				case '7':
				case '8':
				case '9':
					
					padToLength = (padToLength * 10) + c - '0';
					goto more_fmt;
				
				case 'd':
					isSigned = true;
					//fallthrough
					
				case 'u':
					baseTen = true;
					//fallthrough
					
				case 'x':
				case 'X':
					val64 = useVeryLong ? va_arg(vl,uint64_t) : va_arg(vl,uint32_t);
					if (isSigned && !useVeryLong)
						val64 = (int64_t)(int32_t)val64;
					nDone += StrPrvPrintfEx_number(putcharF, putcharD, val64, baseTen, zeroExtend, isSigned, padToLength);
					break;
					
				case 'l':
					if(useLong)
						useVeryLong = true;
					useLong = true;
					goto more_fmt;
			}
		}
		else {
			putcharF(c, putcharD);
			nDone++;
		}
	}

	return nDone;
}

struct StringPrintingData {
	char *dst;
	size_t spaceLeft;
};

static void printfPrvStringPutchar(char ch, void *userData)
{
	struct StringPrintingData *spd = (struct StringPrintingData*)userData;
	
	if (!spd->spaceLeft)
		return;
	
	spd->spaceLeft--;
	*spd->dst++ = ch;
}

static void printfPrvScreenPutchar(char ch, void *userData)
{
	prPutchar(ch);
}

int vprintf(const char *format, va_list ap)
{
	return prvPrintf(printfPrvScreenPutchar, NULL, format, ap);
}

int vsnprintf(char *str, size_t size, const char *format, va_list ap)
{
	struct StringPrintingData spd = {
		.dst = str,
		.spaceLeft = size,
	};
	int ret = prvPrintf(printfPrvStringPutchar, &spd, format, ap);
	
	if (ret < size)
		str[ret] = 0;
	
	return ret;
}

int puts(const char *s)
{
	char ch;
	
	while ((ch = *s++) != 0)
		printfPrvScreenPutchar(ch, NULL);
	
	printfPrvScreenPutchar('\n', NULL);
	
	return 0;
}

int printf(const char *format, ...)
{
	va_list vl;
	int ret;
	
	va_start(vl, format);
	ret = vprintf(format, vl);
	va_end(vl);
	
	return ret;
}

int vsprintf(char *str, const char *format, va_list ap)
{
	return vsnprintf(str, 0x7fffffff, format, ap);
}

int snprintf(char *s, size_t n, const char *format, ... )
{
	va_list vl;
	int ret;
	
	va_start(vl, format);
	ret = vsnprintf(s, n, format, vl);
	va_end(vl);
	
	return ret;
}

int sprintf(char *s, const char *format, ... )
{
	va_list vl;
	int ret;
	
	va_start(vl, format);
	ret = vsnprintf(s, 0x7fffffff, format, vl);
	va_end(vl);
	
	return ret;
}
