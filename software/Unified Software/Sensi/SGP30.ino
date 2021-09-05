/******************************************************************************************************/
// Initialize SGP30
/******************************************************************************************************/
//  uint16_t CO2;
//  uint16_t TVOC;
//  uint16_t baselineCO2;
//  uint16_t baselineTVOC;
//  uint16_t featureSetVersion;
//  uint16_t H2;
//  uint16_t ethanol;
//  uint64_t serialID;
  
#include "VSC.h"
#ifdef EDITVSC
#include "src/SGP30.h"
#include "src/BME680.h"
#include "src/Config.h"
#include "src/Sensi.h"
#include "src/Quality.h"
#endif

const char *SGP30errorString(SGP30ERR sgp30Return) {
  SGP30ERR val;
  val = sgp30Return;
  switch (val) {
    case SGP30_SUCCESS:
      return "Ok";
      break;
    case SGP30_ERR_BAD_CRC:
      return "Bad CRC";
      break;
    case SGP30_ERR_I2C_TIMEOUT:
      return "I2C timeout";
      break;
    case SGP30_SELF_TEST_FAIL:
      return "Self test failed";
      break;
    default:
      return "Default";
      break;
  }
}

bool initializeSGP30() {
  bool success = true;
  
  if (fastMode) { intervalSGP30 = intervalSGP30Fast;}
  else          { intervalSGP30 = intervalSGP30Slow;}
  if (mySettings.debuglevel > 0) { sprintf(tmpStr, "SGP30: Interval: %lu\r\n", intervalSGP30); printSerialTelnet(tmpStr); }
  sgp30_port->begin(sgp30_i2c[0], sgp30_i2c[1]);
  if (sgp30.begin(*sgp30_port) == false) {
    if (mySettings.debuglevel > 0) { printSerialTelnet(F("SGP30: No SGP30 Detected. Check connections\r\n")); }
    sgp30_avail = false;
    stateSGP30 = HAS_ERROR;
    success = false;
  }
  
  //Initializes sensor for air quality readings
  sgp30.initAirQuality();
  if (mySettings.debuglevel > 0) { printSerialTelnet(F("SGP30: measurements initialzed\r\n")); }
  stateSGP30 = IS_MEASURING;
  if (mySettings.baselineSGP30_valid == 0xF0) {
    sgp30.setBaseline((uint16_t)mySettings.baselineeCO2_SGP30, (uint16_t)mySettings.baselinetVOC_SGP30);
    if (mySettings.debuglevel > 0) { printSerialTelnet(F("SGP30: found valid baseline\r\n")); }
    warmupSGP30 = millis() + warmupSGP30_withbaseline;
  } else {
    warmupSGP30 = millis() + warmupSGP30_withoutbaseline;
  }
  
  if (mySettings.debuglevel > 0) { printSerialTelnet(F("SGP30: initialized\r\n")); }
  delay(50);
  return success;
}

