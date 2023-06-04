#include <stdio.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp32ds18b20component.h"

extern "C" {
    void app_main(void);
}

void app_main(void)
{
    esp_log_level_set("*", ESP_LOG_INFO);
    esp_log_level_set("ds18b20", ESP_LOG_DEBUG);

    int count=2;
    DeviceAddressList address[count];
    ds18b20 a((gpio_num_t)23);
    // search sensors 
    count=a.search_all(address,count);

    // in alternative set static sensor address
    ds18b20::HexToDeviceAddress(address[0],"28b10056b5013caf");
    ds18b20::HexToDeviceAddress(address[1],"282beb56b5013c7b");

    

    for(int i=0;i<count;i++)
        a.setResolution((const DeviceAddress *)address[i],10);
    while(true)
    {
        a.requestTemperatures();
        for(int i=0;i<count;i++)
        {
            float t=a.getTempC((const DeviceAddress *)address[i]);
            printf("sens: %d value:%f\n",i,t);
        }
        vTaskDelay(1000);
    }


}
