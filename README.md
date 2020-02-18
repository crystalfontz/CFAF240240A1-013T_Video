# Video playback on a CFAF240240A1-013T

## Description 
This example code plays back a video on the [CFAF240240A1-013T](https://www.crystalfontz.com/product/cfaf240240a1013t) from an SD Card. It was built using the ESP-IDF toolchain and Visual Studio Code. For step-by-step guide on how to use the toolchain, follow the link [here](https://docs.espressif.com/projects/esp-idf/en/latest/get-started/). 

Also, there's a new extension from Espressif that we haven't tested but will definitely look into, [here](https://marketplace.visualstudio.com/items?itemName=espressif.esp-idf-extension).

## Hardware
* SD Card
* SD Card Holder
  * This code does not use the quad spi capabilities at the moment
* [ESP32 Dev Kit](https://www.amazon.com/MELIFE-Development-Dual-Mode-Microcontroller-Integrated/dp/B07Q576VWZ/ref=asc_df_B07Q576VWZ) (we used a MELIFE DEVKITV1 but any should work)
* [Wires](https://www.crystalfontz.com/product/wrjmpy40)

### Connecting the hardware

The following block of texts shows how the hardware was connected.

``  ESP | LCD | Description        ``  
``-------+----+----------------------``  
``GPIO15 | 13 | CS                    ``  
``GPIO16 | 15 | RESET                 ``  
``GPIO17 | 12 | D/C                   ``  
``GPIO13 | 10 | MOSI                  ``  
``GPIO14 | 09 | CLK                  ``  
``-------+----+----------------------``  


``SD control lines                      ``  
`` ESP | SD Description              ``  
``-------+---------------------------- ``  
`` GPIO5 | CS                          ``  
`` GPIO18 | PIN_NUM_CLK                 ``  
`` GPIO19 | MISO                        ``  
`` GPIO23 | MOSI                        ``  

Don't forget to connect your power!


## Configure the project

```
make menuconfig
```

* Set serial port under Serial Flasher Options.

## Build and Flash

Build the project and flash it to the board, then run monitor tool to view serial output:

```
make -j4 flash monitor
```

For a faster build and if the PC has eight cores, -j8 can be used instead of -j4.

(To exit the serial monitor, type ``Ctrl-]``.)
