/******************************************************************************************************/
// BME68x, Pressure, Temperature, Humidity, Gas Resistance
/******************************************************************************************************/
// float bme68x.temperature;
// float bme68x.humidity 
// float bme68x.pressure
// float bme68x_pressure24hrs
// float bme68x.gas_resitance
// float bme68x_ah

#include "src/BME68x.h"
#include "src/Sensi.h"
#include "src/Config.h"
#include "src/Quality.h"
#include "src/Print.h"

bool           bme68x_avail = false;                        // do we hace the sensor?
bool           bme68xNewData = false;                       // do we have new data?
bool           bme68xNewDataWS = false;                     // do we have new data for websocket
uint8_t        bme68x_i2c[2];                               // the pins for the i2c port, set during initialization
uint8_t        bme68x_error_cnt = 0;                        // give a few retiries if error data length occurs while reading sensor values
unsigned long  intervalBME68x;                              // adjusted if sensor reports longer measurement time
unsigned long  lastBME68x;                                  // last time we interacted with sensor
unsigned long  endTimeBME68x;                               // when data will be available
unsigned long  errorRecBME68x;                              // when we plan to recover from the error
unsigned long  startMeasurementBME68x;                      // when we asked for data
unsigned long  bme68x_lastError;                            // when last error occured
float          bme68x_ah = -1.;                             // absolute humidity [gr/m^3]
float          bme68x_pressure24hrs = 0.0;                  // average pressure last 24hrs
float          alphaBME68x;                                 // poorman's low pass filter for f_cutoff = 1 day

volatile       SensorStates stateBME68x = IS_IDLE;          // sensor state
TwoWire        *bme68x_port = 0;                            // pointer to the i2c port
Bme68x         bme68xSensor;                                // the pressure airquality sensor
bme68xData     bme68x;                                      // global data structure for the sensor

// External Variables 
extern Settings      mySettings;   // Config
extern bool          fastMode;     // Sensi
extern bool          BMEhum_avail; // BME280
extern unsigned long lastYield;    // Sensi
extern unsigned long currentTime;  // Sensi
extern char          tmpStr[256];       // Sensi

/******************************************************************************************************/
// Intialize BME68x
/******************************************************************************************************/

bool initializeBME68x() {

  switchI2C(bme68x_port, bme68x_i2c[0], bme68x_i2c[1], bme68x_i2cspeed, bme68x_i2cClockStretchLimit);
  
  bme68xSensor.begin(0x77, *bme68x_port);

  if (bme68xSensor.checkStatus()) { 
    if (bme68xSensor.checkStatus() == BME68X_ERROR) {
      if (mySettings.debuglevel > 0) { 
        printSerialTelnetLog("BME68x: error: ");
        printSerialTelnetLogln(bme68xSensor.statusString());
        bme68x_avail = false; 
        return (false);
      } 
    } else if (bme68xSensor.checkStatus() == BME68X_WARNING) {
      if (mySettings.debuglevel > 0) { 
        printSerialTelnetLog("BME68x: warning: ");
        printSerialTelnetLogln(bme68xSensor.statusString());
      }
    }
  }

  if (mySettings.debuglevel > 0) { R_printSerialTelnetLogln(F("BME68x: setting oversampling for sensors")); }

  if (fastMode) { 
    intervalBME68x = intervalBME68xFast; 
    bme68xSensor.setTPH(bme68x_TempOversampleFast, bme68x_PressureOversampleFast, bme68x_HumOversampleFast );
    bme68xSensor.setFilter(bme68x_FilterSizeFast);
    if (mySettings.debuglevel > 0) { printSerialTelnetLogln(F("BME68x: IIR filter set for fast measurements")); }
  } else {
    intervalBME68x = intervalBME68xSlow; 
    bme68xSensor.setTPH(bme68x_TempOversampleSlow, bme68x_PressureOversampleSlow, bme68x_HumOversampleSlow );
    bme68xSensor.setFilter(bme68x_FilterSizeSlow);
    if (mySettings.debuglevel > 0) { printSerialTelnetLogln(F("BME68x: IIR filter set for slow measurements")); }
  }

  bme68xSensor.setHeaterProf(bme68x_HeaterTemp,bme68x_HeaterDuration); 
  if (mySettings.debuglevel > 0) { printSerialTelnetLogln(F("BME68x: gas measurement set")); }

  BMEhum_avail = true;
  if (mySettings.debuglevel > 0) { printSerialTelnetLogln(F("BME68x: initialized")); }
  stateBME68x = IS_IDLE; 

  if ((mySettings.avgP > 30000.0) && (mySettings.avgP <= 200000.0)) { bme68x_pressure24hrs = mySettings.avgP; }
  delay(50); lastYield = millis();
  return (true);
}

