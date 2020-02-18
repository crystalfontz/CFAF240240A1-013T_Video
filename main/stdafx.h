#ifndef __STDAFX_H__
#define __STDAFX_H__

#define USE_SPI_MODE

// This is to test the screen with BBB_565.raw
#define frames_skipped 0

// When testing SD and SPI modes, keep in mind that once the card has been
// initialized in SPI mode, it can not be reinitialized in SD mode without
// toggling power to the card.

#ifdef USE_SPI_MODE
// Pin mapping when using SPI mode.
// With this mapping, SD card can be used both in SPI and 1-line SD mode.
// Note that a pull-up on CS line is required in SD mode.
#define SD_MISO 19
#define SD_MOSI 23
#define SD_CLK 18
#define SD_CS 5
#endif //USE_SPI_MODE

//Overclock the SPI ports
#define CONFIG_LCD_OVERCLOCK

//To speed up transfers, every SPI transfer sends a bunch of lines. This define specifies how many. More means more memory use,
//but less overhead for setting up / finishing transfers. Make sure 240 is dividable by this.
#define PARALLEL_LINES (120)  //for the video demo, this should be set to be half of the display size. Since the display we're 
                              // using is 240 pixels high, we set the macro to 120
#define BUFF_SIZE (240 * PARALLEL_LINES * sizeof(uint16_t))
#define SCREEN_SIZE (240 * 240 * sizeof(uint16_t))

#define PIN_NUM_MISO 12
#define PIN_NUM_MOSI 13
#define PIN_NUM_CLK 14
#define PIN_NUM_CS 15

#define PIN_NUM_DC 17
#define PIN_NUM_RST 16
#define PIN_NUM_BCKL 3

#define SET_DC gpio_set_level(PIN_NUM_DC, 1)
#define CLR_DC gpio_set_level(PIN_NUM_DC, 0)

#endif