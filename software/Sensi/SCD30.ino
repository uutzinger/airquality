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
  
#include "VSC.h"
#ifdef EDITVSC
#include "src/SCD30.h"
#include "src/BME68x.h"
#include "src/BME280.h"
#include "src/Config.h"
#include "src/Sensi.h"
#include "src/Quality.h"
#endif

void ICACHE_RAM_ATTR handleSCD30Interrupt() {              // Interrupt service routine when data ready is signaled
  stateSCD30 = DATA_AVAILABLE;                             // advance the sensor state
  /* if (mySettings.debuglevel == 4) {                        // for debugging, usually no Serial.print in ISR
    snprintf_P(tmpStr, sizeof(tmpStr), PSTR("SCD30: interrupt occured")); 
    R_printSerialTelnetLogln(tmpStr);  
  } */
}

/******************************************************************************************************/
// Initialize SCD30
/******************************************************************************************************/
bool initializeSCD30() {
    
  if (fastMode) { intervalSCD30 = intervalSCD30Fast; }      // set fast or slow mode
  else          { intervalSCD30 = intervalSCD30Slow; }
  if (mySettings.debuglevel > 0) { 
    snprintf_P(tmpStr, sizeof(tmpStr), PSTR("SCD30: Interval: %lu"), intervalSCD30); 
    R_printSerialTelnetLogln(tmpStr); 
  }

  if (mySettings.debuglevel > 0) { printSerialTelnetLogln(F("SCD30: configuring interrupt")); }
  pinMode(SCD30interruptPin , INPUT);                      // interrupt scd30
  attachInterrupt(digitalPinToInterrupt(SCD30interruptPin),  handleSCD30Interrupt,  RISING);
  
  switchI2C(scd30_port, scd30_i2c[0], scd30_i2c[1], scd30_i2cspeed, scd30_i2cClockStretchLimit);
  if (scd30.begin(*scd30_port, true)) {                    // start with autocalibration
    scd30.setMeasurementInterval(uint16_t(intervalSCD30/1000));
    scd30.setAutoSelfCalibration(true); 
    if (mySettings.tempOffset_SCD30_valid == 0xF0) { scd30.setTemperatureOffset(mySettings.tempOffset_SCD30); }
    mySettings.tempOffset_SCD30 = scd30.getTemperatureOffset();
    if (mySettings.debuglevel > 0) { 
      snprintf_P(tmpStr, sizeof(tmpStr), PSTR("SCD30: current temp offset: %fC"),mySettings.tempOffset_SCD30); 
      printSerialTelnetLogln(tmpStr); 
    }
    stateSCD30 = IS_BUSY;
  } else {
    stateSCD30 = HAS_ERROR;
    errorRecSCD30 = currentTime + 5000;
    return(false);
  }
  if (mySettings.debuglevel > 0) { printSerialTelnetLogln(F("SCD30: initialized")); }
  delay(50); lastYield = millis();
  return(true);
}

