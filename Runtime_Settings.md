## Runtime Settings

```
================================================================================
Sensi, 2020-2022, Urs Utzinger                                             2.3.0
================================================================================
Supports........................................................................
........................................................................LCD 20x4
........................................................SPS30 Senserion Particle
.............................................................SCD30 Senserion CO2
......................................................SGP30 Senserion tVOC, eCO2
..........................BME68x/280 Bosch Temperature, Humidity, Pressure, tVOC
...................................................CCS811 eCO2 tVOC, Air Quality
..........................................MLX90614 Melex Temperature Contactless
==General=======================================================================
| All commands preceeded by ZZ                                                 |
--------------------------------------------------------------------------------
| ?: help screen                        | n: set this device name, nSensi      |
| z: print sensor data                  |                                      |
| j: print sensor data in JSON          | .: execution times                   |
==WiFi==================================|=======================================
| W: WiFi states                        |                                      |
| Ws1: set SSID 1 Ws1UAWiFi             | Wp1: set password                    |
| Ws2: set SSID 2                       | Wp2: set password                    |
| Ws3: set SSID 3                       | Wp3: set password                    |
==MQTT==================================|=======================================
| Mu: set username                      | Mi: set time interval Mi1.0 [s]      |
| Mp: set password                      | Ms: set server                       |
| Mm: individual/single msg             | Mf: set fallback server              |
==NTP===================================|=======================================
| Ns: set server                        | Nn: set night start min after midni  |
| Nf: set fallback server               | Ne: set night end min after midnight |
| Nt: set time zone                     | Nr: set reboot time -1=off           |
| https://raw.githubusercontent.com/nayarsystems/posix_tz_db/master/zones.csv  |
==Weather===============================|=======================================
| Rk: set api key Wae05a02...           | Ra: set altitude in meters           |
| Rc: set city id WcTucson              | Rp: set average pressure in [mbar]   |
| Rl: set country WcUS                  |                                      |
==SGP30=================================|==SCD30================================
| Pc: force eCO2 Gc400 ppm              | Dc: force eCO2 Cc400 ppm             |
| Pv: force tVOC Gv100 ppb              | Dt: set temp offset Ct5.0 [C]        |
| Pg: get baseline                      | DT: get temp offset CT               |
|                                       | Dp: set ambinet pressure Dp5 [mbar]  |
==CCS811================================|==MLX==================================
| Cc: force baseline [uint16]           | Xt: set temp offset Xt5.0 [C]        |
| Cb: get baseline [uint16]             |                                      |
==LCD===================================|=======================================
| Li: display layout 1..4 Li4           | 1:simple 2:engr 3:1p 4:1p w time & W |
==Disable===============================|==Disable==============================
| x: 2 LCD on/off                       | x: 14 CCS811 on/off                  |
| x: 3 WiFi on/off                      | x: 15 LCD backlight on/off           |
| x: 4 SCD30 on/off                     | x: 16 LCD backlight at night on/off  |
| x: 5 SPS30 on/off                     | x: 17 Blink backlgiht at night on/off|
| x: 6 SGP30 on/off                     | x: 18 HTML server on/off             |
| x: 7 MQTT on/off                      | x: 19 OTA on/off                     |
| x: 8 NTP on/off                       | x: 20 Serial on/off                  |
| x: 9 mDNS on/off                      | x: 21 Telnet on/off                  |
| x: 10 MAX30 on/off                    | x: 22 HTTP Updater on/off            |
| x: 11 MLX on/off                      | x: 23 Logfile on/off                 |
| x: 12 BME68x on/off                   | x: 24 Weather on/off                 |
| x: 13 BME280 on/off                   | x: 99 Reset/Reboot                   |
|---------------------------------------|--------------------------------------|
| You will need to reboot with x99 to initialize the sensor                    |
==Debug Level===========================|==Debug Level==========================
| l: 0 ALL off                          | l: 6 SGP30 max level                 |
| l: 1 Errors only                      | l: 7 MAX30 max level                 |
| l: 2 Minimal                          | l: 8 MLX max level                   |
| l: 3 WiFi max level                   | l: 9 BME68x/280 max level            |
| l: 4 SCD30 max level                  | l: 10 CCS811 max level               |
| l: 5 SPS30 max level                  | l: 11 LCD max level                  |
| l: 99 continous                       |                                      |
==Settigns==============================|==Filesystem===========================
| s:   print current settings           | Fff: format filesystem               |
| FsE: save settings to EEPROM          | F:  list content of filesystem       |
| FsJ: save settings to Sensio.json     | Fp: print file Fp/Sensi.json         |
| FrE / FrS: read settings              | Fx: delete file Fx/Sensi.bak         |
| dd: default settings                  | Fw: write into file Fw/index.html ^Z |
================================================================================
```