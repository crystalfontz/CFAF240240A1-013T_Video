#ifndef ___TFT_H___
#define ___TFT_H___

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "driver/spi_master.h"
#include "soc/gpio_struct.h"
#include "driver/gpio.h"
#include "esp_err.h"
#include "esp_log.h"
#include "stdafx.h" //create an initializing function instead of relying on external pin numbering information



// Defines for the ST7789 registers.
// ref: https://www.crystalfontz.com/products/document/3277/ST7735_V2.1_20100505.pdf
#define ST7789_SLPIN (0x10)
#define ST7789_SLPOUT (0x11)
#define ST7789_INVOFF (0x20)
#define ST7789_INVON (0x21)
#define ST7789_DISPOFF (0x28)
#define ST7789_DISPON (0x29)
#define ST7789_CASET (0x2A)
#define ST7789_RASET (0x2B)
#define ST7789_RAMWR (0x2C)
#define ST7789_MADCTL (0x36)
#define ST7789_COLMOD (0x3A)
#define ST7789_PORCTRL (0xB2)
#define ST7789_GCTRL (0xB7)
#define ST7789_VCOMS (0xBB)
#define ST7789_LCMCTRL (0xC0)  //LCM Control
#define ST7789_VDVVRHEN (0xC2) //VDV and VRH Command Enable
#define ST7789_VRHS (0xC3)     //VRH Set
#define ST7789_VDVSET (0xC4)   //VDV Set
#define ST7789_FRCTR2 (0xC6)   //Frame Rate Control in Normal Mode
#define ST7789_PWCTRL1 (0xD0)  //Power Control 1
#define ST7789_PVGAMCTRL (0xE0)
#define ST7789_NVGAMCTRL (0xE1)


/* Send a command to the LCD. Uses spi_device_polling_transmit, which waits
 * until the transfer is complete.
 *
 * Since command transactions are usually small, they are handled in polling
 * mode for higher speed. The overhead of interrupt transactions is more than
 * just waiting for the transaction to complete.
 */
void lcd_cmd(spi_device_handle_t spi, const uint8_t cmd);

/* Send data to the LCD. Uses spi_device_polling_transmit, which waits until the
 * transfer is complete.
 *
 * Since data transactions are usually small, they are handled in polling
 * mode for higher speed. The overhead of interrupt transactions is more than
 * just waiting for the transaction to complete.
 */
void lcd_data(spi_device_handle_t spi, const uint8_t *data, int len);

//This function is called (in irq context!) just before a transmission starts. It will
//set the D/C line to the value indicated in the user field.
void lcd_spi_pre_transfer_callback(spi_transaction_t *t);

uint32_t lcd_get_id(spi_device_handle_t spi);


//Initialize the display
void lcd_init(spi_device_handle_t spi);

void start_at_page(spi_device_handle_t spi, int ypos);
void send_package(spi_device_handle_t spi, uint16_t *linedata, uint32_t size);


/* To send a set of lines we have to send a command, 2 data bytes, another command, 2 more data bytes and another command
 * before sending the line data itself; a total of 6 transactions. (We can't put all of this in just one transaction
 * because the D/C line needs to be toggled in the middle.)
 * This routine queues these commands up as interrupt transactions so they get
 * sent faster (compared to calling spi_device_transmit several times), and at
 * the mean while the lines for next transactions can get calculated.
 */
void send_lines(spi_device_handle_t spi, int ypos, uint16_t *linedata);

void send_line_finish(spi_device_handle_t spi);

//Simple routine to generate some patterns and send them to the LCD. Don't expect anything too
//impressive. Because the SPI driver handles transactions in the background, we can calculate the next line
//while the previous one is being sent.
// void display_pretty_colors(spi_device_handle_t spi);

/* Clear the screen with a given rgb value
 *
 * 
 */
void clear_screen(spi_device_handle_t spi, uint16_t rgb);
/*
 The LCD needs a bunch of command/argument values to be initialized. They are stored in this struct.
*/
typedef struct
{
  uint8_t cmd;
  uint8_t data[16];
  uint8_t databytes; //No of data in data; bit 7 = delay after set; 0xFF = end of cmds.
} lcd_init_cmd_t;

typedef enum
{
  LCD_TYPE_ILI = 1,
  LCD_TYPE_ST,
  LCD_TYPE_MAX,
} type_lcd_t;




