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
#define LOG_LOCAL_LEVEL ESP_LOG_VERBOSE
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp32/rom/ets_sys.h"
#include "esp_timer.h"
#include "string.h"
#include "esp32ds18b20component.h"
static const char *TAG = "ds18b20";


// OneWire commands
#define GETTEMP			0x44  // Tells device to take a temperature reading and put it on the scratchpad
#define SKIPROM			0xCC  // Command to address all devices on the bus
#define SELECTDEVICE	0x55  // Command to address all devices on the bus
#define COPYSCRATCH     0x48  // Copy scratchpad to EEPROM
#define READSCRATCH     0xBE  // Read from scratchpad
#define WRITESCRATCH    0x4E  // Write to scratchpad
#define RECALLSCRATCH   0xB8  // Recall from EEPROM to scratchpad
#define READPOWERSUPPLY 0xB4  // Determine if device needs parasite power
#define ALARMSEARCH     0xEC  // Query bus for devices with an alarm condition
// Scratchpad locations
#define TEMP_LSB        0
#define TEMP_MSB        1
#define HIGH_ALARM_TEMP 2
#define LOW_ALARM_TEMP  3
#define CONFIGURATION   4
#define INTERNAL_BYTE   5
#define COUNT_REMAIN    6
#define COUNT_PER_C     7
#define SCRATCHPAD_CRC  8
// DSROM FIELDS
#define DSROM_FAMILY    0
#define DSROM_CRC       7
// Device resolution
#define TEMP_9_BIT  0x1F //  9 bit
#define TEMP_10_BIT 0x3F // 10 bit
#define TEMP_11_BIT 0x5F // 11 bit
#define TEMP_12_BIT 0x7F // 12 bit



/// Sends one bit to bus
void ds18b20::ds18b20_write(char bit){
	if (bit & 1) {
		gpio_set_direction(DS_GPIO, GPIO_MODE_OUTPUT);
		//noInterrupts();
		gpio_set_level(DS_GPIO,0);
		ets_delay_us(6);
		gpio_set_direction(DS_GPIO, GPIO_MODE_INPUT);	// release bus
		ets_delay_us(64);
		//interrupts();
	} else {
		gpio_set_direction(DS_GPIO, GPIO_MODE_OUTPUT);
		//noInterrupts();
		gpio_set_level(DS_GPIO,0);
		ets_delay_us(60);
		gpio_set_direction(DS_GPIO, GPIO_MODE_INPUT);	// release bus
		ets_delay_us(10);
		//interrupts();
	}
}

// Reads one bit from bus
unsigned char ds18b20::ds18b20_read(void){
	unsigned char value = 0;
	gpio_set_direction(DS_GPIO, GPIO_MODE_OUTPUT);
	//noInterrupts();
	gpio_set_level(DS_GPIO, 0);
	ets_delay_us(6);
	gpio_set_direction(DS_GPIO, GPIO_MODE_INPUT);
	ets_delay_us(9);
	value = gpio_get_level(DS_GPIO);
	ets_delay_us(55);
	//interrupts();
	return (value);
}
// Sends one byte to bus
void ds18b20::ds18b20_write_byte(char data){
  unsigned char i;
  unsigned char x;
  for(i=0;i<8;i++){
    x = data>>i;
    x &= 0x01;
    ds18b20_write(x);
  }
  //ESP_LOGD(TAG, "writebyte %2x\n",data);
  ets_delay_us(100);
}
// Reads one byte from bus
unsigned char ds18b20::ds18b20_read_byte(void){
  unsigned char i;
  unsigned char data = 0;
  for (i=0;i<8;i++)
  {
    if(ds18b20_read()) data|=0x01<<i;
    ets_delay_us(15);
  }
  return(data);
}
// Sends reset pulse
unsigned char ds18b20::ds18b20_reset(void){
	unsigned char presence;
	//ESP_LOGD(TAG, "reset");
	gpio_set_direction(DS_GPIO, GPIO_MODE_OUTPUT);
	//noInterrupts();
	gpio_set_level(DS_GPIO, 0);
	ets_delay_us(480);
	gpio_set_level(DS_GPIO, 1);
	gpio_set_direction(DS_GPIO, GPIO_MODE_INPUT);
	ets_delay_us(70);
	presence = (gpio_get_level(DS_GPIO) == 0);
	ets_delay_us(410);
	//interrupts();
	return presence;
}

