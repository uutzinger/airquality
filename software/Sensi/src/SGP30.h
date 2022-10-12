/******************************************************************************************************/
// SGP30; Senserion TVOC, eCO2
/******************************************************************************************************/
#ifndef SGP30_H_
#define SGP30_H_

#include <SparkFun_SGP30_Arduino_Library.h>

// Sampling rate: minimum is 1s
// There is no recommended approach to put sensor into sleep mode. 
// Once init command is issued, sensor remains in measurement mode.
// Current Consumption:
//   Measurement mode 49mA
//   Sleep mode 0.002..0.0010 mA

#define intervalSGP30Fast                1000              // as low as 10ms, specification state 1s gives best accuracy
#define intervalSGP30Slow                1000              // recommended interval is 1s, no slow version
#define intervalSGP30Baseline          300000              // obtain baseline every 5 minutes
#define intervalSGP30Humidity           60000              // update humidity once a minute if available from other sensor
#define warmupSGP30_withbaseline      3600000              // 60min 
#define warmupSGP30_withoutbaseline  43200000              // 12hrs 

#define sgp30_i2cspeed               I2C_FAST             
#define sgp30_i2cClockStretchLimit   I2C_LONGSTRETCH       // because its on shared bus, otherwise I2C_DEFAULTSTRETCH

enum SGP30SensorStates{SGP30_IS_IDLE = 0, SGP30_IS_MEASURING, SGP30_WAITING_FOR_MEASUREMENT, SGP30_WAITING_FOR_BASELINE, SGP30_HAS_ERROR};

bool initializeSGP30(void);
bool updateSGP30(void);
void sgp30JSON(char *payload, size_t len);                  // convert readings to serialized JSON
void sgp30JSONMQTT(char *payload, size_t len);              // convert readings to serialized JSON

#endif
