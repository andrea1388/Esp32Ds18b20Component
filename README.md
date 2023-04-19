# Esp32Ds18b20Component
## esp32 C++ component for dealing with ds18b20 temperature sensor

1. Manage more sensors on a single wire
1. You can set resolution from 9 to 12 bit on each sensor
1. Single `requestTemperatures()` command 
for all sensors

Based on this work:
[https://github.com/feelfreelinux/ds18b20](https://github.com/feelfreelinux/ds18b20)
***
Basic usage:


    #include <stdio.h>
    #include "sdkconfig.h"
    #include "freertos/FreeRTOS.h"
    #include "freertos/task.h"
    #include "esp_system.h"
    #include "esp_spi_flash.h"
    #include "esp32ds18b20component.h"
    extern "C" {
        void app_main(void);
    }
    void app_main(void)
    {
        ds18b20 c;
        float temp;
        c.start((gpio_num_t)21);
        uint8_t num;
        num=c.search_all();
        printf("found: %u sensor(s)\n",num);
        if(num>0) {
            c.setResolution(0,10);
            while(true) {
                c.requestTemperatures();
                for(uint8_t sens=0;sens<num;sens++)
                {
                    temp=c.getTempC(sens);
                    printf("sens: %u temp: %6.2f\n",sens,temp);
                }
                vTaskDelay(2500 / portTICK_PERIOD_MS);
            }
        }
    }



Call stack

getTempC(address)
    noInterrupts
    ds18b20_isConnected
        ds18b20_readScratchPad
            ds18b20_reset
            ds18b20_select
            ds18b20_write_byte
            ds18b20_read_byte
            ds18b20_reset

ds18b20_select
    ds18b20_write_byte
        ds18b20_write


requestTemperatures
	noInterrupts();
	ds18b20_reset();
	ds18b20_write_byte(SKIPROM);
	ds18b20_write_byte(GETTEMP);
	interrupts();

setResolution
    noInterrupts