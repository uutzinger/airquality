/******************************************************************************************************/
// BME280, Pressure, Temperature [,Humidity]
/******************************************************************************************************/
//float bme280_pressure;
//float bme280_temp;
//float bme280_hum; 

#include "src/BME280.h"
#include "src/Sensi.h"
#include "src/Config.h"
#include "src/Quality.h"
#include "src/Print.h"

//////// ===================================================
float          bme280_pressure = -1.;                      // pressure from sensor
float          bme280_temp = -999.;                        // temperature from sensor
float          bme280_hum =-1.;                            // humidity from sensor
float          bme280_ah = -1.;                            // [gr/m^3]
float          bme280_pressure24hrs = 0.0;                 // average pressure last 24hrs
float          alphaBME280;                                // poorman's low pass filter for f_cutoff = 1 day
bool           bme280_avail = false;                       // do we hace the sensor?
bool           bme280NewData = false;                      // is there new data
bool           bme280NewDataHandeled = false;              // have we handeled the new data?
bool           bme280NewDataWS = false;                    // is there new data for websocket
long           bme280_measuretime = 0;                     // computed time it takes to complete the measurements and to finish standby
bool           BMEhum_avail = false;                       // no humidity sensor if we have BMP instead of BME
uint8_t        bme280_error_cnt = 0;                       // give a few retiries if error data length occurs while reading sensor values
uint8_t        bme280_i2c[2];                              // the pins for the i2c port, set during initialization
unsigned long  intervalBME280 = 0;                         // filled automatically during setup
unsigned long  lastBME280;                                 // last time we interacted with sensor
unsigned long  endTimeBME280;                              // when data will be available
unsigned long  errorRecBME280;
unsigned long  startMeasurementBME280;
unsigned long  bme280_lastError;

TwoWire       *bme280_port = 0;                            // pointer to the i2c port, might be useful for other microcontrollers
volatile       SensorStates stateBME280 = IS_IDLE;         // sensor state
BME280         bme280;                                     // the pressure sensor

// External Variables 
extern Settings      mySettings;   // Config
extern bool          fastMode;     // Sensi
extern bool          intervalNewData;
extern bool          availNewData;
extern unsigned long lastYield;    // Sensi
extern unsigned long currentTime;  // Sensi
extern char          tmpStr[256];       // Sensi

/******************************************************************************************************/
// Intialize BME280
/******************************************************************************************************/

