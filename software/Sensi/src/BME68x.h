/******************************************************************************************************/
// BME68x; Bosch Temp, Humidity, Pressure, VOC
/******************************************************************************************************/
#include <bme68xLibrary.h>
// Gas Sensor
// Continous 1Hz             1s   12000uA
// Low Power 0.33 Hz         3s     900uA
// Ultra Low Power 0.0033 Hz 300s    90uA
//
#define intervalBME68xFast             4000                // 4sec, mimimum 1s 
#define intervalBME68xSlow            60000                // 60sec
// The (*) marked settings are from example programs,
// highest accuracy is when oversampling is largest and IIR filter longest
// IIR filter is for temp and pressure only
// Default OverSampling: osTemp = BME68X_OS_2X, osPres = BME68X_OS_16X, osHum = BME68X_OS_1X
// Default Filter: BME68X_FILTER_OFF
// Fast measurements: high power, high accuracy
#define bme68x_TempOversampleFast     BME68X_OS_16X         // N,1,2,4,8*,16x
#define bme68x_HumOversampleFast      BME68X_OS_16X         // N,1,2*,4,8,16x
#define bme68x_PressureOversampleFast BME68X_OS_16X         // N,1,2,4*,8,16x
#define bme68x_FilterSizeFast         BME68X_FILTER_SIZE_15 // 0,1,3*,7,15,31,63,127 
// Slow measurements; assuming you want a few datapoints every once in a while
#define bme68x_TempOversampleSlow     BME68X_OS_1X         // 1,2,4,8*,16x
#define bme68x_HumOversampleSlow      BME68X_OS_1X         // 1,2*,4,8,16x
#define bme68x_PressureOversampleSlow BME68X_OS_1X         // 1,2,4*,8,16x
#define bme68x_FilterSizeSlow         BME68X_FILTER_OFF    // 0,1,3*,7,15,31,63,127 
// AirQuality Index is Bosch proprietary software and not used here. We only measure sensor resistance.
// Default Heater Profile 300, 100ms
#define bme68x_HeaterTemp             300                  // C
#define bme68x_HeaterDuration         100                  // ms, time it takes for stable reading

#define bme68x_i2cspeed               I2C_FAST             
#define bme68x_i2cClockStretchLimit   I2C_LONGSTRETCH      // because its on shared bus
// #define bme68x_i2cClockStretchLimit   I2C_DEFAULTSTRETCH

bool bme68x_avail = false;                                 // do we hace the sensor?
bool bme68xNewData = false;                                // do we have new data?
bool bme68xNewDataWS = false;                              // do we have new data for websocket
TwoWire *bme68x_port = 0;                                  // pointer to the i2c port
uint8_t bme68x_i2c[2];                                     // the pins for the i2c port, set during initialization
unsigned long  intervalBME68x;                             // adjusted if sensor reports longer measurement time
unsigned long  lastBME68x;                                 // last time we interacted with sensor
unsigned long  endTimeBME68x;                              // when data will be available
unsigned long  errorRecBME68x;                             // when we plan to recover from the error
unsigned long  startMeasurementBME68x;                     // when we asked for data
unsigned long  bme68x_lastError;                           // when last error occured
float          bme68x_ah = -1.;                            // absolute humidity [gr/m^3]
float bme68x_pressure24hrs = 0.0;                          // average pressure last 24hrs
float alphaBME68x;                                         // poorman's low pass filter for f_cutoff = 1 day
volatile SensorStates stateBME68x = IS_IDLE;               // sensor state
bool initializeBME68x(void);                               // get sensor ready
bool updateBME68x(void);                                   // moving through statemachine
bool startMeasurementsBME68x();                            // request data
bool readDataBME68x();                                     // obtain data
void bme68xJSON(char *payload, size_t len);                // convert readings to serialized JSON
void bme68xJSONMQTT(char *payload, size_t len);            // convert readings to serialized JSON
uint8_t bme68x_error_cnt = 0;                              // give a few retiries if error data length occurs while reading sensor values

Bme68x bme68xSensor;                                       // the pressure airquality sensor
bme68xData bme68x;                                         // global data structure for the sensor
