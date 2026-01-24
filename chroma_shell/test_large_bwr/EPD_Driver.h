/*
	EPD_Driver.h

	This header file contains the Main Driver Library.
	
	--COPYRIGHT--
  * Brief The drivers and functions of development board
  * Copyright (c) 2012-2021 Pervasive Displays Inc. All rights reserved.
  *  Authors: Pervasive Displays Inc.
  *  Redistribution and use in source and binary forms, with or without
  *  modification, are permitted provided that the following conditions
  *  are met:
  *  1. Redistributions of source code must retain the above copyright
  *     notice, this list of conditions and the following disclaimer.
  *  2. Redistributions in binary form must reproduce the above copyright
  *     notice, this list of conditions and the following disclaimer in
  *     the documentation and/or other materials provided with the
  *     distribution.
  *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
  *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
  *  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
  *  OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
  *  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
  *  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
  *  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
  *  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
  *  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
  *  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#ifndef EPD_DRIVER_H
#define EPD_DRIVER_H

#ifndef PROXY
   // SDK
   #if defined(ENERGIA)
      #include "Energia.h"
   #else
      #include "Arduino.h"
   #endif

// Other Lib
   #include "SPI.h"
#endif
#include "EPD_Configuration.h"

class EPD_Driver
{
	// Data structures
#ifndef PROXY
	pins_t spi_basic = {0};
#endif
	
  public:

#ifndef PROXY
	EPD_Driver(eScreen_EPD_t eScreen_EPD, pins_t board);
#else
	EPD_Driver(eScreen_EPD_t eScreen_EPD);
#endif
    
	// CoG Functions

	///
  /// @brief COG initialization
  /// @note Implements Tcon (COG) power-on and temperature input to COG
  ///
  void COG_initial();

	///
  /// @brief COG shutdown
  /// @note Turns-off DC/DC circuit
  ///
  void COG_powerOff();

	///
  /// @brief Global update function
  /// @param data1s first image data frame
  /// @param data2s second image data frame
  /// @note Automatically switches between small and medium-sized displays
  ///
	void globalUpdate(const uint8_t * data1m, const uint8_t * data2m, const uint8_t * data1s, const uint8_t * data2s);
	
  protected:

#ifndef PROXY
	// COG Utilities

  ///
  /// @brief SPI transfer protocol for index and data transmit to COG
  /// @param index target register address
  /// @param data pointer to image or data array
  /// @param len length or size of data array
  ///
	void _sendIndexData( uint8_t index, const uint8_t *data, uint32_t len );
	void _sendIndexDataM( uint8_t index, const uint8_t *data, uint32_t len ); 
	void _sendIndexDataS( uint8_t index, const uint8_t *data, uint32_t len );
#endif
	
	// void _softwareSPI(uint8_t data);
	
	void _sendImages(const uint8_t * data1m, const uint8_t * data2m, const uint8_t * data1s, const uint8_t * data2s);

  ///
  /// @brief EPD screen refresh command
  /// @note
  ///
	void _displayRefresh();

  ///
  /// @brief CoG driver power-on hard reset
  /// @param ms1 up to ms5; millisecond delays
  /// @note Implemented after VCC/VDD Turn-on
  ///
	void _reset(uint32_t ms1, uint32_t ms2, uint32_t ms3, uint32_t ms4, uint32_t ms5);
	
  ///
  /// @brief DC-DC soft-start command
  /// @note Specific to mid-sized EPDs only
  ///
  void _DCDC_softStart();

  ///
  /// @brief DC-DC soft-shutdown command
  /// @note Specific to mid-sized EPDs only
  ///
	void _DCDC_softShutdown();
	
	// Internal global variables
	const char* pdi_brd;
	uint16_t pdi_size;
	uint16_t pdi_cp;
	uint32_t image_data_size;
	uint8_t _size;
};

#endif
