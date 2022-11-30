/******************************************************************************************************/
// MLX, ambient and object temperature
/******************************************************************************************************/
// float therm.object()
// float therm.ambient()
// float therm.readEmissivity()

#include "src/MLX.h"
#include "src/Sensi.h"
#include "src/Config.h"
#include "src/Quality.h"
#include "src/Print.h"

// reading a black surface should give the same value as room temperature measuresd with other sensors
// measuring wall or ceiling differs from room temp
float                 mlxOffset = 1.4;                     // offset to adjust for sensor inaccuracy,
bool                  therm_avail = false;                 // do we hav e the sensor?
bool                  mlxNewData = false;                  // do we have new data
bool                  mlxNewDataHandeled = false;          // have we handeled the new data?
bool                  mlxNewDataWS = false;                // do we have new data for websocket
uint8_t               mlx_i2c[2];                          // the pins for the i2c port, set during initialization
unsigned long         intervalMLX = 1000;                  // readout intervall in ms, 250ms minimum
unsigned long         lastMLX;                             // last time we interacted with sensor
unsigned long         sleepTimeMLX;                        // computed internally
unsigned long         errorRecMLX;
uint8_t               mlx_error_cnt = 0;                   //
uint8_t               therm_error_cnt = 0;                 //
unsigned long         startMeasurementMLX;
unsigned long         mlx_lastError;
volatile SensorStates stateMLX = IS_IDLE;                  // sensor state
TwoWire              *mlx_port =0;                         // pointer to the i2c port, might be useful for other microcontrollers
IRTherm               therm;                               // the IR thermal sensor

// Extern variables
extern bool           fastMode;         // Sensi
extern bool           intervalNewData;
extern bool           availNewData;
extern unsigned long  yieldTime;        // Sensi
extern unsigned long  lastYield;        // Sensi
extern Settings       mySettings;       // Config
extern unsigned long  currentTime;      // Sensi
extern char           tmpStr[256];           // Sensi

/******************************************************************************************************/
// Initialize MLX
/******************************************************************************************************/

bool initializeMLX(){
  
  switchI2C(mlx_port, mlx_i2c[0], mlx_i2c[1], mlx_i2cspeed, mlx_i2cClockStretchLimit);
  
  if (therm.begin(0x5A, *mlx_port) == true) { 
    therm.setUnit(TEMP_C);                    // Set the library's units to Centigrade
    therm.setEmissivity(mySettings.emissivity);
    if (mySettings.debuglevel > 0) { 
      snprintf_P(tmpStr, sizeof(tmpStr), PSTR("MLX: emissivity: %f"), therm.readEmissivity()); 
      R_printSerialTelnetLogln(tmpStr); 
    }
    stateMLX = IS_MEASURING;      
  } else {
    if (mySettings.debuglevel > 0) { 
      R_printSerialTelnetLogln(F("MLX: sensor not detected. Please check wiring")); 
    }
    stateMLX = HAS_ERROR;
    errorRecMLX = currentTime + 5000;
    return(false);
  }

  sleepTimeMLX = intervalMLX - timeToStableMLX - 50;
  if (mySettings.debuglevel > 0) {
    snprintf_P(tmpStr, sizeof(tmpStr), PSTR("MLX: sleep time is %lums"),   sleepTimeMLX); 
    printSerialTelnetLogln(tmpStr);
    snprintf_P(tmpStr, sizeof(tmpStr), PSTR("MLX: interval time is %lums"), intervalMLX); 
    printSerialTelnetLogln(tmpStr); 
  }
  intervalNewData = true;

  if (mySettings.tempOffset_MLX_valid == 0xF0) {
    mlxOffset = mySettings.tempOffset_MLX;
    if (mySettings.debuglevel > 0) { printSerialTelnetLogln(F("MLX: offset found")); }
  } else {
    if (mySettings.debuglevel > 0) { printSerialTelnetLogln(F("MLX: no offset found")); }
  }

  if (mySettings.debuglevel > 0) { printSerialTelnetLogln(F("MLX: initialized")); }
  delay(50); lastYield = millis();

  return(true);
}

