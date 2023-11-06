#if defined(CHROMA74)
#include "screenEink74.h"
#elif defined(CHROMA29)
#include "screenEink29.h"
#elif defined(EPOP900)
#include "screenRaw.h"
#elif defined(EPOP50)
#include "screenLcd.h"
#else
#error "screen type not known"
#endif


#ifndef SCREEN_DATA_PASSES
#define SCREEN_DATA_PASSES	1
#define screenEndPass()
#endif