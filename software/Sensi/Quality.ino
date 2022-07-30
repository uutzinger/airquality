/******************************************************************************************************/
// Checks values to expected range and provides assessment
// Longet returned message is 15 characters long
/******************************************************************************************************/
#include "VSC.h"
#ifdef EDITVSC
#include "src/Quality.h"
#include "src/Config.h"
#include "src/Sensi.h"
#include "src/BME280.h"
#include "src/BME680.h"
#include "src/CCS811.h"
#include "src/MLX.h"
#include "src/SCD30.h"
#include "src/SGP30.h"
#include "src/SPS30.h"
#endif

bool checkCO2(float co2, char *message, int len) {
  // Space station CO2 is about 2000ppbm
  // Global outdoor CO2 is well published
  // Indoor: https://www.dhs.wisconsin.gov/chemical/carbondioxide.htm
  bool ok = true;
  if (len > 0) {  
    if ( (co2 < 0.0) || co2 > 2.0e9 ) { // outside reasonable values
      strcpy(message, "?");
    } else if (len == 1) {
      if (co2 < 1000.)      { strncpy_P(message, normal1,    len); }                  
      else if (co2 < 2000.) { strncpy_P(message, threshold1, len); }
      else if (co2 < 5000.) { strncpy_P(message, poor1,      len); ok = false; } 
      else                  { strncpy_P(message, excessive1, len); ok = false; }
  
    } else if (len <= 4) {
      if (co2 < 1000.)      { strncpy_P(message, normal4,    len); }
      else if (co2 < 2000.) { strncpy_P(message, threshold4, len); }
      else if (co2 < 5000.) { strncpy_P(message, poor4,      len); ok = false; }
      else                  { strncpy_P(message, excessive4, len); ok = false; }
  
    } else {
      if (co2 < 1000.)      { strncpy_P(message, normalF,    len); }
      else if (co2 < 2000.) { strncpy_P(message, thresholdF, len); }
      else if (co2 < 5000.) { strncpy_P(message, poorF,      len); ok = false; }
      else                  { strncpy_P(message, excessiveF, len); ok = false; }
    }
    message[len] = '\0';
  } else {
    if (co2 >= 5000.) { ok = false; }
  }
  return ok;
}

bool checkHumidity(float rH, char *message, int len) {
  // Mayo clinic 30-50% is normal
  // Wiki 50-60%
  // Recommended indoor with AC 30-60%
  // Potential lower virus transmission at lower humidity
  // Humidity > 60% viruses thrive
  // Below 40% is considered dry
  // 15-25% humidity affect tear film on eye
  // below 20% extreme
  bool ok = true;
  if (len > 0) {  
    if ((rH<0.0) || (rH>200.0)) { // outside reasonable values
      strcpy(message, "?");
    } else if (len == 1) {
      if ((rH >= 45.) && (rH <= 55.))     { strncpy_P(message, normal1,        len); }
      else if ((rH >= 25.) && (rH < 45.)) { strncpy_P(message, thresholdLow1,  len); }
      else if ((rH >= 55.) && (rH < 65.)) { strncpy_P(message, thresholdHigh1, len); }
      else if ((rH >= 65.) && (rH < 80.)) { strncpy_P(message, high1,          len); }
      else if ((rH >  15.) && (rH < 25.)) { strncpy_P(message, low1,           len); }
      else                                { strncpy_P(message, excessive1,     len); ok = false; }
      
    } else if (len <= 4) {
      if ((rH >= 45.) && (rH <= 55.))     { strncpy_P(message, normal4,        len); }
      else if ((rH >= 25.) && (rH < 45.)) { strncpy_P(message, thresholdLow4,  len); }
      else if ((rH >= 55.) && (rH < 65.)) { strncpy_P(message, thresholdHigh4, len); }
      else if ((rH >= 65.) && (rH < 80.)) { strncpy_P(message, high4,          len); }
      else if ((rH >  15.) && (rH < 25.)) { strncpy_P(message, low4,           len); }
      else                                { strncpy_P(message, excessive4,     len); ok = false; }
  
    } else {
      if ((rH >= 45.) && (rH <= 55.))     { strncpy_P(message, normalF,        len); }
      else if ((rH >= 25.) && (rH < 45.)) { strncpy_P(message, thresholdLowF,  len); }
      else if ((rH >= 55.) && (rH < 65.)) { strncpy_P(message, thresholdHighF, len); }
      else if ((rH >= 65.) && (rH < 80.)) { strncpy_P(message, highF,          len); }
      else if ((rH >  15.) && (rH < 25.)) { strncpy_P(message, lowF,           len); }
      else                                { strncpy_P(message, excessiveF,     len); ok = false; }
    }
    message[len] = '\0';
  } else {
    if ((rH <= 15.) && (rH >= 80.)) { ok = false; }
  }
  return ok;
}

