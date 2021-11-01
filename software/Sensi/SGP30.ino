/******************************************************************************************************/
// SGP30, eCO2, tVOC
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
  
  if (fastMode) { intervalSGP30 = intervalSGP30Fast;}
  else          { intervalSGP30 = intervalSGP30Slow;}
  if (mySettings.debuglevel > 0) { sprintf_P(tmpStr, PSTR("SGP30: Interval: %lu"), intervalSGP30); R_printSerialTelnetLogln(tmpStr); }
  sgp30_port->begin(sgp30_i2c[0], sgp30_i2c[1]);
  sps30_port->setClock(I2C_FAST);
  if (sgp30.begin(*sgp30_port) == false) {
    if (mySettings.debuglevel > 0) { printSerialTelnetLogln(F("SGP30: No SGP30 Detected. Check connections")); }
    stateSGP30 = HAS_ERROR;
    errorRecSGP30 = currentTime + 5000;
    return(false);
  }
  
  //Initializes sensor for air quality readings
  sgp30.initAirQuality();
  if (mySettings.debuglevel > 0) { printSerialTelnetLogln(F("SGP30: measurements initialized")); }
  stateSGP30 = IS_MEASURING;
  if (mySettings.baselineSGP30_valid == 0xF0) {
    sgp30.setBaseline((uint16_t)mySettings.baselineeCO2_SGP30, (uint16_t)mySettings.baselinetVOC_SGP30);
    if (mySettings.debuglevel > 0) { printSerialTelnetLogln(F("SGP30: found valid baseline")); }
    warmupSGP30 = millis() + warmupSGP30_withbaseline;
  } else {
    warmupSGP30 = millis() + warmupSGP30_withoutbaseline;
  }
  
  if (mySettings.debuglevel > 0) { printSerialTelnetLogln(F("SGP30: initialized")); }
  delay(50);
  return(true);
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
  //   Read Baseline (10ms) periodically from sensors dynamic baseline calculations
  //   Measure Airquality Command (12ms)
  //   Wait until measurement interval exceeded
  //

  bool success = true;  // when ERROR recovery fails, success becomes false
  SGP30ERR sgp30Error;
  
  switch(stateSGP30) {
    
    case IS_MEASURING : { //---------------------
      if ((currentTime - lastSGP30Humidity) > intervalSGP30Humidity) {
        D_printSerialTelnet(F("D:U:SGP:H.."));
        if (bme680_avail && mySettings.useBME680) {
          sgp30_port->begin(sgp30_i2c[0], sgp30_i2c[1]);
          sps30_port->setClock(I2C_FAST);
          // Humidity correction, 8.8 bit number
          // 0x0F80 = 15.5 g/m^3
          // 0x0001 = 1/256 g/m^3
          // 0xFFFF = 256 +256/256 g/m^3
          if (bme680_ah > 0.) { sgp30.setHumidity(uint16_t(bme680_ah * 256.0 + 0.5)); }
          if (mySettings.debuglevel == 6) { R_printSerialTelnetLogln(F("SGP30: humidity updated for eCO2")); }
        }
        lastSGP30Humidity = currentTime;        
      } // end humidity update 

      if ((currentTime - lastSGP30Baseline) > intervalSGP30Baseline) {
        D_printSerialTelnet(F("D:U:SGP:B.."));
        sgp30_port->begin(sgp30_i2c[0], sgp30_i2c[1]);  
        sps30_port->setClock(I2C_FAST);
        sgp30Error = sgp30.getBaseline(); // this has 10ms delay
        if (sgp30Error != SGP30_SUCCESS) {
           if (mySettings.debuglevel > 0) { sprintf_P(tmpStr, PSTR("SGP30: error obtaining baseline: %s"), SGP30errorString(sgp30Error)); R_printSerialTelnetLogln(tmpStr); }
           stateSGP30 = HAS_ERROR;
           errorRecSGP30 = currentTime + 5000;
           break;
        } else {
          if (mySettings.debuglevel == 6) { R_printSerialTelnetLogln(F("SGP30: obtaining internal baseline")); }
          lastSGP30Baseline= millis();
        }
      }

      if ((currentTime - lastSGP30) > intervalSGP30) {
        D_printSerialTelnet(F("D:U:SGP:IM.."));
        sgp30_port->begin(sgp30_i2c[0], sgp30_i2c[1]);  
        sps30_port->setClock(I2C_FAST);
        sgp30Error = sgp30.measureAirQuality();
        if (sgp30Error != SGP30_SUCCESS) {
          if (mySettings.debuglevel > 0) { sprintf_P(tmpStr, PSTR("SGP30: error measuring eCO2 & tVOC: %s"), SGP30errorString(sgp30Error)); R_printSerialTelnetLogln(tmpStr); }
          stateSGP30 = HAS_ERROR;
          errorRecSGP30 = currentTime + 5000;
          break;
        } else {
          if (mySettings.debuglevel == 6) { R_printSerialTelnetLogln(F("SGP30: eCO2 & tVOC measured"));  }
          sgp30NewData = true;
          sgp30NewDataWS = true;
          sgp30_error_cnt = 0;
        }
        lastSGP30 = currentTime;
      }
      break;
    }
        
    case HAS_ERROR : { //------------------
      if (currentTime > errorRecSGP30) {      
        D_printSerialTelnet(F("D:U:SGP:E.."));
        if (sgp30_error_cnt++ > 3) { 
          success = false; 
          sgp30_avail = false;
          break; 
        } // give up after 3 tries
        // trying to recover sensor
        sgp30_port->begin(sgp30_i2c[0], sgp30_i2c[1]);  
        sps30_port->setClock(I2C_FAST);
        if (sgp30.begin(*sgp30_port) == false) {
          stateSGP30 = HAS_ERROR;
          errorRecSGP30 = currentTime + 5000;
          success = false;
          if (mySettings.debuglevel > 0) { R_printSerialTelnetLogln(F("SGP30: re-initialization failed")); }
          break;
        }
        sgp30.initAirQuality();
        if (mySettings.debuglevel > 0) { R_printSerialTelnetLogln(F("SGP30: re-initialized")); }
        if (mySettings.baselineSGP30_valid == 0xF0) {
          sgp30.setBaseline((uint16_t)mySettings.baselineeCO2_SGP30, (uint16_t)mySettings.baselinetVOC_SGP30);
          warmupSGP30 = millis() + warmupSGP30_withbaseline;
        } else {
          warmupSGP30 = millis() + warmupSGP30_withoutbaseline;
        }
        stateSGP30 = IS_MEASURING;
      }
      break;
    }

    default: {if (mySettings.debuglevel > 0) { R_printSerialTelnetLogln(F("SGP30 Error: invalid switch statement")); break;}}
    
  } // switch state

  return(success);
}

void sgp30JSON(char *payload){
  char qualityMessage1[16];
  char qualityMessage2[16];
  checkCO2(float(sgp30.CO2), qualityMessage1, 15); 
  checkTVOC(float(sgp30.TVOC), qualityMessage2, 15);
  sprintf(payload, PSTR("{\"sgp30\":{\"avail\":%s,\"eCO2\":%hu,\"tVOC\":%hu,\"eCO2_airquality\":\"%s\",\"tVOC_airquality\":\"%s\"}}"), 
                       sgp30_avail ? "true" : "false", 
                       sgp30.CO2, 
                       sgp30.TVOC,
                       qualityMessage1, 
                       qualityMessage2);
}
