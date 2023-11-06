#ifndef _MSC_H_
#define _MSC_H_

#include <stdbool.h>
#include <stdint.h>

struct Msc;

#define MSC_DEVICE_CURRENT_MA		100

typedef uint32_t (*MscDeviceRead)(void* userData, uint32_t sector, uint32_t nSec, void *data);
typedef uint32_t (*MscDeviceWrite)(void* userData, uint32_t sector, uint32_t nSec, const void *data);

bool mscInit(uint32_t numSec, uint32_t secSize, MscDeviceRead readF, MscDeviceWrite writeF, void *userData);
void mscProcess(void);


#endif
