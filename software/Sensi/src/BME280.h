/******************************************************************************************************/
// BME280; Bosch Temp, Humidity, Pressure
/******************************************************************************************************/
#include <SparkFunBME280.h>
// Continous 1Hz             1s
// Low Power 0.33 Hz         3s
// Ultra Low Power 0.0033 Hz 300s
// 3.6microA H,P,T
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
// stabdby time = 0, with all three sensors seems to take about 5-6ms, with only one sensor 3ms
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
//////// ===================================================
float bme280_pressure;                                     // pressure from sensor
float bme280_temp;                                         // temperature from sensor
float bme280_hum;                                          // humidity from sensor
bool bme280_avail = false;                                 // do we hace the sensor?
bool bme280NewData = false;                                // is there new data
bool bme280NewDataWS = false;                              // is there new data for websocket
long bme280_measuretime = 0;                               // computed time it takes to complete the measurements and to finish standby
bool BMEhum_avail = false;                                 // no humidity sensor if we have BMP instead of BME
TwoWire *bme280_port =0;                                   // pointer to the i2c port, might be useful for other microcontrollers
uint8_t bme280_i2c[2];                                     // the pins for the i2c port, set during initialization
float bme280_ah = 0;                                       // [gr/m^3]
unsigned long  intervalBME280 = 0;                         // filled automatically during setup
unsigned long  lastBME280;                                 // last time we interacted with sensor
unsigned long  endTimeBME280;                              // when data will be available
float bme280_pressure24hrs = 0.0;                          // average pressure last 24hrs
float alphaBME280;                                         // poorman's low pass filter for f_cutoff = 1 day
volatile SensorStates stateBME280 = IS_IDLE;               // sensor state
bool initializeBME280(void);
bool updateBME280(void);
void bme280JSON(char *payload);							   // convert readings to serialized JSON
BME280 bme280;                                             // the pressure sensor
