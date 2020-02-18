#include "tft.h"
static const char *TAG = "tft";
static spi_transaction_t transfer;
  static spi_transaction_t trans[6];

esp_err_t ret;


//==================================================================================================
void lcd_cmd(spi_device_handle_t spi, const uint8_t cmd)
{
  memset(&transfer, 0, sizeof(transfer));                   //Zero out the transaction
  transfer.length = 8;                               //Command is 8 bits
  transfer.tx_buffer = &cmd;                         //The data is the cmd itself
  transfer.user = (void *)0;                         //D/C needs to be set to 0
  ret = spi_device_polling_transmit(spi, &transfer); //Transmit!
  assert(ret == ESP_OK);                      //Should have had no issues.
}

//==================================================================================================
void lcd_data(spi_device_handle_t spi, const uint8_t *data, int len)
{
  if (len == 0)
    return;                                   //no need to send anything
  memset(&transfer, 0, sizeof(transfer));                   //Zero out the transaction
  transfer.length = len * 8;                         //Len is in bytes, transaction length is in bits.
  transfer.tx_buffer = data;                         //Data
  transfer.user = (void *)1;                         //D/C needs to be set to 1
  ret = spi_device_polling_transmit(spi, &transfer); //Transmit!
  assert(ret == ESP_OK);                      //Should have had no issues.
}

//==================================================================================================
void lcd_spi_pre_transfer_callback(spi_transaction_t *t)
{
  int dc = (int)t->user;
  gpio_set_level(PIN_NUM_DC, dc);
}

uint32_t lcd_get_id(spi_device_handle_t spi)
{
  //get_id cmd
  lcd_cmd(spi, 0x04);

  memset(&transfer, 0, sizeof(transfer));
  transfer.length = 8 * 3;
  transfer.flags = SPI_TRANS_USE_RXDATA;
  transfer.user = (void *)1;

  esp_err_t ret = spi_device_polling_transmit(spi, &transfer);
  assert(ret == ESP_OK);

  return *(uint32_t *)transfer.rx_data;
}

//==================================================================================================
void lcd_init(spi_device_handle_t spi)
{
  int cmd = 0;
  const lcd_init_cmd_t *lcd_init_cmds;

  //Initialize non-SPI GPIOs
  gpio_set_direction(PIN_NUM_DC, GPIO_MODE_OUTPUT);
  gpio_set_direction(PIN_NUM_RST, GPIO_MODE_OUTPUT);
  gpio_set_direction(PIN_NUM_BCKL, GPIO_MODE_OUTPUT);
  // gpio_set_direction(PIN_NUM_CS, GPIO_MODE_OUTPUT);
  // gpio_set_level(PIN_NUM_CS, 0);

  //Reset the display
  gpio_set_level(PIN_NUM_RST, 0);
  vTaskDelay(100 / portTICK_RATE_MS);
  gpio_set_level(PIN_NUM_RST, 1);
  vTaskDelay(100 / portTICK_RATE_MS);

  ESP_LOGI(TAG, "LCD ST7789V initialization.\n");
  lcd_init_cmds = st_init_cmds;

  //Send all the commands
  while (st_init_cmds[cmd].databytes != 0xff)
  {
    lcd_cmd(spi, lcd_init_cmds[cmd].cmd);
    lcd_data(spi, lcd_init_cmds[cmd].data, lcd_init_cmds[cmd].databytes & 0x1F);
    if (lcd_init_cmds[cmd].databytes & 0x80)
    {
      vTaskDelay(100 / portTICK_RATE_MS);
    }
    cmd++;
  }

  ///Enable backlight
  gpio_set_level(PIN_NUM_BCKL, 0);
}


//==================================================================================================
void start_at_page(spi_device_handle_t spi, int ypos)
{
  esp_err_t ret;
  int x;
  //Transaction descriptors. Declared static so they're not allocated on the stack; we need this memory even when this
  //function is finished because the SPI driver needs access to it even while we're already calculating the next line.

  //In theory, it's better to initialize trans and data only once and hang on to the initialized
  //variables. We allocate them on the stack, so we need to re-init them each call.
  for (x = 0; x < 5; x++)
  {
    memset(&trans[x], 0, sizeof(spi_transaction_t));
    if ((x & 1) == 0)
    {
      //Even transfers are commands
      trans[x].length = 8;
      trans[x].user = (void *)0;
    }
    else
    {
      //Odd transfers are data
      trans[x].length = 8 * 4;
      trans[x].user = (void *)1;
    }
    trans[x].flags = SPI_TRANS_USE_TXDATA;
  }
  trans[0].tx_data[0] = 0x2A;                           //Column Address Set
  trans[1].tx_data[0] = 0;                              //Start Col High
  trans[1].tx_data[1] = 0;                              //Start Col Low
  trans[1].tx_data[2] = (239) >> 8;                     //End Col High
  trans[1].tx_data[3] = (239) & 0xff;                   //End Col Low
  trans[2].tx_data[0] = 0x2B;                           //Page address set
  trans[3].tx_data[0] = ypos >> 8;                      //Start page high
  trans[3].tx_data[1] = ypos & 0xff;                    //start page low
  trans[3].tx_data[2] = (240) >> 8;   //end page high
  trans[3].tx_data[3] = (240) & 0xff; //end page low
  trans[4].tx_data[0] = 0x2C;                           //memory write

  //Queue all transactions.
  for (x = 0; x < 5; x++)
  {
    ret = spi_device_queue_trans(spi, &trans[x], portMAX_DELAY);
    assert(ret == ESP_OK);
  }

  //When we are here, the SPI driver is busy (in the background) getting the transactions sent. That happens
  //mostly using DMA, so the CPU doesn't have much to do here. We're not going to wait for the transaction to
  //finish because we may as well spend the time calculating the next line. When that is done, we can call
  //send_line_finish, which will wait for the transfers to be done and check their status.
}