bool initializeBME280() {

  switchI2C(bme280_port, bme280_i2c[0], bme280_i2c[1], bme280_i2cspeed, bme280_i2cClockStretchLimit);

  bme280.reset();
  bme280.settings.commInterface = I2C_MODE;
  bme280.settings.I2CAddress = 0x76;
  
  if (mySettings.debuglevel > 0) { R_printSerialTelnetLogln(F("BME280: setting oversampling for sensors")); }

  if (fastMode == true) { 
    intervalBME280                  = intervalBME280Fast;
    bme280.settings.runMode         = bme280_ModeFast;
    bme280.settings.tStandby        = bme280_StandbyTimeFast;
    bme280.settings.filter          = bme280_FilterFast;
    bme280.settings.tempOverSample  = bme280_TempOversampleFast; 
    bme280.settings.pressOverSample = bme280_PressureOversampleFast;
    bme280.settings.humidOverSample = bme280_HumOversampleFast;
  } else { 
    intervalBME280                  = intervalBME280Slow;
    bme280.settings.runMode         = bme280_ModeSlow;
    bme280.settings.tStandby        = bme280_StandbyTimeSlow;
    bme280.settings.filter          = bme280_FilterSlow;
    bme280.settings.tempOverSample  = bme280_TempOversampleSlow; 
    bme280.settings.pressOverSample = bme280_PressureOversampleSlow;
    bme280.settings.humidOverSample = bme280_HumOversampleSlow;
  }
  intervalNewData = true;
  
  if (mySettings.debuglevel >0) { snprintf_P(tmpStr, sizeof(tmpStr), PSTR("BM[E/P]280: interval: %lu"), intervalBME280); printSerialTelnetLogln(tmpStr); }
  
  uint8_t chipID=bme280.beginI2C(*bme280_port);
  if (chipID == 0x58) {
    BMEhum_avail = false;
    if (mySettings.debuglevel > 0) { printSerialTelnetLogln(F("BMP280: initialized")); }
  } else if(chipID == 0x60) {
    BMEhum_avail = true;
    if (mySettings.debuglevel > 0) { printSerialTelnetLogln(F("BME280: initialized")); }
  } else {
    BMEhum_avail = false;
    if (mySettings.debuglevel > 0) { printSerialTelnetLogln(F("BMx280: sensor chip ID neither matches BMP280 nor BME280")); }    
    stateBME280 = HAS_ERROR;
    errorRecBME280 = currentTime + 5000;
    return(false);
  }

  // Calculate the time it takes to complete a measurement cycle
  // -----------------------------------------------------------
  float tmp = 1.0;
  if      (bme280.settings.tempOverSample == 1) {tmp = tmp + 2.;}
  else if (bme280.settings.tempOverSample == 2) {tmp = tmp + 4.;}
  else if (bme280.settings.tempOverSample == 3) {tmp = tmp + 8.;}
  else if (bme280.settings.tempOverSample == 4) {tmp = tmp + 16.;}
  else if (bme280.settings.tempOverSample == 5) {tmp = tmp + 32.;}

  if      (bme280.settings.pressOverSample == 1) {tmp = tmp + 2.5;}
  else if (bme280.settings.pressOverSample == 2) {tmp = tmp + 4.5;}
  else if (bme280.settings.pressOverSample == 3) {tmp = tmp + 8.5;}
  else if (bme280.settings.pressOverSample == 4) {tmp = tmp + 16.5;}
  else if (bme280.settings.pressOverSample == 5) {tmp = tmp + 32.5;}

  if (BMEhum_avail) {
    if      (bme280.settings.humidOverSample == 1) {tmp = tmp + 2.5;}
    else if (bme280.settings.humidOverSample == 2) {tmp = tmp + 4.5;}
    else if (bme280.settings.humidOverSample == 3) {tmp = tmp + 8.5;}
    else if (bme280.settings.humidOverSample == 4) {tmp = tmp + 16.5;}
    else if (bme280.settings.humidOverSample == 5) {tmp = tmp + 32.5;}
  }

  if (bme280.settings.runMode == 3) {
    if      (bme280.settings.tStandby == 0) { tmp = tmp + 0.5; }
    else if (bme280.settings.tStandby == 1) { tmp = tmp + 62.5; }
    else if (bme280.settings.tStandby == 2) { tmp = tmp + 125.0; }
    else if (bme280.settings.tStandby == 3) { tmp = tmp + 250.0; }
    else if (bme280.settings.tStandby == 4) { tmp = tmp + 500.0; }
    else if (bme280.settings.tStandby == 5) { tmp = tmp + 1000.0; }
    else if (bme280.settings.tStandby == 6) { tmp = tmp + 10.0; }
    else if (bme280.settings.tStandby == 7) { tmp = tmp + 20.0; }
  }
  bme280_measuretime = long(tmp);

  // make sure we dont attempt reading faster than it takes the sensor to complete a reading
  if (bme280_measuretime > intervalBME280) {intervalBME280 = bme280_measuretime;}

  // Construct poor man's lowpass filter y = (1-alpha) * y + alpha * x
  // https://dsp.stackexchange.com/questions/54086/single-pole-iir-low-pass-filter-which-is-the-correct-formula-for-the-decay-coe
  // filter cut off frequency is 1/day, we want to know average pressure in a day
  // float   w_c = 2.0 * 3.141 / (24.0 * 3600.0);                        // = cut off frequency [radians / seconds]
  // float   f_s = 1.0 / (intervalBME280 / 1000.0);                      // = sampling frenecy [1/s] 
  //         w_c = w_c / f_s;                                            // = normalize cut off frequency [radians]
  // float     y = 1 - cos(w_c);                                         // = alpha for 3dB attenuation at cut off frequency, cos is almost 1
  float   w_c = float(intervalBME280) * 7.27e-8;
  float     y =  w_c*w_c/2.;                                          // small angle approximation of cosine
  alphaBME280 = -y + sqrt( y*y + 2.*y );                              // is quite small e.g. 1e-6

  if ((chipID == 0x58) || (chipID == 0x60))  {                        //
    if (bme280.settings.runMode == MODE_NORMAL) {                     // for normal mode we obtain readings periodically
        bme280.setMode(MODE_NORMAL);                                  //
        stateBME280 = IS_BUSY;                                        //
    } else if (bme280.settings.runMode == MODE_FORCED) {              // for forced mode we initiate reading manually
      bme280.setMode(MODE_SLEEP);                                     // sleep for now
      stateBME280 = IS_SLEEPING;                                      //
    } else if (bme280.settings.runMode == MODE_SLEEP){                //
       bme280.setMode(MODE_SLEEP);                                    //
       stateBME280 = IS_SLEEPING;                                     //
    }
  }

  if ((mySettings.avgP > 30000.0) && (mySettings.avgP <= 200000.0)) { bme280_pressure24hrs = mySettings.avgP; }
  
  if (mySettings.debuglevel > 0) { printSerialTelnetLogln(F("BME280: initialized")); }
  delay(50); lastYield = millis();
  return(true);

} // end bme280