/******************************************************************************************************/
// Update SCD30
/******************************************************************************************************/
bool updateSCD30 () {
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

  bool success = true;  // when ERROR recovery fails, success becomes false
  
  switch(stateSCD30) {
    
    case IS_MEASURING : { // used when RDY pin interrupt is not enabled
      if ((currentTime - lastSCD30) >= intervalSCD30) {
        D_printSerialTelnet(F("D:U:SCD30:IM.."));
        switchI2C(scd30_port, scd30_i2c[0], scd30_i2c[1], scd30_i2cspeed, scd30_i2cClockStretchLimit);
        startMeasurementSCD30 = millis();
        if (scd30.dataAvailable()) {
          scd30_ppm  = scd30.getCO2(); 
          scd30_temp = scd30.getTemperature();
          scd30_hum  = scd30.getHumidity();
          lastSCD30  = currentTime;
          if (mySettings.debuglevel >= 2) { 
            snprintf_P(tmpStr, sizeof(tmpStr), PSTR("SCD30: CO2, rH, T read in %ldms"), (millis()-startMeasurementSCD30)); 
            R_printSerialTelnetLogln(tmpStr); 
          }
          scd30NewData = true;
          scd30NewDataWS = true;
        } else {
          if (mySettings.debuglevel == 4) { R_printSerialTelnetLogln(F("SCD30: data not yet available")); }
        }
      }
      break;
    } // is measuring
    
    case IS_BUSY: { // used to bootup sensor when RDY pin interrupt is enabled
      if ((currentTime - lastSCD30Busy) > intervalSCD30Busy) {
        D_printSerialTelnet(F("D:U:SCD30:IB.."));
        switchI2C(scd30_port, scd30_i2c[0], scd30_i2c[1], scd30_i2cspeed, scd30_i2cClockStretchLimit);
        if (scd30.dataAvailable()) {
          scd30.readMeasurement();
        } // without reading data, RDY pin will remain high and no interrupt will occur
        lastSCD30Busy = currentTime;
        if (mySettings.debuglevel == 4) { R_printSerialTelnetLogln(F("SCD30: is busy")); }
      }
      break;
    }
    
    case DATA_AVAILABLE : { // used to obtain data when RDY pin interrupt is used
      D_printSerialTelnet(F("D:U:SCD30:DA.."));
      switchI2C(scd30_port, scd30_i2c[0], scd30_i2c[1], scd30_i2cspeed, scd30_i2cClockStretchLimit);
      startMeasurementSCD30 = millis();
      scd30_ppm  = scd30.getCO2();
      scd30_temp = scd30.getTemperature();
      scd30_hum  = scd30.getHumidity();
      float tmp = 273.15 + scd30_temp;
      scd30_ah = scd30_hum * 13.246 / tmp * exp(19.854 - 5423.0/tmp); // [gr/m^3]
      if ( (scd30_ah<0) | (scd30_ah>40.0) ) { scd30_ah = -1.0; } // make sure its reasonable
      lastSCD30  = currentTime;
      scd30NewData = true;
      scd30NewDataWS = true;
      stateSCD30 = IS_IDLE; 
      if (mySettings.debuglevel >= 2) { 
        snprintf_P(tmpStr, sizeof(tmpStr), PSTR("SCD30: CO2, rH, T read in %ldms"), (millis()-startMeasurementSCD30)); 
        R_printSerialTelnetLogln(tmpStr); 
      }
      break;
    }
    
    case IS_IDLE : { // used when RDY pin is used with ISR
      D_printSerialTelnet(F("D:U:SCD30:I.."));
      // wait for intrrupt to occur, 
      // if interrupt timed out, obtain data manually
      if ( (currentTime - lastSCD30) > (INTERVAL_TIMEOUT_FACTOR*intervalSCD30) ) {
        if (mySettings.debuglevel > 0) { R_printSerialTelnetLogln(F("SCD30: interrupt timeout occured")); }
        switchI2C(scd30_port, scd30_i2c[0], scd30_i2c[1], scd30_i2cspeed, scd30_i2cClockStretchLimit);
        if (scd30.dataAvailable()) { 
          stateSCD30 = DATA_AVAILABLE; 
          scd30_error_cnt = 0;
        } else {
          stateSCD30 = HAS_ERROR;
          errorRecSCD30 = currentTime + 5000;
          if (mySettings.debuglevel > 0) { R_printSerialTelnetLogln(F("SCD30: could not recover from interrupt timeout")); }
          break;
        }
      }

      // update pressure if available
      if (bme68x_avail && mySettings.useBME68x) { // update pressure settings
        if ((currentTime - lastPressureSCD30) >= intervalPressureSCD30) {
          switchI2C(bme68x_port, bme68x_i2c[0], bme68x_i2c[1], bme68x_i2cspeed, bme68x_i2cClockStretchLimit);
          scd30.setAmbientPressure(uint16_t(bme68x.pressure/100.0));  // update with value from pressure sensor, needs to be mbar
          lastPressureSCD30 = currentTime;
          if (mySettings.debuglevel >= 2) { 
            snprintf_P(tmpStr, sizeof(tmpStr), PSTR("SCD30: pressure updated to %fmbar"), bme68x.pressure/100.0);
            R_printSerialTelnetLogln(tmpStr); 
          }
       }
      } else if ((bme280_avail && mySettings.useBME280)) {
        if ((currentTime - lastPressureSCD30) >= intervalPressureSCD30) {
          switchI2C(bme280_port, bme280_i2c[0], bme280_i2c[1], bme280_i2cspeed, bme280_i2cClockStretchLimit);
          scd30.setAmbientPressure(uint16_t(bme280_pressure/100.0));  // pressure is in Pa and scd30 needs mBar
          lastPressureSCD30 = currentTime;
          if (mySettings.debuglevel >= 2) { 
            snprintf_P(tmpStr, sizeof(tmpStr), PSTR("SCD30: pressure updated to %fmbar"), bme280_pressure/100.0); 
            R_printSerialTelnetLogln(tmpStr); }
        }
      }
      break;        
    }
    
    case HAS_ERROR : {
      if (currentTime > errorRecSCD30) {      
        D_printSerialTelnet(F("D:U:SCD30:E.."));
        if (scd30_error_cnt++ > ERROR_COUNT) { 
          success = false; 
          scd30_avail = false; 
          if (mySettings.debuglevel > 0) { R_printSerialTelnetLogln(F("SCD30: reinitialization attempts exceeded, SCD30: no longer available.")); }
          break;
        } // give up after ERROR_COUNT tries

        scd30_lastError = currentTime;

        // trying to recover sensor
        if (initializeSCD30()) {
          if (mySettings.debuglevel > 0) { R_printSerialTelnetLogln(F("SCD30: recovered.")); }
        }
      }
      break;
    }

    default: {
      if (mySettings.debuglevel > 0) { 
        R_printSerialTelnetLogln(F("SCD30 Error: invalid switch statement"));
      } 
      break;
    }
    
  } // switch state

  return success;
}

