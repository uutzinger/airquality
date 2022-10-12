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
  
#include "src/SGP30.h"
#include "src/BME68x.h"
#include "src/Config.h"
#include "src/Sensi.h"
#include "src/Quality.h"
#include "src/Print.h"

bool          sgp30_avail  = false;                        // do we have this sensor
bool          sgp30NewData = false;                        // do we have new data
bool          sgp30NewDataWS = false;                      // do we have new data for websocket
bool          baslineSGP30_valid = false;
uint8_t       sgp30_i2c[2];                                // the pins for the i2c port, set during initialization
unsigned long lastSGP30;                                   // last time we obtained data
unsigned long lastSGP30Humidity;                           // last time we upated humidity
unsigned long lastSGP30Baseline;                           // last time we obtained baseline
unsigned long intervalSGP30 = 1000;                        // populated during setup
unsigned long warmupSGP30;                                 // populated during setup
unsigned long errorRecSGP30;
unsigned long startMeasurementSGP30;
unsigned long sgp30_lastError;

volatile      SGP30SensorStates stateSGP30 = SGP30_IS_IDLE; 
TwoWire      *sgp30_port = 0;                              // pointer to the i2c port, might be useful for other microcontrollers
uint8_t       sgp30_error_cnt = 0;
SGP30         sgp30;

// External Variables
extern Settings      mySettings;   // Config
extern bool          fastMode;     // Sensi
extern unsigned long currentTime;  // Sensi
extern char          tmpStr[256];  // Sensi
extern bool          bme68x_avail;
extern float         bme68x_ah;
extern bool          scd30_avail;
extern float         scd30_ah;


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

/******************************************************************************************************/
// Initialize SGP30
/******************************************************************************************************/

