/******************************************************************************************************/
// BME680, Pressure, Temperature, Humidity, Gas Resistance
/******************************************************************************************************/
// float bme680_pressure24hrs
// uint32_t gas_resistance
// float readTemperature()
// float readPressure()
// float readHumidity()
// uint32_t readGas()
// float readAltitude(float seaLevel)
  
#include "VSC.h"
#ifdef EDITVSC
#include "src/BME680.h"
#include "src/Sensi.h"
#include "src/Config.h"
#include "src/Quality.h"
#endif

bool initializeBME680() {

  switchI2C(bme680_port, bme680_i2c[0], bme680_i2c[1], bme680_i2cspeed, bme680_i2cClockStretchLimit);
  
  if (bme680.begin(0x77, true) == true) { 
    if (mySettings.debuglevel > 0) { R_printSerialTelnetLogln(F("BME680: setting oversampling for sensors")); }
    if (fastMode == true) { 
      intervalBME680 = intervalBME680Fast; 
      bme680.setHumidityOversampling(bme680_HumOversampleFast); 
      bme680.setTemperatureOversampling(bme680_TempOversampleFast);
      bme680.setPressureOversampling(bme680_PressureOversampleFast); 
      bme680.setIIRFilterSize(bme680_FilterSizeFast);
      // should setODR (output data rate)
      // dont know how to set low power and ultra low power mode
      if (mySettings.debuglevel > 0) { printSerialTelnetLogln(F("BME680: IIR filter set for fast measurements")); }
    } else { 
      intervalBME680 = intervalBME680Slow; 
      bme680.setHumidityOversampling(bme680_HumOversampleSlow); 
      bme680.setTemperatureOversampling(bme680_TempOversampleSlow);
      bme680.setPressureOversampling(bme680_PressureOversampleSlow); 
      bme680.setIIRFilterSize(bme680_FilterSizeSlow); 
      // should setODR (output data rate)
      // dont know how to set low power and ultra low power mode
      if (mySettings.debuglevel > 0) { printSerialTelnetLogln(F("BME680: IIR filter set for slow measurements")); }
    }

    bme680.setGasHeater(bme680_HeaterTemp,bme680_HeaterDuration); 
    if (mySettings.debuglevel > 0) { printSerialTelnetLogln(F("BME680: gas measurement set to 320\xC2\xB0\x43 for 150ms")); }

    BMEhum_avail = true;

    if (mySettings.debuglevel > 0) { printSerialTelnetLogln(F("BME680: initialized")); }

    tmpTime = millis();
    endTimeBME680 = bme680.beginReading(); // sensors tells us when its going to have new data
    lastBME680 = currentTime;

    if (endTimeBME680 == 0) { // if measurement was not started
      if (mySettings.debuglevel > 0) { printSerialTelnetLogln(F("BME680: failed to begin reading")); }
      stateBME680 = HAS_ERROR; 
      errorRecBME680 = currentTime + 5000;
      return(false);
    } else {
      unsigned long tmpInterval = endTimeBME680 - tmpTime;
      if (tmpInterval > intervalBME680) {intervalBME680 = tmpInterval;}
      stateBME680 = IS_BUSY; 
      if (mySettings.debuglevel == 9) { snprintf_P(tmpStr, sizeof(tmpStr), PSTR("BME680: reading started. Completes in %ldms"), tmpInterval); printSerialTelnetLogln(tmpStr); }

      // Construct poor man s lowpass filter y = (1-alpha) * y + alpha * x
      // https://dsp.stackexchange.com/questions/54086/single-pole-iir-low-pass-filter-which-is-the-correct-formula-for-the-decay-coe
      // filter cut off frequency is 1/day
      // float   f_s = 1.0 / (intervalBME680 / 1000.0);                     // = sampling frequency [1/s] 
      // float   f_c = (1.0 / (24.0 * 3600.0)) / f_s;                       // = normalzied cut off frequency = 1 per day / sampling frequency 
      // float   w_c = 2.0 * 3.141 * f_c;                                   // = cut off frequency in [radians / seconds]
      // float     y = 1 - cos(w_c);                                        // = alpha for 3dB attenuation at cut off frequency, cos is almost 1
      float   w_c = float(intervalBME680) * 7.27e-8;
      float     y =  w_c*w_c/2.;                                           // small angle approximation
      alphaBME680 = -y + sqrt( y*y + 2.*y );                               // is quite small e.g. 1e-6
    }

    if (mySettings.debuglevel > 0) { snprintf_P(tmpStr, sizeof(tmpStr), PSTR("BME680: interval: %lu"), intervalBME680); printSerialTelnetLog(tmpStr); }

  } else {
    if (mySettings.debuglevel > 0) { printSerialTelnetLogln(F("BME680: sensor not detected, please check wiring")); }
    stateBME680 = HAS_ERROR;
    errorRecBME680 = currentTime + 5000;
    return(false);
  }   

  if ((mySettings.avgP > 30000.0) && (mySettings.avgP <= 200000.0)) { bme680_pressure24hrs = mySettings.avgP; }
  delay(50); lastYield = millis();
  return(true);
}

