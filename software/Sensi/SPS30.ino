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
  bool success = true;

  sps30.EnableDebugging(SPS30Debug);

  if (fastMode == true) { intervalSPS30 = intervalSPS30Fast; }
  else                  { intervalSPS30 = intervalSPS30Slow; }

  if (mySettings.debuglevel >0) { sprintf_P(tmpStr, PSTR("SPS: Interval: %lums\r\n"),intervalSPS30); printSerialTelnet(tmpStr); }
  sps30_port->begin(sps30_i2c[0], sps30_i2c[1]);  
  delay(1);

  if (sps30.begin(sps30_port) == false) {
    if (mySettings.debuglevel > 0) { printSerialTelnet(F("SPS30: Sensor not detected in I2C. Please check wiring\r\n")); }
    stateSPS30 = HAS_ERROR;
    sps30_avail = false;
    success = false;
  }

  if (sps30.probe() == false) { 
    // attempt reset and retry, I dont know how to restart sps30 without physical access.
    sps30.reset();
    delay(100);
    if (sps30.probe() == false) {
      if (mySettings.debuglevel > 0) {
        printSerialTelnet(F("SPS30: could not probe / connect\r\n"));
        printSerialTelnet(F("SPS30: powercycle the system and reopen serial console\r\n"));  
      }
      stateSPS30 = HAS_ERROR;
      sps30_avail = false;
      success = false;
    }
  } else { 
    if (mySettings.debuglevel > 0) {printSerialTelnet(F("SPS30: detected\r\n")); }
  }

  if (sps30.reset() == false) { 
    if (mySettings.debuglevel > 0) {printSerialTelnet(F("SPS30: could not reset\r\n")); }
    stateSPS30 = HAS_ERROR;
    sps30_avail = false;
    success = false;
  }  

  // read device info

  if (mySettings.debuglevel > 0) {
    printSerialTelnet(F("SPS30: obtaining device information:\r\n"));
    ret = sps30.GetSerialNumber(buf, 32);
    if (ret == ERR_OK) {
      if (mySettings.debuglevel > 0) { 
        printSerialTelnet("SPS30: serial number : ");
        if(strlen(buf) > 0) { printSerialTelnet(buf); }
        else { printSerialTelnet(F("not available\r\n")); }
      }
    }  else { printSerialTelnet(F("SPS30: could not obtain serial number\r\n")); }

    ret = sps30.GetProductName(buf, 32);
    if (ret == ERR_OK)  {
        printSerialTelnet(F("SPS30: product name : "));
        if(strlen(buf) > 0)  { printSerialTelnet(buf); }
        else { printSerialTelnet("not available\r\n"); }
    } else { if (mySettings.debuglevel > 0) { printSerialTelnet(F("SPS30: could not obtain product name\r\n")); } }
  }
  
  ret = sps30.GetVersion(&v);
  if (ret != ERR_OK) { // I have sensor that reports v.minor = 255 and CRC error when reading version information
    if (mySettings.debuglevel > 0 ) { printSerialTelnet("SPS30: error when reading version information\r\n"); } 
    v.major = 1;  // is reasonable minimal version
    v.minor = 0;  // is not reasonable
  }
  if (mySettings.debuglevel > 0 ) {
    sprintf_P(tmpStr, PSTR("SPS30: firmware level: %hu.%hu\r\n"),v.major,v.minor); printSerialTelnet(tmpStr);
    sprintf_P(tmpStr, PSTR("SPS30: hardware level: %hu\r\n"), v.HW_version); printSerialTelnet(tmpStr);
    sprintf_P(tmpStr, PSTR("SPS30: SHDLC protocol: %hu.%hu\r\n"),v.SHDLC_major,v.SHDLC_minor); printSerialTelnet(tmpStr);
    sprintf_P(tmpStr, PSTR("SPS30: library level : %hu.%hu\r\n"),v.DRV_major,v.DRV_minor); printSerialTelnet(tmpStr);
  }
  
  ret = sps30.GetAutoCleanInt(&autoCleanIntervalSPS30);
  if (ret == ERR_OK) {
    if (mySettings.debuglevel > 0) { sprintf_P(tmpStr, PSTR("SPS30: current Auto Clean interval: %us\r\n"), autoCleanIntervalSPS30); printSerialTelnet(tmpStr); }
  } else { 
    if (mySettings.debuglevel > 0) { printSerialTelnet(F("SPS30: coulnd not obtain autoclean information\r\n")); }
  }

  // Start the device, will create internal readings every  1 sec
  if (sps30.start() == true) { // takes 20ms
    if (mySettings.debuglevel > 0) { printSerialTelnet(F("SPS30: measurement started\r\n")); }
    stateSPS30 = IS_BUSY; 
  } else { 
    if (mySettings.debuglevel > 0) { printSerialTelnet(F("SPS30: could NOT start measurement\r\n")); }
    stateSPS30 = HAS_ERROR;
    success = false;
  }

  lastSPS30 = currentTime;

  // addresse issue with limited RAM on some microcontrollers
  if (mySettings.debuglevel > 0) {
    if (sps30.I2C_expect() == 4) { printSerialTelnet(F("SPS30: due to I2C buffersize only the SPS30 MASS concentration is available\r\n")); }
  }
  if (mySettings.debuglevel > 0) { printSerialTelnet(F("SPS30: initialized\r\n")); }
  delay(50);
  return success;
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
//   if newer sensor get status register and report errors *** need only once
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

  bool success = true;

  switch(stateSPS30) { 
    
    case IS_BUSY: { //--------------------- getting system going
      if (mySettings.debuglevel == 5) {  Serial.println(F("SPS30: is busy")); }
      if ((currentTime - lastSPS30) > 1020) { // start command needs 20ms to complete but it takes 1 sec to produce data
        sps30_port->begin(sps30_i2c[0], sps30_i2c[1]); 
        delay(1);
        tmpTime = millis();
        ret = sps30.GetValues(&valSPS30);               
        if (mySettings.debuglevel == 5)  { sprintf_P(tmpStr, PSTR("SPS30: values read in %ims\r\n"), millis() - tmpTime); printSerialTelnet(tmpStr); }
        if (ret == ERR_DATALENGTH || valSPS30.MassPM1 == 0) { 
          if (mySettings.debuglevel > 0.0) { printSerialTelnet(F("SPS30: error data length while reading values or zero reading\r\n")); }
          // allow a few more attempts to read data
          if (sps_error_cnt++>3) {stateSPS30 = HAS_ERROR;} // give up after 3 tries
          lastSPS30 = currentTime; 
          break; // remains in same state and try again in about a second
        } else if (ret != ERR_OK) {
          if (mySettings.debuglevel > 0) { sprintf_P(tmpStr, PSTR("SPS30: error reading values: %hu\r\n"), ret); printSerialTelnet(tmpStr); }
          stateSPS30 = HAS_ERROR; // got to recovery
          break;
        }
        sps_error_cnt = 0;
        lastSPS30 = currentTime; 
        // adjust time to get stable readings, it takes longer  with lower concentration to get a precise reading
        totalParticles = (valSPS30.NumPM0 + valSPS30.NumPM1 + valSPS30.NumPM2 + valSPS30.NumPM4 + valSPS30.NumPM10);
        if (totalParticles < 100.)      { timeToStableSPS30 = 30000; } // hard coded from data sheet
        else if (totalParticles < 200.) { timeToStableSPS30 = 16000; } // hard coded from data sheet
        else                            { timeToStableSPS30 =  8000; } // hard coded from data sheet
        timeSPS30Stable = currentTime +  timeToStableSPS30;
        if (mySettings.debuglevel == 5) { sprintf_P(tmpStr, PSTR("SPS30: total particles - %f\r\n"), totalParticles); printSerialTelnet(tmpStr); }
        if (mySettings.debuglevel == 5) {
          sprintf_P(tmpStr, PSTR("SPS30: current time: %lu\r\n"), currentTime); printSerialTelnet(tmpStr);
          sprintf_P(tmpStr, PSTR("SPS30: time when stable: %lu\r\n"), timeSPS30Stable); printSerialTelnet(tmpStr);
        }
        stateSPS30 = WAIT_STABLE;            
      }
      break; 
    } // is BUSY

    case WAIT_STABLE: { //--------------------- system is stable, read data
      if (mySettings.debuglevel == 5) {
        printSerialTelnet("SPS30: wait until stable\r\n");
        sprintf_P(tmpStr, PSTR("SPS30: current time: %lu\r\n"),currentTime); printSerialTelnet(tmpStr);
        sprintf_P(tmpStr, PSTR("SPS30: time when stable: %lu\r\n"), timeSPS30Stable); printSerialTelnet(tmpStr);
      }
      if (currentTime >= timeSPS30Stable) {
        sps30_port->begin(sps30_i2c[0], sps30_i2c[1]);
        delay(1);  
        tmpTime = millis();
        ret = sps30.GetValues(&valSPS30);
        if (mySettings.debuglevel == 5) { sprintf_P(tmpStr, PSTR("SPS30: values read in %ldms\r\n"), millis() - tmpTime); printSerialTelnet(tmpStr); }
        if (ret == ERR_DATALENGTH || valSPS30.MassPM1 == 0.0) { 
          if (mySettings.debuglevel > 0) { printSerialTelnet(F("SPS30: error data length while reading values or zero reading\r\n"));  }
          // we will allow this to happen up to 3 times in a row until we throw error
          if (sps_error_cnt++>3) { stateSPS30 = HAS_ERROR; } // give up after 3 tries
          timeSPS30Stable = currentTime + 1000;  
          break; // remain in same state and try again in a second
        } else if (ret != ERR_OK) {
          if (mySettings.debuglevel > 0) { sprintf_P(tmpStr, PSTR("SPS30: error reading values: %hu\r\n"), ret); printSerialTelnet(tmpStr); }
          stateSPS30 = HAS_ERROR;
          break;
        }
        sps_error_cnt = 0; // we got values
        sps30NewData   = true;
        sps30NewDataWS = true;

        // obtain device status
        if (((v.major==2) && (v.minor>=2)) || (v.major>2)) {
          ret = sps30.GetStatusReg(&st);// takes 20ms
          if (ret == ERR_OK) {
            if (mySettings.debuglevel == 5) { printSerialTelnet(F("SPS30: reading status completed\r\n")); }
            if (st == STATUS_OK) {
              if (mySettings.debuglevel == 5) { printSerialTelnet(F("SPS30: ok\r\n")); }
            } else {
              if (mySettings.debuglevel > 0) {
                if (st & STATUS_SPEED_ERROR) { printSerialTelnet(F("SPS30: warning, fan is turning too fast or too slow\r\n"));  }
                if (st & STATUS_LASER_ERROR) { printSerialTelnet(F("SPS30: error laser failure\r\n")); }
                if (st & STATUS_FAN_ERROR)   { printSerialTelnet(F("SPS30: error fan failure, fan is mechanically blocked or broken\r\n")); }
              }
              // stateSPS30 = HAS_ERROR; // continue using the sensor anyway?
            } // status ok/notok
          } else {
            if (mySettings.debuglevel > 0) { printSerialTelnet(F("SPS30: could not read status\r\n")); }
            // stateSPS30 = HAS_ERROR; // continue using the sensor anyway?
          } // read status
        } // Firmware >= 2.2

        if (mySettings.debuglevel == 5) { printSerialTelnet(F("SPS30: going to idle\r\n")); }
        lastSPS30 = currentTime; 
        stateSPS30 = IS_IDLE;          
      } //if time stable
      break;
    } // wait stable
                
    case IS_IDLE : { //--------------------- after reading data either go to sleep or go back to read again
      if (mySettings.debuglevel == 5) { printSerialTelnet("SPS30: is idle"); }
      if ((fastMode) || (v.major<2) ) { // fastmode or no sleep function
        timeSPS30Stable = (unsigned long) (lastSPS30 + intervalSPS30); 
        if (mySettings.debuglevel == 5) {
          sprintf_P(tmpStr, PSTR("SPS30: lastSPS30: %lu\r\n"), lastSPS30); printSerialTelnet(tmpStr);
          sprintf_P(tmpStr, PSTR("SPS30: current time:: %lu\r\n"), currentTime); printSerialTelnet(tmpStr);
          sprintf_P(tmpStr, PSTR("SPS30: time when stable: %lu\r\n"), timeSPS30Stable); printSerialTelnet(tmpStr);
          printSerialTelnet(F("SPS30: going to wait until stable\r\n"));
        }
        stateSPS30 = WAIT_STABLE;             
      } else {
        wakeTimeSPS30 = (unsigned long) (lastSPS30 + intervalSPS30 - 50 - timeToStableSPS30);
        sps30_port->begin(sps30_i2c[0], sps30_i2c[1]); 
        delay(1); 
        ret = sps30.sleep(); // takes 5ms
        if (ret != ERR_OK) { 
          if (mySettings.debuglevel > 0) { printSerialTelnet(F("SPS30: error, could not go to sleep\r\n")); }
          stateSPS30 = HAS_ERROR;
        } else {
          if (mySettings.debuglevel == 5) { printSerialTelnet(F("SPS30: going to sleep\r\n")); }
          stateSPS30 = IS_SLEEPING;
        }
      }
      break;
    }
      
    case IS_SLEEPING : { //--------------------- in slow mode we put system to sleep if new sensor version
      if (mySettings.debuglevel == 5) { printSerialTelnet(F("SPS30: is sleepig\r\n")); }
      if (currentTime >= wakeTimeSPS30) { // Wake up if sleep time exceeded
        sps30_port->begin(sps30_i2c[0], sps30_i2c[1]);  
        delay(1);
        if (mySettings.debuglevel == 5) { printSerialTelnet(F("SPS30: waking up\r\n")); }
        ret = sps30.wakeup(); // takes 5ms
        if (ret != ERR_OK) {
          if (mySettings.debuglevel > 0) { printSerialTelnet(F("SPS30: error could not wakeup\r\n")); }
          stateSPS30 = HAS_ERROR; 
        } else {
          wakeSPS30 = currentTime;
          stateSPS30 = IS_WAKINGUP;
        }
      } // time interval
      break; 
    } // case is sleeping
      
    case IS_WAKINGUP : {  // ------------------  startup the sensor
      if (mySettings.debuglevel == 5) { printSerialTelnet(F("SPS30: is waking up\r\n")); }
      if ((currentTime - wakeSPS30) >= 50) { // Give some time (50ms)to wake up 
        sps30_port->begin(sps30_i2c[0], sps30_i2c[1]);  
        delay(1);
        ret = sps30.start();  //takes 20ms
        if (ret != ERR_OK) { 
          if (mySettings.debuglevel > 0) { printSerialTelnet(F("SPS30: error, could not start SPS30 measurements\r\n")); }
          stateSPS30 = HAS_ERROR; 
        } else {
          if (mySettings.debuglevel == 5) { printSerialTelnet(F("SPS30: started\r\n")); }
          stateSPS30 = IS_BUSY;
        }
        lastSPS30 = currentTime;
      }        
      break;
    }

    case HAS_ERROR : { //--------------------  trying to recover sensor
      sps30_port->begin(sps30_i2c[0], sps30_i2c[1]); 
      delay(1); 
      sps30.EnableDebugging(SPS30Debug);
      if (sps30.begin(sps30_port) == false) {
        stateSPS30 = HAS_ERROR;
        sps30_avail = false;
        success = false;
        if (mySettings.debuglevel > 0) { printSerialTelnet(F("SPS30: could not re-intialize\r\n")); }
        break;
      }
      if (sps30.probe() == false) { 
        sps30.reset();
        delay(500);
        if (sps30.probe() == false) {
          stateSPS30 = HAS_ERROR;
          sps30_avail = false;
          success = false;
          if (mySettings.debuglevel > 0) { printSerialTelnet(F("SPS30: probe not successful, could not re-intialize\r\n")); }
          break;
        }
      }
      if (sps30.reset() == false) { 
        stateSPS30 = HAS_ERROR;
        sps30_avail = false;
        success = false;
        if (mySettings.debuglevel > 0) { printSerialTelnet(F("SPS30: could not re-intialize/reset\r\n")); }
        break;
      }  
      // read device info
      ret = sps30.GetVersion(&v);
      if (ret != ERR_OK) { // I have sensor that reports v.minor = 255 and CRC error when reading version information
        v.major = 1;  // is reasonable minimal version
        v.minor = 0;  // is not reasonable
      }
      ret = sps30.GetAutoCleanInt(&autoCleanIntervalSPS30);
      if (sps30.start() == true) { 
        stateSPS30 = IS_BUSY; 
      } else { 
        stateSPS30 = HAS_ERROR;
        success = false;
        if (mySettings.debuglevel > 0) { printSerialTelnet(F("SPS30: could not re-intialize\r\n")); }
        break;
      }
      break;
    }

    default: {if (mySettings.debuglevel > 0) { printSerialTelnet(F("SPS30 Error: invalid switch statement")); break;}}

  } // end cases

  return success;
}

void sps30JSON(char *payload) {
  char qualityMessage1[16];
  char qualityMessage2[16];
  checkPM2(valSPS30.MassPM2, qualityMessage1, 15); 
  checkPM10(valSPS30.MassPM10, qualityMessage2, 15);
  sprintf_P(payload, PSTR("{\"sps30\":{\"avail\":%s,\"PM1\":%4.1f,\"PM2\":%4.1f,\"PM4\":%4.1f,\"PM10\":%4.1f,\"nPM0\":%4.1f,\"nPM1\":%4.1f,\"nPM2\":%4.1f,\"nPM4\":%4.1f,\"nPM10\":%4.1f,\"PartSize\":%4.1f,\"PM2_airquality\":\"%s\",\"PM10_airquality\":\"%s\"}}"), 
                       sps30_avail ? "true" : "false", 
                       valSPS30.MassPM1,
                       valSPS30.MassPM2,
                       valSPS30.MassPM4,
                       valSPS30.MassPM10,
                       valSPS30.NumPM0,
                       valSPS30.NumPM1,
                       valSPS30.NumPM2,
                       valSPS30.NumPM4,
                       valSPS30.NumPM10,
                       valSPS30.PartSize,
                       qualityMessage1, 
                       qualityMessage2);
}