/******************************************************************************************************/
// Update SGP30
/******************************************************************************************************/
bool updateSGP30() {
  // Operation Sequence:
  //  Setup
  //   Power up, puts the sensor in sleep mode
  //   Init Command (10ms to complete)
  //   Read baseline from EEPROM
  //   If valid set baseline
  //  Update
  //   Humidity interval exceeded?
  //     write humidity from third party sensors to sensor (10ms to complete)
  //   Start Measure Airquality Command 
  //   Unresponsive for 12ms
  //   Initiate Get Airquality Command
  //   Wait until measurement interval exceeded
  //
  // Read Baseline (10ms) periodically from sensors dynamic baseline calculations
  // Set Humidity from third party sensor periodically to increase accuracy

  bool success = true;
  SGP30ERR sgp30Error;
  
  switch(stateSGP30) {
    
    case IS_MEASURING : { //---------------------
      if ((currentTime - lastSGP30Humidity) > intervalSGP30Humidity) {
        if (bme680_avail && mySettings.useBME680) {
          sgp30_port->begin(sgp30_i2c[0], sgp30_i2c[1]);  
          // Humidity correction, 8.8 bit number
          // 0x0F80 = 15.5 g/m^3
          // 0x0001 = 1/256 g/m^3
          // 0xFFFF = 256 +256/256 g/m^3
          sgp30.setHumidity(uint16_t(bme680_ah * 256.0 + 0.5)); 
          if (mySettings.debuglevel == 6) { printSerialTelnet(F("SGP30: humidity updated for eCO2\r\n")); }
        }
        lastSGP30Humidity = currentTime;        
      } // end humidity update 

      if ((currentTime - lastSGP30Baseline) > intervalSGP30Baseline) {
        sgp30_port->begin(sgp30_i2c[0], sgp30_i2c[1]);  
        sgp30Error = sgp30.startGetBaseline(); // this has 10ms delay
        if (sgp30Error != SGP30_SUCCESS) {
           if (mySettings.debuglevel > 0) { sprintf(tmpStr, "SGP30: error obtaining baseline: %s\r\n", SGP30errorString(sgp30Error)); printSerialTelnet(tmpStr); }
           stateSGP30 = HAS_ERROR;
           success = false;
           sgp30_avail = false;
        } else {
          if (mySettings.debuglevel == 6) { printSerialTelnet(F("SGP30: obtaining internal baseline\r\n")); }
          lastSGP30Baseline= millis();
          stateSGP30 = GET_BASELINE;
        }
        break;
      }

      if ((currentTime - lastSGP30) > intervalSGP30) {
        sgp30_port->begin(sgp30_i2c[0], sgp30_i2c[1]);  
        sgp30Error = sgp30.startAirQuality();
        if (sgp30Error != SGP30_SUCCESS) {
          if (mySettings.debuglevel > 0) { sprintf(tmpStr, "SGP30: error initiating airquality measurement: %s\r\n", SGP30errorString(sgp30Error)); printSerialTelnet(tmpStr); }
          stateSGP30 = HAS_ERROR;
          success = false;
          sgp30_avail = false;
        } else {
          if (mySettings.debuglevel == 6) { printSerialTelnet(F("SGP30: airquality measurement initiated\r\n")); }
          stateSGP30 = IS_BUSY;
        }
        lastSGP30 = currentTime;
      }
      break;
    }
    
    case IS_BUSY : { //---------------------
      if ((currentTime - lastSGP30) >= 20) {
        sgp30_port->begin(sgp30_i2c[0], sgp30_i2c[1]);  
        sgp30Error = sgp30.getAirQuality();
        if (sgp30Error != SGP30_SUCCESS) {
          if (mySettings.debuglevel > 0) { sprintf(tmpStr, "SGP30: error obtaining measurements: %s\r\n", SGP30errorString(sgp30Error)); printSerialTelnet(tmpStr); }
          stateSGP30 = HAS_ERROR;
          success = false;
          sgp30_avail = false;
        } else {
          if (mySettings.debuglevel == 6) { printSerialTelnet(F("SGP30: eCO2 & tVOC measured\r\n"));  }
          sgp30NewData = true;
          sgp30NewDataWS = true;
          stateSGP30 = IS_MEASURING;
        }
        lastSGP30 = currentTime;
      }
      break;
    }
    
    case GET_BASELINE : { //---------------------
      if ((currentTime - lastSGP30Baseline) > 10) {
        sgp30_port->begin(sgp30_i2c[0], sgp30_i2c[1]);  
        sgp30Error = sgp30.finishGetBaseline(); 
        if (sgp30Error == SGP30_SUCCESS) {
          if (mySettings.debuglevel == 6) { printSerialTelnet(F("SGP30: baseline obtained\r\n")); }
          stateSGP30 = IS_MEASURING; 
        } else { 
          if (mySettings.debuglevel > 0) { sprintf(tmpStr, "SGP30: error baseline: %s\r\n", SGP30errorString(sgp30Error)); printSerialTelnet(tmpStr); }
          stateSGP30 = HAS_ERROR; 
          success = false;
          sgp30_avail = false;
        }
      }
      break;
    }

    case HAS_ERROR : { //------------------
      if (mySettings.debuglevel > 0) { printSerialTelnet(F("SGP30: error\r\n")); }
      success = false;
      break;
    }
    
  } // switch state

  return success;
}

void sgp30JSON(char *payload){
  char qualityMessage1[16];
  char qualityMessage2[16];
  checkCO2(float(sgp30.CO2), qualityMessage1, 15); 
  checkTVOC(float(sgp30.TVOC), qualityMessage2, 15);
  sprintf(payload, "{\"sgp30\":{\"avail\":%s,\"eCO2\":%hu,\"tVOC\":%hu,\"eCO2_airquality\":\"%s\",\"tVOC_airquality\":\"%s\"}}", 
                       sgp30_avail ? "true" : "false", 
                       sgp30.CO2, 
                       sgp30.TVOC,
                       qualityMessage1, 
                       qualityMessage2);
}
