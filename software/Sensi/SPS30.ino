/******************************************************************************************************/
// SPS30 PM 1.0 PM 2.5, PM 4, PM 10.0 averge Particle Size, [PM 2.5 and 10 used for airquality]
/******************************************************************************************************/
// float   mc_1p0;        // Mass Concentration PM1.0 [μg/m3]
// float   mc_2p5;        // Mass Concentration PM2.5 [μg/m3]
// float   mc_4p0;        // Mass Concentration PM4.0 [μg/m3]
// float   mc_10p0;       // Mass Concentration PM10 [μg/m3]
// float   nc_0p5;        // Number Concentration PM0.5 [#/cm3]
// float   nc_1p0;        // Number Concentration PM1.0 [#/cm3]
// float   nc_2p5;        // Number Concentration PM2.5 [#/cm3]
// float   nc_4p0;        // Number Concentration PM4.0 [#/cm3]
// float   nc_10p0;       // Number Concentration PM10.0 [#/cm3]
// float   typical_particle_size;  // micro meters

#include "src/SPS30.h"
#include "src/Config.h"
#include "src/Sensi.h"
#include "src/Quality.h"
#include "src/Print.h"

unsigned long intervalSPS30 = 0;                           // measurement interval
unsigned long timeSPS30Stable;                             // time when readings are stable, is adjusted automatically based on particle counts
unsigned long lastSPS30;                                   // last time we interacted with sensor
unsigned long wakeSPS30;                                   // time when wakeup was issued
unsigned long wakeTimeSPS30;                               // time when sensor is supposed to be woken up
unsigned long timeToStableSPS30;                           // how long it takes to get stable readings, automatically pupulated based on total particles
unsigned long errorRecSPS30;
unsigned long startMeasurementSPS30;
unsigned long sps30_lastError;

bool     sps30_avail = false;                              // do we have this sensor?
bool     sps30NewData = false;                             // do we have new data to display?
bool     sps30NewDataWS = false;                           // do we have new data for websocket

uint8_t  sps30_i2c[2];                                     // the pins for the i2c port, set during initialization
uint8_t  sps30_error_cnt = 0;                              // give a few retries with rebooting
uint8_t  sps30_timeout_cnt = 0;                            // how many times did we have to extend the waiting period
uint16_t sps30_data_ready = 0;                             // does sensor have new data?
uint32_t sps30AutoCleanInterval;                           // current cleaning interval setting in sensor
uint32_t sps30_st;                                         // sps30 status register

TwoWire *sps30_port = 0;                                   // pointer to the i2c port, might be useful for other microcontrollers
SPS30    sps30;                                            // the particle sensor
sps30_measurement valSPS30;                                // will hold the readings from sensor
volatile SensorStates stateSPS30 = IS_BUSY;                // sensor state

// External Variables
extern Settings      mySettings;   // Config
extern bool          fastMode;     // Sensi
extern bool          BMEhum_avail; // BME280
extern unsigned long lastYield;    // Sensi
extern unsigned long currentTime;  // Sensi
extern char          tmpStr[256];  // Sensi

/******************************************************************************************************/
// Initialize SPS30
/******************************************************************************************************/

// Initialize
//   Start i2c port
//   Probe for sensor (obtain version information), if not successful software reset and probe again
//   Reset
//   Get Serialnumber
//   Get Productname
//   Get Version information
//   Get Autoclean interval, if not 7 days set to 7 days
//   Start, new data is created every 1 seconds