bool checkGasResistance(uint32_t res, char *message, int len) {
  // The airquality index caculation of the Bosch sensor is proprietary and only available as precompiled library.
  // Direct assessment of resistance and association to airquality is not well documented.
  // 521177 - 431331 - good ?
  // 297625 - 213212 - average ?
  // 148977 - 108042 - little bad ?
  //  75010 -  54586 - bad ?
  //  37395 -  27080 - worse ?
  //  18761 -  13591 - very bad ?
  //   9008 -   8371 - can’t see the exit ?

  bool ok = true;
  if (len > 0) {  
    if (res == 0) {
      strcpy(message, "?");
    } else if (len == 1) {
      if      (res > 250000) { strncpy_P(message, normal1,    len); }
      else if (res > 110000) { strncpy_P(message, threshold1, len); }
      else if (res >  55000) { strncpy_P(message, poor1,      len); }
      else                   { strncpy_P(message, excessive1, len); ok = false; }
    
    } else if (len <= 4) {
      if      (res > 250000) { strncpy_P(message, normal4,    len); }
      else if (res > 110000) { strncpy_P(message, threshold4, len); }
      else if (res >  55000) { strncpy_P(message, poor4,      len); }
      else                   { strncpy_P(message, excessive4, len); ok = false; }
    
    } else {
      if      (res > 250000) { strncpy_P(message, normalF,    len ); }
      else if (res > 110000) { strncpy_P(message, thresholdF, len ); }
      else if (res >  55000) { strncpy_P(message, poorF,      len ); }
      else                   { strncpy_P(message, excessiveF, len );  ok = false; }
    }
    message[len] = '\0';
  } else {
    if (res <= 55000) { ok = false; }    
  }
  return ok;
}


bool checkTVOC(float tVOC, char *message, int len) {
  // usually measured in parts per billion
  bool ok = true;
  if (len > 0) {  
    if ( (tVOC<0.0) || (tVOC> 2.0e9) ) {
      strcpy(message, "?");
    } else if (len == 1) {
      if (tVOC < 220.)       { strncpy_P(message, normal1,    len); }
      else if (tVOC < 660.)  { strncpy_P(message, threshold1, len); }
      else if (tVOC < 2200.) { strncpy_P(message, poor1,      len); }
      else                   { strncpy_P(message, excessive1, len); ok = false;}
  
    } else if (len <= 4) {
      if (tVOC < 220.)       { strncpy_P(message, normal4, len); }
      else if (tVOC < 660.)  { strncpy_P(message, threshold4, len); }
      else if (tVOC < 2200.) { strncpy_P(message, poor4, len); }
      else                   { strncpy_P(message, excessive4, len); ok = false; }
  
    } else {
      if (tVOC < 220.)       { strncpy_P(message, normalF,    len); }
      else if (tVOC < 660.)  { strncpy_P(message, thresholdF, len); }
      else if (tVOC < 2200.) { strncpy_P(message, poorF,      len); }
      else                   { strncpy_P(message, excessiveF, len); ok = false;}
    }
    message[len] = '\0';
  } else {
    if (tVOC >= 2200.) { ok = false; }
  }
  return ok;
}

