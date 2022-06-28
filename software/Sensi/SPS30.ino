/******************************************************************************************************/
// SPS30 PM 1.0 PM 2.5, PM 4, PM 10.0 averge Particle Size, [PM 2.5 and 10 used for airquality]
/******************************************************************************************************/
// float   MassPM1;        // Mass Concentration PM1.0 [μg/m3]
// float   MassPM2;        // Mass Concentration PM2.5 [μg/m3]
// float   MassPM4;        // Mass Concentration PM4.0 [μg/m3]
// float   MassPM10;       // Mass Concentration PM10 [μg/m3]
// float   NumPM0;         // Number Concentration PM0.5 [#/cm3]
// float   NumPM1;         // Number Concentration PM1.0 [#/cm3]
// float   NumPM2;         // Number Concentration PM2.5 [#/cm3]
// float   NumPM4;         // Number Concentration PM4.0 [#/cm3]
// float   NumPM10;        // Number Concentration PM10.0 [#/cm3]
// float   PartSize; 
    
#include "VSC.h"
#ifdef EDITVSC
#include "src/SPS30.h"
#include "src/Config.h"
#include "src/Sensi.h"
#include "src/Quality.h"
#endif

// Initialize
//   Start i2c port
//   Probe for sensor (obtain version information), if not successful software reset and probe again
//   Reset
//   Get Serialnumber
//   Get Productname
//   Get Version information, if CRC error set version to 1.0
//   Get Autoclean interval
//   Start, new data is created every 1 seconds
//   Check if enough memory available to buffer data