/******************************************************************************************************/
// Update BME680
/******************************************************************************************************/
bool updateBME680() {
  bool success = true; // when ERROR recovery fails, success becomes false
  
  switch(stateBME680) { 
    
    case IS_IDLE : { //---------------------
      if ((currentTime - lastBME680) >= intervalBME680) {
        D_printSerialTelnet(F("D:U:BME680:II.."));
        switchI2C(bme680_port, bme680_i2c[0], bme680_i2c[1], bme680_i2cspeed, bme680_i2cClockStretchLimit);
        tmpTime = millis();          
        startMeasurementBME680 = millis();
        endTimeBME680 = bme680.beginReading(); // sensors tells us when its going to have new data
        lastBME680 = currentTime;
        if (endTimeBME680 == 0) { // if measurement was not started
          if (mySettings.debuglevel > 0) { R_printSerialTelnetLogln(F("BME680: failed to begin reading")); }
          stateBME680 = HAS_ERROR; 
          errorRecBME680 = currentTime + 5000;
        } else {
          stateBME680 = IS_BUSY; 
          // check if measurement time is longer than intervalBME680, if yes adjust interval and adjust lowpass filter
          unsigned long tmpInterval = endTimeBME680 - tmpTime;
          if (tmpInterval > intervalBME680) {
            intervalBME680 = tmpInterval;
            // Construct poor man s lowpass filter y = (1-alpha) * y + alpha * x
            // https://dsp.stackexchange.com/questions/54086/single-pole-iir-low-pass-filter-which-is-the-correct-formula-for-the-decay-coe
            // filter cut off frequency is 1/day
            // float   f_s = 1.0 / (intervalBME680 / 1000.0);                     // = sampling frequency [1/s] 
            // float   f_c = (1.0 / (24.0 * 3600.0)) / f_s;                       // = normalzied cut off frequency = 1 per day / sampling frequency 
            // float   w_c = 2.0 * 3.141 * f_c;                                   // = cut off frequency in [radians / seconds]
            // float     y = 1 - cos(w_c);                                        // = alpha for 3dB attenuation at cut off frequency, cos is almost 1
            float   w_c = float(intervalBME680) * 7.27e-8;
            float     y =  w_c*w_c/2.;                                           // small angle approximation
            alphaBME680 = -y + sqrt( y*y + 2.*y );                               // is quite small e.g. 1e-6
          }
          if (mySettings.debuglevel == 9) { snprintf_P(tmpStr, sizeof(tmpStr), PSTR("BME680: reading started. Completes in %ldms"), tmpInterval); printSerialTelnetLogln(tmpStr); }
        }
      }
      break;
    }
    
    case IS_BUSY : { //---------------------
      if (currentTime > endTimeBME680) {
        D_printSerialTelnet(F("D:U:BME680:IB.."));
        stateBME680 = DATA_AVAILABLE;
      }
      break;
    }

    case DATA_AVAILABLE : { //---------------------
      D_printSerialTelnet(F("D:U:BME680:DA.."));
      switchI2C(bme680_port, bme680_i2c[0], bme680_i2c[1], bme680_i2cspeed, bme680_i2cClockStretchLimit);
      if (bme680.endReading() ==  false) {
        if (mySettings.debuglevel > 0) { R_printSerialTelnetLogln(F("BME680: Failed to complete reading, timeout")); }
        stateBME680 = HAS_ERROR;
        errorRecBME680 = currentTime + 5000;
      } else {
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
        float tmp = 273.15 + bme680.temperature;
        bme680_ah = bme680.humidity * 13.246 / tmp * exp(19.854 - 5423.0/tmp); // [gr/m^3]
        if ( (bme680_ah<0) | (bme680_ah>40.0) ) { bme680_ah = -1.0; } // make sure its reasonable

        // NEED TO IMPLEMENT IF DEWPOINT is WANTED
        // DewPoint
        // https://en.wikipedia.org/wiki/Dew_point
        // T Celsius
        // a = 6.1121 mbar, b = 18.678, c = 257.14 °C, d = 234.5 °C.
        // Tdp = c g_m / (b - g_m)
        // g_m = ln(RH/100) + (b-T/d)*(T/(c+T)))
        
        if (bme680_pressure24hrs == 0.0) {bme680_pressure24hrs = bme680.pressure; } 
        else {bme680_pressure24hrs = (1.0-alphaBME680) * bme680_pressure24hrs + alphaBME680* bme680.pressure; }
        mySettings.avgP = bme680_pressure24hrs;
        if (mySettings.debuglevel >= 2) { snprintf_P(tmpStr, sizeof(tmpStr), PSTR("BME680: T, rH, P read in %ldms"), (millis()-startMeasurementBME680)); R_printSerialTelnetLogln(tmpStr); }
        bme680NewData = true;
        bme680NewDataWS = true;

        stateBME680 = IS_IDLE;
        bme680_error_cnt = 0;
      }
      break;
    }

    case HAS_ERROR : {
      if (currentTime > errorRecBME680) {
        D_printSerialTelnet(F("D:U:BME680:E.."));
        if (bme680_error_cnt++ > ERROR_COUNT) { 
          success = false; 
          bme680_avail = false;
          if (mySettings.debuglevel > 0) { R_printSerialTelnetLogln(F("BME680: reinitialization attempts exceeded, BME680: no longer available.")); }
          break; 
        } // give up after ERROR_COUNT tries
        bme680_lastError = currentTime;
        switchI2C(bme680_port, bme680_i2c[0], bme680_i2c[1], bme680_i2cspeed, bme680_i2cClockStretchLimit);
        if (bme680.begin(0x77, true) == true) { 
          if (fastMode == true) { 
            intervalBME680 = intervalBME680Fast; 
            bme680.setTemperatureOversampling(bme680_TempOversampleFast);
            bme680.setHumidityOversampling(bme680_HumOversampleFast); 
            bme680.setPressureOversampling(bme680_PressureOversampleFast); 
            bme680.setIIRFilterSize(bme680_FilterSizeFast); 
          } else { 
            intervalBME680 = intervalBME680Slow; 
            bme680.setTemperatureOversampling(bme680_TempOversampleSlow);
            bme680.setHumidityOversampling(bme680_HumOversampleSlow); 
            bme680.setPressureOversampling(bme680_PressureOversampleSlow); 
            bme680.setIIRFilterSize(bme680_FilterSizeSlow); 
          }
          bme680.setGasHeater(bme680_HeaterTemp,bme680_HeaterDuration); 
          BMEhum_avail = true;
        } else {
          if (mySettings.debuglevel > 0) { R_printSerialTelnetLogln(F("BME680: sensor not detected, please check wiring")); }
          stateBME680 = HAS_ERROR;
          errorRecBME680 = currentTime + 5000;
          break;
        }   
  
        tmpTime = millis();
        startMeasurementBME680 = millis();
        endTimeBME680 = bme680.beginReading(); // sensors tells us when its going to have new data
        lastBME680 = currentTime;
  
        if (endTimeBME680 == 0) { // if measurement was not started
          if (mySettings.debuglevel > 0) { R_printSerialTelnetLogln(F("BME680: failed to begin reading")); }
          stateBME680 = HAS_ERROR; 
          errorRecBME680 = currentTime + 5000;
        } else {
          unsigned long tmpInterval = endTimeBME680 - tmpTime;
          if (tmpInterval > intervalBME680) { 
            intervalBME680 = tmpInterval;
            // Construct poor man s lowpass filter y = (1-alpha) * y + alpha * x
            // https://dsp.stackexchange.com/questions/54086/single-pole-iir-low-pass-filter-which-is-the-correct-formula-for-the-decay-coe
            // filter cut off frequency is 1/day
            // float   f_s = 1.0 / (intervalBME680 / 1000.0);                     // = sampling frequency [1/s] 
            // float   f_c = (1.0 / (24.0 * 3600.0)) / f_s;                       // = normalzied cut off frequency = 1 per day / sampling frequency 
            // float   w_c = 2.0 * 3.141 * f_c;                                   // = cut off frequency in [radians / seconds]
            // float     y = 1 - cos(w_c);                                        // = alpha for 3dB attenuation at cut off frequency, cos is almost 1
            float   w_c = float(intervalBME680) * 7.27e-8;
            float     y =  w_c*w_c/2.;                                           // small angle approximation
            alphaBME680 = -y + sqrt( y*y + 2.*y );                               // is quite small e.g. 1e-6
          }
          stateBME680 = IS_BUSY; 
  
          if (mySettings.debuglevel == 9) { snprintf_P(tmpStr, sizeof(tmpStr), PSTR("BME680: reading started. Completes in %ldms"), tmpInterval); R_printSerialTelnetLogln(tmpStr); }
        }
  
        if (mySettings.debuglevel > 0) { R_printSerialTelnetLogln(F("BME680: re-initialized")); }
      }
      break; 
    }

    default: {if (mySettings.debuglevel > 0) { R_printSerialTelnetLogln(F("BME680 Error: invalid switch statement")); break;}}
   
  } // end cases
  
  return success;
}

