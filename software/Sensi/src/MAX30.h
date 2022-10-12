/******************************************************************************************************/
// MAX30105; pulseox
/******************************************************************************************************/
#ifndef MAX30_H_
#define MAX30_H_

// Not implemented yet
//#include ...

#define intervalMAX                1000                  // fix this once code is available
#define max30_i2cspeed             I2C_FAST              // i2c speed for MAX30105
#define max30_i2cClockStretchLimit I2C_LONGSTRETCH       // because its on shared bus, otherwise I2C_DEFAULTSTRETCH

bool initializeMAX30(void);
bool updateMAX30(void);
void max30JSON(char *payload, size_t len);				   // convert readings to serialized JSON
void max30JSONMQTT(char *payload, size_t len);			   // convert readings to serialized JSON

#endif