bool initializeSPS30() { 

  sps30.EnableDebugging(SPS30Debug);

  if (fastMode == true) { intervalSPS30 = intervalSPS30Fast; }
  else                  { intervalSPS30 = intervalSPS30Slow; }

  if (mySettings.debuglevel > 0) { sprintf_P(tmpStr, PSTR("SPS30: Interval: %lums"),intervalSPS30); R_printSerialTelnetLogln(tmpStr); }
  sps30_port->begin(sps30_i2c[0], sps30_i2c[1]);  
  sps30_port->setClock(I2C_SLOW);
  yieldI2C();

  if (sps30.begin(sps30_port) == false) {
    if (mySettings.debuglevel > 0) { printSerialTelnetLogln(F("SPS30: Sensor not detected in I2C. Please check wiring")); }
    stateSPS30 = HAS_ERROR;
    errorRecSPS30 = currentTime + 5000;
    return(false);
  }

  if (sps30.probe() == false) { 
    // attempt reset and retry, I dont know how to restart sps30 without physical access.
    // should wakeup first
    sps30.wakeup();
    delay(500); lastYield = millis(); // 5ms needed
    sps30.reset();
    delay(1000); lastYield = millis(); // takes 100ms to finish reset
    if (sps30.probe() == false) {
      if (mySettings.debuglevel > 0) {
        printSerialTelnetLogln(F("SPS30: could not probe / connect"));
        printSerialTelnetLogln(F("SPS30: powercycle the system and reopen serial console"));  
      }
      stateSPS30 = HAS_ERROR;
      errorRecSPS30 = currentTime + 5000;
      return(false);
    }
  } else { 
    if (mySettings.debuglevel > 0) { printSerialTelnetLogln(F("SPS30: detected")); }
  }

  if (sps30.reset() == false) { 
    if (mySettings.debuglevel > 0) { printSerialTelnetLogln(F("SPS30: could not reset")); }
    stateSPS30 = HAS_ERROR;
    errorRecSPS30 = currentTime + 5000;
    return(false);
  } else { delay(100); lastYield = millis(); }

  // read device info

  if (mySettings.debuglevel > 0) {
    printSerialTelnetLogln(F("SPS30: obtaining device information"));
    ret = sps30.GetSerialNumber(buf, 32);
    if (ret == ERR_OK) {
      printSerialTelnetLog("SPS30: serial number : ");
      if(strlen(buf) > 0) { printSerialTelnetLog(buf); printSerialTelnetLog(F("\r\n")); }
      else { printSerialTelnetLog(F("not available\r\n")); }
    } else { printSerialTelnetLogln(F("SPS30: could not obtain serial number")); }

    ret = sps30.GetProductName(buf, 32);
    if (ret == ERR_OK)  {
        printSerialTelnetLog(F("SPS30: product name : "));
        if(strlen(buf) > 0)  { printSerialTelnetLog(buf); printSerialTelnetLog(F("\r\n")); }
        else { printSerialTelnetLogln("not available"); }
    } else { printSerialTelnetLogln(F("SPS30: could not obtain product name")); }
  }
  
  ret = sps30.GetVersion(&v);
  if (ret != ERR_OK) { // I have sensor that reports v.minor = 255 and CRC error when reading version information
    if (mySettings.debuglevel > 0 ) { printSerialTelnetLog("SPS30: error when reading version information\r\n"); } 
    v.major = 1;  // is reasonable minimal version
    v.minor = 0;  // is not reasonable
  }
  if (mySettings.debuglevel > 0 ) {
    sprintf_P(tmpStr, PSTR("SPS30: firmware level: %hu.%hu"),v.major,v.minor);             printSerialTelnetLogln(tmpStr);
    sprintf_P(tmpStr, PSTR("SPS30: hardware level: %hu"),    v.HW_version);                printSerialTelnetLogln(tmpStr);
    sprintf_P(tmpStr, PSTR("SPS30: SHDLC protocol: %hu.%hu"),v.SHDLC_major,v.SHDLC_minor); printSerialTelnetLogln(tmpStr);
    sprintf_P(tmpStr, PSTR("SPS30: library level : %hu.%hu"),v.DRV_major,v.DRV_minor);     printSerialTelnetLogln(tmpStr);
  }
  
  ret = sps30.GetAutoCleanInt(&autoCleanIntervalSPS30);
  if (ret == ERR_OK) {
    if (mySettings.debuglevel > 0) { sprintf_P(tmpStr, PSTR("SPS30: current Auto Clean interval: %us"), autoCleanIntervalSPS30); printSerialTelnetLogln(tmpStr); }
  } else { 
    if (mySettings.debuglevel > 0) { printSerialTelnetLogln(F("SPS30: coulnd not obtain autoclean information")); }
  }

  // Start the device, will create internal readings every  1 sec
  if (sps30.start() == true) { // takes 20ms
    if (mySettings.debuglevel > 0) { printSerialTelnetLogln(F("SPS30: measurement started")); }
    stateSPS30 = IS_BUSY; 
  } else { 
    if (mySettings.debuglevel > 0) { printSerialTelnetLogln(F("SPS30: could NOT start measurement")); }
    stateSPS30 = HAS_ERROR;
    errorRecSPS30 = currentTime + 5000;
    return(false);
  }

  lastSPS30 = currentTime;

  // addresse issue with limited RAM on some microcontrollers
  if (mySettings.debuglevel > 0) {
    if (sps30.I2C_expect() == 4) { printSerialTelnetLogln(F("SPS30: due to I2C buffersize only the SPS30 MASS concentration is available")); }
  }
  
  if (mySettings.debuglevel > 0) { printSerialTelnetLogln(F("SPS30: initialized")); }
  delay(50); lastYield = millis();
  return(true);
}

/******************************************************************************************************/
// Update SPS30
/******************************************************************************************************/
// Start with IS_BUSY or HAS_ERROR state
//
// IS BUSY:
//   get Values and report errors
//   compute time when SPS30 is stable based on total particles
//   go to WAIT STABLE
//
// WAIT_STABLE
//   wait until SPS30 is stable
//   if newer sensor get status register and report errors 
//   get values and report errors
//   go to IDLE
//   
// IDLE
//   if fastmode or old version next new data available in default interval
//       next state is WAIT_STABLE
//   otherwise put to sleep and set waketime to (lastSPS30 + intervalSPS30 - 50 - timeToStableSPS30);
//       next state is SLEEPING
//
// IS_SLEEPING
//   wait until wakeup time
//   wake up
//   got to state IS_WAKINGUP
//
// IS_WAKINGUP
//   wait 50ms
//   start measurements
//   go to state IS_BUSY
//
// HAS_ERROR
//   similar to initialize without setting global variabled

