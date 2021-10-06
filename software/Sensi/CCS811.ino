/******************************************************************************************************/
// CCS811, eCO2, tVOC
/******************************************************************************************************/
//  uint16_t tVOC
//  uint16_t CO2
//  uint16_t vrefCounts
//  uint16_t ntcCounts
//  float temperature
  
#include "VSC.h"
#ifdef EDITVSC
#include "src/CCS811.h"
#include "src/Sensi.h"
#include "src/Config.h"
#include "src/Quality.h"
#endif

/******************************************************************************************************/
// Interrupt Handler CCS811
/******************************************************************************************************/
void ICACHE_RAM_ATTR handleCCS811Interrupt() {             // interrupt service routine to handle data ready signal
    if (fastMode == true) { 
      stateCCS811 = DATA_AVAILABLE;                        // update the sensor state
    } else { 
      digitalWrite(CCS811_WAKE, LOW);                      // set CCS811 to wake up 
      stateCCS811 = IS_WAKINGUP;                           // update the sensor state
      lastCCS811Interrupt = millis();
    }
    if (mySettings.debuglevel == 10) { printSerialTelnet(F("CCS811: interrupt occured\r\n")); } // usually no serial in ISR
}

/******************************************************************************************************/
// INITIALIZE CCS811
/******************************************************************************************************/
bool initializeCCS811(){

  CCS811Core::CCS811_Status_e css811Ret;
  
  if (fastMode == true) {
    ccs811Mode = ccs811ModeFast;
    intervalCCS811Baseline = baselineCCS811Fast; 
    intervalCCS811Humidity = updateCCS811HumitityFast;
  } else {
    ccs811Mode = ccs811ModeSlow;
    intervalCCS811Baseline = baselineCCS811Slow; 
    intervalCCS811Humidity = updateCCS811HumititySlow; 
    if (mySettings.debuglevel > 0) { printSerialTelnet(F("CCS811: it will take about 5 minutes until readings are non-zero\r\n")); }
  }
  warmupCCS811 = currentTime + stablebaseCCS811;    

  // 4=0.25s, 3=60s, 2=10sec, 1=1sec.
  if ( ccs811Mode == 1 )      { intervalCCS811 =  1000; } 
  else if ( ccs811Mode == 2 ) { intervalCCS811 = 10000; } 
  else if ( ccs811Mode == 3 ) { intervalCCS811 = 60000; } 
  else                        { intervalCCS811 =   250; }
  if (mySettings.debuglevel >0) { sprintf_P(tmpStr, PSTR("CCS811: Interval: %lu\r\n"), intervalCCS811); printSerialTelnet(tmpStr); }

  pinMode(CCS811_WAKE, OUTPUT); // CCS811 not Wake Pin
  pinMode(CCS811interruptPin, INPUT_PULLUP); // CCS811 not Interrupt Pin
  attachInterrupt(digitalPinToInterrupt(CCS811interruptPin), handleCCS811Interrupt, FALLING);
  if (mySettings.debuglevel > 0) { printSerialTelnet(F("CCS811: interrupt configured\r\n")); }

  digitalWrite(CCS811_WAKE, LOW); // Set CCS811 to wake
  delay(1); // wakeup takes 50 microseconds
  if (mySettings.debuglevel > 0) { printSerialTelnet(F("CCS811: sensor waking up\r\n")); }
  
  ccs811_port->begin(ccs811_i2c[0], ccs811_i2c[1]);
  ccs811_port->setClock(I2C_FAST);
  
  css811Ret = ccs811.beginWithStatus(*ccs811_port); // has delays and wait loops
  if (css811Ret != CCS811Core::CCS811_Stat_SUCCESS) {
    if (mySettings.debuglevel > 0) { sprintf_P(tmpStr, PSTR("CCS811: error opening port - %s\r\n"), ccs811.statusString(css811Ret)); printSerialTelnet(tmpStr); }
    stateCCS811 = HAS_ERROR;
    return(false);
  }
  
  if (mySettings.debuglevel > 0) { sprintf_P(tmpStr, "CCS811: begin - %s\r\n", ccs811.statusString(css811Ret)); printSerialTelnet(tmpStr); }
  css811Ret = ccs811.setDriveMode(ccs811Mode);
  if (css811Ret != CCS811Core::CCS811_Stat_SUCCESS) {
    if (mySettings.debuglevel > 0) { sprintf_P(tmpStr, PSTR("CCS811: error setting drive mode - %s\r\n"), ccs811.statusString(css811Ret)); printSerialTelnet(tmpStr); }
    stateCCS811 = HAS_ERROR;
    return(false);
  }
  
  if (mySettings.debuglevel > 0) { sprintf_P(tmpStr, PSTR("CCS811: mode request set - %s\r\n"), ccs811.statusString(css811Ret)); printSerialTelnet(tmpStr); }
  css811Ret = ccs811.enableInterrupts(); // Configure and enable the interrupt line, then print error status
  if (css811Ret != CCS811Core::CCS811_Stat_SUCCESS) {
    if (mySettings.debuglevel > 0) { sprintf_P(tmpStr, PSTR("CCS811: error enable interrupts - %s\r\n"), ccs811.statusString(css811Ret)); printSerialTelnet(tmpStr); }
    stateCCS811 = HAS_ERROR;
    return(false);
  }
  
  if (mySettings.debuglevel > 0) { sprintf_P(tmpStr, PSTR("CCS811: interrupt configuation - %s\r\n"), ccs811.statusString(css811Ret)); printSerialTelnet(tmpStr); }
  if (mySettings.baselineCCS811_valid == 0xF0) {
    css811Ret = ccs811.setBaseline(mySettings.baselineCCS811);
    if (css811Ret == CCS811Core::CCS811_Stat_SUCCESS) { 
      if (mySettings.debuglevel > 0) { printSerialTelnet(F("CCS811: baseline programmed\r\n")); }
    } else {
      if (mySettings.debuglevel > 0) { sprintf_P(tmpStr, PSTR("CCS811: error writing baseline - %s\r\n"), ccs811.statusString(css811Ret)); printSerialTelnet(tmpStr); }
      stateCCS811 = HAS_ERROR;
      return(false);
    }
  }
  if (mySettings.debuglevel > 0) { printSerialTelnet(F("CCS811: initialized\r\n")); }
  stateCCS811 = IS_IDLE;
  delay(50);
  return(true);
}

