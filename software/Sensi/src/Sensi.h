/******************************************************************************************************/
// Sensi Main Program
/******************************************************************************************************/
#ifndef SENSI_H_
#define SENSI_H_

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define       BAUDRATE      115200                         // serial communicaiton speed, terminal settings need to match
#define       SERIALMAXRATE    500                         // milli secs between serial command inputs

/******************************************************************************************************/
// Sensor
/******************************************************************************************************/
#include <Wire.h>

enum SensorStates{IS_IDLE = 0, IS_MEASURING, IS_BUSY, DATA_AVAILABLE, GET_BASELINE, IS_SLEEPING, IS_WAKINGUP, WAIT_STABLE, HAS_ERROR};
                                                           // IS_IDLE        the sensor is powered up
                                                           // IS_MEASURING   the sensor is creating data autonomously
                                                           // IS_BUSY        the sensor is producing data and will not respond to commands
                                                           // DATA_AVAILABLE new data is available in sensor registers
                                                           // GET_BASELINE   the sensor is creating baseline
                                                           // IS_SLEEPING    the sensor or parts of the sensot are in sleep mode
                                                           // IS_WAKINGUP    the sensor is getting out of sleep mode
                                                           // WAIT_STABLE    readings are not stable yet    
                                                           // HAS_ERROR      the communication with the sensor failed

// Error Handling
// --------------
// Error Count
#define ERROR_COUNT 24              // Number of tries to recover from device error until we give up
#define INTERVAL_TIMEOUT_FACTOR 4   // When data available interrupt is over due (SCD30 and CCS811)
#define rebootHour 3                // what hour will the esp reboot if sensors have error and ntp is available

// Hardware Pin Configuration
// --------------------------
#define SCD30_RDY D8                                       // normal low, active high pin indicating data ready from SCD30 sensor (raising)
#define CCS811_INT D7                                      // normal high, active low interrupt, high to low (falling) at end of measurement
#define CCS811_WAKE D0                                     // normal high, active low to wake up sensor, might need to pull low and high w resistor

#define intervalBaselineSlow 3600000                       // 1 hour
#define intervalBaselineFast  600000                       // 10 minutes

// i2c speed
#define I2C_FAST           100000                          // Fast   400,000 but can also downgrade to regular
#define I2C_REGULAR        100000                          // Normal 100,000
#define I2C_SLOW            50000                          // Slow    50,000

// i2c clock stretch limit
#define I2C_DEFAULTSTRETCH    230                          // Normal 230 micro seconds
#define I2C_LONGSTRETCH    200000                          // Slow   200 milli seconds
// SCD30 needs 150ms timeout
// CCS811 needs 100ms timeout
// Default is 230us

// Blinking settings due to sensors warnings
// ------------------------------------------
// The implementation allows off/on intervals to be different
#define intervalBlinkOff 500                               // blink off time in ms
#define intervalBlinkOn 9500                               // blink on time in ms
#define intervalWarning 60000                              // Check sensors every minute wether to create warning message/blinking

#define LOGFILESIZE 100000
#define INTERVALLOGFILE 5000

/******************************************************************************************************/
// Support Functions
/******************************************************************************************************/
unsigned long yieldOS(void);                                    // returns how long yield took
bool checkI2C(uint8_t address, TwoWire *myWire);                // is device attached at this address?
void switchI2C(TwoWire *myPort, int sdaPin, int sclPin, uint32_t i2cSpeed, uint32_t i2cStretch);
void serialTrigger(const char* mess, int timeout);              // delays until user input
bool inputHandle(void);                                         // hanldes user input
void helpMenu(void);                                            // lists user functions
void printSettings(void);                                       // lists program settings
void defaultSettings(void);                                     // converts settings back to default. need to save to EEPROM to retain
void printSensors(void);                                        // lists current sensor values
void printState(void);                                          // lists states of system devices and sensors
void printProfile();
void timeJSON(char *payload, size_t len);                       // provide time
void timeJSONMQTT(char *payload, size_t len);                   // provide time
void dateJSON(char *payload, size_t len);                       // provide date
void dateJSONMQTT(char *payload, size_t len);                   // provide date
void systemJSON(char *payload, size_t len);                     // provide system stats
void systemJSONMQTT(char *payload, size_t len);                 // provide system stats

#endif