/******************************************************************************************************/
// Update BME280
/******************************************************************************************************/

bool updateBME280() {
  bool success = true; // when ERROR recovery fails, success becomes false

  switch(stateBME280) { 

    case IS_SLEEPING: { // ------------------ Slow and Energy Saving Mode: Wait until time to start measurement
      if ((currentTime - lastBME280) >= intervalBME280) {
        D_printSerialTelnet(F("D:U:BME280:IS.."));
        switchI2C(bme280_port, bme280_i2c[0], bme280_i2c[1], bme280_i2cspeed, bme280_i2cClockStretchLimit);
        bme280.setMode(MODE_FORCED); // Start reading
        lastBME280 = currentTime;
        stateBME280 = IS_BUSY;
      }
      break;
    }

    case IS_BUSY: {  // ---------------------- Slow and Energy Saving Mode: Wait until measurement complete
      if ((currentTime - lastBME280) >= bme280_measuretime) { // wait until measurement completed
        D_printSerialTelnet(F("D:U:BME280:IB.."));
        switchI2C(bme280_port, bme280_i2c[0], bme280_i2c[1], bme280_i2cspeed, bme280_i2cClockStretchLimit);
        // check if measurement is actually completed, if not wait some longer
        if (bme280.isMeasuring() == true) { 
          if ((currentTime - lastBME280) >= INTERVAL_TIMEOUT_FACTOR * bme280_measuretime)
            if (mySettings.debuglevel > 0) { R_printSerialTelnetLogln(F("BM[E/P]280: failed to complete reading")); }
            stateBME280 = HAS_ERROR;
            errorRecBME280 = currentTime + 5000;
        }  else { 
          stateBME280 = DATA_AVAILABLE; 
        } // yes its completed
      }
      break;
    }
    
    case IS_MEASURING: { // ------------------ Fast and Accurate Mode: Wait until mesaurement complete
      if ((currentTime - lastBME280) >= intervalBME280) {
        D_printSerialTelnet(F("D:U:BME280:IM.."));
        stateBME280 = DATA_AVAILABLE;
      }
      break;
    }

    case DATA_AVAILABLE : { //--------------- Data is available either from slow or fast mode
      D_printSerialTelnet(F("D:U:BME280:DA.."));
      switchI2C(bme280_port, bme280_i2c[0], bme280_i2c[1], bme280_i2cspeed, bme280_i2cClockStretchLimit);
      startMeasurementBME280 = millis();
      bme280_temp     = bme280.readTempC();
      bme280_pressure = bme280.readFloatPressure();
      if (BMEhum_avail) { 
        bme280_hum = bme280.readFloatHumidity(); // relative humidity
        float tmp = 273.15 + bme280_temp; // calculate absolute humidity
        bme280_ah = bme280_hum * 13.246 / tmp * exp(19.854 - 5423.0/tmp); // [gr/m^3]
      } else {
        bme280_hum = -1.0;
        bme280_ah =-1.0;
      }
      if (mySettings.debuglevel >= 2) { snprintf_P(tmpStr, sizeof(tmpStr), PSTR("BM[E/P]280: T, P read in %ldms"), (millis()-startMeasurementBME280)); R_printSerialTelnetLogln(tmpStr); }
      bme280NewData = true;
      bme280NewDataWS = true;
      lastBME280 = currentTime;

      // update average daily pressure      
      if (bme280_pressure24hrs == 0.0) {bme280_pressure24hrs = bme280_pressure;} 
      else { bme280_pressure24hrs = (1.0-alphaBME280) * bme280_pressure24hrs + alphaBME280 * bme280_pressure; } 
      mySettings.avgP = bme280_pressure24hrs;

      if (fastMode) { stateBME280 = IS_MEASURING; }
      else {          stateBME280 = IS_SLEEPING; }
 
      bme280_error_cnt = 0;

      break;
    }

    case HAS_ERROR : {
      if (currentTime > errorRecBME280) {      
        D_printSerialTelnet(F("D:U:BME280:E.."));
        if (bme280_error_cnt++ > ERROR_COUNT) { 
          success = false; 
          bme280_avail = false;
          availNewData = true;
          if (mySettings.debuglevel > 0) { R_printSerialTelnetLogln(F("BME280: reinitialization attempts exceeded, BME280: no longer available")); }
          break; 
        } // give up after ERROR_COUNT tries
        bme280_lastError = currentTime;
        if (initializeBME280()) {        
          if (mySettings.debuglevel > 0) { R_printSerialTelnetLogln(F("BME280: recovered")); }
        }
      }
      break; 
    }

    default: {if (mySettings.debuglevel > 0) { R_printSerialTelnetLogln(F("BME280 Error: invalid switch statement")); break;}}
   
  } // end cases
  return success;
}

