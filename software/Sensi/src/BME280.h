/******************************************************************************************************/
// BME280; Bosch Temp, Humidity, Pressure
/******************************************************************************************************/
#ifndef BME280_H_
#define BME280_H_

#include <SparkFunBME280.h>

// --------------------------------------
// runMode  0=SLEEP 1=FORCED 2=FORCED 3=NORMAL
// tStandby 0=0.5ms 1=62.5ms 2=125ms 3=250ms 4=500 ms 5=1000ms 6=10ms 7=20ms   
// filter   0=off,1(75% response) 1=2,2 2=4,5 3=8,11 4=16,22 
// tempOverSample & pressOverSample & humidOverSample can be: 
// 0=off 1=1x 2=2x 3=4x 4=8x 5=16x
// 
// oversampling does not seem to affect noise, nor measurement time
// filter reduces noise, does not affect mesurement time
// standby time = 5, measurements completes in about 800ms
// standby time = 0, with all three sensors seems to take about 5-6ms, with only one sensor 3ms
// humidity sensor is broken and reports 0
// No User Defined Section =================================
// Fast measurements: high power consmpution and high accuracy
//   Uses standbytime
#define bme280_TempOversampleFast     5                    // 16x
#define bme280_HumOversampleFast      5                    // 16x
#define bme280_PressureOversampleFast 5                    // 16x
#define bme280_FilterFast             4                    // 16 data poinrts
#define bme280_ModeFast               MODE_NORMAL          // run autonomously
#define bme280_StandbyTimeFast        5                    // 1000ms
#define intervalBME280Fast            0                    // this will use the calculated interval time 
// Slow measurements: low energy, low accuracy
//   MODE_FORCED does not use standby time, sensor goes back to sleep right after reading
#define bme280_TempOversampleSlow     1                    // 1x
#define bme280_HumOversampleSlow      1                    // 1x
#define bme280_PressureOversampleSlow 1                    // 1x
#define bme280_FilterSlow             4                    // 16 data points
#define bme280_ModeSlow               MODE_FORCED          // measure manually
#define bme280_StandbyTimeSlow        0                    // is not relevant
#define intervalBME280Slow            60000                // 1 minute
//
#define bme280_i2cspeed               I2C_FAST             
#define bme280_i2cClockStretchLimit   I2C_LONGSTRETCH      // because its on shared bus otherwise I2C_DEFAULTSTRETCH

// Continous 1Hz             1s
// Low Power 0.33 Hz         3s
// Ultra Low Power 0.0033 Hz 300s
// 3.6microA H,P,T
bool initializeBME280(void);
bool updateBME280(void);
void bme280JSON(char *payload, size_t len);				   // convert readings to serialized JSON
void bme280JSONMQTT(char *payload, size_t len);			   // convert readings to serialized JSON

#endif