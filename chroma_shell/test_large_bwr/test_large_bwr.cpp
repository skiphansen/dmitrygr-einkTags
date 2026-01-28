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

#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "EPD_Driver.h"

#include "Image_970_Masterfm_01.c"
#include "Image_970_Masterfm_02.c"
#include "Image_970_Slavefm_01.c"
#include "Image_970_Slavefm_02.c"
#define Masterfm1        Image_970_Masterfm_01
#define Masterfm2        Image_970_Masterfm_02
#define Slavefm1         Image_970_Slavefm_01
#define Slavefm2         Image_970_Slavefm_02

#include "logging.h"

/* 
   0,0                0,479            0,959
      +----------------+----------------+
      |                |                |
      |                |                |
      |                |                |
      |                |                |
  Y   |     Master     |   Slave        | Physical
      |                |                |
      |                |                |
      |                |                |
      |                |                |
671,0 +----------------+----------------+ 671,959
                       X
 
 8 pixels per byte in the X direction
 
*/
extern "C" void EpdTestBWR_9_7(char *CmdLine)
{
   do {
      int TestType = 0;

      EPD_Driver epdtest(eScreen_EPD_969);

      sscanf(CmdLine,"%d",&TestType);
      if(TestType <= 0 || TestType > 2) {
         if(TestType != 0) {
            printf("Invalid test type\n");
         }
         printf("select test type:\n");
         printf("  1 - test pattern\n");
         printf("  2 - test image\n");
         break;
      }
      if(TestType == 1) {
         memset(Image_970_Masterfm_01,0,sizeof(Image_970_Masterfm_01));
         memset(Image_970_Masterfm_02,0,sizeof(Image_970_Masterfm_01));
         memset(Image_970_Slavefm_01,0,sizeof(Image_970_Slavefm_01));
         memset(Image_970_Slavefm_02,0,sizeof(Image_970_Slavefm_02));

      // draw horizontal black line across middle of display
         int xIncrement = (960 / 8) / 2;  // byte address increase for each line of y
         int yOffset = xIncrement * (672 / 2);

         memset(&Image_970_Masterfm_01[yOffset],0xff,(960/8) / 2);
         memset(&Image_970_Slavefm_01[yOffset],0xff,(960/8) / 2);

      // draw double width vertical red line across middle of display
         for(int y = 0; y < 672; y++) {
         // last bit byte of master image
            Image_970_Masterfm_02[(xIncrement * y) + xIncrement - 1] = 0x80;
         // first bit byte of slave image
            Image_970_Slavefm_02[(xIncrement * y)] = 0x01;
         }
      // draw short arrow pointing to the upper left hand corner
         for(int y = 0; y < 8; y++) {
            Image_970_Masterfm_01[(xIncrement * y)] = (1 << y) | 1;
         }
         Image_970_Masterfm_01[0] = 0xff;
      }
      epdtest.globalUpdate(Masterfm1, Masterfm2, Slavefm1, Slavefm2);
      epdtest.COG_powerOff();
   } while(false);
}

