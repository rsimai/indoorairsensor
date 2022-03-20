# indoor air sensor

## Hardware

Powered by ESP32. 5V through micro USB connector, all devices run on 3.3V.

Uses a bme280 for 

* temperature
* pressure
* humidity

Uses a cs811 for

* CO2
* TVOC

Uses a SSD1306 display with 128x64 pixels and the basic u8x8lib for fixed fonts (16 cols, 8 lines).

Uses three LEDs (green, yellow, red) to show the CO2 level and need to air.

## Features

* just power on, it will display the above values and provide indication if CO2 and humidity are within range.
* toggle oled display off/on through button 1
* toggle LEDs off/on through button 2
* connect through terminal prog (115200 8N1)
* writes actual values to ttyUSB for long term monitoring
* hold down button 1 to enter serial setup. Set preferences and save to flash.

## Build

Load ino into the Arduino IDE and make sure the libs are around. Adafruit BME280, Adafruit CCS811 and U8g2 should be easily downloadable.

## Copy

Sure, just do according to the license, it's Open Source! Please reference me and don't forget to share your work, too.

## Pictures

Prototype front
![prototype front](./front.jpg)


Prototype, back open
![prototype back open](./back.jpg)

## Issues

The ccs811 sometimes goes nuts, in particular when temperature drops quickly. CO2 readings go above 2500ppm and stay there until the device is reset. It's better with 10 seconds readings.

## Todo

* create Webserver and book device into Wifi (needs config) to curl the values.
* clean that code :-)
* CAD and 3d-print a more professional case