bool updateSPS30() {

  bool success = true;  // when ERROR recovery fails, success becomes false

  switch(stateSPS30) { 
    
    case IS_BUSY: { //--------------------- getting system going
      if (mySettings.debuglevel == 5) {  R_printSerialTelnetLogln(F("SPS30: is busy")); }
      if ((currentTime - lastSPS30) > 1020) { // start command needs 20ms to complete but it takes 1 sec to produce data
        D_printSerialTelnet(F("D:U:SPS30:IB.."));
        sps30_port->begin(sps30_i2c[0], sps30_i2c[1]); 
        sps30_port->setClock(I2C_SLOW);
        yieldI2C();
        tmpTime = millis();
        ret = sps30.GetValues(&valSPS30);               
        if (mySettings.debuglevel == 5)  { sprintf_P(tmpStr, PSTR("SPS30: values read in %ims"), millis() - tmpTime); R_printSerialTelnetLogln(tmpStr); }
        if (ret == ERR_DATALENGTH || valSPS30.MassPM1 == 0) { 
          if (mySettings.debuglevel > 0) { 
            sprintf_P(tmpStr, PSTR("SPS30: error data length or zero reading, %u"), sps_error_cnt); R_printSerialTelnetLogln(tmpStr); 
          }
          if (sps_error_cnt++ > 3) { // i2c error recovery with retries, this will not initiate reset
            stateSPS30 = HAS_ERROR; 
            errorRecSPS30 = currentTime + 5000; 
            sps_error_cnt = 0; 
          } // give up after 3 tries
          lastSPS30 = currentTime; 
          break; // remains in same state and try again in about a second
        } else if (ret != ERR_OK) {
          if (mySettings.debuglevel > 0) { sprintf_P(tmpStr, PSTR("SPS30: error reading values: %hu"), ret); R_printSerialTelnetLogln(tmpStr); }
          stateSPS30 = HAS_ERROR; // got to recovery
          errorRecSPS30 = currentTime + 5000;
          break;
        }
        sps_error_cnt = 0;
        sps30_error_cnt = 0;
        lastSPS30 = currentTime; 
        // adjust time to get stable readings, it takes longer  with lower concentration to get a precise reading
        totalParticles = (valSPS30.NumPM0 + valSPS30.NumPM1 + valSPS30.NumPM2 + valSPS30.NumPM4 + valSPS30.NumPM10);
        if (totalParticles < 100.)      { timeToStableSPS30 = 30000; } // hard coded from data sheet
        else if (totalParticles < 200.) { timeToStableSPS30 = 16000; } // hard coded from data sheet
        else                            { timeToStableSPS30 =  8000; } // hard coded from data sheet
        timeSPS30Stable = currentTime +  timeToStableSPS30;
        if (mySettings.debuglevel == 5) { sprintf_P(tmpStr, PSTR("SPS30: total particles - %f"), totalParticles); R_printSerialTelnetLogln(tmpStr); }
        if (mySettings.debuglevel == 5) {
          sprintf_P(tmpStr, PSTR("SPS30: current time: %lu"), currentTime); printSerialTelnetLogln(tmpStr);
          sprintf_P(tmpStr, PSTR("SPS30: time when stable: %lu"), timeSPS30Stable); printSerialTelnetLogln(tmpStr);
        }
        stateSPS30 = WAIT_STABLE;            
      }
      break; 
    } // is BUSY

    case WAIT_STABLE: { //--------------------- system is stable, read data
      /**if (mySettings.debuglevel == 5) {
        R_printSerialTelnetLog("SPS30: wait until stable\r\n");
        sprintf_P(tmpStr, PSTR("SPS30: current time: %lu\r\n"),currentTime); R_printSerialTelnetLog(tmpStr);
        sprintf_P(tmpStr, PSTR("SPS30: time when stable: %lu\r\n"), timeSPS30Stable); R_printSerialTelnetLog(tmpStr);
      }**/
      if (currentTime >= timeSPS30Stable) {
        D_printSerialTelnet(F("D:U:SPS30:WS.."));
        sps30_port->begin(sps30_i2c[0], sps30_i2c[1]);
        sps30_port->setClock(I2C_SLOW);
        yieldI2C(); 
        tmpTime = millis();
        ret = sps30.GetValues(&valSPS30);
        if (mySettings.debuglevel == 5) { sprintf_P(tmpStr, PSTR("SPS30: values read in %ldms"), millis() - tmpTime); R_printSerialTelnetLogln(tmpStr); }
        if (ret == ERR_DATALENGTH || valSPS30.MassPM1 == 0.0) { 
          if (mySettings.debuglevel > 0) { sprintf_P(tmpStr, PSTR("SPS30: error data length while reading values or zero reading, %u"), sps_error_cnt); R_printSerialTelnetLogln(tmpStr); }
          // we will allow this to happen 3 times in a row until we throw an error
          if ( sps_error_cnt++ > 3 ) { 
            stateSPS30 = HAS_ERROR; 
            errorRecSPS30 = currentTime + 5000; 
            sps_error_cnt = 0;
            break;
          } // give up after 3 tries
          timeSPS30Stable = currentTime + 1000;  
          break; // remain in same state and try again in a second
        } else if (ret != ERR_OK) {
          if (mySettings.debuglevel > 0) { sprintf_P(tmpStr, PSTR("SPS30: error reading values: error x%hu"), ret); R_printSerialTelnetLogln(tmpStr); }
          stateSPS30 = HAS_ERROR;
          errorRecSPS30 = currentTime + 5000;
          break;
        }
        sps_error_cnt = 0; // we got values
        sps30_error_cnt = 0;
        sps30NewData   = true;
        sps30NewDataWS = true;

        // obtain device status
        if (((v.major==2) && (v.minor>=2)) || (v.major>2)) {
          ret = sps30.GetStatusReg(&st);// takes 20ms
          if (ret == ERR_OK) {
            if (mySettings.debuglevel == 5) { R_printSerialTelnetLogln(F("SPS30: reading status completed")); }
            if (st == STATUS_OK) {
              if (mySettings.debuglevel == 5) { R_printSerialTelnetLogln(F("SPS30: ok")); }
            } else {
              if (mySettings.debuglevel > 0) {
                if (st & STATUS_SPEED_ERROR) { R_printSerialTelnetLogln(F("SPS30: warning, fan is turning too fast or too slow"));  }
                if (st & STATUS_LASER_ERROR) { R_printSerialTelnetLogln(F("SPS30: error laser failure")); }
                if (st & STATUS_FAN_ERROR)   { R_printSerialTelnetLogln(F("SPS30: error fan failure, fan is mechanically blocked or broken")); }
              }
              // stateSPS30 = HAS_ERROR; // continue using the sensor anyway?
            } // status ok/notok
          } else {
            if (mySettings.debuglevel > 0) { R_printSerialTelnetLogln(F("SPS30: could not read status")); }
            // stateSPS30 = HAS_ERROR; // continue using the sensor anyway?
          } // read status
        } // Firmware >= 2.2

        if (mySettings.debuglevel == 5) { R_printSerialTelnetLogln(F("SPS30: going to idle")); }
        lastSPS30 = currentTime; 
        stateSPS30 = IS_IDLE;
      } //if time stable
      break;
    } // wait stable
                
    case IS_IDLE : { //--------------------- after reading data either go to sleep or go back to read again
      D_printSerialTelnet(F("D:U:SPS30:II.."));
      if (mySettings.debuglevel == 5) { R_printSerialTelnetLogln("SPS30: is idle"); }
      if ((fastMode) || (v.major<2) ) { // fastmode or no sleep function
        timeSPS30Stable = (unsigned long) (lastSPS30 + intervalSPS30); 
        if (mySettings.debuglevel == 5) {
          sprintf_P(tmpStr, PSTR("SPS30: lastSPS30: %lu"), lastSPS30);            R_printSerialTelnetLogln(tmpStr);
          sprintf_P(tmpStr, PSTR("SPS30: current time:: %lu"), currentTime);        printSerialTelnetLogln(tmpStr);
          sprintf_P(tmpStr, PSTR("SPS30: time when stable: %lu"), timeSPS30Stable); printSerialTelnetLogln(tmpStr);
          printSerialTelnetLogln(F("SPS30: going to wait until stable"));
        }
        stateSPS30 = WAIT_STABLE;             
      } else {
        wakeTimeSPS30 = (unsigned long) (lastSPS30 + intervalSPS30 - 50 - timeToStableSPS30);
        sps30_port->begin(sps30_i2c[0], sps30_i2c[1]); 
        sps30_port->setClock(I2C_SLOW);
        yieldI2C();
        ret = sps30.sleep(); // takes 5ms
        if (ret != ERR_OK) { 
          if (mySettings.debuglevel > 0) { printSerialTelnetLogln(F("SPS30: error, could not go to sleep")); }
          stateSPS30 = HAS_ERROR;
          errorRecSPS30 = currentTime + 5000;
        } else {
          if (mySettings.debuglevel == 5) { printSerialTelnetLogln(F("SPS30: going to sleep")); }
          stateSPS30 = IS_SLEEPING;
        }
      }
      break;
    }
      
    case IS_SLEEPING : { //--------------------- in slow mode we put system to sleep if new sensor version
      if (mySettings.debuglevel == 5) { R_printSerialTelnetLogln(F("SPS30: is sleepig")); }
      if (currentTime >= wakeTimeSPS30) { // Wake up if sleep time exceeded
        D_printSerialTelnet(F("D:U:SPS30:IS.."));
        sps30_port->begin(sps30_i2c[0], sps30_i2c[1]);  
        sps30_port->setClock(I2C_SLOW);
        yieldI2C();
        if (mySettings.debuglevel == 5) { R_printSerialTelnetLogln(F("SPS30: waking up")); }
        ret = sps30.wakeup(); // takes 5ms
        if (ret != ERR_OK) {
          if (mySettings.debuglevel > 0) { printSerialTelnetLogln(F("SPS30: error could not wakeup")); }
          stateSPS30 = HAS_ERROR; 
          errorRecSPS30 = currentTime + 5000;
        } else {
          wakeSPS30 = currentTime;
          stateSPS30 = IS_WAKINGUP;
        }
      } // time interval
      break; 
    } // case is sleeping
      
    case IS_WAKINGUP : {  // ------------------  startup the sensor
      if (mySettings.debuglevel == 5) { R_printSerialTelnetLogln(F("SPS30: is waking up")); }
      if ((currentTime - wakeSPS30) >= 50) { // Give some time (50ms)to wake up 
        D_printSerialTelnet(F("D:U:SPS30:IW.."));
        sps30_port->begin(sps30_i2c[0], sps30_i2c[1]);  
        sps30_port->setClock(I2C_SLOW);
        yieldI2C();
        ret = sps30.start();  //takes 20ms
        if (ret != ERR_OK) { 
          if (mySettings.debuglevel > 0) { R_printSerialTelnetLogln(F("SPS30: error, could not start SPS30 measurements")); }
          stateSPS30 = HAS_ERROR; 
          errorRecSPS30 = currentTime + 5000;
        } else {
          if (mySettings.debuglevel == 5) { R_printSerialTelnetLogln(F("SPS30: started")); }
          stateSPS30 = IS_BUSY;
        }
        lastSPS30 = currentTime;
      }        
      break;
    }

    case HAS_ERROR : { //--------------------  trying to recover sensor
      if (currentTime > errorRecSPS30) {
        D_printSerialTelnet(F("D:U:SPS30:E.."));
        if (sps30_error_cnt++ > 3) { 
          success = false; 
          sps30_avail = false; 
          if (mySettings.debuglevel > 0) { R_printSerialTelnetLogln(F("SPS30: reinitialization attempts exceeded, SPS30: no longer available.")); }
          break;
        } // give up after 3 tries
        sps30_port->begin(sps30_i2c[0], sps30_i2c[1]); 
        sps30_port->setClock(I2C_SLOW);
        yieldI2C();
        sps30.EnableDebugging(SPS30Debug);
        if (sps30.begin(sps30_port) == false) {
          stateSPS30 = HAS_ERROR;
          errorRecSPS30 = currentTime + 5000;
          // success = false;
          if (mySettings.debuglevel > 0) { R_printSerialTelnetLogln(F("SPS30: could not re-intialize")); }
          break;
        }
        if (sps30.probe() == false) { 
          sps30.wakeup();
          delay(5); lastYield = millis(); // 5ms needed to wake up
          sps30.reset(); 
          delay(100); lastYield = millis(); // 100ms needed to reset
          if (sps30.probe() == false) {
            stateSPS30 = HAS_ERROR;
            errorRecSPS30 = currentTime + 5000;
            // success = false;
            if (mySettings.debuglevel > 0) { printSerialTelnetLogln(F("SPS30: probe not successful, could not re-intialize")); }
            break;
          }
        }
        if (sps30.reset() == false) { 
          stateSPS30 = HAS_ERROR;
          errorRecSPS30 = currentTime + 5000;
          delay(1); lastYield = millis();
          // success = false;
          if (mySettings.debuglevel > 0) { printSerialTelnetLogln(F("SPS30: could not re-intialize/reset")); }
          break;
        } else { delay(100); lastYield = millis();}
        // read device info
        ret = sps30.GetVersion(&v);
        if (ret != ERR_OK) { // I have sensor that reports v.minor = 255 and CRC error when reading version information
          v.major = 1;  // is reasonable minimal version
          v.minor = 0;  // is not reasonable
        }
        ret = sps30.GetAutoCleanInt(&autoCleanIntervalSPS30);
        if (sps30.start() == true) { 
          delay(1); lastYield = millis();
          stateSPS30 = IS_BUSY; 
        } else { 
          stateSPS30 = HAS_ERROR;
          errorRecSPS30 = currentTime + 5000;
          // success = false;
          if (mySettings.debuglevel > 0) { printSerialTelnetLogln(F("SPS30: could not re-intialize")); }
          break;
        }
      }
      break;
    }

    default: {if (mySettings.debuglevel > 0) { R_printSerialTelnetLogln(F("SPS30 Error: invalid switch statement")); break;}}

  } // end cases

  return success;
}