void scd30JSON(char *payload, size_t len){
  char qualityMessage1[16];
  char qualityMessage2[16];
  char qualityMessage3[16];
  if (scd30_avail) { 
    checkCO2(float(scd30_ppm), qualityMessage1, 15); 
    checkHumidity(scd30_hum, qualityMessage2, 15);
    checkAmbientTemperature(scd30_temp, qualityMessage3, 15);
  } else {
    strcpy(qualityMessage1, "not available");
    strcpy(qualityMessage2, "not available");
    strcpy(qualityMessage3, "not available");
  } 
  snprintf_P(payload, len, PSTR("{ \"scd30\": { \"avail\": %s, \"CO2\": %hu, \"rH\": %4.1f, \"aH\": %4.1f, \"T\": %5.2f, \"CO2_airquality\": \"%s\", \"rH_airquality\": \"%s\", \"T_airquality\": \"%s\"}}"), 
                       scd30_avail ? "true" : "false", 
                       scd30_avail ? scd30_ppm : 0, 
                       scd30_avail ? scd30_hum : -1.0,
                       scd30_avail ? scd30_ah : -1.0,
                       scd30_avail ? scd30_temp : -999.0,
                       qualityMessage1, 
                       qualityMessage2,
                       qualityMessage3);
}

void scd30JSONMQTT(char *payload, size_t len){
  char qualityMessage1[16];
  char qualityMessage2[16];
  char qualityMessage3[16];
  if (scd30_avail) { 
    checkCO2(float(scd30_ppm), qualityMessage1, 15); 
    checkHumidity(scd30_hum, qualityMessage2, 15);
    checkAmbientTemperature(scd30_temp, qualityMessage3, 15);
  } else {
    strcpy(qualityMessage1, "not available");
    strcpy(qualityMessage2, "not available");
    strcpy(qualityMessage3, "not available");
  } 
  snprintf_P(payload, len, PSTR("{ \"avail\": %s, \"CO2\": %hu, \"rH\": %4.1f, \"aH\": %4.1f, \"T\": %5.2f, \"CO2_airquality\": \"%s\", \"rH_airquality\": \"%s\", \"T_airquality\": \"%s\"}"), 
                       scd30_avail ? "true" : "false", 
                       scd30_avail ? scd30_ppm : 0, 
                       scd30_avail ? scd30_hum : -1.0,
                       scd30_avail ? scd30_ah : -1.0,
                       scd30_avail ? scd30_temp : -999.0,
                       qualityMessage1, 
                       qualityMessage2,
                       qualityMessage3);
}