/******************************************************************************************************/
// Update MLX
/******************************************************************************************************/
bool updateMLX() {
  bool success = true;  // when ERROR recovery fails, success becomes false
  
  switch(stateMLX) {
    
    case IS_MEASURING : { //---------------------
      if ( (currentTime - lastMLX) > intervalMLX ) {
        D_printSerialTelnet(F("D:U:MLX:IM.."));
        switchI2C(mlx_port, mlx_i2c[0], mlx_i2c[1], mlx_i2cspeed, mlx_i2cClockStretchLimit);
        startMeasurementMLX = millis();
        if (therm.read()) {
          if (mySettings.debuglevel >= 2) { 
            snprintf_P(tmpStr, sizeof(tmpStr), PSTR("MLX: T read in %ldms"), (millis()-startMeasurementMLX)); 
            R_printSerialTelnetLogln(tmpStr); 
          }
          lastMLX = currentTime;
          // check if temperature is in valid range
          if ( (therm.object() < -273.15) || ( therm.ambient() < -273.15) ) { 
            if (mySettings.debuglevel > 0) { 
              snprintf_P(tmpStr, sizeof(tmpStr), PSTR("MLX: data range error, %u"), mlx_error_cnt); 
              R_printSerialTelnetLogln(tmpStr); 
            }
            if (mlx_error_cnt++ > 3) { // allow 3 retries on i2c bus until ERROR state or sensor is triggered
              stateMLX = HAS_ERROR;
              errorRecMLX = currentTime + 5000;
              mlx_error_cnt = 0;
            }
          } else { 
            mlx_error_cnt = 0; 
            therm_error_cnt = 0;
            mlxNewData = true;
            mlxNewDataWS = true;
          }
          if (fastMode == false) {
            therm.sleep();
            if (mySettings.debuglevel >= 2) { printSerialTelnetLogln(F("MLX: sent to sleep")); }
            stateMLX = IS_SLEEPING;
          }
        } else {
          // read error
          lastMLX = currentTime;
          if (mySettings.debuglevel > 0) { 
            snprintf_P(tmpStr, sizeof(tmpStr), PSTR("MLX: read error, %u"), mlx_error_cnt); 
            R_printSerialTelnetLogln(tmpStr);          
          }
          if (mlx_error_cnt++ > 3) { // allow 3 retires on i2c bus until ERROR state of sensor is triggered
            stateMLX = HAS_ERROR;
            errorRecMLX = currentTime + 5000;
            mlx_error_cnt = 0;
            break;
          }
        }
      }
      break;
    }

    case IS_SLEEPING : { //---------------------
      if ((currentTime - lastMLX) > sleepTimeMLX) {
        D_printSerialTelnet(F("D:U:MLX:IS.."));
        switchI2C(mlx_port, mlx_i2c[0], mlx_i2c[1], mlx_i2cspeed, mlx_i2cClockStretchLimit);
        therm.wake(); // takes 250ms to wake up, sleepTime is shorter than intervalMLX to accomodate that
        if (mySettings.debuglevel == 8) { R_printSerialTelnetLogln(F("MLX: initiated wake up")); }
        stateMLX = IS_MEASURING;
      }
      break;
    }
    
    case HAS_ERROR : { // ----------------------
      if (currentTime > errorRecMLX) {
        D_printSerialTelnet(F("D:U:MLX:E.."));
        if (therm_error_cnt++ > ERROR_COUNT) { 
          success = false; 
          therm_avail = false;
          availNewData = true;
          if (mySettings.debuglevel > 0) { R_printSerialTelnetLogln(F("MLX: reinitialization attempts exceeded, MLX: no longer available")); }
          break; 
        } // give up after ERROR_COUNT tries
        
        mlx_lastError = currentTime;
        
        // trying to recover sensor
        if (initializeMLX()){
          if (mySettings.debuglevel > 0) { R_printSerialTelnetLogln(F("MLX: recovered")); }
        }
      }
      break;
    }

    default: {
      if (mySettings.debuglevel > 0) { R_printSerialTelnetLogln(F("MLX Error: invalid switch statement")); }
      break;
    }

  } // switch

  return success;
}

/******************************************************************************************************/
// JSON MLX
/******************************************************************************************************/

void mlxJSON(char *payLoad, size_t len){
  const char * str = "{ \"mlx\": ";
  size_t l = strlen(str);
  strlcpy(payLoad, str, l+1);
  mlxJSONMQTT(payLoad+l, len-l-1);
  strlcat(payLoad, "}", len);
}

void mlxJSONMQTT(char *payload, size_t len){
  char qualityMessage1[16];
  char qualityMessage2[16];
  if (therm_avail) { 
    checkFever(therm.object()+mlxOffset+fhDelta, qualityMessage1, 15);
    checkAmbientTemperature(therm.ambient(), qualityMessage2, 15);
  } else {
    strcpy(qualityMessage1, "not available");
    strcpy(qualityMessage2, "not available");
  }  
  snprintf_P(payload, len, PSTR("{ \"avail\": %s, \"To\": %5.1f, \"Ta\": %5.2f, \"fever\": \"%s\", \"T_airquality\": \"%s\" }"), 
                       therm_avail ? "true" : "false", 
                       therm_avail ? therm.object()+mlxOffset : -999., 
                       therm_avail ? therm.ambient() : -999., 
                       qualityMessage1, 
                       qualityMessage2);
}
