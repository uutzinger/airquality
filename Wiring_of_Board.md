
## I2C interfaces

### I2C_1 D1 D2 400kHz no clock stretch
* SGP30
* BME680
* MAX30105

### I2C_2 D3 D4 100kHz with extra clock stretch
* SPS30 with 5V power and pullup to 3.3V
* SCD30
* CCS811

### I2C_3 D5 D6 100kHz no extra clock stretch
* LCD after 5V Level Shifter, this sensor affects operation of other sensors
* MLX90614  5V Level Shifter if you have 5V vesion, this sensor might be affected by other breakout boards and erroniously reporting negative temp and temp above 500C.

## Wake and Interrupt Pin assignements
* D8: SCD30_RDY interrupt, goes high when data available, remains high until data is read, this requires the sensors to be power switched after uploadiong new program to ESP, if the sensor has data available, the pin is high and the mcu can no longer be programmed

* D0: CCS811_Wake, needs to be low when communicating with sensors, most of the time its high

* D7: CCS811_INT, goes low when data is available, most of the time its high, needs pull up configuration via software

* CCS811 address pin needs to be set to high which is accomplished with connecting adress pin to 3.3V signal

