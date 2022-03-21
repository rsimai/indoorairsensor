# indoor air sensor

## Hardware

Driven by regular ESP32 dev module. Powered through micro USB connector 5V, all devices run on 3.3V provided by the ESP module. Consumes approx. 50mA with no additional powersaving measures, 70mA during serial config.

Uses a bme280 for 

* temperature
* pressure
* humidity

Uses a cs811 for

* CO2
* TVOC

Uses a SSD1306 display with u8x8lib for fixed fonts

* 128x64 pixels
* 16 cols, 8 lines

Uses three LEDs to show the CO2 level and need to air

* green
* yellow
* red

## Features

* power on to display the above values and provide indication if CO2 and humidity are within range
* toggle oled display off/on through button 1
* toggle LEDs off/on through button 2
* connect through terminal prog (115200 8N1)
* writes actual values to ttyUSB for long term monitoring
* hold down button 1 to enter serial setup, to change values and save to flash for next boot

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

The ccs811 is heated and influences the bme280. Since the pins are in same order on both modules I directly bridged them and physically made it one module. I deliberately put the BME below but thermic coupling is strong enough to influence the BME sensor's temperature reading, it's 2-3°C too high. Need to rework this and split them up.

Not an issue but I realized too late the ESP32 has built in capacitive sensor capabilites that doesn't require additional hardware for sensor keys.

## Todo

* split BME and ccs modules
* mark the buttons on the front
* create Webserver and book device into Wifi (needs config) to curl the values
* clean that code :-)
* CAD and 3d-print a more professional case
