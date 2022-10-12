/******************************************************************************************************/
// SPS30; Senserion Particle Sensor
/******************************************************************************************************/
#ifndef SPS30_H_
#define SPS30_H_

#include <SPS30_Arduino_Library.h>

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
#define intervalSPS30Fast  4000                            // minimum is 1 sec
#define intervalSPS30Slow 60000                            // system will sleep for intervalSPS30Slow - timetoStable if sleep function is available

#define sps30_i2cspeed               I2C_REGULAR             
#define sps30_i2cClockStretchLimit   I2C_LONGSTRETCH       // or I2C_DEFAULTSTRETCH
#define SPS30_TIMEOUT_DELAY          100
#define SPS30_ERROR_COUNT            24                    // 

bool initializeSPS30(void);
bool updateSPS30(void);
void sps30JSON(char *payload, size_t len);                 // convert readings to serialized JSON
void sps30JSONMQTT(char *payload, size_t len);             // convert readings to serialized JSON

#endif
