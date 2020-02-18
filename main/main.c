//===========================================================================
//
//  Code written for ESP32 DEVKIT V1
//
//  CRYSTALFONTZ CFAF240240A1-013T 240X240 SPI COLOR 1.3" TFT
//  ref: https://www.crystalfontz.com/product/cfaf240240a1013t
//
//  This code uses 4-wire SPI mode.
//
// The controller is a Sitronix ST7789H2
//   http://www.crystalfontz.com/controllers/Sitronix/ST7789H2
//
//===========================================================================
//This is free and unencumbered software released into the public domain.
//
//Anyone is free to copy, modify, publish, use, compile, sell, or
//distribute this software, either in source code form or as a compiled
//binary, for any purpose, commercial or non-commercial, and by any
//means.
//
//In jurisdictions that recognize copyright laws, the author or authors
//of this software dedicate any and all copyright interest in the
//software to the public domain. We make this dedication for the benefit
//of the public at large and to the detriment of our heirs and
//successors. We intend this dedication to be an overt act of
//relinquishment in perpetuity of all present and future rights to this
//software under copyright law.
//
//THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
//EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
//MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
//IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR
//OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
//ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
//OTHER DEALINGS IN THE SOFTWARE.
//
//For more information, please refer to <http://unlicense.org/>
//============================================================================
//
// Display is Crystalfontz  CFAF240240A1-013T 240X240 SPI COLOR 1.3" TFT
//   https://www.crystalfontz.com/product/cfaf240240a1013t
//
//============================================================================
//
//============================================================================
//
// LCD SPI & control lines
//   ESP   | LCD  | Description          
// --------+------+----------------------
//  GPIO15 |  13  | CS                   
//  GPIO16 |  15  | RESET                
//  GPIO17 |  12  | D/C                  
//  GPIO13 |  10  | MOSI                 
//  GPIO14 |  9   | CLK                  
// --------+------+----------------------
//
//
// SD control lines
//   ESP   | SD Description              
// --------+-----------------------------
//  GPIO5  | CS
//  GPIO18 | PIN_NUM_CLK
//  GPIO19 | MISO
//  GPIO23 | MOSI
//
//============================================================================

#include <stdio.h>
#include <stdlib.h>
#include <sys/unistd.h>
#include <sys/stat.h>

#include "stdafx.h" //Should get rid of this

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

// Start of SD stuf
#include "esp_vfs_fat.h"
#include "driver/sdmmc_host.h"
#include "driver/sdspi_host.h"
#include "sdmmc_cmd.h"

#include "tft.h"

#include "esp_log.h"
#include "esp_err.h"






#define TASK_PRIO 3

void sd_init(void);

// Display buffer - more info about these is available in the main function
uint16_t *screenbuff;
uint16_t *buff1; 
uint16_t *buff2;
uint16_t *buff3;
volatile uint8_t flag1;
volatile uint8_t flag2;

// for displaying to the serial port
static const char *TAG = "main";

static SemaphoreHandle_t load_buffer_task;
static SemaphoreHandle_t display_buffer_task;

/**
 * @brief   Function to read from an SD card and load the data in the screen buffer
 */
static void load_buff_loop(void *arg)
{
  xSemaphoreTake(load_buffer_task, portMAX_DELAY);
  esp_err_t ret;

  // SD Stuff
  ESP_LOGI(TAG, "Initializing SD card");
  ESP_LOGI(TAG, "Using SPI peripheral");

  sdmmc_host_t host = SDSPI_HOST_DEFAULT();
  host.slot = VSPI_HOST;
  sdspi_slot_config_t slot_config = SDSPI_SLOT_CONFIG_DEFAULT();
  slot_config.gpio_miso = 19;
  slot_config.gpio_mosi = 23;
  slot_config.gpio_sck = 18;
  slot_config.gpio_cs = 5;
  slot_config.dma_channel = 2;
  sdspi_host_set_card_clk(VSPI_HOST, 40);
  // This initializes the slot without card detect (CD) and write protect (WP) signals.
  // Modify slot_config.gpio_cd and slot_config.gpio_wp if your board has these signals.

  // Options for mounting the filesystem.
  // If format_if_mount_failed is set to true, SD card will be partitioned and
  // formatted in case when mounting fails.
  esp_vfs_fat_sdmmc_mount_config_t mount_config = {
      .format_if_mount_failed = false,
      .max_files = 5};

  // Use settings defined above to initialize SD card and mount FAT filesystem.
  // Note: esp_vfs_fat_sdmmc_mount is an all-in-one convenience function.
  // Please check its source code and implement error recovery when developing
  // production applications.
  sdmmc_card_t *card;
  ret = esp_vfs_fat_sdmmc_mount("/sdcard", &host, &slot_config, &mount_config, &card);

  if (ret != ESP_OK)
  {
    if (ret == ESP_FAIL)
    {
      ESP_LOGE(TAG, "Failed to mount filesystem. "
                    "If you want the card to be formatted, set format_if_mount_failed = true.");
    }
    else
    {
      ESP_LOGE(TAG, "Failed to initialize the card (%d). "
                    "Make sure SD card lines have pull-up resistors in place.",
               ret);
    }
  }
  else
  {
    // Card has been initialized, print its properties
    sdmmc_card_print_info(stdout, card);
  }

  //Open renamed file for reading
  ESP_LOGI(TAG, "Reading file");

  FILE *f = fopen("/sdcard/BBB_565.raw", "r");
  if (f == NULL)
  {
    ESP_LOGE(TAG, "Failed to open file for reading");
    return;
  }

  // Find the file size
  fseek(f, 0, SEEK_END);
  // 1,649,088,000 for "Big Buck Bunny"
  unsigned long fileEnd;
  fileEnd = ftell(f);
  // Find the number of frames in the file
  long frames = fileEnd / (240 * PARALLEL_LINES * 2);
  ESP_LOGI(TAG, "size of file: %ld", fileEnd);
  ESP_LOGI(TAG, "frames: %ld", frames);

  long i = 0;
  while (true)
  {
    // Start at the beginning of the file
    fseek(f, 0, 0);
    // Loop through all the frames in the file
    for (i = 0; i < frames / 2; i++)
    {
      // Skip frames? it's faster to do this externally and load a file that already
      //  has the frames skipped
#if frames_skipped
      fseek(f, 240 * 240 * 2 * frames_skipped, SEEK_CUR);
#endif
      while (!flag1)
        ;
      read(fileno(f), buff3, 240 * PARALLEL_LINES * 2);
      flag1 = 0x00;

      while (!flag2)
        ;
      read(fileno(f), buff2, 240 * PARALLEL_LINES * 2);
      flag2 = 0x00;
    }
  }

  fclose(f);
  // All done, unmount partition and disable SDMMC or SPI peripheral
  esp_vfs_fat_sdmmc_unmount();
  ESP_LOGI(TAG, "Card unmounted");
  xSemaphoreGive(load_buffer_task); //start the SD task
}