bool checkPM2(float PM2, char *message, int len) {
  // some references would be useful here
  // I did not implement daily averages
  // measure in ug/m3 and #/cm3
  // in underground coal mine max allowed is 1.5 mg/m3 
  bool ok = true;
  if (len > 0) {  
    if ( (PM2 < 0.0) || (PM2 > 100000.0) ) {
       strcpy(message, "?");
    } else if (len == 1) {
      if (PM2 < 10.0)      { strncpy_P(message, normal1,    len); }
      else if (PM2 < 25.0) { strncpy_P(message, threshold1, len); }
      else if (PM2 < 65.0) { strncpy_P(message, poor1,      len); }
      else                 { strncpy_P(message, excessive1, len); ok = false; }
  
    } else if (len <= 4) {
      if (PM2 < 10.0)      { strncpy_P(message, normal4,    len); }
      else if (PM2 < 25.0) { strncpy_P(message, threshold4, len); }
      else if (PM2 < 65.0) { strncpy_P(message, poor4,      len); }
      else                 { strncpy_P(message, excessive4, len); ok = false; }
  
    } else {
      if (PM2 < 10.0)      { strncpy_P(message, normalF,    len); }
      else if (PM2 < 25.0) { strncpy_P(message, thresholdF, len); }
      else if (PM2 < 65.0) { strncpy_P(message, poorF,      len); }
      else                 { strncpy_P(message, excessiveF, len); ok = false; }
    }
    message[len] = '\0';
  } else {
      if (PM2 >= 65.0) { ok = false; }
  }
  return ok;
}

bool checkPM10(float PM10, char *message, int len) {
  // some references would be useful here
  // I did not implement daily averages
  bool ok = true;
  if (len > 0) {  
    if ( (PM10 < 0.0) || (PM10 > 100000.0) ) {
       strcpy(message, "?");
    } else if (len == 1) {
      if (PM10 < 20.0)       { strncpy_P(message, normal1,    len); }
      else if (PM10 < 50.0)  { strncpy_P(message, threshold1, len); }
      else if (PM10 < 150.0) { strncpy_P(message, poor1,      len); }
      else                   { strncpy_P(message, excessive1, len);  ok = false; }
  
    } else if (len <= 4) {
      if (PM10 < 20.0)       { strncpy_P(message, normal4,    len); }
      else if (PM10 < 50.0)  { strncpy_P(message, threshold4, len); }
      else if (PM10 < 150.0) { strncpy_P(message, poor4,      len); }
      else                   { strncpy_P(message, excessive4, len); ok = false; }
  
    } else {
      if (PM10 < 20.0)       { strncpy_P(message, normalF,    len); }
      else if (PM10 < 50.0)  { strncpy_P(message, thresholdF, len); }
      else if (PM10 < 150.0) { strncpy_P(message, poorF,      len); }
      else                   { strncpy_P(message, excessiveF, len);  ok = false; }
    }
    message[len] = '\0';
  } else {
    if (PM10 >= 150.0) { ok = false; }
  }
  return ok;
}

bool checkPM(float PM2, float PM10,  char *message, int len) {
  bool ok = true;
  if (len > 0) {  
    if ( (PM10 < 0.0) || (PM10 > 100000.0) || (PM2 < 0.0) || (PM2 > 100000.0) ) {
       strcpy(message, "?");
    } else if (len == 1) {
      if ((PM2 < 10.0) && (PM10 < 20.0))       { strncpy_P(message, normal1,    len); }
      else if ((PM2 < 25.0) && (PM10 < 50.0))  { strncpy_P(message, threshold1, len); }
      else if ((PM2 < 65.0) && (PM10 < 150.0)) { strncpy_P(message, poor1,      len); }
      else                                     { strncpy_P(message, excessive1, len); ok = false; }
  
    } else if (len <= 4) {   
      if      ((PM2 < 10.0) && (PM10 < 20.0))  { strncpy_P(message, normal4,    len); }
      else if ((PM2 < 25.0) && (PM10 < 50.0))  { strncpy_P(message, threshold4, len); }
      else if ((PM2 < 65.0) && (PM10 < 150.0)) { strncpy_P(message, poor4,      len); }
      else                                     { strncpy_P(message, excessive4, len);  ok = false; }
  
    } else {
      if      ((PM2 < 10.0) && (PM10 < 20.0))  { strncpy_P(message, normalF,    len); }
      else if ((PM2 < 25.0) && (PM10 < 50.0))  { strncpy_P(message, thresholdF, len); }
      else if ((PM2 < 65.0) && (PM10 < 150.0)) { strncpy_P(message, poorF,      len); }
      else                                     { strncpy_P(message, excessiveF, len); ok = false; }
    }
    message[len] = '\0';
  } else {
    if ((PM2 >= 65.0) || (PM10 >= 150.0)) { ok = false; }
  }
  return ok;
}

