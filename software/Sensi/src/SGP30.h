/******************************************************************************************************/
// SGP30; Senserion TVOC, eCO2
/******************************************************************************************************/
#include <SparkFun_SGP30_Arduino_Library.h>
// Sampling rate: minimum is 1s
// There is no recommended approach to put sensor into sleep mode. 
// Once init command is issued, sensor remains in measurement mode.
// Current Consumption:
//   Measurement mode 49mA
//   Sleep mode 0.002..0.0010 mA
//
#define intervalSGP30Fast 1000                             // as low as 10ms, specification state 1s gives best accuracy
#define intervalSGP30Slow 1000                             // recommended interval is 1s, no slow version
#define intervalSGP30Baseline 300000                       // obtain baseline every 5 minutes
#define intervalSGP30Humidity  60000                       // update humidity once a minute if available from other sensor
#define warmupSGP30_withbaseline 3600000                   // 60min 
#define warmupSGP30_withoutbaseline 43200000               // 12hrs 

#define sgp30_i2cspeed               I2C_FAST             
#define sgp30_i2cClockStretchLimit   I2C_LONGSTRETCH       // because its on shared bus
//#define sgp30_i2cClockStretchLimit   I2C_DEFAULTSTRETCH

bool sgp30_avail  = false;                                 // do we have this sensor
bool sgp30NewData = false;                                 // do we have new data
bool sgp30NewDataWS = false;                               // do we have new data for websocket
TwoWire *sgp30_port = 0;                                   // pointer to the i2c port, might be useful for other microcontrollers
uint8_t sgp30_i2c[2];                                      // the pins for the i2c port, set during initialization
bool baslineSGP30_valid = false;
unsigned long lastSGP30;                                   // last time we obtained data
unsigned long lastSGP30Humidity;                           // last time we upated humidity
unsigned long lastSGP30Baseline;                           // last time we obtained baseline
unsigned long intervalSGP30 = 1000;                        // populated during setup
unsigned long warmupSGP30;                                 // populated during setup
unsigned long errorRecSGP30;
unsigned long startMeasurementSGP30;
unsigned long sgp30_lastError;
enum SGP30SensorStates{SGP30_IS_IDLE = 0, SGP30_IS_MEASURING, SGP30_WAITING_FOR_MEASUREMENT, SGP30_WAITING_FOR_BASELINE, SGP30_HAS_ERROR};
volatile SGP30SensorStates stateSGP30 = SGP30_IS_IDLE; 
bool initializeSGP30(void);
bool updateSGP30(void);
void sgp30JSON(char *payload, size_t len);                  // convert readings to serialized JSON
const char *SGP30errorString(SGP30ERR sgp30Error);          // create error messages
uint8_t sgp30_error_cnt = 0;
SGP30 sgp30;
