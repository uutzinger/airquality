/******************************************************************************************************/
// SPS30; Senserion Particle Sensor
/******************************************************************************************************/
#include <sps30.h>
// Sample interval min 1+/-0.04s
// After power up, sensor is idle
// Execute start command to start measurements
// Stop measurement, the sensor goes to idle
// Default cleaning interval is 604800 seconds (one week)
// Supply Current:
// Max              80.  mA
// Measurement Mode 65.  mA
// Idle Mode         0.36mA
// Sleep Mode        0.05mA (needs firmware 2.2 and newer)
// Time to reach stable measurement: 8s for 200 – 3000 #/cm3 particles
//                                  16s for 100 – 200  #/cm3 particles
//                                  30s for  50 – 100  #/cm3 particles
// Sleep mode can be entered when sensor is idle but only with firmware 2.2 or greater.
// There appears to be no firmware updater on Sensirion support website.
//
#define intervalSPS30Fast  5000                            // minimum is 1 sec
#define intervalSPS30Slow 60000                            // system will sleep for intervalSPS30Slow - timetoStable if sleep function is available
#define SPS30Debug 0                                       // define driver debug: 0 np debug
//   0 - no messages,
//   1 - request sending and receiving,
//   2 - request sending and receiving + show protocol errors */
unsigned long intervalSPS30 = 0;                           // measurement interval
unsigned long timeSPS30Stable;                             // time when readings are stable, is adjusted automatically based on particle counts
unsigned long lastSPS30;                                   // last time we interacted with sensor
unsigned long wakeSPS30;                                   // time when wakeup was issued
unsigned long wakeTimeSPS30;                               // time when sensor is supposed to be woken up
unsigned long timeToStableSPS30;                           // how long it takes to get stable readings, automatically pupulated based on total particles
char buf[64];                                              // messaging buffer
uint8_t ret, st;                                           // return variables
float totalParticles;                                      // we need to calculate time to stable readings depending on total particle concentration
uint32_t autoCleanIntervalSPS30;                           // current cleaning interval setting in sensor
bool sps30_avail = false;                                  // do we have this sensor?
bool sps30NewData = false;                                 // do we have new data?
bool sps30NewDataWS = false;                               // do we have new data for websocket
TwoWire *sps30_port =0;                                    // pointer to the i2c port, might be useful for other microcontrollers
uint8_t sps30_i2c[2];                                      // the pins for the i2c port, set during initialization
volatile SensorStates stateSPS30 = IS_BUSY;                // sensor state
struct sps_values valSPS30;                                // will hold the readings from sensor
unsigned int sps_error_cnt = 0;                            // give a few retiries if error data length occurs while reading sensor values
SPS30_version v;                                           // version structure of sensor
bool initializeSPS30(void);
bool updateSPS30(void);
void sps30JSON(char *payload);							   // convert readings to serialized JSON
SPS30 sps30;                                               // the particle sensor
