/******************************************************************************************************/
// SCD30, CO2, Humidity, Temperature
/******************************************************************************************************/
//float scd30_temp
//float scd30_hum
//float scd30_ah
// uint16_t getCO2(void);
// float getHumidity(void);
// float getTemperature(void);
// float getTemperatureOffset(void);
// uint16_t getAltitudeCompensation(void);
  
#include "VSC.h"
#ifdef EDITVSC
#include "src/SCD30.h"
#include "src/BME680.h"
#include "src/BME280.h"
#include "src/Config.h"
#include "src/Sensi.h"
#include "src/Quality.h"
#endif

void ICACHE_RAM_ATTR handleSCD30Interrupt() {              // Interrupt service routine when data ready is signaled
  stateSCD30 = DATA_AVAILABLE;                             // advance the sensor state
  if (mySettings.debuglevel == 4) {                        // for debugging, usually no Serial.print in ISR
    sprintf_P(tmpStr, PSTR("SCD30: interrupt occured\r\n")); printSerialTelnet(tmpStr);  
  }
}

/******************************************************************************************************/
// Initialize SCD30
/******************************************************************************************************/
bool initializeSCD30() {
  bool success = true;
  
  if (fastMode) { intervalSCD30 = intervalSCD30Fast; }      // set fast or slow mode
  else          { intervalSCD30 = intervalSCD30Slow; }
  if (mySettings.debuglevel >0) { sprintf_P(tmpStr, PSTR("SCD30: Interval: %lu\r\n"), intervalSCD30); printSerialTelnet(tmpStr); }

  if (mySettings.debuglevel > 0) { printSerialTelnet(F("SCD30: configuring interrupt\r\n")); }
  pinMode(SCD30interruptPin , INPUT);                      // interrupt scd30
  attachInterrupt(digitalPinToInterrupt(SCD30interruptPin),  handleSCD30Interrupt,  RISING);
  scd30_port->begin(scd30_i2c[0], scd30_i2c[1]);
  if (scd30.begin(*scd30_port, true)) {                    // start with autocalibration
    scd30.setMeasurementInterval(uint16_t(intervalSCD30/1000));
    scd30.setAutoSelfCalibration(true); 
    if (mySettings.tempOffset_SCD30_valid == 0xF0) { scd30.setTemperatureOffset(mySettings.tempOffset_SCD30); }
    mySettings.tempOffset_SCD30 = scd30.getTemperatureOffset();
    if (mySettings.debuglevel > 0) { sprintf_P(tmpStr, PSTR("SCD30: current temp offset: %fC\r\n"),mySettings.tempOffset_SCD30); printSerialTelnet(tmpStr); }
    stateSCD30 = IS_BUSY;
  } else {
    scd30_avail = false;
    stateSCD30 = HAS_ERROR;
    success = false;
  }
  if (mySettings.debuglevel > 0) { printSerialTelnet(F("SCD30: initialized\r\n")); }
  delay(50);
  return success;
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

  bool success = true;
  
  switch(stateSCD30) {
    
    case IS_MEASURING : { // used when RDY pin interrupt is not enabled
      if ((currentTime - lastSCD30) >= intervalSCD30) {
        scd30_port->begin(scd30_i2c[0], scd30_i2c[1]);  
        if (scd30.dataAvailable()) {
          scd30_ppm  = scd30.getCO2(); 
          scd30_temp = scd30.getTemperature();
          scd30_hum  = scd30.getHumidity();
          lastSCD30  = currentTime;
          if (mySettings.debuglevel == 4) { printSerialTelnet(F("SCD30: data read\r\n")); }
          scd30NewData = true;
          scd30NewDataWS = true;
        } else {
          if (mySettings.debuglevel == 4) { printSerialTelnet(F("SCD30: data not yet available\r\n")); }
        }
      }
      break;
    } // is measuring
    
    case IS_BUSY: { // used to bootup sensor when RDY pin interrupt is enabled
      if ((currentTime - lastSCD30Busy) > intervalSCD30Busy) {
        scd30_port->begin(scd30_i2c[0], scd30_i2c[1]);  
        if (scd30.dataAvailable()) {scd30.readMeasurement();} // without reading data, RDY pin will remain high and no interrupt will occur
        lastSCD30Busy = currentTime;
        if (mySettings.debuglevel == 4) { printSerialTelnet(F("SCD30: is busy\r\n")); }
      }
      break;
    }
    
    case DATA_AVAILABLE : { // used to obtain data when RDY pin interrupt is used
      scd30_port->begin(scd30_i2c[0], scd30_i2c[1]);  
      scd30_ppm  = scd30.getCO2();
      scd30_temp = scd30.getTemperature();
      scd30_hum  = scd30.getHumidity();
      lastSCD30  = currentTime;
      scd30NewData = true;
      scd30NewDataWS = true;
      stateSCD30 = IS_IDLE; 
      if (mySettings.debuglevel == 4) { printSerialTelnet(F("SCD30: data read\r\n")); }
      break;
    }
    
    case IS_IDLE : { // used when RDY pin is used with ISR

      // wait for intrrupt to occur, 
      // backup: if interrupt timed out, check the sensor
      if ( (currentTime - lastSCD30) > (2*intervalSCD30) ) {
        if (mySettings.debuglevel == 4) { printSerialTelnet(F("SCD30: interrupt timeout occured\r\n")); }
        scd30_port->begin(scd30_i2c[0], scd30_i2c[1]);  
        if (scd30.dataAvailable()) { 
          stateSCD30 = DATA_AVAILABLE; 
        } else {
          stateSCD30 = HAS_ERROR;
          success= false;
          if (mySettings.debuglevel > 0) { printSerialTelnet(F("SCD30: could not recover from interrupt timeout\r\n")); }
          break;
        }
      }

      // update pressure if available
      if (bme680_avail && mySettings.useBME680) { // update pressure settings
        if ((currentTime - lastPressureSCD30) >= intervalPressureSCD30) {
          scd30_port->begin(scd30_i2c[0], scd30_i2c[1]);  
          scd30.setAmbientPressure(uint16_t(bme680.pressure/100.0));  // update with value from pressure sensor, needs to be mbar
          lastPressureSCD30 = currentTime;
          if (mySettings.debuglevel == 4) { sprintf_P(tmpStr, PSTR("SCD30: pressure updated to %fmbar\r\n"), bme680.pressure/100.0); printSerialTelnet(tmpStr); }
        }
      } else if ((bme280_avail && mySettings.useBME280)) {
        if ((currentTime - lastPressureSCD30) >= intervalPressureSCD30) {
          scd30_port->begin(scd30_i2c[0], scd30_i2c[1]);  
          scd30.setAmbientPressure(uint16_t(bme280_pressure/100.0));  // pressure is in Pa and scd30 needs mBar
          lastPressureSCD30 = currentTime;
          if (mySettings.debuglevel == 4) { sprintf_P(tmpStr, PSTR("SCD30: pressure updated to %fmbar\r\n"), bme280_pressure/100.0); printSerialTelnet(tmpStr); }
        }
      }
      break;        
    }
    
    case HAS_ERROR : {
      // trying to recover sensor
      scd30_port->begin(scd30_i2c[0], scd30_i2c[1]);
      if (scd30.begin(*scd30_port, true)) { 
        scd30.setMeasurementInterval(uint16_t(intervalSCD30/1000));
        scd30.setAutoSelfCalibration(true); 
        mySettings.tempOffset_SCD30 = scd30.getTemperatureOffset();
        stateSCD30 = IS_BUSY;
      } else {
        scd30_avail = false;
        stateSCD30 = HAS_ERROR;
        success = false;
        if (mySettings.debuglevel > 0) { printSerialTelnet(F("SCD30: re-initialization failed\r\n")); }
        break;
      }
      if (mySettings.debuglevel > 0) { printSerialTelnet(F("SCD30: re-initialized\r\n")); }
      break;
    }

    default: {if (mySettings.debuglevel > 0) { printSerialTelnet(F("SCD30 Error: invalid switch statementh")); break;}}
    
  } // switch state

  return success;
}

void scd30JSON(char *payload){
  char qualityMessage1[16];
  char qualityMessage2[16];
  char qualityMessage3[16];
  checkCO2(float(scd30_ppm), qualityMessage1, 15); 
  checkHumidity(scd30_hum, qualityMessage2, 15);
  checkAmbientTemperature(scd30_temp, qualityMessage3, 15);
  sprintf_P(payload, PSTR("{\"scd30\":{\"avail\":%s,\"CO2\":%hu,\"rH\":%4.1f,\"aH\":%4.1f,\"T\":%4.1f,\"CO2_airquality\":\"%s\",\"rH_airquality\":\"%s\",\"T_airquality\":\"%s\"}}"), 
                       scd30_avail ? "true" : "false", 
                       scd30_ppm, 
                       scd30_hum,
                       scd30_ah,
                       scd30_temp,
                       qualityMessage1, 
                       qualityMessage2,
                       qualityMessage3);
}