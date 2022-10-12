/******************************************************************************************************/
// BME68x; Bosch Temp, Humidity, Pressure, VOC
/******************************************************************************************************/
#ifndef BME68x_H_
#define BME68x_H_

#include <bme68xLibrary.h>

// Gas Sensor
// Continous 1Hz             1s   12000uA
// Low Power 0.33 Hz         3s     900uA
// Ultra Low Power 0.0033 Hz 300s    90uA
//

// The (*) marked settings are from example programs,
// highest accuracy is when oversampling is largest and IIR filter longest
// IIR filter is for temp and pressure only
// Default OverSampling: 
//  osTemp = BME68X_OS_2X, 
//  osPres = BME68X_OS_16X, 
//  osHum = BME68X_OS_1X
// Default Filter: 
//  BME68X_FILTER_OFF
//
// Fast measurements: high power, high accuracy
#define intervalBME68xFast             4000                 // 4sec, mimimum 1s 
#define intervalBME68xSlow            60000                 // 60sec
#define bme68x_TempOversampleFast     BME68X_OS_16X         // N,1,2,4,8*,16x
#define bme68x_HumOversampleFast      BME68X_OS_16X         // N,1,2*,4,8,16x
#define bme68x_PressureOversampleFast BME68X_OS_16X         // N,1,2,4*,8,16x
#define bme68x_FilterSizeFast         BME68X_FILTER_SIZE_15 // 0,1,3*,7,15,31,63,127 
// Slow measurements; assuming you want a few datapoints every once in a while
#define bme68x_TempOversampleSlow     BME68X_OS_1X          // 1,2,4,8*,16x
#define bme68x_HumOversampleSlow      BME68X_OS_1X          // 1,2*,4,8,16x
#define bme68x_PressureOversampleSlow BME68X_OS_1X          // 1,2,4*,8,16x
#define bme68x_FilterSizeSlow         BME68X_FILTER_OFF     // 0,1,3*,7,15,31,63,127 
// AirQuality Index is Bosch proprietary software and not used here. We only measure sensor resistance.
// Default Heater Profile 300, 100ms
#define bme68x_HeaterTemp             300                   // C, heater temperature
#define bme68x_HeaterDuration         100                   // ms, time it takes for stable reading

#define bme68x_i2cspeed               I2C_FAST             
#define bme68x_i2cClockStretchLimit   I2C_LONGSTRETCH       // because its on shared bus
// #define bme68x_i2cClockStretchLimit   I2C_DEFAULTSTRETCH

bool initializeBME68x(void);                               // get sensor ready
bool updateBME68x(void);                                   // moving through statemachine
bool startMeasurementsBME68x();                            // request data
bool readDataBME68x();                                     // obtain the requested data
void bme68xJSON(char *payload, size_t len);                // convert readings to serialized JSON
void bme68xJSONMQTT(char *payload, size_t len);            // convert readings to serialized JSON

#endif
