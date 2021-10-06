/******************************************************************************************************/
// MAX30105; pulseox
/******************************************************************************************************/
// Not implemented yet
//#include ...
#define intervalMAX 1000 // fix this once code is available
const long intervalMAX30 = 1000;                           // readout intervall in ms
bool max_avail    = false;                                 // do we have sensor?
bool maxNewData   = false;                                 // do we have new data?
bool maxNewDataWS = false;                                 // do we have new data for websocket
TwoWire *max_port =0;                                      // pointer to the i2c port, might be useful for other microcontrollers
uint8_t max_i2c[2];                                        // the pins for the i2c port, set during initialization
unsigned long lastMAX;                                     // last time we interacted with sensor
volatile SensorStates stateMAX = IS_IDLE;                  // sensor state
