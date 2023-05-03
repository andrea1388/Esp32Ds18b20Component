/*
    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
#include <esp_system.h>
#include <hal/gpio_types.h>

// #ifndef DS18B20_H_  
// #define DS18B20_H_

#define noInterrupts() portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;taskENTER_CRITICAL(&mux)
#define interrupts() taskEXIT_CRITICAL(&mux)

#define DEVICE_DISCONNECTED_C -127
#define DEVICE_DISCONNECTED_F -196.6
#define DEVICE_DISCONNECTED_RAW -7040
#define constrain(amt,low,high) ((amt)<(low)?(low):((amt)>(high)?(high):(amt)))
#define pgm_read_byte(addr)   (*(const unsigned char *)(addr))
#define MAXSENSORCOUNT 5







#define ds18b20_send ds18b20_write
#define ds18b20_send_byte ds18b20_write_byte
#define ds18b20_RST_PULSE ds18b20_reset

    static const uint8_t dscrc2x16_table[] = {
        0x00, 0x5E, 0xBC, 0xE2, 0x61, 0x3F, 0xDD, 0x83,
        0xC2, 0x9C, 0x7E, 0x20, 0xA3, 0xFD, 0x1F, 0x41,
        0x00, 0x9D, 0x23, 0xBE, 0x46, 0xDB, 0x65, 0xF8,
        0x8C, 0x11, 0xAF, 0x32, 0xCA, 0x57, 0xE9, 0x74
    };

typedef uint8_t DeviceAddress[8];
typedef DeviceAddress DeviceAddressList[8];

class ds18b20
{
    
    typedef uint8_t ScratchPad[9];

    gpio_num_t DS_GPIO;
    uint8_t init=0;
    uint8_t bitResolution=12;
    
    uint8_t LastDiscrepancy=0;
    uint8_t LastFamilyDiscrepancy=0;
    bool LastDeviceFlag=false;

    // Dow-CRC using polynomial X^8 + X^5 + X^4 + X^0
    // Tiny 2x16 entry CRC table created by Arjen Lentz
    // See http://lentz.com.au/blog/calculating-crc-with-a-tiny-32-entry-lookup-table


    void ds18b20_write(char bit);
    unsigned char ds18b20_read(void);
    void ds18b20_write_byte(char data);
    unsigned char ds18b20_read_byte(void);
    unsigned char ds18b20_reset(void);
    bool ds18b20_isConnected(const DeviceAddress *deviceAddress, uint8_t *scratchPad);
    void ds18b20_writeScratchPad(const DeviceAddress *deviceAddress, const uint8_t *scratchPad);
    bool ds18b20_readScratchPad(const DeviceAddress *deviceAddress, uint8_t *scratchPad);
    void ds18b20_select(const DeviceAddress *deviceAddress);
    uint8_t ds18b20_crc8(const uint8_t *addr, uint8_t len);
    bool ds18b20_isAllZeros(const uint8_t * const scratchPad);
    bool isConversionComplete();
    uint16_t millisToWaitForConversion();
    bool search( bool search_mode,DeviceAddress*);
    int16_t calculateTemperature(uint8_t* scratchPad);


    public:
    ds18b20(gpio_num_t GPIO);
    uint8_t search_all(DeviceAddressList*,uint8_t max);
    bool setResolution(const DeviceAddress *deviceAddress, uint8_t newResolution);
    void requestTemperatures();
    float getTempF(const DeviceAddress *deviceAddress);
    float getTempC(const DeviceAddress *deviceAddress);
    float readSingleSensorTemp(void);
    static void HexToDeviceAddress(uint8_t *,const char*);


};