//Place data into DRAM. Constant data gets placed into DROM by default, which is not accessible by DMA.
DRAM_ATTR static const lcd_init_cmd_t st_init_cmds[] = {

    // //{ST7789_SLPOUT,{0x00},0x80},
    // {ST7789_SLPIN, {0x00}, 0x80}, // Sleep In (Low power mode)

    //-----------------------------Display setting--------------------------------
    {ST7789_MADCTL, {0x60}, 1}, //Page 215
    //SPI_sendData(0x00); //DEFAULT
    //SPI_sendData(0x48); //TEST

    // Bit D7- Page Address Order
    // “0” = Top to Bottom (When MADCTL D7=”0”).
    // “1” = Bottom to Top (When MADCTL D7=”1”).
    // Bit D6- Column Address Order
    // “0” = Left to Right (When MADCTL D6=”0”).
    // “1” = Right to Left (When MADCTL D6=”1”).
    // Bit D5- Page/Column Order
    // “0” = Normal Mode (When MADCTL D5=”0”).
    // “1” = Reverse Mode (When MADCTL D5=”1”)
    // Note: Bits D7 to D5, alse refer to section 8.12 Address Control
    // Bit D4- Line Address Order
    // “0” = LCD Refresh Top to Bottom (When MADCTL D4=”0”)
    // “1” = LCD Refresh Bottom to Top (When MADCTL D4=”1”)
    // Bit D3- RGB/BGR Order
    // “0” = RGB (When MADCTL D3=”0”)
    // “1” = BGR (When MADCTL D3=”1”)
    // Bit D2- Display Data Latch Data Order
    // “0” = LCD Refresh Left to Right (When MADCTL D2=”0”)
    // “1” = LCD Refresh Right to Left (When MADCTL D2=”1”)
    //Address control
    {ST7789_COLMOD, {0x55}, 1}, //Interface pixel format Pg 224

    //{ST7789_INVOFF,{0x00},0},
    {ST7789_INVON, {0x00}, 0},
    {ST7789_CASET, {0x00, 0x00, 0x00, 0xEF}, 4},

    {ST7789_RASET, {0x00, 0x00, 0x00, 0xEF}, 4},

    //------------------------- Frame rate setting-------------------
    {ST7789_PORCTRL, {0x0C, 0x0C, 0x00, 0x33, 0x33}, 5},

    {ST7789_GCTRL, {0x35}, 1}, //Gate Control
                               //VGH:13.26/VGL:-10.43

    //----------------------- Power setting-------------------

    {ST7789_VCOMS, {0x1F}, 1}, //VCOM Setting
                               //VCOM = 0.875V

    {ST7789_LCMCTRL, {0x2C}, 1}, //LCM Control
                                 //Power On Sequence default

    {ST7789_VDVVRHEN, {0x01}, 1}, //VDV and VRH Command Enable

    {ST7789_VRHS, {0x12}, 1}, //VRH Set
                              //4.45+( vcom+vcom offset+0.5vdv)

    {ST7789_VDVSET, {0x20}, 1}, //VDV Set
                                //VDV = 0V

    {ST7789_FRCTR2, {0x0F}, 1}, //Frame Rate Control in Normal Mode
                                //60HZ

    {ST7789_PWCTRL1, {0xA4, 0xA1}, 2}, //Power Control 1
                                       //VDS=2.3V/AVCL = -4.8V /AVDD=6.8V

    //               --------------------
    //               --------------------
    //               Set Gamma     for BOE 1.3
    //               --------------------
    // Set_Gamma: //  Is this a goto?

    {ST7789_PVGAMCTRL, {0xD0, 0x08, 0x11, 0x08, 0x0C, 0x15, 0x39, 0x33, 0x50, 0x36, 0x13, 0x14, 0x29, 0x2D}, 14},

    //--------------------
    {ST7789_NVGAMCTRL, {0xD0, 0x08, 0x10, 0x08, 0x06, 0x06, 0x39, 0x44, 0x51, 0x0B, 0x16, 0x14, 0x2F, 0x31}, 14},
    /* Sleep Out */
    {ST7789_SLPOUT, {0}, 0x80},
    /* Display On */
    {ST7789_DISPON, {0}, 0x80},
    {0, {0}, 0xff}



};


#endif