/******************************************************************************************************/
// UPDATE CCS811
/******************************************************************************************************/
bool updateCCS811() {
  // Operation:
  //  Setup 
  //   turn on sensor and starts reading
  //  When measurement complete, interrupt is asserted and ISR is called
  //  if fastMode = false
  //    ISR wakes up i2c communication on wakeup pin
  //    Sensor is given 1ms to wake up
  //    Senor data is read and sensor logic is switched back to sleep mode
  //  if fastMode = true
  //    Sensor data is read
  //  Status is sleeping
  //    Waiting until data interrupt occurs

  bool success = true;  // when ERROR recovery fails, success becomes false
  CCS811Core::CCS811_Status_e css811Ret;
  
  switch(stateCCS811) {
    
    case IS_WAKINGUP : { // ISR will activate this when getting out of sleeping
      if ((currentTime - lastCCS811Interrupt) >= 1) { // wakeup time is 50micro seconds
        stateCCS811 = DATA_AVAILABLE;
        if (mySettings.debuglevel == 10) { printSerialTelnet(F("CCS811: wake up completed\r\n")); }
      }
      break;
    }
    
    case DATA_AVAILABLE : { // executed after sleeping or ideling
      tmpTime = millis();
      ccs811_port->begin(ccs811_i2c[0], ccs811_i2c[1]);
      ccs811_port->setClock(I2C_FAST);
      css811Ret = ccs811.readAlgorithmResults(); 
      if ( css811Ret != CCS811Core::CCS811_Stat_SUCCESS) { // Calling this function updates the global tVOC and CO2 variables
        stateCCS811 = HAS_ERROR;
      } else {
        if (mySettings.debuglevel == 10) { sprintf_P(tmpStr, PSTR("CCS811: readAlgorithmResults completed in %ldms\r\n"), (millis()-tmpTime)); printSerialTelnet(tmpStr); }
        ccs811NewData=true;
        ccs811NewDataWS=true;
        uint8_t error = ccs811.getErrorRegister();
        if (mySettings.debuglevel > 0) {
          if (error == 0xFF) { printSerialTelnet(F("CCS811: failed to read ERROR_ID register\r\n")); }
          else  {
            if (error & 1 << 5) { printSerialTelnet(F("CCS811: error HeaterSupply\r\n")); }
            if (error & 1 << 4) { printSerialTelnet(F("CCS811: error HeaterFault\r\n")); }
            if (error & 1 << 3) { printSerialTelnet(F("CCS811: error MaxResistance\r\n")); }
            if (error & 1 << 2) { printSerialTelnet(F("CCS811: error MeasModeInvalid\r\n")); }
            if (error & 1 << 1) { printSerialTelnet(F("CCS811: error ReadRegInvalid\r\n")); }
            if (error & 1 << 0) { printSerialTelnet(F("CCS811: error Msg Invalid\r\n")); }
          }
        }
        
        if ((currentTime - lastCCS811Baseline) >= intervalCCS811Baseline ) {
          tmpTime = millis();
          mySettings.baselineCCS811 = ccs811.getBaseline();
          lastCCS811Baseline = currentTime;
          if (mySettings.debuglevel == 10) { sprintf_P(tmpStr, PSTR("CCS811: baseline obtained in: %ldms\r\n"), (millis()-tmpTime)); printSerialTelnet(tmpStr); }
        }
        
        if ((currentTime - lastCCS811Humidity) > intervalCCS811Humidity ) {
          lastCCS811Humidity = currentTime;
          if (bme680_avail && mySettings.useBME680) {
            tmpTime = millis();
            ccs811.setEnvironmentalData(bme680.humidity, bme680.temperature);
            if (mySettings.debuglevel == 10) { sprintf_P(tmpStr, PSTR("CCS811: humidity and temperature compensation updated in %ldms\r\n"), (millis()-tmpTime)); printSerialTelnet(tmpStr); }
          }
        }
        
        if (fastMode == false) { 
          digitalWrite(CCS811_WAKE, HIGH); // put CCS811 to sleep
          if (mySettings.debuglevel == 10) { printSerialTelnet(F("CCS811: puttting sensor to sleep\r\n")); }
          stateCCS811 = IS_SLEEPING;
        } else {
          stateCCS811 = IS_IDLE;
        }

      }
      lastCCS811 = currentTime;
      break;
    }
    
    case IS_IDLE : { //---------------------
      // We want to continue ideling as we are waiting for interrupt      
      // However if interrupt timed out, we obtain data manually
      if ( (currentTime - lastCCS811) > 2*intervalCCS811 ) { 
        if (mySettings.debuglevel > 0) { printSerialTelnet(F("CCS811: interrupt timeout occured\r\n")); }
        ccs811_port->begin(ccs811_i2c[0], ccs811_i2c[1]);  
        ccs811_port->setClock(I2C_FAST);
        if ( ccs811.dataAvailable() ) {
          if (fastMode == true) { 
            stateCCS811 = DATA_AVAILABLE;                        // update the sensor state
          } else { 
            digitalWrite(CCS811_WAKE, LOW);                      // set CCS811 to wake up 
            stateCCS811 = IS_WAKINGUP;                           // update the sensor state
            lastCCS811Interrupt = millis();
          }
        } else {
          stateCCS811 = HAS_ERROR;
          if (mySettings.debuglevel > 0) { printSerialTelnet(F("CCS811: could not recover from interrupt timeout\r\n")); }
          break;
        }
      }
      
      if (mySettings.debuglevel == 10) { printSerialTelnet(F("CCS811: is idle\r\n")); }
      // if we dont use ISR method use this:
      //if (digitalRead(CCS811_INT) == 0) {
      //  stateCCS811 = DATA_AVAILABLE;          // update the sensor state
      //  if (mySettings.debuglevel == 10) {Serial.println(F("CCS811: interrupt occured")); }
      //} // end if not use ISR
      break;
    }
    
    case IS_SLEEPING : { //---------------------
      // Continue Sleeping, we are waiting for interrupt
      if (mySettings.debuglevel == 10) { printSerialTelnet(F("CCS811: is sleeping\r\n")); }
      // if we dont want to use ISR method use this:
      //if (digitalRead(CCS811_INT) == 0) {
      //  digitalWrite(CCS811_WAKE, LOW);        // set CCS811 to wake up 
      //  stateCCS811 = IS_WAKINGUP;             // update the sensor state
      //  lastCCS811Interrupt = millis();
      //  if (mySettings.debuglevel == 10) {Serial.println(F("CCS811: interrupt occured")); }
      //} // end if not use ISR
      break;
    }

    case HAS_ERROR : { //-----------------------
      // trying to recover sensor, reinitialize it
      digitalWrite(CCS811_WAKE, LOW); // Set CCS811 to wake
      delay(1); // wakeup takes 50 microseconds      
      ccs811_port->begin(ccs811_i2c[0], ccs811_i2c[1]);  
      ccs811_port->setClock(I2C_FAST);
      css811Ret = ccs811.beginWithStatus(*ccs811_port); // has delays and wait loops
      if (css811Ret != CCS811Core::CCS811_Stat_SUCCESS) { 
        ccs811_avail = false; 
        success = false; 
        if (mySettings.debuglevel > 0) { printSerialTelnet(F("CCS811: re-initialization failed\r\n")); }
        break;
      }
      css811Ret = ccs811.setDriveMode(ccs811Mode);
      if (css811Ret != CCS811Core::CCS811_Stat_SUCCESS) { 
        ccs811_avail = false; 
        success = false; 
        if (mySettings.debuglevel > 0) { printSerialTelnet(F("CCS811: re-initialization failed\r\n")); }
        break;
      }
      css811Ret = ccs811.enableInterrupts(); // Configure and enable the interrupt line, then print error status
      if (css811Ret != CCS811Core::CCS811_Stat_SUCCESS) { 
        ccs811_avail = false; 
        success = false; 
        if (mySettings.debuglevel > 0) { printSerialTelnet(F("CCS811: re-initialization failed\r\n")); }
        break;
      }
      if (mySettings.baselineCCS811_valid == 0xF0) {
        css811Ret = ccs811.setBaseline(mySettings.baselineCCS811);
        if (css811Ret != CCS811Core::CCS811_Stat_SUCCESS) { 
          ccs811_avail = false; 
          success = false; 
          if (mySettings.debuglevel > 0) { printSerialTelnet(F("CCS811: re-initialization failed\r\n")); }
          break;
        }
      }
      if (mySettings.debuglevel > 0) {  printSerialTelnet(F("CCS811: re-initialized\r\n")); }
      stateCCS811 = IS_IDLE;
      break;
    }

    default: {if (mySettings.debuglevel > 0) { printSerialTelnet(F("CCS811 Error:  invalid switch statement")); break;}}
    
  } // end switch
  
  return success;
}

void ccs811JSON(char *payload){
  char qualityMessage1[16];
  char qualityMessage2[16];
  checkCO2(float(ccs811.getCO2()), qualityMessage1, 15);
  checkTVOC(float(ccs811.getTVOC()), qualityMessage2, 15);
  sprintf_P(payload, PSTR("{\"ccs811\":{\"avail\":%s,\"eCO2\":%hu,\"tVOC\":%hu,\"eCO2_airquality\":\"%s\",\"tVOC_airquality\":\"%s\"}}"), 
                       ccs811_avail ? "true" : "false", 
                       ccs811.getCO2(), 
                       ccs811.getTVOC(), 
                       qualityMessage1, 
                       qualityMessage2);
}
