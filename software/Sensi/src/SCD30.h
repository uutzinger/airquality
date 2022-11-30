//******************************************************************************************************//
// SCD30; Sensirion CO2 sensor
//******************************************************************************************************//
#ifndef SCD30_H_
#define SCD30_H_

#include <SparkFun_SCD30_Arduino_Library.h>

// Response time is 20sec
// Bootup time 2sec
// Atomatic Basline Correction might take up to 7 days
// Measurement Interval 2...1800 secs
// Does not have sleep mode, but can set measurement intervals
// 2sec interval requires 19mA average current
// The maximum current during reading is 75mA
//
#define intervalSCD30Fast              4000                // measure every 2sec ... 30minutes, default is 4 
#define intervalSCD30Slow             60000                // once a minute
#define intervalSCD30Busy               500                // how frequently to read dataReady when sensor boots up, this is needed to clear the dataready signal for the interrupt
#define intervalPressureSCD30        120000                // if we have pressure data from other sensor we will provide it to the co2 sensor to improve accuracy in this interval

#define scd30_i2cspeed               I2C_REGULAR             
#define scd30_i2cClockStretchLimit   I2C_LONGSTRETCH

bool      initializeSCD30(void);
bool      updateSCD30(void);
void      ICACHE_RAM_ATTR handleSCD30Interrupt(void);      // Interrupt service routine when data ready is signaled
void      scd30JSON(char *payload, size_t len);                        // convert readings to serialized JSON
void      scd30JSONMQTT(char *payload, size_t len);                    // convert readings to serialized JSON

#endif
