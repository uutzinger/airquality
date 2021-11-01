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
    sprintf_P(tmpStr, PSTR("SCD30: interrupt occured")); R_printSerialTelnetLogln(tmpStr);  
  }
}

/******************************************************************************************************/
// Initialize SCD30
/******************************************************************************************************/
bool initializeSCD30() {
    
  if (fastMode) { intervalSCD30 = intervalSCD30Fast; }      // set fast or slow mode
  else          { intervalSCD30 = intervalSCD30Slow; }
  if (mySettings.debuglevel > 0) { sprintf_P(tmpStr, PSTR("SCD30: Interval: %lu"), intervalSCD30); R_printSerialTelnetLogln(tmpStr); }

  if (mySettings.debuglevel > 0) { printSerialTelnetLogln(F("SCD30: configuring interrupt")); }
  pinMode(SCD30interruptPin , INPUT);                      // interrupt scd30
  attachInterrupt(digitalPinToInterrupt(SCD30interruptPin),  handleSCD30Interrupt,  RISING);
  scd30_port->begin(scd30_i2c[0], scd30_i2c[1]);
  scd30_port->setClock(I2C_SLOW);
  if (scd30.begin(*scd30_port, true)) {                    // start with autocalibration
    scd30.setMeasurementInterval(uint16_t(intervalSCD30/1000));
    scd30.setAutoSelfCalibration(true); 
    if (mySettings.tempOffset_SCD30_valid == 0xF0) { scd30.setTemperatureOffset(mySettings.tempOffset_SCD30); }
    mySettings.tempOffset_SCD30 = scd30.getTemperatureOffset();
    if (mySettings.debuglevel > 0) { sprintf_P(tmpStr, PSTR("SCD30: current temp offset: %fC"),mySettings.tempOffset_SCD30); printSerialTelnetLogln(tmpStr); }
    stateSCD30 = IS_BUSY;
  } else {
    stateSCD30 = HAS_ERROR;
    errorRecSCD30 = currentTime + 5000;
    return(false);
  }
  if (mySettings.debuglevel > 0) { printSerialTelnetLogln(F("SCD30: initialized")); }
  delay(50);
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
        scd30_port->begin(scd30_i2c[0], scd30_i2c[1]);  
        scd30_port->setClock(I2C_SLOW);
        if (scd30.dataAvailable()) {
          scd30_ppm  = scd30.getCO2(); 
          scd30_temp = scd30.getTemperature();
          scd30_hum  = scd30.getHumidity();
          lastSCD30  = currentTime;
          if (mySettings.debuglevel == 4) { R_printSerialTelnetLogln(F("SCD30: data read")); }
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
        scd30_port->begin(scd30_i2c[0], scd30_i2c[1]);  
        scd30_port->setClock(I2C_SLOW);
        if (scd30.dataAvailable()) {scd30.readMeasurement();} // without reading data, RDY pin will remain high and no interrupt will occur
        lastSCD30Busy = currentTime;
        if (mySettings.debuglevel == 4) { R_printSerialTelnetLogln(F("SCD30: is busy")); }
      }
      break;
    }
    
    case DATA_AVAILABLE : { // used to obtain data when RDY pin interrupt is used
      D_printSerialTelnet(F("D:U:SCD30:DA.."));
      scd30_port->begin(scd30_i2c[0], scd30_i2c[1]);  
      scd30_port->setClock(I2C_SLOW);
      scd30_ppm  = scd30.getCO2();
      scd30_temp = scd30.getTemperature();
      scd30_hum  = scd30.getHumidity();
      lastSCD30  = currentTime;
      scd30NewData = true;
      scd30NewDataWS = true;
      stateSCD30 = IS_IDLE; 
      if (mySettings.debuglevel == 4) { R_printSerialTelnetLogln(F("SCD30: data read")); }
      break;
    }
    
    case IS_IDLE : { // used when RDY pin is used with ISR

      D_printSerialTelnet(F("D:U:SCD30:I.."));
      // wait for intrrupt to occur, 
      // backup: if interrupt timed out, check the sensor
      if ( (currentTime - lastSCD30) > (2*intervalSCD30) ) {
        if (mySettings.debuglevel > 0) { R_printSerialTelnetLogln(F("SCD30: interrupt timeout occured")); }
        scd30_port->begin(scd30_i2c[0], scd30_i2c[1]);  
        scd30_port->setClock(I2C_SLOW);
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
      if (bme680_avail && mySettings.useBME680) { // update pressure settings
        if ((currentTime - lastPressureSCD30) >= intervalPressureSCD30) {
          scd30_port->begin(scd30_i2c[0], scd30_i2c[1]);  
          scd30_port->setClock(I2C_SLOW);
          scd30.setAmbientPressure(uint16_t(bme680.pressure/100.0));  // update with value from pressure sensor, needs to be mbar
          lastPressureSCD30 = currentTime;
          if (mySettings.debuglevel == 4) { sprintf_P(tmpStr, PSTR("SCD30: pressure updated to %fmbar"), bme680.pressure/100.0); R_printSerialTelnetLogln(tmpStr); }
       }
      } else if ((bme280_avail && mySettings.useBME280)) {
        if ((currentTime - lastPressureSCD30) >= intervalPressureSCD30) {
          scd30_port->begin(scd30_i2c[0], scd30_i2c[1]);  
          scd30_port->setClock(I2C_SLOW);
          scd30.setAmbientPressure(uint16_t(bme280_pressure/100.0));  // pressure is in Pa and scd30 needs mBar
          lastPressureSCD30 = currentTime;
          if (mySettings.debuglevel == 4) { sprintf_P(tmpStr, PSTR("SCD30: pressure updated to %fmbar"), bme280_pressure/100.0); R_printSerialTelnetLogln(tmpStr); }
        }
      }
      break;        
    }
    
    case HAS_ERROR : {
      if (currentTime > errorRecSCD30) {      
        D_printSerialTelnet(F("D:U:SCD30:E.."));
        if (scd30_error_cnt++ > 3) { 
          success = false; 
          scd30_avail = false; 
          break;
        } // give up after 3 tries
        // trying to recover sensor
        scd30_port->begin(scd30_i2c[0], scd30_i2c[1]);
        scd30_port->setClock(I2C_SLOW);
        if (scd30.begin(*scd30_port, true)) { 
          scd30.setMeasurementInterval(uint16_t(intervalSCD30/1000));
          scd30.setAutoSelfCalibration(true); 
          mySettings.tempOffset_SCD30 = scd30.getTemperatureOffset();
          stateSCD30 = IS_BUSY;
        } else {
          stateSCD30 = HAS_ERROR;
          errorRecSCD30 = currentTime + 5000;
          success = false;
          if (mySettings.debuglevel > 0) { R_printSerialTelnetLogln(F("SCD30: re-initialization failed")); }
          break;
        }
        if (mySettings.debuglevel > 0) { R_printSerialTelnetLogln(F("SCD30: re-initialized")); }
      }
      break;
    }

    default: {if (mySettings.debuglevel > 0) { R_printSerialTelnetLogln(F("SCD30 Error: invalid switch statement")); break;}}
    
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