bool initializeSGP30() {
  
  if (fastMode) { intervalSGP30 = intervalSGP30Fast;}
  else          { intervalSGP30 = intervalSGP30Slow;}
  if (mySettings.debuglevel > 0) { 
    snprintf_P(tmpStr, sizeof(tmpStr), PSTR("SGP30: Interval: %lu"), intervalSGP30); 
    R_printSerialTelnetLogln(tmpStr); 
  }
  switchI2C(sgp30_port, sgp30_i2c[0], sgp30_i2c[1], sgp30_i2cspeed, sgp30_i2cClockStretchLimit);
  if (sgp30.begin(*sgp30_port) == false) {
    if (mySettings.debuglevel > 0) { printSerialTelnetLogln(F("SGP30: No SGP30 Detected. Check connections")); }
    stateSGP30 = SGP30_HAS_ERROR;
    errorRecSGP30 = currentTime + 5000;
    return(false);
  }
  
  // Initializes sensor for air quality readings
  sgp30.initAirQuality();
  if (mySettings.debuglevel > 0) { printSerialTelnetLogln(F("SGP30: measurements initialized")); }
  stateSGP30 = SGP30_IS_MEASURING;
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
  //   Time to read Baseline? 
  //   Measure Airquality Command (12ms)
  //   Wait until measurement interval exceeded
  //

  bool success = true;  // when ERROR recovery fails, success becomes false
  SGP30ERR sgp30Error;
  
  switch(stateSGP30) {
    
    case SGP30_IS_MEASURING : { //---------------------

      // if its time, start update humidity to improve eCO2
      if ((currentTime - lastSGP30Humidity) > intervalSGP30Humidity) {
        D_printSerialTelnet(F("D:U:SGP:H.."));
        if (bme68x_avail && mySettings.useBME68x) {
          switchI2C(sgp30_port, sgp30_i2c[0], sgp30_i2c[1], sgp30_i2cspeed, sgp30_i2cClockStretchLimit);
          // Humidity correction, 8.8 bit number
          // 0x0F80 = 15.5 g/m^3
          // 0x0001 = 1/256 g/m^3
          // 0xFFFF = 256 +256/256 g/m^3
          if (bme68x_ah > 0.) { sgp30.setHumidity(uint16_t(bme68x_ah * 256.0 + 0.5)); }
          if (mySettings.debuglevel >= 2) { R_printSerialTelnetLogln(F("SGP30: humidity updated for eCO2")); }
        } else if (scd30_avail && mySettings.useSCD30) {
            if (scd30_ah > 0.) { sgp30.setHumidity(uint16_t(scd30_ah * 256.0 + 0.5)); }
            if (mySettings.debuglevel >= 2) { R_printSerialTelnetLogln(F("SGP30: humidity updated for eCO2")); }
        }
        lastSGP30Humidity = currentTime;        
      } // end humidity update 

      // if its time, start baseline
      if ((currentTime - lastSGP30Baseline) > intervalSGP30Baseline) {
        D_printSerialTelnet(F("D:U:SGP:B.."));
        switchI2C(sgp30_port, sgp30_i2c[0], sgp30_i2c[1], sgp30_i2cspeed, sgp30_i2cClockStretchLimit);
        sgp30.startGetBaseline();
        stateSGP30 = SGP30_WAITING_FOR_BASELINE;
        lastSGP30 = millis();
        break; // need to go to wait dor base line
      }

      // if its time, start measurement
      if ((currentTime - lastSGP30) > intervalSGP30) { // -------- Start Measurement
        D_printSerialTelnet(F("D:U:SGP:IM.."));
        switchI2C(sgp30_port, sgp30_i2c[0], sgp30_i2c[1], sgp30_i2cspeed, sgp30_i2cClockStretchLimit);
        startMeasurementSGP30 = millis();
        sgp30.startAirQuality();
        stateSGP30 = SGP30_WAITING_FOR_MEASUREMENT;
        lastSGP30 = millis();
        break; // need to go to wait for measurement
      }
      
      break;
    }

    case SGP30_WAITING_FOR_MEASUREMENT : { //------------------
      // command was sent, wait until we know that reponse is available
      if ((currentTime - lastSGP30) > 12) { // it takes 12 ms for measurement to complete
        switchI2C(sgp30_port, sgp30_i2c[0], sgp30_i2c[1], sgp30_i2cspeed, sgp30_i2cClockStretchLimit);
        sgp30Error = sgp30.getAirQuality(); 
        if (sgp30Error != SGP30_SUCCESS) {
          if (mySettings.debuglevel > 0) { 
            snprintf_P(tmpStr, sizeof(tmpStr), PSTR("SGP30: error measuring eCO2 & tVOC: %s"), SGP30errorString(sgp30Error)); 
            R_printSerialTelnetLogln(tmpStr); 
          }
          stateSGP30 = SGP30_HAS_ERROR;
          errorRecSGP30 = currentTime + 5000;
          break; // breaks switch
        } else {
          if (mySettings.debuglevel >= 2) { 
            snprintf_P(tmpStr, sizeof(tmpStr), PSTR("SGP30: eCO2 & tVOC read in %ldms"), (millis()-startMeasurementSGP30)); 
            R_printSerialTelnetLogln(tmpStr); 
          }
          sgp30NewData = true;
          sgp30NewDataWS = true;
          sgp30_error_cnt = 0;
        }
        lastSGP30 = currentTime;
        stateSGP30 = SGP30_IS_MEASURING;
      }
      break;
    }

    case SGP30_WAITING_FOR_BASELINE : { //------------------
      if ((currentTime - lastSGP30) > 10) { // it takes 10 ms for baseline to become available
        switchI2C(sgp30_port, sgp30_i2c[0], sgp30_i2c[1], sgp30_i2cspeed, sgp30_i2cClockStretchLimit);
        sgp30Error = sgp30.finishGetBaseline();
        if (sgp30Error != SGP30_SUCCESS) {
          if (mySettings.debuglevel > 0) { 
            snprintf_P(tmpStr, sizeof(tmpStr), PSTR("SGP30: error obtaining baseline: %s"), SGP30errorString(sgp30Error)); 
            R_printSerialTelnetLogln(tmpStr); 
          }
          stateSGP30 = SGP30_HAS_ERROR;
          errorRecSGP30 = currentTime + 5000;
          break;
        } else {
          if (mySettings.debuglevel == 6) { R_printSerialTelnetLogln(F("SGP30: obtained internal baseline")); }
        }
        lastSGP30Baseline = currentTime;
        stateSGP30 = SGP30_IS_MEASURING;
      }
      break;
    }
    
    case SGP30_HAS_ERROR : { //------------------
      if (currentTime > errorRecSGP30) {      
        D_printSerialTelnet(F("D:U:SGP:E.."));
        if (sgp30_error_cnt++ > ERROR_COUNT) { 
          success = false; 
          sgp30_avail = false;
          if (mySettings.debuglevel > 0) { R_printSerialTelnetLogln(F("SGP30: reinitialization attempts exceeded, SGP30: no longer available")); }
          break; 
        } // give up after ERROR_COUNT tries

        sgp30_lastError = currentTime;

        // trying to recover sensor
        if (initializeSGP30()) {
          if (mySettings.debuglevel > 0) { R_printSerialTelnetLogln(F("SGP30: recovered")); }
        }
      }
      break;
    }

    default: {
      if (mySettings.debuglevel > 0) { 
        R_printSerialTelnetLogln(F("SGP30 Error: invalid switch statement"));
      } 
      break;
    }
    
  } // switch state

  return(success);
}

/******************************************************************************************************/
// JSON SGP30
/******************************************************************************************************/

void sgp30JSON(char *payLoad, size_t len){
  const char * str = "{ \"sgp30\": ";
  size_t l = strlen(str);
  strlcpy(payLoad, str, l+1);
  sgp30JSONMQTT(payLoad+l, len-l-1);
  strlcat(payLoad, "}", len);
}

void sgp30JSONMQTT(char *payload, size_t len){
  char qualityMessage1[16];
  char qualityMessage2[16];
  if (sgp30_avail) { 
    checkCO2(float(sgp30.CO2), qualityMessage1, 15); 
    checkTVOC(float(sgp30.TVOC), qualityMessage2, 15);
  } else {
    strcpy(qualityMessage1, "not available");
    strcpy(qualityMessage2, "not available");
  } 
  snprintf_P(payload, len, PSTR("{ \"avail\": %s, \"eCO2\": %hu, \"tVOC\": %hu, \"eCO2_airquality\": \"%s\", \"tVOC_airquality\": \"%s\"}"), 
                       sgp30_avail ? "true" : "false", 
                       sgp30_avail ? sgp30.CO2 : 0, 
                       sgp30_avail ? sgp30.TVOC : 0,
                       qualityMessage1, 
                       qualityMessage2);
}