/******************************************************************************************************/
// Update BME68x
/******************************************************************************************************/

bool updateBME68x() {
  bool success = true; // when ERROR recovery fails, success becomes false
  
  switch(stateBME68x) { 
    
    case IS_SLEEPING: { // -----------------
      if ((currentTime - lastBME68x) >= intervalBME68x) {
        startMeasurementBME68x = millis();
        D_printSerialTelnet(F("D:U:BME68x:IS.."));
        if (startMeasurementsBME68x() == true) {
          stateBME68x = IS_BUSY; 
          lastBME68x = currentTime;
        } else {
          stateBME68x = HAS_ERROR; 
          errorRecBME68x = currentTime + 5000;
        }
      }
      break;
    }
    
    case IS_IDLE : { //---------------------
      if ((currentTime - lastBME68x) >= intervalBME68x) {
        startMeasurementBME68x = millis();
        D_printSerialTelnet(F("D:U:BME68x:II.."));  
        if (startMeasurementsBME68x() == true) {
          stateBME68x = IS_BUSY; 
          lastBME68x = currentTime;
        } else {
          stateBME68x = HAS_ERROR; 
          errorRecBME68x = currentTime + 5000;
        }
      }
      break;
    }
    
    case IS_BUSY : { //---------------------
      if (currentTime > endTimeBME68x) {
        D_printSerialTelnet(F("D:U:BME68x:IB.."));
        stateBME68x = DATA_AVAILABLE;
      }
      break;
    }

    case DATA_AVAILABLE : { //---------------------
      D_printSerialTelnet(F("D:U:BME68x:DA.."));
      if (readDataBME68x()== true) {
        bme68xNewData = true;
        bme68xNewDataWS = true;
        stateBME68x = IS_IDLE;
        bme68x_error_cnt = 0;
      } else {
        stateBME68x = HAS_ERROR;
        errorRecBME68x = currentTime + 5000;
      }
      break;
    }

    case HAS_ERROR : {
      if (currentTime > errorRecBME68x) {
        D_printSerialTelnet(F("D:U:BME68x:E.."));

        // did we exceed retry amount ?
        if (bme68x_error_cnt++ > ERROR_COUNT) { 
          success = false; 
          bme68x_avail = false;
          if (mySettings.debuglevel > 0) { R_printSerialTelnetLogln(F("BME68x: reinitialization attempts exceeded, BME68x: no longer available")); }
          break; 
        }

        bme68x_lastError = currentTime; // keep track for main loop

        // try reading again
        if (readDataBME68x() == true) {
          stateBME68x = IS_IDLE;
          bme68x_error_cnt = 0;
          if (mySettings.debuglevel > 0) { R_printSerialTelnetLogln(F("BME68x: recovered")); }
        } else {
          // reading again did not work, restart the sensor
          if (initializeBME68x() == true) {
            if (mySettings.debuglevel > 0) { R_printSerialTelnetLogln(F("BME68x: recovered")); }
          }
        }
      }
      break; 
    }

    default: {
      if (mySettings.debuglevel > 0) { R_printSerialTelnetLogln(F("BME68x Error: invalid switch statement")); }
      break;
    }
   
  } // end cases
  
  return (success);
}

