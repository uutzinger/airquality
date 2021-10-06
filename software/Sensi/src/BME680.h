/******************************************************************************************************/
// BME680; Bosch Temp, Humidity, Pressure, VOC
/******************************************************************************************************/
#include <Adafruit_Sensor.h>
#include "Adafruit_BME680.h"
// Gas Sensor
// Continous 1Hz             1s   12000uA
// Low Power 0.33 Hz         3s     900uA
// Ultra Low Power 0.0033 Hz 300s    90uA
//
#define intervalBME680Fast             4000                // 4sec, mimimum 1s 
#define intervalBME680Slow            60000                // 60sec
// The (*) marked settings are from example programs,
// highest accuracy is when oversampling is largest and IIR filter longest
// IIR filter is for temp and pressure only
// Fast measurements: high power, high accuracy
#define bme680_TempOversampleFast     BME680_OS_16X        // 1,2,4,8*,16x
#define bme680_HumOversampleFast      BME680_OS_16X        // 1,2*,4,8,16x
#define bme680_PressureOversampleFast BME680_OS_16X        // 1,2,4*,8,16x
#define bme680_FilterSizeFast         BME680_FILTER_SIZE_15 // 0,1,3*,7,15,31,63,127 
// Slow measurements; assuming you want a few datapoints every once in a while
#define bme680_TempOversampleSlow     BME680_OS_1X         // 1,2,4,8*,16x
#define bme680_HumOversampleSlow      BME680_OS_1X         // 1,2*,4,8,16x
#define bme680_PressureOversampleSlow BME680_OS_1X         // 1,2,4*,8,16x
#define bme680_FilterSizeSlow         BME680_FILTER_SIZE_0 // 0,1,3*,7,15,31,63,127 
// AirQuality Index is Bosch proprietary software and not used here. We only measure sensor resistance.
#define bme680_HeaterTemp             320                  // C
#define bme680_HeaterDuration         150                  // ms, time it takes for stable reading
bool bme680_avail = false;                                 // do we hace the sensor?
bool bme680NewData = false;                                // do we have new data?
bool bme680NewDataWS = false;                              // do we have new data for websocket
TwoWire *bme680_port =0;                                   // pointer to the i2c port
uint8_t bme680_i2c[2];                                     // the pins for the i2c port, set during initialization
float          bme680_ah = 0.;                             // absolute humidity [gr/m^3]
unsigned long  intervalBME680;                             // adjusted if sensor reports longer measurement time
unsigned long  lastBME680;                                 // last time we interacted with sensor
unsigned long  endTimeBME680;                              // when data will be available
float bme680_pressure24hrs = 0.0;                          // average pressure last 24hrs
float alphaBME680;                                         // poorman's low pass filter for f_cutoff = 1 day
volatile SensorStates stateBME680 = IS_IDLE;               // sensor state
bool initializeBME680(void);
bool updateBME680(void);
void bme680JSON(char *payload);                            // convert readings to serialized JSON
uint8_t bme680_error_cnt = 0;                                 // give a few retiries if error data length occurs while reading sensor values
Adafruit_BME680 bme680;                                    // the pressure airquality sensor