bool checkFever(float T, char *message, int len) {
  // While there are many resources on the internet to reference fever
  // its more difficult to associate forehead temperature with fever
  // That is because forhead sensors are not very reliable and
  // The conversion from temperature measured on different places of the body
  // to body core temperature is not well established.
  // It can be assumed that measurement of ear drum is likely closest to brain tempreature which is best circulated organ.
  //
  // https://www.singlecare.com/blog/fever-temperature/
  // https://www.hopkinsmedicine.org/health/conditions-and-diseases/fever
  // https://www.ncbi.nlm.nih.gov/pmc/articles/PMC7115295/
  bool ok = true;
  if (len > 0) {
    if ( (T<-273.15) || (T>2000.0) ) {
      strcpy(message, "?");
    } else if (len == 1) {
      if (T < 35.0)       { strncpy_P(message, low1,            len); ok = false;}
      else if (T <= 36.4) { strncpy_P(message, thresholdLow1,   len); ok = false;}
      else if (T <  37.2) { strncpy_P(message, normal1,         len);            }
      else if (T <  38.3) { strncpy_P(message, thresholdHigh1,  len); ok = false;}
      else if (T <  41.5) { strncpy_P(message, fever1,          len); ok = false;}
      else                { strncpy_P(message, excessiveFever1, len); ok = false;}
  
    } else if (len <= 4) {
      if (T < 35.0)       { strncpy_P(message, low4,            len); ok = false;}
      else if (T <= 36.4) { strncpy_P(message, thresholdLow4,   len); ok = false;}
      else if (T <  37.2) { strncpy_P(message, normal4,         len);            }
      else if (T <  38.3) { strncpy_P(message, thresholdHigh4,  len); ok = false;}
      else if (T <  41.5) { strncpy_P(message, fever4,          len); ok = false;}
      else                { strncpy_P(message, excessiveFever4, len); ok = false;}
  
    } else {
      if (T < 35.0)       { strncpy_P(message, lowF,            len); ok = false;}
      else if (T <= 36.4) { strncpy_P(message, thresholdLowF,   len); ok = false;}
      else if (T <  37.2) { strncpy_P(message, normalF,         len);            }
      else if (T <  38.3) { strncpy_P(message, thresholdHighF,  len); ok = false;}
      else if (T <  41.5) { strncpy_P(message, feverF,          len); ok = false;}
      else                { strncpy_P(message, excessiveFeverF, len); ok = false;}
    }
    message[len] = '\0';
  } else {
      if (T <= 36.4)       { ok = false; }
      else if (T >=  37.2) { ok = false; }
  }
  return ok;
}

bool checkAmbientTemperature(float T, char *message, int len) {
  bool ok = true;
  if (len > 0) {
    if ( (T<-100.0) || (T>100.0) ) {
      // coldest temp on earth is -95C
      // hottest temp on earth is 57C
      // sauna goes up to 90C
      // 20-25.5 normal
      strcpy(message, "?");
    } else if (len == 1) {
      if (T < 0.0)        { strncpy_P(message, cold1,    len); }
      else if (T <= 20.0) { strncpy_P(message, coldish1, len); }
      else if (T <= 25.5) { strncpy_P(message, normal1,  len); }
      else if (T <= 40.0) { strncpy_P(message, warm1,    len); }
      else                { strncpy_P(message, hot1,     len); }
  
    } else if (len <= 4) {
      if (T <= 0.0)       { strncpy_P(message, cold4,    len);  }
      else if (T <= 20.5) { strncpy_P(message, coldish4, len);  }
      else if (T <= 25.5) { strncpy_P(message, normal4,  len);  }
      else if (T <= 40.0) { strncpy_P(message, warm4,    len);  }
      else                { strncpy_P(message, hot4,     len); }
  
    } else {
      if (T < 20.0)       { strncpy_P(message, coldF,    len); }
      else if (T <= 20.0) { strncpy_P(message, coldishF, len); }
      else if (T <= 25.5) { strncpy_P(message, normalF,  len); }
      else if (T <= 40.0) { strncpy_P(message, warmF,    len); }
      else                { strncpy_P(message, hotF,     len); }
    }
    message[len] = '\0';
  } else {
  //     
  }
  return ok;
}