/******************************************************************************************************/
// JSON BME280
/******************************************************************************************************/

void bme280JSON(char *payLoad, size_t len){
  const char * str = "{ \"bme280\": ";
  size_t l = strlen(str);
  strlcpy(payLoad, str, l+1);
  bme280JSONMQTT(payLoad+l, len-l-1);
  strlcat(payLoad, "}", len);
}

void bme280JSONMQTT(char *payload, size_t len){
  //{"avail":true, "p":123.4, "pavg":1234.5, "rH":123.4,"aH":123.4,"T":-25.0,"dp_airquality":"normal", "rh_airquality":"normal"}
  // about 150 Ccharacters
  char qualityMessage1[16];
  char qualityMessage2[16];
  char qualityMessage3[16];
  if (bme280_avail) { 
    checkdP((bme280_pressure-bme280_pressure24hrs)/100.0, qualityMessage1, 15);
    checkHumidity(bme280_hum, qualityMessage2, 15);
    checkAmbientTemperature(bme280_temp, qualityMessage3, 15);
  } else {
    strcpy(qualityMessage1, "not available");
    strcpy(qualityMessage2, "not available");
    strcpy(qualityMessage3, "not available");
  }  
  snprintf_P(payload, len, PSTR("{ \"avail\": %s, \"p\": %5.1f, \"pavg\": %5.1f, \"rH\": %4.1f, \"aH\": %4.1f, \"T\": %5.2f, \"dp_airquality\": \"%s\", \"rH_airquality\": \"%s\", \"T_airquality\": \"%s\" }"), 
             bme280_avail ? "true" : "false", 
             bme280_avail ? bme280_pressure/100.0 : -1.0, 
             bme280_avail ? bme280_pressure24hrs/100.0 : -1.0 , 
             bme280_avail ? bme280_hum : -1.0, 
             bme280_avail ? bme280_ah : -1.0, 
             bme280_avail ? bme280_temp : -999.0, 
             qualityMessage1, 
             qualityMessage2,
             qualityMessage3);
}
