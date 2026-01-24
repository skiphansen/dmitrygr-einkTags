///
/// @file EPD_Configuration.h
/// @brief Configuration of the options for Pervasive Displays Library Suite
///
/// @details Project Pervasive Displays Library Suite
/// @n Based on highView technology
///
/// 1- List of supported Pervasive Displays screens
/// 2- List of pre-configured boards
/// 3- Register initializations
///
/// @author Pervasive Displays, Inc.
/// @date 25 Nov 2021
///
/// @copyright (c) Pervasive Displays, Inc., 2010-2021
/// @copyright All rights reserved
///
/// * Basic edition: for hobbyists and for personal usage
/// @n Creative Commons Attribution-NonCommercial-ShareAlike 4.0 Unported (CC BY-NC-SA 4.0)
///
/// * Advanced edition: for professionals or organisations, no commercial usage
/// @n All rights reserved
///
/// * Commercial edition: for professionals or organisations, commercial usage
/// @n All rights reserved
///

// SDK
#include "stdint.h"

#ifndef hV_CONFIGURATION_RELEASE
///
/// @brief Release
///
#define hV_CONFIGURATION_RELEASE
\
///
/// @name 1- List of supported Pervasive Displays screens
/// @see https://www.pervasivedisplays.com/products/
/// @{
///
#define eScreen_EPD_t uint32_t ///< screen type
///
/// * Monochrome screens and default colour screens
/// * SMALL-sized
#define eScreen_EPD_154 (uint32_t)0x1509 ///< reference xE2154CSxxx
#define eScreen_EPD_213 (uint32_t)0x2100 ///< reference xE2213CSxxx
#define eScreen_EPD_266 (uint32_t)0x2600 ///< reference xE2266CSxxx
#define eScreen_EPD_271 (uint32_t)0x2700 ///< reference xE2271CSxxx
#define eScreen_EPD_287 (uint32_t)0x2800 ///< reference xE2287CSxxx
#define eScreen_EPD_370 (uint32_t)0x3700 ///< reference xE2370CSxxx
#define eScreen_EPD_417 (uint32_t)0x4100 ///< reference xE2417CSxxx
#define eScreen_EPD_437 (uint32_t)0x430C ///< reference xE2437CSxxx

/// * MID-sized
#define eScreen_EPD_581 (uint32_t)0x580B ///< reference xE2581CS0Bx, same as eScreen_EPD_581_0B
#define eScreen_EPD_741 (uint32_t)0x740B ///< reference xE2741CS0Bx, same as eScreen_EPD_741_0B

/// * Specific large screens, previous type, global update
#define eScreen_EPD_969 (uint32_t)0x9608 ///< reference xE2969CS08x, previous type
#define eScreen_EPD_B98 (uint32_t)0xB908 ///< reference xE2B98CS08x, previous type

/// @}

///
/// @name Frame Frame-buffer sizes
/// @details Frame-buffer size = width * height / 8 * depth, uint32_t
/// @note Only one frame buffer is required.
/// @n Depth = 2 for black-white-red screens and monochrome screens
///

// Screen resolutions for small and mid-sized EPDs
const uint16_t EPD_idx[] = {0x15, 0x21, 0x26, 0x27, 0x28, 0x37, 0x41, 0x43, 0x58, 0x74};
const long image_data_size[] = { 2888, 2756, 5624, 5808, 4736, 12480, 15000, 10560, 23040, 48000};


/// @name 2- List of pre-configured boards
/// @{

///
/// @brief Not connected pin
///
#define NOT_CONNECTED (uint8_t)0xff

#ifndef PROXY
///
/// @brief Board configuration structure
///

struct pins_t
{
    ///< EXT3 pin 1 Black -> +3.3V
    ///< EXT3 pin 2 Brown -> SPI SCK
    uint8_t panelBusy; ///< EXT3 pin 3 Red
    uint8_t panelDC; ///< EXT3 pin 4 Orange
    uint8_t panelReset; ///< EXT3 pin 5 Yellow
    ///< EXT3 pin 6 Green -> SPI MISO
    ///< EXT3 pin 7 Blue -> SPI MOSI
    uint8_t panelCS;
	uint8_t panelCSS;
    uint8_t panelON_EXT2;
    uint8_t panelSPI43_EXT2;
    uint8_t flashCS;
	uint8_t flashCSS;
};

///
/// @brief MSP430 and MSP432 LaunchPad configuration, tested
///
const pins_t boardLaunchPad_EXT3 =
{
    .panelBusy = 11, ///< EXT3 pin 3 Red
    .panelDC = 12, ///< EXT3 pin 4 Orange
    .panelReset = 13, ///< EXT3 pin 5 Yellow
    .panelCS = 19,
	.panelCSS = 39,
    .panelON_EXT2 = NOT_CONNECTED,
    .panelSPI43_EXT2 = NOT_CONNECTED,
    .flashCS = 18,
    .flashCSS = 38
};