bool checkdP(float dP, char *message, int len) {
  bool ok = true;
  if (len > 0) {
    if ( (dP<-10000.0) || (dP>10000.0) ){
      // maximum pressure for human being is 2500mbar
      strcpy(message, "?");
    } else  if (len == 1) {
      if  (dP >= 5.0)      { strncpy_P(message, high1,   len); ok = false; }
      else if (dP <= -5.0) { strncpy_P(message, low1,    len); ok = false; }
      else                 { strncpy_P(message, normal1, len); }
  
    } else if (len <= 4) {
      if (dP >= 5.0)       { strncpy_P(message, high4,   len); ok = false; }
      else if (dP <= -5.0) { strncpy_P(message, low4,    len); ok = false; }
      else                 { strncpy_P(message, normal4, len); }
  
    } else {
      if (dP >= 5.0)       { strncpy_P(message, highF,   len); ok = false; }
      else if (dP <= -5.0) { strncpy_P(message, lowF,    len); ok = false; }
      else                 { strncpy_P(message, normalF, len); }
    }
    message[len] = '\0';
  } else {
      if (dP >= 5.0)       { ok = false; }
      else if (dP <= -5.0) { ok = false; }    
  }
  return ok;
}

bool sensorsWarning(void) {  
  char qualityMessage[2];
  bool ok = true;
  // Check CO2
  // Check Humidity
  // Check Ambient
  // Check tVOC
  // Check Particle

  // Check CO2
  if (scd30_avail && mySettings.useSCD30)  { 
    if ( checkCO2(float(scd30_ppm), qualityMessage, 0) == false )           { ok = false; }
  } else if (sgp30_avail    && mySettings.useSGP30)  { 
      if ( checkCO2(float(scd30_ppm), qualityMessage, 0) == false )         { ok = false; }
  } else if (ccs811_avail   && mySettings.useCCS811) { 
    if ( checkCO2(ccs811.getCO2(), qualityMessage, 0) == false )            { ok = false; }
  }
  
  // Check Particle
  if (sps30_avail && mySettings.useSPS30)  { 
    if ( checkPM2(valSPS30.MassPM2, qualityMessage, 0) == false)            { ok = false; }
    if ( checkPM10(valSPS30.MassPM10, qualityMessage, 0) == false)          { ok = false; }
  } 

  // Check tVOC
  if (ccs811_avail && mySettings.useCCS811) { 
    if ( checkTVOC(ccs811.getTVOC(), qualityMessage, 0) == false )          { ok = false; }
  } else if (sgp30_avail && mySettings.useSGP30) {
    if ( checkTVOC(sgp30.TVOC, qualityMessage, 0) == false )                { ok = false; }
  }

  // Check Humidity
  if (bme280_avail && mySettings.useBME280) { 
    if ( checkHumidity(bme280_hum, qualityMessage, 0) == false)             { ok = false; }
  } else if (bme680_avail && mySettings.useBME680) { 
    if ( checkHumidity(bme680.humidity, qualityMessage, 0) == false)        { ok = false; }
  } else if (scd30_avail && mySettings.useSCD30)  { 
    if ( checkHumidity(scd30_hum, qualityMessage, 0) == false )             { ok = false; }
  }

  // Check dP
  if (bme280_avail && mySettings.useBME280) { 
    if ( checkdP((bme280_pressure-bme280_pressure24hrs)/100.0, qualityMessage, 0) == false) { ok = false; }
  } else if (bme680_avail && mySettings.useBME680) { 
    if ( checkdP((bme680.pressure-bme680_pressure24hrs)/100.0, qualityMessage, 0) == false){ ok = false; }
  }

  return ok;
}
