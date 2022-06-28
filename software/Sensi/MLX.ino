/******************************************************************************************************/
// MLX, ambient and object temperature
/******************************************************************************************************/
// float object(void);
// float ambient(void);
// float readEmissivity();
//
#include "VSC.h"
#ifdef EDITVSC
#include "src/MLX.h"
#include "src/Sensi.h"
#include "src/Config.h"
#endif

bool initializeMLX(){
  
  mlx_port->begin(mlx_i2c[0], mlx_i2c[1]);  
  mlx_port->setClock(I2C_REGULAR);
  yieldI2C();
  
  if (therm.begin(0x5A, *mlx_port) == true) { 
    therm.setUnit(TEMP_C); // Set the library's units to Centigrade
    therm.setEmissivity(emissivity);
    if (mySettings.debuglevel > 0) { sprintf_P(tmpStr, PSTR("MLX: Emissivity: %f"), therm.readEmissivity()); R_printSerialTelnetLogln(tmpStr); }
    stateMLX = IS_MEASURING;      
  } else {
    if (mySettings.debuglevel > 0) { R_printSerialTelnetLogln(F("MLX: sensor not detected. Please check wiring")); }
    stateMLX = HAS_ERROR;
    errorRecMLX = currentTime + 5000;
    return(false);
  }
  sleepTimeMLX = intervalMLX - timeToStableMLX - 50;
  if (mySettings.debuglevel > 0) {
    sprintf_P(tmpStr, PSTR("MLX: sleep time is %lums"),   sleepTimeMLX); printSerialTelnetLogln(tmpStr);
    sprintf_P(tmpStr, PSTR("MLX: interval time is %lums"), intervalMLX); printSerialTelnetLogln(tmpStr); 
  }

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
        mlx_port->begin(mlx_i2c[0], mlx_i2c[1]);  
        mlx_port->setClock(I2C_REGULAR);
        yieldI2C();
        if (therm.read()) {
          if (mySettings.debuglevel == 8) { R_printSerialTelnetLogln(F("MLX: temperature measured")); }
          lastMLX = currentTime;
          // check if temperature is in valid range
          if ( (therm.object() < -273.15) || ( therm.ambient() < -273.15) ) { 
            if (mySettings.debuglevel > 0) { sprintf_P(tmpStr, PSTR("MLX: data range error, %u"), mlx_error_cnt); R_printSerialTelnetLogln(tmpStr); }
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
            if (mySettings.debuglevel == 8) { printSerialTelnetLogln(F("MLX: sent to sleep")); }
            stateMLX = IS_SLEEPING;
          }
        } else {
          // read error
          lastMLX = currentTime;
          if (mySettings.debuglevel > 0) { 
            sprintf_P(tmpStr, PSTR("MLX: read error, %u"), mlx_error_cnt); R_printSerialTelnetLogln(tmpStr);          
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
        mlx_port->begin(mlx_i2c[0], mlx_i2c[1]);  
        mlx_port->setClock(I2C_REGULAR);
        yieldI2C();
        therm.wake(); // takes 250ms to wake up, sleepTime is shorter than intervalMLX to accomodate that
        if (mySettings.debuglevel == 8) { R_printSerialTelnetLogln(F("MLX: initiated wake up")); }
        stateMLX = IS_MEASURING;
      }
      break;
    }
    
    case HAS_ERROR : { // ----------------------
      if (currentTime > errorRecMLX) {
        D_printSerialTelnet(F("D:U:MLX:E.."));
        if (therm_error_cnt++ > 3) { 
          success = false; 
          therm_avail = false;
          if (mySettings.debuglevel > 0) { R_printSerialTelnetLogln(F("MLX: reinitialization attempts exceeded, MLX: no longer available.")); }
          break; 
        } // give up after 3 tries
        // trying to recover sensor
        mlx_port->begin(mlx_i2c[0], mlx_i2c[1]);  
        mlx_port->setClock(I2C_REGULAR);
        yieldI2C();
        if (therm.begin(0x5A, *mlx_port) == true) { 
          therm.setUnit(TEMP_C); // Set the library's units to Centigrade
          therm.setEmissivity(emissivity); // hard coded from definitions file
          therm_error_cnt = 0;
          stateMLX = IS_MEASURING;
        } else { // could not recover
          stateMLX = HAS_ERROR;
          errorRecMLX = currentTime + 5000;
          // success = false;
          if (mySettings.debuglevel > 0) { R_printSerialTelnetLogln(F("MLX: re-initialization failed")); }
          break;
        }
        if (mySettings.debuglevel > 0) { R_printSerialTelnetLogln(F("MLX: re-initialized")); }
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

void mlxJSON(char *payload){
  char qualityMessage1[16];
  char qualityMessage2[16];
  if (therm_avail) { 
    checkFever(therm.object()+mlxOffset+fhDelta, qualityMessage1, 15);
    checkAmbientTemperature(therm.ambient(), qualityMessage2, 15);
  } else {
    strncpy(qualityMessage1, "not available", sizeof(qualityMessage1));
    strncpy(qualityMessage2, "not available", sizeof(qualityMessage2));
  }  
  sprintf_P(payload, PSTR("{ \"mlx\": { \"avail\": %s, \"To\": %5.1f, \"Ta\": %5.1f, \"fever\": \"%s\", \"T_airquality\": \"%s\"}}"), 
                       therm_avail ? "true" : "false", 
                       therm_avail ? therm.object()+mlxOffset : -999., 
                       therm_avail ? therm.ambient() : -999., 
                       qualityMessage1, 
                       qualityMessage2);
}