bool ds18b20::setResolution(const DeviceAddress *deviceAddress, uint8_t newResolution) {
	bool success = false;
	// handle the sensors with configuration register
	newResolution = constrain(newResolution, 9, 12);
	uint8_t newValue = 0;
	ScratchPad scratchPad;
	// we can only update the sensor if it is connected
	noInterrupts();
	if (ds18b20_isConnected(deviceAddress, scratchPad)) {
		switch (newResolution) {
		case 12:
			newValue = TEMP_12_BIT;
			break;
		case 11:
			newValue = TEMP_11_BIT;
			break;
		case 10:
			newValue = TEMP_10_BIT;
			break;
		case 9:
		default:
			newValue = TEMP_9_BIT;
			break;
		}
		// if it needs to be updated we write the new value
		if (scratchPad[CONFIGURATION] != newValue) {
			scratchPad[CONFIGURATION] = newValue;
			ds18b20_writeScratchPad(deviceAddress, scratchPad);
		}
		// done
		success = true;
	}
	interrupts();
	ESP_LOGD(TAG, "SetResolution bit: %u ok: %d", newResolution,success);

	return success;
}

void ds18b20::ds18b20_writeScratchPad(const DeviceAddress *deviceAddress, const uint8_t *scratchPad) {
	ds18b20_reset();
	ds18b20_select(deviceAddress);
	ds18b20_write_byte(WRITESCRATCH);
	ds18b20_write_byte(scratchPad[HIGH_ALARM_TEMP]); // high alarm temp
	ds18b20_write_byte(scratchPad[LOW_ALARM_TEMP]); // low alarm temp
	ds18b20_write_byte(scratchPad[CONFIGURATION]);
	ds18b20_reset();
}

bool ds18b20::ds18b20_readScratchPad(const DeviceAddress *deviceAddress, uint8_t* scratchPad) {
	// send the reset command and fail fast
	//ESP_LOGD(TAG, "read scratchpad");
	int b = ds18b20_reset();
	if (b == 0) return false;
	ds18b20_select(deviceAddress);
	ds18b20_write_byte(READSCRATCH);
	// Read all registers in a simple loop
	// byte 0: temperature LSB
	// byte 1: temperature MSB
	// byte 2: high alarm temp
	// byte 3: low alarm temp
	// byte 4: DS18B20 & DS1822: configuration register
	// byte 5: internal use & crc
	// byte 6: DS18B20 & DS1822: store for crc
	// byte 7: DS18B20 & DS1822: store for crc
	// byte 8: SCRATCHPAD_CRC
	for (uint8_t i = 0; i < 9; i++) {
		scratchPad[i] = ds18b20_read_byte();
	}
	b = ds18b20_reset();


	return (b == 1);
}

void ds18b20::ds18b20_select(const DeviceAddress *deviceAddress)
{
    uint8_t i;
	//ESP_LOGD(TAG, "select");
    ds18b20_write_byte(SELECTDEVICE);           // Choose ROM
	for (i = 0; i < 8; i++) ds18b20_write_byte(((uint8_t *)deviceAddress)[i]);
}

void ds18b20::requestTemperatures(){
	ESP_LOGD(TAG, "requestTemperatures");
	noInterrupts();
	ds18b20_reset();
	ds18b20_write_byte(SKIPROM);
	ds18b20_write_byte(GETTEMP);
	interrupts();
    unsigned long start = esp_timer_get_time() / 1000ULL;
    while (!isConversionComplete() && ((esp_timer_get_time() / 1000ULL) - start < millisToWaitForConversion())) vPortYield();
}

bool ds18b20::isConversionComplete() {
	uint8_t b = ds18b20_read();
	return (b == 1);
}

uint16_t ds18b20::millisToWaitForConversion() {
	switch (bitResolution) {
	case 9:
		return 94;
	case 10:
		return 188;
	case 11:
		return 375;
	default:
		return 750;
	}
}

bool ds18b20::ds18b20_isConnected(const DeviceAddress *deviceAddress, uint8_t *scratchPad) {
	bool b = ds18b20_readScratchPad(deviceAddress, scratchPad);

	return b && !ds18b20_isAllZeros(scratchPad) && (ds18b20_crc8(scratchPad, 8) == scratchPad[SCRATCHPAD_CRC]);
}

uint8_t ds18b20::ds18b20_crc8(const uint8_t *addr, uint8_t len){
	uint8_t crc = 0;
	while (len--) {
		crc = *addr++ ^ crc;  // just re-using crc as intermediate
		crc = pgm_read_byte(dscrc2x16_table + (crc & 0x0f)) ^
		pgm_read_byte(dscrc2x16_table + 16 + ((crc >> 4) & 0x0f));
	}
	return crc;
}