bool startMeasurementsBME68x(){

  switchI2C(bme68x_port, bme68x_i2c[0], bme68x_i2c[1], bme68x_i2cspeed, bme68x_i2cClockStretchLimit);
  
  // BME68X_SLEEP_MODE, 
  // BME68X_FORCED_MODE, 
  // BME68X_PARALLEL_MODE, 
  // BME68X_SEQUENTIAL_MODE
  bme68xSensor.setOpMode(BME68X_FORCED_MODE);

  unsigned long tmpInterval = (((unsigned long)bme68xSensor.getMeasDur(BME68X_FORCED_MODE))/1000) + bme68x_HeaterDuration;

  endTimeBME68x = (millis() + tmpInterval + 1); // in milliseconds
      
  if (tmpInterval == 0) { 
    // measurement did not start
    if (mySettings.debuglevel > 0) { printSerialTelnetLogln(F("BME68x: failed to begin reading")); }
    return (false);
  } else {
    // started succesfully
    if (tmpInterval > intervalBME68x) {intervalBME68x = tmpInterval;}
    if (mySettings.debuglevel == 9) { 
      snprintf_P(tmpStr, sizeof(tmpStr), PSTR("BME68x: reading started. Completes in %ldms"), tmpInterval); printSerialTelnetLogln(tmpStr);
      snprintf_P(tmpStr, sizeof(tmpStr), PSTR("BME68x: interval: %lums"), intervalBME68x); printSerialTelnetLogln(tmpStr); 
    }
    // Construct poor man s lowpass filter y = (1-alpha) * y + alpha * x
    // https://dsp.stackexchange.com/questions/54086/single-pole-iir-low-pass-filter-which-is-the-correct-formula-for-the-decay-coe
    // filter cut off frequency is 1/day
    // f_s = 1.0 / (intervalBME68x / 1000.0);    // = sampling frequency [1/s] 
    // f_c = (1.0 / (24.0 * 3600.0)) / f_s;      // = normalzied cut off frequency = 1 per day / sampling frequency 
    // w_c = 2.0 * 3.141 * f_c;                  // = cut off frequency in [radians / seconds]
    // y = 1 - cos(w_c);                         // = alpha for 3dB attenuation at cut off frequency, cos is almost 1
    float   w_c = float(intervalBME68x) * 7.27e-8;
    float     y =  w_c*w_c/2.;                 // small angle approximation
    alphaBME68x = -y + sqrt( y*y + 2.*y );     // is quite small e.g. 1e-6
  }
  return (true);
}

