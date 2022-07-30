/******************************************************************************************************/
// MLX contact less tempreture sensor
/******************************************************************************************************/
#include <SparkFunMLX90614.h>
// The MLX sensor has a sleep mode.
// It is possible that traffic on the I2C bus by other sensors wakes it up though.
#define timeToStableMLX 250                                // time until stable internal readings in ms
#define fhDelta 0.5                                        // difference from forehead to oral temperature: https://www.mottchildren.org/health-library/tw9223
#define emissivity 0.98                                    // emissivity of skin

#define mlx_i2cspeed               I2C_REGULAR             
#define mlx_i2cClockStretchLimit   I2C_DEFAULTSTRETCH

unsigned long intervalMLX = 1000;                          // readout intervall in ms, 250ms minimum
float mlxOffset = 1.4;                                     // offset to adjust for sensor inaccuracy,
// reading a black surface should give the same value as room temperature measuresd with other sensors
// measuring wall or ceiling differs from room temp
bool therm_avail = false;                                  // do we hav e the sensor?
bool mlxNewData = false;                                   // do we have new data
bool mlxNewDataWS = false;                                 // do we have new data for websocket
TwoWire *mlx_port =0;                                      // pointer to the i2c port, might be useful for other microcontrollers
uint8_t mlx_i2c[2];                                        // the pins for the i2c port, set during initialization
unsigned long lastMLX;                                     // last time we interacted with sensor
unsigned long sleepTimeMLX;                                // computed internally
unsigned long errorRecMLX;
uint8_t mlx_error_cnt = 0;                                 //
uint8_t therm_error_cnt = 0;                               //
unsigned long  startMeasurementMLX;
unsigned long  mlx_lastError;
volatile SensorStates stateMLX = IS_IDLE;                  // sensor state
bool initializeMLX(void);
bool updateMLX(void);
void mlxJSON(char *payload, size_t len);	               // convert readings to serialized JSON
IRTherm therm;                                             // the IR thermal sensor