void sps30JSON(char *payload) {
  char qualityMessage1[16];
  char qualityMessage2[16];
  if (sps30_avail) { 
    checkPM2(valSPS30.MassPM2, qualityMessage1, 15); 
    checkPM10(valSPS30.MassPM10, qualityMessage2, 15);
  } else {
    strncpy(qualityMessage1, "not available", sizeof(qualityMessage1));
    strncpy(qualityMessage2, "not available", sizeof(qualityMessage2));
  } 
  sprintf_P(payload, PSTR("{ \"sps30\": { \"avail\": %s, \"PM1\": %4.1f, \"PM2\": %4.1f, \"PM4\": %4.1f, \"PM10\": %4.1f, \"nPM0\": %4.1f, \"nPM1\": %4.1f, \"nPM2\": %4.1f, \"nPM4\": %4.1f, \"nPM10\": %4.1f, \"PartSize\": %4.1f, \"PM2_airquality\": \"%s\", \"PM10_airquality\": \"%s\"}}"), 
                       sps30_avail ? "true" : "false", 
                       sps30_avail ? valSPS30.MassPM1 : -1.,
                       sps30_avail ? valSPS30.MassPM2 : -1.,
                       sps30_avail ? valSPS30.MassPM4 : -1.,
                       sps30_avail ? valSPS30.MassPM10 : -1.,
                       sps30_avail ? valSPS30.NumPM0 : -1.,
                       sps30_avail ? valSPS30.NumPM1 : -1.,
                       sps30_avail ? valSPS30.NumPM2 : -1.,
                       sps30_avail ? valSPS30.NumPM4 : -1.,
                       sps30_avail ? valSPS30.NumPM10 : -1.,
                       sps30_avail ? valSPS30.PartSize : -1.,
                       qualityMessage1, 
                       qualityMessage2);
}
