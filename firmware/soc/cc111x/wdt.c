#include "wdt.h"
#include "cpu.h"


void wdtDeviceReset(void)
{
   WDCTL = 0x0b;  //WDT: enable, fast, reset
}