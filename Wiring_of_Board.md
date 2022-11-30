
## I2C interfaces

### I2C_1 D1 D2 [5,4] 400kHz with extra clock stretch limit
* SGP30
* BME680
* MAX30105
* CCS811 (needs extra clock stretch of 100ms)
* all breakout boards have 5-10k pullup

### I2C_2 D3 D4 [0,2] 100kHz with extra clock stretch limit
* SPS30 with 5V power and 10k pullup to 3.3V, particle sensor, sensor can crash and only recovers after disconnecting power
* SCD30, CO2 sensor (most unusual i2c requirements, signal high is 2.4V, needs extra clock stretch limit 150ms, and prefers slow clock)
* needs 5k pullup

### I2C_3 D5 D6 [14,12] 100kHz no extra clock stretch limit
* LCD after 5V Level Shifter, this sensor affects, operation of other sensors
* MLX90614  5V Level Shifter if you have 5V vesion, this sensor might be affected by other breakout boards and erroniously reports negative temp and temp above 500C, this can be recovered in software
* Level shifter has 5k pullup, MLX has no pullup


## Wake and Interrupt Pin assignements
* D8 [15]: SCD30_RDY interrupt, goes high when data available, remains high until data is read, this requires the sensors to be power switched after uploadiong new program to ESP, if the sensor has data available, the pin is high and the mcu can no longer be programmed

* D0 [16]: CCS811_Wake, needs to be low when communicating with sensors, most of the time its high

* D7 [13]: CCS811_INT, goes low when data is available, most of the time its high, needs pull up configuration via software

* CCS811 address pin needs to be set to high which is accomplished with connecting adress pin to 3.3V signal

## Wiring

### BME280 rH, p, T, resistance
| Pin | Connection |
| --- | ---------- |
| SDA | D2
| SCL | D1
| GND | GND
| VCC | 3.3V
| SD0 | N.C. or GND, default address 0x76
| CSB | N.C. or 3.3V active high, active high

### BME680 rH, p, T, resistance
| Pin | Connection |
| --- | ---------- |
| SDA | D2
| SCL | D1
| GND | GND
| VCC | 3.3V
| SD0 | 3.3V alternate address 0x77
| CS  | N.C. or 3.3V active high, active high

### CCS811 tVOC eCO2
| Pin | Connection |
| --- | ---------- |
| SDA |            D2
| SCL |            D1
| GND |            GND
| VCC |            3.3
| CCS811_WAKE |    D0 active low
| CCS811_INT  |    D7 active low
| CCS811_Address | 3.3V address us 0x5B 
| CCS811_RST     | N.C. active low

### Display
| Pin | Connection |
| --- | ---------- |
| VCC | 5V |
| GND | GND |
| SDA | D6 5V with level shifter |
| SCL | D5 5V  with level shifter |

### SPS30 particulate
| Pin | Connection |
| --- | ---------- |
| VIN | 5V
| GND | GND
| SDA | D4 3.3V
| SCL | D3 3.3V
| SEL | GND for i2c

### SCD30 CO2
| Pin | Connection |
| --- | ---------- |
| VDD | 3.3V
| GND | GND
| SCL | D3
| SDA | D4
| RDY | D8 input, active high
| SEL | GND for i2c

### SGP30 tVOC eCO2
| Pin | Connection |
| --- | ---------- |
| VDD | 3.3
| GND | GND
| SCL | D1
| SDA | D2