bool ds18b20::ds18b20_isAllZeros(const uint8_t * const scratchPad) {
	for (size_t i = 0; i < 9; i++) {
		if (scratchPad[i] != 0) {
			return false;
		}
	}
	return true;
}

float ds18b20::getTempF(const DeviceAddress *deviceAddress) {
	ScratchPad scratchPad;
	if (ds18b20_isConnected(deviceAddress, scratchPad)){
		int16_t rawTemp = calculateTemperature(scratchPad);
		if (rawTemp <= DEVICE_DISCONNECTED_RAW)
			return DEVICE_DISCONNECTED_F;
		// C = RAW/128
		// F = (C*1.8)+32 = (RAW/128*1.8)+32 = (RAW*0.0140625)+32
		return ((float) rawTemp * 0.0140625f) + 32.0f;
	}
	return DEVICE_DISCONNECTED_F;
}

float ds18b20::getTempC(const DeviceAddress *deviceAddress) {
	ScratchPad scratchPad;
/* 	for (uint8_t i = 0; i < 8; i++){
		ESP_LOGD(TAG, "getTempC %02x", ((uint8_t *)deviceAddress)[i]);
	} */
	noInterrupts();
	bool ok=ds18b20_isConnected(deviceAddress, scratchPad);
	interrupts();
	if (ok) {
		int16_t rawTemp = calculateTemperature(scratchPad);
		if (rawTemp <= DEVICE_DISCONNECTED_RAW)
			return DEVICE_DISCONNECTED_F;
		// C = RAW/128
		// F = (C*1.8)+32 = (RAW/128*1.8)+32 = (RAW*0.0140625)+32
		return (float) rawTemp/128.0f;
	}
	
	return DEVICE_DISCONNECTED_F;
}

// reads scratchpad and returns fixed-point temperature, scaling factor 2^-7
int16_t ds18b20::calculateTemperature(uint8_t* scratchPad) {
	int16_t fpTemperature = (((int16_t) scratchPad[TEMP_MSB]) << 11) | (((int16_t) scratchPad[TEMP_LSB]) << 3);
	return fpTemperature;
}

// Returns temperature from the (only) sensor. Don't use if there are more than 1 sensors.
float ds18b20::readSingleSensorTemp(void) {
  if(init==1){
    unsigned char check;
    char temp1=0, temp2=0;
      check=ds18b20_RST_PULSE();
      if(check==1)
      {
        ds18b20_send_byte(0xCC);
        ds18b20_send_byte(0x44);
        vTaskDelay(750 / portTICK_PERIOD_MS);
        check=ds18b20_RST_PULSE();
        ds18b20_send_byte(0xCC);
        ds18b20_send_byte(0xBE);
        temp1=ds18b20_read_byte();
        temp2=ds18b20_read_byte();
        check=ds18b20_RST_PULSE();
        float temp=0;
        temp=(float)(temp1+(temp2*256))/16;
        return temp;
      }
      else{return 0;}

  }
  else{return 0;}
}

ds18b20::ds18b20(gpio_num_t GPIO) 
{
	DS_GPIO = GPIO;
	//gpio_pad_select_gpio(DS_GPIO);
	ESP_ERROR_CHECK(gpio_reset_pin(DS_GPIO));
	//esp_rom_gpio_pad_select_gpio(DS_GPIO);
	init = 1;
}


// --- Replaced by the one from the Dallas Semiconductor web site ---
//--------------------------------------------------------------------------
// Perform the 1-Wire Search Algorithm on the 1-Wire bus using the existing
// search state.
// Return TRUE  : device found, ROM number in ROM_NO buffer
//        FALSE : device not found, end of search

