#ifndef _HEAP_H_
#define _HEAP_H_

#include <stdint.h>



void heapInit(void);
void* heapAlloc(uint32_t sz);
void heapFree(void* ptr);





#endif