//==================================================================================================
void send_lines(spi_device_handle_t spi, int ypos, uint16_t *linedata)
{
  esp_err_t ret;
  int x;
  //Transaction descriptors. Declared static so they're not allocated on the stack; we need this memory even when this
  //function is finished because the SPI driver needs access to it even while we're already calculating the next line.

  //In theory, it's better to initialize trans and data only once and hang on to the initialized
  //variables. We allocate them on the stack, so we need to re-init them each call.
  for (x = 0; x < 6; x++)
  {
    memset(&trans[x], 0, sizeof(spi_transaction_t));
    if ((x & 1) == 0)
    {
      //Even transfers are commands
      trans[x].length = 8;
      trans[x].user = (void *)0;
    }
    else
    {
      //Odd transfers are data
      trans[x].length = 8 * 4;
      trans[x].user = (void *)1;
    }
    trans[x].flags = SPI_TRANS_USE_TXDATA;
  }
  trans[0].tx_data[0] = 0x2A;                           //Column Address Set
  trans[1].tx_data[0] = 0;                              //Start Col High
  trans[1].tx_data[1] = 0;                              //Start Col Low
  trans[1].tx_data[2] = (239) >> 8;                     //End Col High
  trans[1].tx_data[3] = (239) & 0xff;                   //End Col Low
  trans[2].tx_data[0] = 0x2B;                           //Page address set
  trans[3].tx_data[0] = ypos >> 8;                      //Start page high
  trans[3].tx_data[1] = ypos & 0xff;                    //start page low
  trans[3].tx_data[2] = (ypos + PARALLEL_LINES) >> 8;   //end page high
  trans[3].tx_data[3] = (ypos + PARALLEL_LINES) & 0xff; //end page low
  trans[4].tx_data[0] = 0x2C;                           //memory write
  trans[5].tx_buffer = linedata;                        //finally send the line data
  trans[5].length = 240 * 2 * 8 * PARALLEL_LINES;       //Data length, in bits
  trans[5].flags = 0;                                   //undo SPI_TRANS_USE_TXDATA flag

  //Queue all transactions.
  for (x = 0; x < 6; x++)
  {
    ret = spi_device_queue_trans(spi, &trans[x], portMAX_DELAY);
    assert(ret == ESP_OK);
  }

  //When we are here, the SPI driver is busy (in the background) getting the transactions sent. That happens
  //mostly using DMA, so the CPU doesn't have much to do here. We're not going to wait for the transaction to
  //finish because we may as well spend the time calculating the next line. When that is done, we can call
  //send_line_finish, which will wait for the transfers to be done and check their status.
}

//==================================================================================================
void send_package(spi_device_handle_t spi, uint16_t *linedata, uint32_t size)
{

  memset(&transfer, 0, sizeof(spi_transaction_t));

  transfer.length = size * 8;
  transfer.tx_buffer = linedata;
  transfer.flags = 0;
  transfer.user = (void *)1;

  ret = spi_device_queue_trans(spi, &transfer, portMAX_DELAY);
  assert(ret == ESP_OK);

  //When we are here, the SPI driver is busy (in the background) getting the transactions sent. That happens
  //mostly using DMA, so the CPU doesn't have much to do here. We're not going to wait for the transaction to
  //finish because we may as well spend the time calculating the next line. When that is done, we can call
  //send_line_finish, which will wait for the transfers to be done and check their status.
}

//==================================================================================================
void send_line_finish(spi_device_handle_t spi)
{
  spi_transaction_t *rtrans;
  esp_err_t ret;
  //Wait for all 6 transactions to be done and get back the results.
  for (int x = 0; x < 6; x++)
  {
    ret = spi_device_get_trans_result(spi, &rtrans, portMAX_DELAY);
    assert(ret == ESP_OK);
    //We could inspect rtrans now if we received any info back. The LCD is treated as write-only, though.
  }
}


void clear_screen(spi_device_handle_t spi, uint16_t rgb)
{

  uint16_t *buffer;
  //Allocate memory for the pixel buffer
  buffer = heap_caps_malloc(240 * PARALLEL_LINES * sizeof(uint16_t), MALLOC_CAP_DMA);
  assert(buffer != NULL);

  //Indexes of the line currently being sent to the LCD and the line we're calculating.
  while (1)
  {
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    memset(buffer, rgb, 240 * PARALLEL_LINES * sizeof(uint16_t));
    for (int y = 0; y < 240; y += PARALLEL_LINES)
    {

      //Send the line we currently calculated.
      send_lines(spi, y, buffer);
      //Finish up the sending process of the previous line, if any
      send_line_finish(spi);
      //The line set is queued up for sending now; the actual sending happens in the
      //background. We can go on to calculate the next line set as long as we do not
      //touch line[sending_line]; the SPI sending process is still reading from that.
    }
  }
}