bool ds18b20::search(bool search_mode, DeviceAddress* deviceAddress) {
	uint8_t id_bit_number;
	uint8_t last_zero, rom_byte_number;
	bool search_result,ok=false;
	uint8_t id_bit, cmp_id_bit;

	unsigned char rom_byte_mask, search_direction;
	uint8_t ROM_NO[8];


	// reset the search state

	for (int i = 7; i >= 0; i--) {
		ROM_NO[i] = 0;
	}

	// initialize for search
	id_bit_number = 1;
	last_zero = 0;
	rom_byte_number = 0;
	rom_byte_mask = 1;
	search_result = false;



	if (!LastDeviceFlag) {
		// 1-Wire reset
		if (!ds18b20_reset()) {
			// reset the search
			LastDiscrepancy = 0;
			LastDeviceFlag = false;
			LastFamilyDiscrepancy = 0;
			return false;
		}

		// issue the search command
		if (search_mode == true) {
			ds18b20_write_byte(0xF0);   // NORMAL SEARCH
		} else {
			ds18b20_write_byte(0xEC);   // ALARM SEARCH (only slaves with a set alarm flag will respond)
		}

		// loop to do the search
		do {
			// read a bit and its complement
			id_bit = ds18b20_read();
			cmp_id_bit = ds18b20_read();

			// check for no devices on 1-wire
			if ((id_bit == 1) && (cmp_id_bit == 1)) {
				break;
			} else {
				// all devices coupled have 0 or 1
				if (id_bit != cmp_id_bit) {
					search_direction = id_bit;  // bit write value for search
				} else {
					// if this discrepancy if before the Last Discrepancy
					// on a previous next then pick the same as last time
					if (id_bit_number < LastDiscrepancy) {
						search_direction = ((ROM_NO[rom_byte_number]
								& rom_byte_mask) > 0);
					} else {
						// if equal to last pick 1, if not then pick 0
						search_direction = (id_bit_number == LastDiscrepancy);
					}
					// if 0 was picked then record its position in LastZero
					if (search_direction == 0) {
						last_zero = id_bit_number;

						// check for Last discrepancy in family
						if (last_zero < 9)
							LastFamilyDiscrepancy = last_zero;
					}
				}

				// set or clear the bit in the ROM byte rom_byte_number
				// with mask rom_byte_mask
				if (search_direction == 1)
					ROM_NO[rom_byte_number] |= rom_byte_mask;
				else
					ROM_NO[rom_byte_number] &= ~rom_byte_mask;

				// serial number search direction write bit
				ds18b20_write(search_direction);

				// increment the byte counter id_bit_number
				// and shift the mask rom_byte_mask
				id_bit_number++;
				rom_byte_mask <<= 1;

				// if the mask is 0 then go to new SerialNum byte rom_byte_number and reset mask
				if (rom_byte_mask == 0) {
					rom_byte_number++;
					rom_byte_mask = 1;
				}
			}
		} while (rom_byte_number < 8);  // loop until through all ROM bytes 0-7
		//ESP_LOGD(TAG, "S1 search result: %d rom_byte_number %d", search_result, rom_byte_number);
	
		// if the search was successful then
		if (!(id_bit_number < 65)) {
			// search successful so set LastDiscrepancy,LastDeviceFlag,search_result
			LastDiscrepancy = last_zero;

			// check for last device
			if (LastDiscrepancy == 0) {
				LastDeviceFlag = true;
			}
			search_result = true;
		}
	}
	interrupts();
	// if no device found then reset counters so next 'search' will be like a first
	if (search_result && ROM_NO[0]) {
		for (uint8_t i = 0; i < 8; i++){

			*deviceAddress[i]=ROM_NO[i];

		}
		ok=true;
	}
	//ESP_LOGD(TAG, "S2 search result: %d rom_byte_number %d", search_result, rom_byte_number);
	return ok;
}

uint8_t ds18b20::search_all(DeviceAddressList* dal, uint8_t max) {
	uint8_t devices=0;
	LastDiscrepancy = 0;
	LastDeviceFlag = false;
	LastFamilyDiscrepancy = 0;


	while(true) 
	{
		noInterrupts();
		bool ok=search(true,dal[devices]);
		interrupts();
		if(!ok) break;
		ESP_LOGI(TAG,"sensor: %d address: ", devices);
		for (uint8_t i = 0; i < 8; i++) {
			printf("%02x", *dal[devices][i]);
		}
		//ESP_LOG_BUFFER_HEX_LEVEL(TAG,dal[devices][0],8,ESP_LOG_INFO);
		printf("\n");
		vTaskDelay(1);
		devices++;
		if(devices>max) break;
	}
	ESP_LOGI(TAG,"found %d devices: ", devices);
	return devices;
}

void ds18b20::HexToDeviceAddress(uint8_t * deviceAddress,const char* hexstring)
{
	uint8_t i;
    uint8_t str_len = strlen(hexstring);
	if(str_len != 16) {
		ESP_LOGE(TAG,"must be 16\n");
		abort();
	}

    for (i = 0; i < (str_len / 2); i++) {
        sscanf(hexstring + 2*i, "%02x", (unsigned int*)&deviceAddress[i]);
        //printf("bytearray %d: %02x\n", i, deviceAddress[i]);
    }
}
