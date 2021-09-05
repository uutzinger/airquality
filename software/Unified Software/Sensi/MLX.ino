/******************************************************************************************************/
// Initialize MLX
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
  bool success = true;
  
  mlx_port->begin(mlx_i2c[0], mlx_i2c[1]);  
  
  if (therm.begin(0x5A, *mlx_port) == true) { 
    therm.setUnit(TEMP_C); // Set the library's units to Centigrade
    therm.setEmissivity((float)emissivity);
    if (mySettings.debuglevel > 0) { sprintf(tmpStr, "MLX: Emissivity: %f\r\n", therm.readEmissivity()); printSerialTelnet(tmpStr); }
    stateMLX = IS_MEASURING;      
  } else {
    if (mySettings.debuglevel > 0) { printSerialTelnet(F("MLX: sensor not detected. Please check wiring\r\n")); }
    stateMLX = HAS_ERROR;
    therm_avail = false;
  }
  sleepTimeMLX = intervalMLX - timeToStableMLX - 50;
  if (mySettings.debuglevel > 0) {
    sprintf(tmpStr, "MLX: sleep time is %lums\r\n", sleepTimeMLX); printSerialTelnet(tmpStr);
    sprintf(tmpStr, "MLX: interval time is %lums\r\n", intervalMLX); printSerialTelnet(tmpStr); 
  }

  if (mySettings.tempOffset_MLX_valid == 0xF0) {
    mlxOffset = mySettings.tempOffset_MLX;
    if (mySettings.debuglevel > 0) { printSerialTelnet(F("MLX: offset found\r\n")); }
  } else {
    if (mySettings.debuglevel > 0) { printSerialTelnet(F("MLX: no offset found\r\n")); }
  }

  if (mySettings.debuglevel > 0) { printSerialTelnet(F("MLX: initialized\r\n")); }
  delay(50);

  return success;
}

/******************************************************************************************************/
// Update MLX
/******************************************************************************************************/
bool updateMLX() {
  bool success = true;
  
  switch(stateMLX) {
    
    case IS_MEASURING : { //---------------------
      if ((currentTime - lastMLX) > intervalMLX) {
        mlx_port->begin(mlx_i2c[0], mlx_i2c[1]);  
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
          stateMLX = HAS_ERROR;
          therm_avail = false;
          success = false;
        }
      }
      break;
    }

    case IS_SLEEPING : { //---------------------
      if ((currentTime - lastMLX) > sleepTimeMLX) {
        mlx_port->begin(mlx_i2c[0], mlx_i2c[1]);  
        therm.wake(); // takes 250ms to wake up, sleepTime is shorter than intervalMLX to accomodate that
        if (mySettings.debuglevel == 8) { printSerialTelnet(F("MLX: initiated wake up\r\n")); }
        stateMLX = IS_MEASURING;
      }
      break;
    }

    case HAS_ERROR : { // ----------------------
      success = false;
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
  sprintf(payload, "{\"mlx\":{\"avail\":%s,\"To\":%5.1f,\"Ta\":%5.1f,\"fever\":\"%s\",\"T_airquality\":\"%s\"}}", 
                       therm_avail ? "true" : "false", 
                       therm.object()+mlxOffset, 
                       therm.ambient(), 
                       qualityMessage1, 
                       qualityMessage2);
}
