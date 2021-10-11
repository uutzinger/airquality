## Runtime Settings

```
================================================================================
Supports........................................................................
........................................................................LCD 20x4
........................................................SPS30 Senserion Particle
.............................................................SCD30 Senserion CO2
......................................................SGP30 Senserion tVOC, eCO2
..........................BME680/280 Bosch Temperature, Humidity, Pressure, tVOC
...................................................CCS811 eCO2 tVOC, Air Quality
..........................................MLX90614 Melex Temperature Contactless
==All Sensors===========================|=======================================
| z: print all sensor data              | n: this device Name, nSensi          |
| p: print current settings             | s: save settings (EEPROM) sj: JSON   |
| d: create default seetings            | r: read settings (EEPROM) rj: JSON   |
| .: execution times                    | L: list content of filesystem        |
==Network===============================|==MQTT=================================
| P1: SSID 1, P1myssid                  | u: mqtt username, umqtt or empty     |
| P2: SSID 2, P2myssid                  | w: mqtt password, ww1ldc8ts or empty |
| P3: SSID 3, P3myssid                  | q: toggle send mqtt immediatly, q    |
| P4: password SSID 1, P4mypas or empty |                                      |
| P5: password SSID 2, P5mypas or empty | P8: mqtt server, P8my.mqtt.com       |
| P6: password SSID 3, P6mypas or empty | P9: mqtt fall back server            |
|-Network Time--------------------------|--------------------------------------|
| o: set time zone oMST7                | N: ntp server, Ntime.nist.gov        |
| https://raw.githubusercontent.com/nayarsystems/posix_tz_db/master/zones.csv  |
| a: night start in min after midnight  | A: night end                         |
| R: reboot time in min after midnight: -1=off                                 |
==Sensors=======================================================================
|-SGP30-eCO2----------------------------|-MAX----------------------------------|
| e: force eCO2, e400                   |                                      |
| v: force tVOC, t3000                  |                                      |
| g: get baselines                      |                                      |
|-SCD30=CO2-----------------------------|--CCS811-eCO2-------------------------|
| f: force CO2, f400.0 in ppm           | c: force basline, c400               |
| t: force temperature offset, t5.0 in C| b: get baseline                      |
|-MLX Temp------------------------------|-LCD----------------------------------|
| m: set temp offset, m1.4              | i: simplified display                |
==Disable===============================|==Disable==============================
| x: 2 LCD on/off                       | x: 11 MLX on/off                     |
| x: 3 WiFi on/off                      | x: 12 BME680 on/off                  |
| x: 4 SCD30 on/off                     | x: 13 BME280 on/off                  |
| x: 5 SPS30 on/off                     | x: 14 CCS811 on/off                  |
| x: 6 SGP30 on/off                     | x: 15 LCD backlight on/off           |
| x: 7 MQTT on/off                      | x: 16 HTML server on/off             |
| x: 8 NTP on/off                       | x: 17 OTA on/off                     |
| x: 9 mDNS on/off                      | x: 18 Serial on/off                  |
| x: 10 MAX30 on/off                    | x: 19 Telnet on/off                  |
| x: 99 reset microcontroller           | x: 20 HTTP Updater on/off            |
|---------------------------------------|--------------------------------------|
| You will need to x99 to initialize the sensor                                |
==Debug Level===========================|==Debug Level==========================
| l: 0 ALL off                          | l: 99 continous                      |
| l: 1 minimal                          | l: 6 SGP30 max level                 |
| l: 2 LCD max level                    | l: 7 MAX30 max level                 |
| l: 3 WiFi max level                   | l: 8 MLX max level                   |
| l: 4 SCD30 max level                  | l: 9 BME680/280 max level            |
| l: 5 SPS30 max level                  | l: 10 CCS811 max level               |
================================================================================
|  Dont forget to save settings                                                |
================================================================================
```