void bme680JSON(char *payload, size_t len){
  // {"bme680":{"avail":true, "p":1234.5, "rH":12.3, "ah":123.4, "T":-25.1, "resistance":1234, "rH_airquality":"normal", "resistance_airquality":"normal"}}
  char qualityMessage1[16];
  char qualityMessage2[16];
  char qualityMessage3[16];
  char qualityMessage4[16];
  if (bme680_avail) { 
    checkdP((bme680.pressure-bme680_pressure24hrs)/100.0, qualityMessage1, 15);
    checkHumidity(bme680.humidity, qualityMessage2, 15);
    checkGasResistance(bme680.gas_resistance, qualityMessage3, 15); 
    checkAmbientTemperature(bme680.temperature, qualityMessage4, 15); 
  } else {
    strcpy(qualityMessage1, "not available");
    strcpy(qualityMessage2, "not available");
    strcpy(qualityMessage3, "not available");
    strcpy(qualityMessage4, "not available");
  }  
  snprintf_P(payload, len, PSTR("{ \"bme680\": { \"avail\": %s, \"p\": %5.1f, \"pavg\": %5.1f, \"rH\": %4.1f, \"aH\": %4.1f, \"T\": %5.1f, \"resistance\": %d, \"dp_airquality\": \"%s\", \"rH_airquality\": \"%s\", \"resistance_airquality\": \"%s\", \"T_airquality\": \"%s\"}}"), 
                       bme680_avail ? "true" : "false", 
                       bme680_avail ? bme680.pressure/100.0 : -1., 
                       bme680_avail ? bme680_pressure24hrs/100.0 : -1.0, 
                       bme680_avail ? bme680.humidity : -1., 
                       bme680_avail ? bme680_ah : -1.0, 
                       bme680_avail ? bme680.temperature: -999., 
                       bme680_avail ? bme680.gas_resistance : -1,
                       qualityMessage1, 
                       qualityMessage2,
                       qualityMessage3,
                       qualityMessage4);
}