bool readDataBME68x() {
    
  switchI2C(bme68x_port, bme68x_i2c[0], bme68x_i2c[1], bme68x_i2cspeed, bme68x_i2cClockStretchLimit);

  if (bme68xSensor.fetchData()==false) {     
    if (mySettings.debuglevel > 0) { R_printSerialTelnetLogln(F("BME68x: Failed to complete reading")); }
    return (false);
  } else {
    bme68xSensor.getData(bme68x);
    // float bme68x.temperature in C
    // float bme68x.pressure in Pascal
    // float bme68x.humdity in %
    // float bme68x.gas_resitance in Ohms

    // Absolute Humidity
    // https://www.eoas.ubc.ca/books/Practical_Meteorology/prmet102/Ch04-watervapor-v102b.pdf
    //
    // 1) Input: Temperature -> Water Saturtion Vapor Pressure
    // 2) Input: Relative Humidity, Saturation Vapor Pressure -> Mass/Volume Ratio
    // 
    // T [K]
    // T0 = 273.15 [K]
    // Rv = R*/Mv = 461.5 J K-1 kg-1
    // L= 2.83*10^6 J/kg
    // Saturation Vapor Pressure: es = 611.3 exp (L/Rv *(1/T0 - 1/T))) [P]
    //                               = 611.3 exp (5423 [K] *(1/273.15[K] - 1/T[K]))) [P]
    // Relative Humidity = RH [%] = e/es * 100
    // Absolute Humidity = mass/volume = rho_v 
    // = e / (Rv * T)
    // = RH / 100 * 611.3 / (461.5 * T) * exp(5423 (1/273.15 - 1/T)) [kgm^-3]
    float tmp = 273.15 + bme68x.temperature;
    bme68x_ah = bme68x.humidity * 13.246 / tmp * exp(19.854 - 5423.0/tmp); // [gr/m^3]
    if ( (bme68x_ah<0) | (bme68x_ah>40.0) ) { bme68x_ah = -1.0; } // make sure its reasonable

    // NEED TO IMPLEMENT IF DEWPOINT is WANTED
    // DewPoint
    // https://en.wikipedia.org/wiki/Dew_point
    // T Celsius
    // a = 6.1121 mbar, b = 18.678, c = 257.14 °C, d = 234.5 °C.
    // Tdp = c g_m / (b - g_m)
    // g_m = ln(RH/100) + (b-T/d)*(T/(c+T)))

    if (bme68x_pressure24hrs == 0.0) {bme68x_pressure24hrs = bme68x.pressure; } 
    else                             {bme68x_pressure24hrs = (1.0-alphaBME68x) * bme68x_pressure24hrs + alphaBME68x * bme68x.pressure; }
    mySettings.avgP = bme68x_pressure24hrs;
    
    if (mySettings.debuglevel >= 2) { snprintf_P(tmpStr, sizeof(tmpStr), PSTR("BME68x: T, rH, P read in %ldms"), (millis()-startMeasurementBME68x)); R_printSerialTelnetLogln(tmpStr); }
    return (true);
  }
}

/******************************************************************************************************/
// JSON BME68x
/******************************************************************************************************/

void bme68xJSON(char *payLoad, size_t len){
  const char * str = "{ \"bme68x\": ";
  size_t l = strlen(str);
  strlcpy(payLoad, str, l+1);
  bme68xJSONMQTT(payLoad+l, len-l-1);
  strlcat(payLoad, "}", len);
}

void bme68xJSONMQTT(char *payload, size_t len){
  // { "avail":true, "p":1234.5, "rH":12.3, "ah":123.4, "T":-25.1, "resistance":1234, "rH_airquality":"normal", "resistance_airquality":"normal"}
  char qualityMessage1[16];
  char qualityMessage2[16];
  char qualityMessage3[16];
  char qualityMessage4[16];
  if (bme68x_avail) { 
    checkdP((bme68x.pressure-bme68x_pressure24hrs)/100.0, qualityMessage1, 15);
    checkHumidity(bme68x.humidity,                        qualityMessage2, 15);
    checkGasResistance(bme68x.gas_resistance,             qualityMessage3, 15); 
    checkAmbientTemperature(bme68x.temperature,           qualityMessage4, 15); 
  } else {
    strcpy(qualityMessage1, "not available");
    strcpy(qualityMessage2, "not available");
    strcpy(qualityMessage3, "not available");
    strcpy(qualityMessage4, "not available");
  }  
  snprintf_P(payload, len, PSTR("{ \"avail\": %s, \"p\": %5.1f, \"pavg\": %5.1f, \"rH\": %4.1f, \"aH\": %4.1f, \"T\": %5.2f, \"resistance\": %6.0f, \"dp_airquality\": \"%s\", \"rH_airquality\": \"%s\", \"resistance_airquality\": \"%s\", \"T_airquality\": \"%s\"}"), 
             bme68x_avail ? "true" : "false", 
             bme68x_avail ? bme68x.pressure/100.0 : -1., 
             bme68x_avail ? bme68x_pressure24hrs/100.0 : -1.0, 
             bme68x_avail ? bme68x.humidity : -1., 
             bme68x_avail ? bme68x_ah : -1.0, 
             bme68x_avail ? bme68x.temperature: -999., 
             bme68x_avail ? bme68x.gas_resistance : -1.,
             qualityMessage1, 
             qualityMessage2,
             qualityMessage3,
             qualityMessage4);
}
