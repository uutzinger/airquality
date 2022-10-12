/******************************************************************************************************/
// SCD30, CO2, Humidity, Temperature
/******************************************************************************************************/
// float scd30_temp
// float scd30_hum
// float scd30_ah
// uint16_t getCO2()
// float getHumidity()
// float getTemperature()
// float getTemperatureOffset()
// uint16_t getAltitudeCompensation()
  
// #include "src/SCD4x.h"
#include "src/BME68x.h"
#include "src/BME280.h"
#include "src/Config.h"
#include "src/Sensi.h"
#include "src/Quality.h"
#include "src/Print.h"

/******************************************************************************************************/
// Initialize SCD30
/******************************************************************************************************/

/******************************************************************************************************/
// Update SCD4x
/******************************************************************************************************/
// Operation:
//   Sensor is initilized
//     Measurement interval is set
//     Autocalibrate is enabled
//     Temperature offset to compensate self heating, improves humidity reading
//     Pressure is set
//   Begin Measureing
//   If sensor has completed measurement ISR is triggered
//   ISR advances statemachine
//    Check if Data Available
//     get CO2, Humidity, Temperature
//    Update barometric pressure every once in a while if third party sensor is avaialble
//    Wait until next ISR
