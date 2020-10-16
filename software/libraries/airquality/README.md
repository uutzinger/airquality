# Air Quality Index

## ===========================================================

This library will store PM2.5 and PM10. Based on the captured data and
the region setting, it can calculate an air qualityindex that is applicable
for that region. The data during-hour and during-day is stored RAM and
at end-of-day it will be combined with data from previous days that is stored in NVRAM.
<br> See the seperate document (AQI-odt) for reasons and more information

## Getting Started

As part of a longer term project to understand air quality, I have been working
to test and implement  different sensors. One of the aspects that keeps coming back
is Air Quality Index (AQI). Actually I should say index-es. (plural)<br>
There are MANY different AQI definitions and while they all provide an
indication of the air quality: they are not the same. They use different
bands/ranges, different limits, different pollutants, different timing
(hourly, daily or yearly), different….. different… <br>
Next to AQI in the different regions of the world (some have multiple AQI
in the same region) there are also suppliers that have added
to the confusion by creating their own definition.

While most AQI use a combination of pollutants, this library is only using
PM2.5 and/or PM10 to determine an index. So it is not a 100% correct.

I have used the Air Quality definitions from the USA, Europe, India, Canada and UK.

It is designed and tested for ESP32 and supports ESP8266. See document for reasons

## Prerequisites
Examples 10 has a dependency on other libraries. Documented in sketch.

## Software installation
Obtain the zip and install like any other

## Program usage
### Program options
Please see the description in the top of the sketch and read the documentation (odt)

## Versioning

### version 1.0 / March 2019
 * Initial version ESP32

### version 1.0.1 / March 2019
 * Added base-option to GetAqi()
 * number cosmetic changes

### version 1.0.2 / March 2019
 * Added support for ESP8266-thing from Sparkfun (https://www.sparkfun.com/products/13231)
 * MAKE SURE TO READ the remark about reset in the sketch and document
 * some cosmetic changes

### version 1.0.3 / March 2020
 * Fixed compiler warnings with IDE 1.8.12

## Author
 * Paul van Haastrecht (paulvha@hotmail.com)

## License
This project is licensed under the GNU GENERAL PUBLIC LICENSE 3.0

## Acknowledgements
https://en.wikipedia.org/wiki/Air_quality_index <br>

