## Sensor Electrical Characteristics
| NAME | Power Sup | Logic | Clock Speed | Extra Clock Stretch Limit| Pull ups on breakout board | Board | 
| --- | --- | --- | --- | --- | --- | --- |
| LCD20x4| 5V| 5V | 100kHz| No | | LCD display
| LEVEL Shift|  3.3 & 5V | | | |   5k | Level Shifter
| SPS30 | 5V | 3.3 & 5 V w pullup | 100kHz |           No| Inf (add 10k)| Senserion particle, not compatible with LCD driver
| SCD30  | 3.3-5V | out: 2.4=h 0.4=l in: 1.75-3=h |  50 [...100]kHz | 150ms | Inf (add 10k)| Senserion CO2
| SGP30  | 1.8-3.3V | | 400kHz | No | 10k | Senserion VOC, eCO2 |
| BME680 | 1.8-3.3V | | 400kHz| No | 10k | Bosch Temp, Humidity, Pressure, VOC
| CCS811 | 1.8-3.6V | | 400kHz| 100ms | 5k | Airquality eCO2 tVOC
| MLX90614 | 3.3 or 5V | 3.3V or 5V version | 10-100kHz| No | Inf |  Melex Temp Contactless
| MAX30105 | 1.8, 3.3V| | 400kHz | No|  5k |  Maxim pulseox

Sensirion sensors without pullups (Inf) need an external 10k pullup on SDA and SCL All pullups will add in parallel and lower the total pullup resistor value, therfore requiring more current to operate the i2c bus.

## Sensor characteristics cont.
| Sensor | Measurement Interval | Power Consumption | Sleepmode/Idlemode | Data ready hardware | Cost |
| --- | --- | --- | --- | --- | --- |
|| min, max [default] [s]| [mW]| [mW]|| $
| LCD      | 0.3...
| SCD30    | 2..1800 [4]  | 19  | Reduce Interval      | Yes | 54 |
| SPS30    | 1       | 60  | 0.36    | No  | 45 |
| SGP30    | 1...    | 48  | NA      | No  | 24 |
| BME680   | 1,3,300  | 0.9 | 0.00015 | No  | 10 |
| CCS811   | 0.25,1,10,60   | 4   | 0.019   | Yes | 12 |
| MLX90614 | 0.25/1    |     |         |     |  9 |
| MAX30105 | 0.01... |     |         |     | 12 |

## Sensor characteristics cont.
| Sensor | i2c Address |
| --- | --- | 
| LCD      | x27 generic, x20 Adafruit
| SCD30    | x61
| SCD40    | x62
| SCD41    | x62
| SPS30    | x69
| SGP30    | x58
| BME680   | x76/x77
| BME688   | x76/x77
| BME280   | x76/x77
| CCS811   | x5B (address pin on high) otherwise x5A)
| MLX90614 | x5A
| MAX30105 | x57

