# Esp32Ds18b20Component
esp32 C++ component for dealing with ds18b20 temperature sensor

Based on this work:
[https://github.com/feelfreelinux/ds18b20](https://github.com/feelfreelinux/ds18b20)

Basic usage:
If you have only one sensor on the bus:

ds18b20 s1;
float temp;
s1.ds18b20_init((gpio_num_t)21);
temp=s1.ds18b20_get_temp();