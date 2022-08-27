/******************************************************************************************************/
// CCS811; Airquality CO2 tVOC
/******************************************************************************************************/
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
#define ccs811ModeFast 2                                   // 4=0.25s, 3=60s, 2=10sec, 1=1sec.
#define ccs811ModeSlow 3                                   // 
#define baselineCCS811Fast       300000                    // 5 mins
#define baselineCCS811Slow      3600000                    // 1 hour
#define updateCCS811HumitityFast 120000                    // 2 min, update humitidy from other sensor to improve accuracy
#define updateCCS811HumititySlow 300000                    // 5 mins, update humidity from other sensor to improve accuracy
//#define stablebaseCCS811     86400000                    // sensor needs 24hr until baseline stable
#define stablebaseCCS811       43200000                    // sensor needs 12hr until baseline stable
#define burninCCS811          172800000                    // sensor needs 48hr burn in

#define ccs811_i2cspeed               I2C_FAST             
#define ccs811_i2cClockStretchLimit   I2C_LONGSTRETCH
bool ccs811_avail = false;                                 // do we have this sensor?
bool ccs811NewData = false;                                // do we have new data
bool ccs811NewDataWS = false;                              // do we have new data for websocket
TwoWire *ccs811_port =0;                                   // pointer to the i2c port, might be useful for other microcontrollers
uint8_t ccs811_i2c[2];                                     // the pins for the i2c port, set during initialization
unsigned long lastCCS811;                                  // last time we interacted with sensor
unsigned long lastCCS811Baseline;                          // last time we obtained baseline
unsigned long lastCCS811Humidity;                          // last time we update Humidity on sensor
volatile unsigned long lastCCS811Interrupt;                // last time we update Humidity on sensor
unsigned long warmupCCS811;                                // sensor needs 20min conditioning 
unsigned long intervalCCS811Baseline;                      // get the baseline every few minutes
unsigned long intervalCCS811Humidity;                      // update the humidity every few minutes
unsigned long intervalCCS811;                              // to check if interrupt timed out
unsigned long errorRecCCS811;
unsigned long startMeasurementCCS811;
unsigned long ccs811_lastError;
uint8_t ccs811Mode;                                        // operation mode, see above 
uint8_t ccs811_error_cnt =0;
const uint8_t CCS811interruptPin = CCS811_INT;                // CCS811 not Interrupt Pin
volatile SensorStates stateCCS811 = IS_IDLE;               // sensor state
bool initializeCCS811(void);                               // 
bool updateCCS811(void);                                   //
void ICACHE_RAM_ATTR handleCCS811Interrupt(void);          // interrupt service routine to handle data ready signal
//CCS811Core::CCS811_Status_e ccs811Error                  // Error structure
void ccs811JSON(char *payload, size_t len);			       // convert readings to serialzied JSON
void ccs811JSONMQTT(char *payload, size_t len);			   // convert readings to serialzied JSON
CCS811 ccs811(0X5B);                                       // the sensor, if alternative address is used, the address pin will need to be set to high