/**
 * @brief   Function to load the display buffer data into the SPI buffer and send the data off to the display
 */
static void display_loop(void *arg)
{
  xSemaphoreTake(display_buffer_task, portMAX_DELAY);

  esp_err_t ret;

  spi_device_handle_t spi;
  spi_bus_config_t buscfg = {
      .miso_io_num = PIN_NUM_MISO,
      .mosi_io_num = PIN_NUM_MOSI,
      .sclk_io_num = PIN_NUM_CLK,
      .quadwp_io_num = -1,
      .quadhd_io_num = -1,
      .max_transfer_sz = 240 * 240 * 2 + 16}; //make the transfer size the size of a full update
  spi_device_interface_config_t devcfg = {
      .clock_speed_hz = 38 * 1000 * 1000, //Clock out at 38 MHz
      .mode = 0,                          //SPI mode 0
      .spics_io_num = PIN_NUM_CS,         //CS pin
      // .spics_io_num = -1,              //CS pin
      .queue_size = 7,                         //We want to be able to queue 7 transactions at a time
      .pre_cb = lcd_spi_pre_transfer_callback, //Specify pre-transfer callback to handle D/C line
  };
  // Initialize the SPI bus
  ret = spi_bus_initialize(HSPI_HOST, &buscfg, 1);
  assert(ret == ESP_OK);
  // Attach the LCD to the SPI bus
  ret = spi_bus_add_device(HSPI_HOST, &devcfg, &spi);
  assert(ret == ESP_OK);

  // Initialize the LCD
  lcd_init(spi);

  // Initialze some variables used in the loop
  spi_transaction_t *rtrans;

  start_at_page(spi, 0);

  while (1)
  {
    // flag1 is for the first half of the buffer. If high, it means that the data in
    //  buff3 is ready to be copied into buff1
    while (flag1)
      ;
    // Copy the info over that the uSD just loaded in buff3
    memcpy(buff1, buff3, BUFF_SIZE);
    flag1 = 0x01; // Reset the flag

    // flag2 is for the second half of the buffer. If high, it means that the data in
    //  buff2 is ready
    while (flag2)
      ;
    // At this point, screenbuff has new screen data in it and needs to be sent to
    //  the display. Send it off!
    send_package(spi, screenbuff, SCREEN_SIZE);
    flag2 = 0x01; // Reset the flag

    // Confirm there were no issues sending the data. We need to do this otherwise we could
    //  potentially mess with the screenbuff which is being used by the SPI buffer. 
    ret = spi_device_get_trans_result(spi, &rtrans, portMAX_DELAY);
    assert(ret == ESP_OK);
  }
}

void app_main()
{
  //Allow other core to finish initialization
  vTaskDelay(pdMS_TO_TICKS(100));

  //Allocate memory for the buffer that holds the screen
  screenbuff = malloc(SCREEN_SIZE);

  //Since we don't have enough memory to store two full screen's worth of display data,
  // we will set "screenbuff" as the buffer that holds all of the screen data. While the
  // screenbuff is being sent to the display, we will start loading a third buffer from
  // the SD card to not waste any time. Therefor, we will need to create three buffers:
  //  buff1: the first half of screenbuff
  //  buff2: the second half of screenbuff
  //  buff3: the image data that gets read from the SD card while screebuff is being sent
  //    to the display. While the second half of the screen data is being read from the
  //    SD card, buff3 data is copied to buff1
  buff3 = malloc(BUFF_SIZE);

  buff1 = screenbuff;                     //First half of the screen
  buff2 = screenbuff + (SCREEN_SIZE / 4); //Second half of the screen
                                          //" / 4" because these are uint16_t so they hold twice
                                          // as many bytes

  //Free up the flags that let us know when a buffer is free
  flag1 = 0x00;
  flag2 = 0x00;

  //Create semaphores to synchronize - we don't actually need this for this demo but it might
  // be useful if this demo is utilized in a different way
  load_buffer_task = xSemaphoreCreateBinary();
  display_buffer_task = xSemaphoreCreateBinary();

  //Create and start stats task
  xTaskCreatePinnedToCore(load_buff_loop, "SD", 3 * 1024, NULL, TASK_PRIO, NULL, 0);
  xTaskCreatePinnedToCore(display_loop, "display", 3 * 1024, NULL, TASK_PRIO, NULL, 1);
  xSemaphoreGive(load_buffer_task);    //start the SD task
  xSemaphoreGive(display_buffer_task); // start the display task
}
