//******************************************************************************************************//
// SCD30; Sensirion CO2 sensor
//******************************************************************************************************//
#include <SparkFun_SCD30_Arduino_Library.h>
// Response time is 20sec
// Bootup time 2sec
// Atomatic Basline Correction might take up to 7 days
// Measurement Interval 2...1800 secs
// Does not have sleep mode, but can set measurement intervals
// 2sec interval requires 19mA average current
// The maximum current during reading is 75mA
//
#define intervalSCD30Fast  4000                            // measure every 2sec ... 30minutes, default is 4 
#define intervalSCD30Slow 60000                            // once a minute
#define intervalSCD30Busy   500                            // how frequently to read dataReady when sensor boots up, this is needed to clear the dataready signal for the interrupt
#define intervalPressureSCD30 120000                       // if we have pressure data from other sensor we will provide it to the co2 sensor to improve accuracy in this interval
uint16_t scd30_ppm=0;                                      // co2 concentration from sensor
float scd30_temp=-999.;                                    // temperature from sensor
float scd30_hum=-1.;                                       // humidity from sensor
float scd30_ah=-1.;                                        // absolute humidity, calculated
bool scd30_avail = false;                                  // do we have this sensor?
bool scd30NewData = false;                                 // do we have new data?
bool scd30NewDataWS = false;                               // do we have new data for websocket
TwoWire *scd30_port =0;                                    // pointer to the i2c port, might be useful for other microcontrollers
uint8_t scd30_i2c[2];                                      // the pins for the i2c port, set during initialization
unsigned long intervalSCD30 = 0;                           // will bet set at initilization to either values for Fast or Slow opertion
unsigned long lastSCD30;                                   // last time we interacted with sensor
unsigned long lastPressureSCD30;                           // last time we updated pressure
unsigned long lastSCD30Busy;                               // for the statemachine
unsigned long errorRecSCD30;
const int SCD30interruptPin = SCD30_RDY;                  // 
volatile SensorStates stateSCD30 = IS_IDLE;                // keeping track of sensor state
bool initializeSCD30(void);
bool updateSCD30(void);
void ICACHE_RAM_ATTR handleSCD30Interrupt(void);           // Interrupt service routine when data ready is signaled
void scd30JSON(char *payload);                             // convert readings to serialized JSON
uint8_t scd30_error_cnt = 0;
SCD30 scd30;                                               // the sensor
