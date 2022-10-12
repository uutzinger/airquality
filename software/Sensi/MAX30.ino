/******************************************************************************************************/
// MAX30105
/******************************************************************************************************/
#include "src/MAX30.h"
#include "src/Sensi.h"
#include "src/Config.h"
#include "src/Quality.h"

const long            intervalMAX30  = 1000;               // readout intervall in ms
bool                  max30_avail    = false;              // do we have sensor?
bool                  max30NewData   = false;              // do we have new data?
bool                  max30NewDataWS = false;              // do we have new data for websocket
uint8_t               max30_i2c[2];                        // the pins for the i2c port, set during initialization
unsigned long         lastMAX30;                           // last time we interacted with sensor

volatile SensorStates stateMAX = IS_IDLE;                  // sensor state
TwoWire              *max30_port = 0;                        // pointer to the i2c port, might be useful for other microcontrollers

/******************************************************************************************************/
// Initialize
/******************************************************************************************************/
bool initializeMAX30() {
  return false;
}

/******************************************************************************************************/
// Update
/******************************************************************************************************/
bool updateMAX30(void) {
  return false;
}

/******************************************************************************************************/
// JSON
/******************************************************************************************************/

void max30JSON(char *payLoad, size_t len){
  const char * str = "{ \"max30\": ";
  size_t l = strlen(str);
  strlcpy(payLoad, str, l+1);
  max30JSONMQTT(payLoad+l, len-l-1);
  strlcat(payLoad, "}", len);
}

void max30JSONMQTT(char *payLoad, size_t len) {
  // Not Implemented Yet
  snprintf_P(payLoad, len, PSTR("{ \"avail\": %s, \"HR\": %5.1f, \"O2Sat\": %5.1f, \"MAX_quality\": \"%s\"}"), 
                       max30_avail ? "true" : "false", 
                       -1.0,
                       -1.0,
                       "n.a.");  
}