///
/// @brief MSP430 and MSP432 LaunchPad configuration, tested
///
const pins_t boardLaunchPad_EXT2 =
{
    .panelBusy = 8, ///< EXT3 pin 3 Red
    .panelDC = 9, ///< EXT3 pin 4 Orange
    .panelReset = 10, ///< EXT3 pin 5 Yellow
    .panelCS = 19,
	.panelCSS = 39,
    .panelON_EXT2 = 11,
    .panelSPI43_EXT2 = 17,
    .flashCS = 18,
    .flashCSS = 38
};

///
/// @brief Raspberry Pi Pico with default RP2040 configuration, tested
///
const pins_t boardRaspberryPiPico_RP2040_EXT3 =
{
    .panelBusy = 13, ///< EXT3 pin 3 Red -> GP13
    .panelDC = 12, ///< EXT3 pin 4 Orange -> GP12
    .panelReset = 11, ///< EXT3 pin 5 Yellow -> GP11
    .panelCS = 17,
	.panelCSS = 14,
    .panelON_EXT2 = NOT_CONNECTED,
    .panelSPI43_EXT2 = NOT_CONNECTED,
    .flashCS = 10,
    .flashCSS = 15
};

///
/// @brief Raspberry Pi Pico with default RP2040 configuration, tested
///
const pins_t boardRaspberryPiPico_RP2040_EXT2 =
{
    .panelBusy = 13, ///< EXT3 pin 3 Red -> GP13
    .panelDC = 12, ///< EXT3 pin 4 Orange -> GP12
    .panelReset = 11, ///< EXT3 pin 5 Yellow -> GP11
    .panelCS = 17,
	.panelCSS = 14,
    .panelON_EXT2 = 8,
    .panelSPI43_EXT2 = 7,
    .flashCS = 10,
    .flashCSS = 15
};

///
/// @brief Arduino M0Pro configuration, tested
///
const pins_t boardArduinoM0Pro_EXT3 =
{
    .panelBusy = 4, ///< EXT3 pin 3 Red
    .panelDC = 5, ///< EXT3 pin 4 Orange
    .panelReset = 6, ///< EXT3 pin 5 Yellow
    .panelCS = 8,
	.panelCSS = 9,
    .panelON_EXT2 = NOT_CONNECTED,
    .panelSPI43_EXT2 = NOT_CONNECTED,
    .flashCS = 7,
    .flashCSS = 10
};

///
/// @brief Arduino M0Pro configuration, tested
///
const pins_t boardArduinoM0Pro_EXT2 =
{
    .panelBusy = 4, ///< EXT3 pin 3 Red
    .panelDC = 5, ///< EXT3 pin 4 Orange
    .panelReset = 6, ///< EXT3 pin 5 Yellow
    .panelCS = 8,
	.panelCSS = 9,
    .panelON_EXT2 = 11,
    .panelSPI43_EXT2 = 9,
    .flashCS = 7,
    .flashCSS = 10
};

///
/// @brief Espressif ESP32-DevKitC
/// @note Numbers refer to GPIOs not pins
///
const pins_t boardESP32DevKitC_EXT3 =
{
    .panelBusy = 27, ///< EXT3 pin 3 Red -> GPIO27
    .panelDC = 26, ///< EXT3 pin 4 Orange -> GPIO26
    .panelReset = 25, ///< EXT3 pin 5 Yellow -> GPIO25
    .panelCS = 32, ///< EXT3 pin 9 Grey -> GPIO32
	.panelCSS = 15,
	.panelON_EXT2 = NOT_CONNECTED,
    .panelSPI43_EXT2 = NOT_CONNECTED, ///< BS
    .flashCS = 33, ///< EXT3 pin 8 Violet -> GPIO33
    .flashCSS = NOT_CONNECTED
};

///
/// @brief Espressif ESP32-DevKitC
/// @note Numbers refer to GPIOs not pins
///
const pins_t boardESP32DevKitC_EXT2 =
{
    .panelBusy = 27, ///< EXT3 pin 3 Red -> GPIO27
    .panelDC = 26, ///< EXT3 pin 4 Orange -> GPIO26
    .panelReset = 25, ///< EXT3 pin 5 Yellow -> GPIO25
    .panelCS = 32, ///< EXT3 pin 9 Grey -> GPIO32
	.panelCSS = 15,
	.panelON_EXT2 = 16,
    .panelSPI43_EXT2 = 17, ///< BS
    .flashCS = 33, ///< EXT3 pin 8 Violet -> GPIO33
    .flashCSS = NOT_CONNECTED
};
#endif   // PROXY

/// @}

///
/// @name 3- Register initializations
/// @{
///

/// @}

#endif // hV_CONFIGURATION_RELEASE
