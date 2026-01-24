/*
  EPD_Driver_demo
  This is a demo sketch for the 9.7" large EPD from Pervasive Displays, Inc.
  The aim of this demo and the library used is to introduce CoG usage and global update functionality.
  
  Hardware Suggested:
  * Launchpad MSP432P401R or (Tiva-C) with TM4C123/Arduino M0 Pro/Raspberry Pi Pico
  * EPD Extension Kit (EXT2 or EXT3)
  * 9.7" EPD
  * 20-pin rainbow jumper cable
*/

#include <stdint.h>

#include "EPD_Driver.h"

#include "Image_970_Masterfm_01.c"
#include "Image_970_Masterfm_02.c"
#include "Image_970_Slavefm_01.c"
#include "Image_970_Slavefm_02.c"
#define Masterfm1        (uint8_t *)&Image_970_Masterfm_01
#define Masterfm2        (uint8_t *)&Image_970_Masterfm_02
#define Slavefm1         (uint8_t *)&Image_970_Slavefm_01
#define Slavefm2         (uint8_t *)&Image_970_Slavefm_02

//------------------------------------------------------------

extern "C" void EpdTestBWR_9_7()
{
   EPD_Driver epdtest(eScreen_EPD_969);
// EPD_Driver epdtest(eScreen_EPD_B98, boardESP32DevKitC_EXT3);
  // Initialize CoG
  // epdtest.COG_initial();

  // Global Update Call
  epdtest.globalUpdate(Masterfm1, Masterfm2, Slavefm1, Slavefm2);

  // Turn off CoG
  epdtest.COG_powerOff();
}

