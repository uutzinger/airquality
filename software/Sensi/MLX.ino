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
  delay(1); // might resolves -600C issues
  
  if (therm.begin(0x5A, *mlx_port) == true) { 
    therm.setUnit(TEMP_C); // Set the library's units to Centigrade
    therm.setEmissivity(emissivity);
    if (mySettings.debuglevel > 0) { sprintf_P(tmpStr, PSTR("MLX: Emissivity: %f\r\n"), therm.readEmissivity()); printSerialTelnet(tmpStr); }
    stateMLX = IS_MEASURING;      
  } else {
    if (mySettings.debuglevel > 0) { printSerialTelnet(F("MLX: sensor not detected. Please check wiring\r\n")); }
    stateMLX = HAS_ERROR;
    return(false);
  }
  sleepTimeMLX = intervalMLX - timeToStableMLX - 50;
  if (mySettings.debuglevel > 0) {
    sprintf_P(tmpStr, PSTR("MLX: sleep time is %lums\r\n"), sleepTimeMLX); printSerialTelnet(tmpStr);
    sprintf_P(tmpStr, PSTR("MLX: interval time is %lums\r\n"), intervalMLX); printSerialTelnet(tmpStr); 
  }

  if (mySettings.tempOffset_MLX_valid == 0xF0) {
    mlxOffset = mySettings.tempOffset_MLX;
    if (mySettings.debuglevel > 0) { printSerialTelnet(F("MLX: offset found\r\n")); }
  } else {
    if (mySettings.debuglevel > 0) { printSerialTelnet(F("MLX: no offset found\r\n")); }
  }

  if (mySettings.debuglevel > 0) { printSerialTelnet(F("MLX: initialized\r\n")); }
  delay(50);

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
        mlx_port->begin(mlx_i2c[0], mlx_i2c[1]);  
        mlx_port->setClock(I2C_REGULAR);
        delay(1); // might resolves -600C issues
        if (therm.read()) {
          if (mySettings.debuglevel == 8) { printSerialTelnet(F("MLX: temperature measured\r\n")); }
          lastMLX = currentTime;
          mlxNewData = true;
          mlxNewDataWS = true;
          if (fastMode == false) {
            therm.sleep();
            if (mySettings.debuglevel == 8) { printSerialTelnet(F("MLX: sent to sleep\r\n")); }
            stateMLX = IS_SLEEPING;
          }
        } else {
          // read error
          lastMLX = currentTime;
          if (mySettings.debuglevel > 0) { 
            sprintf_P(tmpStr, PSTR("MLX: read error, %u\r\n"), mlx_error_cnt); printSerialTelnet(tmpStr);          
          }
          if (mlx_error_cnt++ > 3) { // allow 3 retires
            stateMLX = HAS_ERROR;
            break;
          }
        }
        // check if temperature is in valid range
        if ( (therm.object() < -273.15) || ( therm.ambient() < -273.15) ) { 
          if (mySettings.debuglevel > 0) { sprintf_P(tmpStr, PSTR("MLX: data range error, %u\r\n"), mlx_error_cnt); printSerialTelnet(tmpStr); }
          if (mlx_error_cnt++ > 3) { // allow 3 retries
            stateMLX = HAS_ERROR;
          }
        } else { 
          mlx_error_cnt = 0; 
        }
      }
      break;
    }

    case HAS_ERROR : { // ----------------------
      // trying to recover sensor
      mlx_port->begin(mlx_i2c[0], mlx_i2c[1]);  
      mlx_port->setClock(I2C_REGULAR);
      delay(1);
      if (therm.begin(0x5A, *mlx_port) == true) { 
        therm.setUnit(TEMP_C); // Set the library's units to Centigrade
        therm.setEmissivity(emissivity); // hard coded from definitions file
        mlx_error_cnt = 0;
        stateMLX = IS_MEASURING;      
      } else { // could not recover
        stateMLX = HAS_ERROR;
        therm_avail = false;
        success = false;
        if (mySettings.debuglevel > 0) { printSerialTelnet(F("MLX: re-initialization failed\r\n")); }
        break;
      }
      if (mySettings.debuglevel > 0) { printSerialTelnet(F("MLX: re-initialized\r\n")); }
      break;
    }

    case IS_SLEEPING : { //---------------------
      if ((currentTime - lastMLX) > sleepTimeMLX) {
        mlx_port->begin(mlx_i2c[0], mlx_i2c[1]);  
        mlx_port->setClock(I2C_REGULAR);
        therm.wake(); // takes 250ms to wake up, sleepTime is shorter than intervalMLX to accomodate that
        if (mySettings.debuglevel == 8) { printSerialTelnet(F("MLX: initiated wake up\r\n")); }
        stateMLX = IS_MEASURING;
      }
      break;
    }

    default: {
      if (mySettings.debuglevel > 0) { printSerialTelnet(F("MLX Error:  invalid switch statement")); }
      break;
    }

  } // switch

  return success;
}

void mlxJSON(char *payload){
  char qualityMessage1[16];
  char qualityMessage2[16];
  checkFever(therm.object()+mlxOffset+fhDelta, qualityMessage1, 15);
  checkAmbientTemperature(therm.ambient(), qualityMessage2, 15);
  sprintf_P(payload, PSTR("{\"mlx\":{\"avail\":%s,\"To\":%5.1f,\"Ta\":%5.1f,\"fever\":\"%s\",\"T_airquality\":\"%s\"}}"), 
                       therm_avail ? "true" : "false", 
                       therm.object()+mlxOffset, 
                       therm.ambient(), 
                       qualityMessage1, 
                       qualityMessage2);
}
