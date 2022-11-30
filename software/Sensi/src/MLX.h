/******************************************************************************************************/
// MLX contactless tempreture sensor
/******************************************************************************************************/
#ifndef MLX_H_
#define MLX_H_

#include <SparkFunMLX90614.h>

// The MLX sensor has a sleep mode.
// It is possible that traffic on the I2C bus by other sensors wakes it up though.
#define timeToStableMLX            250                     // time until stable internal readings in ms
#define fhDelta                    0.5                     // difference from forehead to oral temperature: https://www.mottchildren.org/health-library/tw9223
#define mlx_i2cspeed               I2C_REGULAR             //
#define mlx_i2cClockStretchLimit   I2C_DEFAULTSTRETCH      //

bool initializeMLX(void);
bool updateMLX(void);
void mlxJSON(char *payload, size_t len);	               // convert readings to serialized JSON
void mlxJSONMQTT(char *payload, size_t len);	           // convert readings to serialized JSON

#endif
