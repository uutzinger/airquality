/******************************************************************************************************/
// CCS811; Airquality CO2 tVOC
/******************************************************************************************************/
#ifndef CCS811_H_
#define CCS811_H_

#include <SparkFunCCS811.h> 

// Sensor has 20min equilibrium time and 48hrs burn in time
// Sensor is read through interrupt handling (in this software implementation):
// Power on takes 20ms
// Mode 0 – Idle (Measurements are disabled in this mode)
// Mode 1 – Constant power mode, IAQ measurement every second
// Mode 2 – Pulse heating mode IAQ measurement every 10 seconds
// Mode 3 – Low power pulse heating mode IAQ measurement every 60 seconds
// Mode 4 – Const power pulse heating mode IAQ measurement every 0.25 seconds
//
// Baseline takes 24 hrs to establish, R_A is reference resistance
// Sleep mode: turns off i2c, 0.019 mA power consumption
//    
#define ccs811ModeFast                        2            // 4=0.25s, 3=60s, 2=10sec, 1=1sec.
#define ccs811ModeSlow                        3            // 
#define baselineCCS811Fast               300000            // 5 mins
#define baselineCCS811Slow              3600000            // 1 hour
#define updateCCS811HumitityFast         120000            // 2 min, update humitidy from other sensor to improve accuracy
#define updateCCS811HumititySlow         300000            // 5 mins, update humidity from other sensor to improve accuracy
#define stablebaseCCS811               43200000            // sensor needs 12hr until baseline stable, 24hrs=86400000 
#define burninCCS811                  172800000            // sensor needs 48hr burn in
//
#define ccs811_i2cspeed               I2C_FAST             
#define ccs811_i2cClockStretchLimit   I2C_LONGSTRETCH

bool initializeCCS811(void);                               // 
bool updateCCS811(void);                                   //
void ICACHE_RAM_ATTR handleCCS811Interrupt(void);          // interrupt service routine to handle data ready signal
void ccs811JSON(char *payload, size_t len);			       // convert readings to serialzied JSON
void ccs811JSONMQTT(char *payload, size_t len);			   // convert readings to serialzied JSON

#endif