bool initializeSPS30() { 

  if (fastMode == true) { intervalSPS30 = intervalSPS30Fast; }
  else                  { intervalSPS30 = intervalSPS30Slow; }

  if (mySettings.debuglevel > 0) { 
    snprintf_P(tmpStr, sizeof(tmpStr), PSTR("SPS30: Interval: %lums"),intervalSPS30);
    R_printSerialTelnetLogln(tmpStr); 
  }

  switchI2C(sps30_port, sps30_i2c[0], sps30_i2c[1], sps30_i2cspeed, sps30_i2cClockStretchLimit);
  sps30.begin(sps30_port);

  if ( sps30.probe() ) {

    if (mySettings.debuglevel > 0) {
      printSerialTelnetLogln(F("SPS30: detected"));
      snprintf_P(tmpStr, sizeof(tmpStr), PSTR("SPS30: serial: %s major: %d minor: %d"), 
                sps30.serial_number(), sps30.fw_major(), sps30.fw_minor()); 
      printSerialTelnetLogln(tmpStr);
      snprintf_P(tmpStr, sizeof(tmpStr), PSTR("SPS30: product type: %s"), sps30.product_type()); 
      printSerialTelnetLogln(tmpStr);
      snprintf_P(tmpStr, sizeof(tmpStr), PSTR("SPS30: driver: %s"), sps30.driver_version()); 
      printSerialTelnetLogln(tmpStr);
    }
  } else {
    // reset
    if ( sps30.reset() ) { if (mySettings.debuglevel > 0) { printSerialTelnetLogln(F("SPS30: reset")); } }
    else                 { if (mySettings.debuglevel > 0) { printSerialTelnetLogln(F("SPS30: could not reset")); } }

    // probe again
    if ( sps30.probe() ) {

      if (mySettings.debuglevel > 0) {
        printSerialTelnetLogln(F("SPS30: detected"));
        snprintf_P(tmpStr, sizeof(tmpStr), PSTR("SPS30: serial: %s major: %d minor: %d"), 
                  sps30.serial_number(), sps30.fw_major(), sps30.fw_minor()); 
        printSerialTelnetLogln(tmpStr);
        snprintf_P(tmpStr, sizeof(tmpStr), PSTR("SPS30: product type: %s"), sps30.product_type()); 
        printSerialTelnetLogln(tmpStr);
        snprintf_P(tmpStr, sizeof(tmpStr), PSTR("SPS30: driver: %s"), sps30.driver_version()); 
        printSerialTelnetLogln(tmpStr);
      }
    } else {
      if (mySettings.debuglevel > 0) { printSerialTelnetln(F("SPS30: could not probe / connect. resetting I2C")); }
      if ( sps30.i2c_general_call_reset() ) { if (mySettings.debuglevel > 0) { printSerialTelnetln(F("SPS30: reset I2C bus")); } } 
      else                                  { if (mySettings.debuglevel > 0) { printSerialTelnetln(F("SPS30: could not reset I2C bus")); } }
      switchI2C(sps30_port, sps30_i2c[0], sps30_i2c[1], sps30_i2cspeed, sps30_i2cClockStretchLimit);
      delay(2000);
      
      // probe again
      if ( sps30.probe() ) {
        if (mySettings.debuglevel > 0) {
          printSerialTelnetLogln(F("SPS30: detected"));
          snprintf_P(tmpStr, sizeof(tmpStr), PSTR("SPS30: serial: %s major: %d minor: %d"), 
                    sps30.serial_number(), sps30.fw_major(), sps30.fw_minor()); 
          printSerialTelnetLogln(tmpStr);
          snprintf_P(tmpStr, sizeof(tmpStr), PSTR("SPS30: product type: %s"), sps30.product_type()); 
          printSerialTelnetLogln(tmpStr);
          snprintf_P(tmpStr, sizeof(tmpStr), PSTR("SPS30: driver: %s"), sps30.driver_version()); 
          printSerialTelnetLogln(tmpStr);
        }
      } else {
        if (mySettings.debuglevel > 0) { printSerialTelnetLogln(F("SPS30: could not probe / connect. giving up")); }
        stateSPS30 = HAS_ERROR;
        errorRecSPS30 = currentTime + 12000;
        return(false);        
      }
    } // end attempt probe recover
  } // end probe
  
  // reset
  if ( sps30.reset() ) { 
    if (mySettings.debuglevel > 0) { printSerialTelnetLogln(F("SPS30: reset")); }
    delay(100); lastYield = millis();
  } else { 
    if (mySettings.debuglevel > 0) { printSerialTelnetLogln(F("SPS30: could not reset")); }
    stateSPS30 = HAS_ERROR;
    errorRecSPS30 = currentTime + 12000;
    return(false);
  }
  
  // get cleaning interval
  if ( sps30.get_fan_auto_cleaning_interval(&sps30AutoCleanInterval) ) {
    if (mySettings.debuglevel > 0) {
      snprintf_P(tmpStr, sizeof(tmpStr), PSTR("SPS30: auto clean interval: %us"), sps30AutoCleanInterval); 
      printSerialTelnetLogln(tmpStr);
    } 
  } else { 
    if (mySettings.debuglevel > 0) { printSerialTelnetLogln(F("SPS30: coulnd not obtain autoclean information"));  }
  }

  // Start the device, it will create internal readings every 1 sec
  if ( sps30.start_measurement() ) { // takes 20ms
    if (mySettings.debuglevel > 0) { printSerialTelnetLogln(F("SPS30: measurement started")); }
    stateSPS30 = IS_BUSY; 
    sps30_timeout_cnt = 0;
  } else { 
    if (mySettings.debuglevel > 0) { printSerialTelnetLogln(F("SPS30: could NOT start measurement")); }
    stateSPS30 = HAS_ERROR;
    errorRecSPS30 = currentTime + 12000;
    return(false);
  }

  lastSPS30 = currentTime;

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
//   get values
//   go to IDLE
//   
// IDLE
//   if fastmode or old firmware version next new data available in default interval
//       next state is WAIT_STABLE
//   otherwise put sensor to sleep and set waketime to (lastSPS30 + intervalSPS30 - 50 - timeToStableSPS30);
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
//   similar to initialize

bool updateSPS30() {

  bool success = true;  // when ERROR recovery fails, success becomes false

  switch(stateSPS30) { 
    
    case IS_BUSY: { //--------------------- getting system going
      if (mySettings.debuglevel == 5) { R_printSerialTelnetLogln(F("SPS30: is busy")); }

      if ( (currentTime - lastSPS30) > 1000 ) { // start command needs 20ms to complete but it takes 1 sec to produce data
        D_printSerialTelnet(F("D:U:SPS30:IB.."));
        switchI2C(sps30_port, sps30_i2c[0], sps30_i2c[1], sps30_i2cspeed, sps30_i2cClockStretchLimit);
        
        //check if data available
        if ( sps30.read_data_ready(&sps30_data_ready) ) {
          if (mySettings.debuglevel == 5)  { R_printSerialTelnetLogln(F("SPS30: checked for data")); }
        } else { 
          if (mySettings.debuglevel > 0)  { R_printSerialTelnetLogln(F("SPS30: error checking for data")); }
          stateSPS30 = HAS_ERROR; 
          errorRecSPS30 = currentTime + 12000; 
          break; 
        }

        if (sps30_data_ready) { 
          // read data
          if ( sps30.read_measurement(&valSPS30) ) { 
            if (mySettings.debuglevel >= 2)  { R_printSerialTelnetLogln(F("SPS30: data read")); }

            // adjust time to get stable readings, it takes longer  with lower concentration to get a precise reading
            if (valSPS30.nc_10p0 < 100.)      { timeToStableSPS30 = 30000; } // hard coded from data sheet
            else if (valSPS30.nc_10p0 < 200.) { timeToStableSPS30 = 16000; } // hard coded from data sheet
            else                              { timeToStableSPS30 =  8000; } // hard coded from data sheet
            timeSPS30Stable = currentTime +  timeToStableSPS30;

            if (mySettings.debuglevel == 5) {
              snprintf_P(tmpStr, sizeof(tmpStr), PSTR("SPS30: nc PM10 %f"), valSPS30.nc_10p0); 
              R_printSerialTelnetLogln(tmpStr); 
              snprintf_P(tmpStr, sizeof(tmpStr), PSTR("SPS30: current time: %lu"), currentTime); 
              R_printSerialTelnetLogln(tmpStr);
              snprintf_P(tmpStr, sizeof(tmpStr), PSTR("SPS30: time when stable: %lu"), timeSPS30Stable); 
              R_printSerialTelnetLogln(tmpStr);
            }

            sps30_error_cnt = 0;
            sps30_timeout_cnt = 0;
            lastSPS30 = currentTime; 
            stateSPS30 = WAIT_STABLE;            
            break;
          } else {
            if (mySettings.debuglevel > 0)  { R_printSerialTelnetLogln(F("SPS30: error reading data")); }
            stateSPS30 = HAS_ERROR; 
            errorRecSPS30 = currentTime + 12000; 
            break; 
          }
        } else {
          if (sps30_timeout_cnt == 0) { lastSPS30 = lastSPS30 + SPS30_TIMEOUT_DELAY; }    // retry again in 100ms
          else {                        lastSPS30 = lastSPS30 + 10*SPS30_TIMEOUT_DELAY; } // sensor might be in cleaning mode, wait longer
          if (sps30_timeout_cnt++ > SPS30_ERROR_COUNT) { // check if number of retries exceeded, it should try at least for 10 secs
            stateSPS30 = HAS_ERROR; 
            errorRecSPS30 = currentTime + 12000; 
            success = false; 
            if (mySettings.debuglevel > 0)  { R_printSerialTelnetLogln(F("SPS30: data ready timeout")); }
            break;
          } // timeout count exceeded
          break; 
        } // end if data ready
      } // end if time
      break; 
    } // is BUSY

    case WAIT_STABLE: { //--------------------- system is stable, read data
      if (mySettings.debuglevel == 5) {
        R_printSerialTelnetLogln("SPS30: wait until stable");
        snprintf_P(tmpStr, sizeof(tmpStr), PSTR("SPS30: current time: %lu"),currentTime); 
        R_printSerialTelnetLogln(tmpStr);
        snprintf_P(tmpStr, sizeof(tmpStr), PSTR("SPS30: time when stable: %lu"), timeSPS30Stable); 
        R_printSerialTelnetLogln(tmpStr);
      }

      if (currentTime >= timeSPS30Stable) {
        D_printSerialTelnet(F("D:U:SPS30:WS.."));
        switchI2C(sps30_port, sps30_i2c[0], sps30_i2c[1], sps30_i2cspeed, sps30_i2cClockStretchLimit);

        //check if data available
        if ( sps30.read_data_ready(&sps30_data_ready) ) {
          if (mySettings.debuglevel == 5)  { R_printSerialTelnetLogln(F("SPS30: checked for data")); }
        } else { 
          if (mySettings.debuglevel > 0 )  { R_printSerialTelnetLogln(F("SPS30: error checking for data")); }
          stateSPS30 = HAS_ERROR; 
          errorRecSPS30 = currentTime + 12000; 
          break; 
        }

        if (sps30_data_ready) { 
          // read data
          if ( sps30.read_measurement(&valSPS30) ) { 
            if (mySettings.debuglevel >= 2)  { R_printSerialTelnetLogln(F("SPS30: data read")); }
            sps30NewData   = true;
            sps30NewDataWS = true;
            sps30_error_cnt = 0;
            sps30_timeout_cnt = 0;
            lastSPS30 = currentTime; 
            if (mySettings.debuglevel == 5)  { R_printSerialTelnetLogln(F("SPS30: going to idle")); }
            stateSPS30 = IS_IDLE;            
            break;
          } else {
            if (mySettings.debuglevel > 0 )  { R_printSerialTelnetLogln(F("SPS30: error reading data")); }
            stateSPS30 = HAS_ERROR; 
            errorRecSPS30 = currentTime + 12000; 
            break; 
          }
        } else {
          if (sps30_timeout_cnt == 0) { timeSPS30Stable = timeSPS30Stable + SPS30_TIMEOUT_DELAY; }    // retry in 100ms
          else {                        timeSPS30Stable = timeSPS30Stable + 10*SPS30_TIMEOUT_DELAY; } // sensor might be in cleaning mode, wait 1sec to retry
          // we need to take into account the autocleaning, SPS30_ERROR_COUNT * SPS30_TIMEOUT_DELAY > 10secs
          if (sps30_timeout_cnt++ > SPS30_ERROR_COUNT) { // check if number of retries exceeded
            stateSPS30 = HAS_ERROR; 
            errorRecSPS30 = currentTime + 12000; 
            success = false; 
            if (mySettings.debuglevel > 0)  { R_printSerialTelnetLogln(F("SPS30: data ready timeout")); }
            break;
          } // timeout count exceeded
          break; 
        }

        // obtain device status
        if ( ( (sps30.fw_major()==2) && (sps30.fw_minor()>=2) ) || (sps30.fw_major()>2) ) {
          if ( sps30.read_device_status_register(&sps30_st) ) { // takes 20ms
            if (mySettings.debuglevel >= 2) { R_printSerialTelnetLogln(F("SPS30: read status")); }
            if (sps30_st == SPS30_STATUS_OK) { 
              if (mySettings.debuglevel >= 2) { R_printSerialTelnetLogln(F("SPS30: ok")); }
            } else {
              if (mySettings.debuglevel > 0) {
                if (sps30_st & SPS30_STATUS_SPEED_ERROR) { R_printSerialTelnetLogln(F("SPS30: warning, fan is turning too fast or too slow"));  }
                if (sps30_st & SPS30_STATUS_LASER_ERROR) { R_printSerialTelnetLogln(F("SPS30: error laser failure")); }
                if (sps30_st & SPS30_STATUS_FAN_ERROR)   { R_printSerialTelnetLogln(F("SPS30: error fan failure, fan is mechanically blocked or broken")); }
              }
            } // status ok/notok
          } else {
            if (mySettings.debuglevel > 0)        { R_printSerialTelnetLogln(F("SPS30: error reading status")); } // read status
          }
        } // Firmware >= 2.2

      } //if time stable
      break;
    } // wait stable
     
    case IS_IDLE : { //--------------------- after reading data either go to sleep or go back to read again
      D_printSerialTelnet(F("D:U:SPS30:II.."));
      if (mySettings.debuglevel == 5) { R_printSerialTelnetLogln("SPS30: is idle"); }

      if ((fastMode) || (sps30.fw_major()<2) ) { // fastmode or no sleep function
        timeSPS30Stable = (unsigned long) (lastSPS30 + intervalSPS30); 
        if (mySettings.debuglevel == 5) {
          snprintf_P(tmpStr, sizeof(tmpStr), PSTR("SPS30: lastSPS30: %lu"), lastSPS30);
          R_printSerialTelnetLogln(tmpStr);
          snprintf_P(tmpStr, sizeof(tmpStr), PSTR("SPS30: current time:: %lu"), currentTime);
          printSerialTelnetLogln(tmpStr);
          snprintf_P(tmpStr, sizeof(tmpStr), PSTR("SPS30: time when stable: %lu"), timeSPS30Stable); 
          printSerialTelnetLogln(tmpStr);
          printSerialTelnetLogln(F("SPS30: going to wait until stable"));
        }
        sps30_timeout_cnt = 0;
        stateSPS30 = WAIT_STABLE;     

      } else {
        wakeTimeSPS30 = (unsigned long) (lastSPS30 + intervalSPS30 - 50 - timeToStableSPS30);
        switchI2C(sps30_port, sps30_i2c[0], sps30_i2c[1], sps30_i2cspeed, sps30_i2cClockStretchLimit);

        if ( sps30.stop_measurement() ) { // go to idle, takes 20ms
          if (mySettings.debuglevel >= 2) { printSerialTelnetLogln(F("SPS30: stopped measurement")); }
        } else {
          if (mySettings.debuglevel > 0)  { printSerialTelnetLogln(F("SPS30: error could not stop measurement")); }
          stateSPS30 = HAS_ERROR;
          errorRecSPS30 = currentTime + 12000;
          break;
        }

        if ( sps30.sleep() ) { // takes 5ms
          if (mySettings.debuglevel == 5) { printSerialTelnetLogln(F("SPS30: going to sleep")); }
          stateSPS30 = IS_SLEEPING;
        } else {
          if (mySettings.debuglevel > 0) { printSerialTelnetLogln(F("SPS30: error could not go to sleep")); }
          stateSPS30 = HAS_ERROR;
          errorRecSPS30 = currentTime + 12000;
          break;
        }
      }
      break;
    }

    case IS_SLEEPING : { //--------------------- in slow mode we put system to sleep if new sensor version
      if (mySettings.debuglevel == 5) { R_printSerialTelnetLogln(F("SPS30: is sleepig")); }

      if (currentTime >= wakeTimeSPS30) { // Wake up if sleep time exceeded
        D_printSerialTelnet(F("D:U:SPS30:IS.."));
        if (mySettings.debuglevel == 5) { R_printSerialTelnetLogln(F("SPS30: waking up")); }

        switchI2C(sps30_port, sps30_i2c[0], sps30_i2c[1], sps30_i2cspeed, sps30_i2cClockStretchLimit);
        if ( sps30.wake_up() ) { // takes 5ms
          wakeSPS30 = currentTime;
          stateSPS30 = IS_WAKINGUP;
        } else {
          if (mySettings.debuglevel > 0) { printSerialTelnetLogln(F("SPS30: error could not wakeup")); }
          stateSPS30 = HAS_ERROR; 
          errorRecSPS30 = currentTime + 12000;
          break;
        }

      } // time interval
      break; 
    } // case is sleeping
      
    case IS_WAKINGUP : {  // ------------------  startup the sensor
      if (mySettings.debuglevel == 5) { R_printSerialTelnetLogln(F("SPS30: is waking up")); }

      if ((currentTime - wakeSPS30) >= 50) { // Give some time (50ms)to wake up 
        D_printSerialTelnet(F("D:U:SPS30:IW.."));
        switchI2C(sps30_port, sps30_i2c[0], sps30_i2c[1], sps30_i2cspeed, sps30_i2cClockStretchLimit);
        
        if ( sps30.start_measurement() ) { 
          if (mySettings.debuglevel == 5) { R_printSerialTelnetLogln(F("SPS30: measurement started")); }
          stateSPS30 = IS_BUSY; 
          sps30_timeout_cnt = 0;
        } else {
          if (mySettings.debuglevel > 0) { R_printSerialTelnetLogln(F("SPS30: error could not start SPS30 measurements")); }
          stateSPS30 = HAS_ERROR; 
          errorRecSPS30 = currentTime + 12000;
          break;
        }
        lastSPS30 = currentTime;
      } // end current time 
      break;
    }

    case HAS_ERROR : { //--------------------  trying to recover sensor
      if (mySettings.debuglevel == 5) { R_printSerialTelnetLogln(F("SPS30: has error")); }
      if (currentTime > errorRecSPS30) {
        D_printSerialTelnet(F("D:U:SPS30:E.."));
        if (sps30_error_cnt++ > ERROR_COUNT) { 
          success = false; 
          sps30_avail = false; 
          if (mySettings.debuglevel > 0) { R_printSerialTelnetLogln(F("SPS30: reinitialization attempts exceeded, SPS30: no longer available")); }
          break;
        } // give up after ERROR_COUNT tries

        sps30_lastError = currentTime;

        if ( initializeSPS30() ) {
          sps30_error_cnt = 0;
          if (mySettings.debuglevel > 0) { R_printSerialTelnetLogln(F("SPS30: recovered")); }
        } else {
          if (mySettings.debuglevel > 0) { R_printSerialTelnetLogln(F("SPS30: could not recover")); }
          stateSPS30 = HAS_ERROR; 
          errorRecSPS30 = currentTime + 12000;
          break;
        }
      }
      break;
    }

    default: {
      if (mySettings.debuglevel > 0) { R_printSerialTelnetLogln(F("SPS30 Error: invalid switch statement")); } 
      break;
    }

  } // end cases

  return success;
}

/******************************************************************************************************/
// JSON SPS30
/******************************************************************************************************/
void sps30JSON(char *payLoad, size_t len){
  const char * str = "{ \"sps30\": ";
  size_t l = strlen(str);
  strlcpy(payLoad, str, l+1);
  sps30JSONMQTT(payLoad+l, len-l-1);
  strlcat(payLoad, "}", len);
}


void sps30JSONMQTT(char *payload, size_t len) {
  char qualityMessage1[16];
  char qualityMessage2[16];
  if (sps30_avail) { 
    checkPM2(valSPS30.mc_2p5, qualityMessage1, 15); 
    checkPM10(valSPS30.mc_10p0, qualityMessage2, 15);
  } else {
    strcpy(qualityMessage1, "not available");
    strcpy(qualityMessage2, "not available");
  } 
  snprintf_P(payload, len, PSTR("{ \"avail\": %s, \"PM1\": %4.1f, \"PM2\": %4.1f, \"PM4\": %4.1f, \"PM10\": %4.1f, \"nPM0\": %4.1f, \"nPM1\": %4.1f, \"nPM2\": %4.1f, \"nPM4\": %4.1f, \"nPM10\": %4.1f, \"PartSize\": %4.1f, \"PM2_airquality\": \"%s\", \"PM10_airquality\": \"%s\"}"), 
                       sps30_avail ? "true" : "false", 
                       sps30_avail ? valSPS30.mc_1p0  : -1.,
                       sps30_avail ? valSPS30.mc_2p5  : -1.,
                       sps30_avail ? valSPS30.mc_4p0  : -1.,
                       sps30_avail ? valSPS30.mc_10p0 : -1.,
                       sps30_avail ? valSPS30.nc_0p5  : -1.,
                       sps30_avail ? valSPS30.nc_1p0  : -1.,
                       sps30_avail ? valSPS30.nc_2p5  : -1.,
                       sps30_avail ? valSPS30.nc_4p0  : -1.,
                       sps30_avail ? valSPS30.nc_10p0 : -1.,
                       sps30_avail ? valSPS30.typical_particle_size : -1.,
                       qualityMessage1, 
                       qualityMessage2);
}
