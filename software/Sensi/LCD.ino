/******************************************************************************************************/
// LCD
/******************************************************************************************************/
#include "src/Sensi.h"
#include "src/LCD.h"
#include "src/LCDlayout.h"
#include "src/Config.h"
#include "src/Quality.h"
#include "src/WiFi.h"
#include "src/bme280.h"
#include "src/bme68x.h"
#include "src/ccs811.h"
#include "src/scd30.h"
#include "src/sgp30.h"
#include "src/sps30.h"
#include "src/Weather.h"
#include "src/MLX.h"
#include "src/MAX30.h"

bool          lcd_avail = false;                           // is LCD attached?
int           altDisplay = 0;                              // Alternate between sensors, when not enough space on display
uint8_t       lcd_i2c[2];                                  // the pins for the i2c port, set during initialization
unsigned long intervalLCD = 0;                             // LCD refresh rate, is set depending on fastmode during setup
unsigned long lastLCD;                                     // last time LCD was modified
unsigned long lastLCDReset;                                // last time LCD was reset
char          lcdDisplay[4][20];                           // 4 lines of 20 characters, Display 0
char          lcdDisplayAlt[4][20];                        // 4 lines of 20 characters, Display 1
bool          lastLCDInten = false;

TwoWire      *lcd_port = 0;                                // Pointer to the i2c port

// --- LCD Display
#if defined(ADALCD)
  #include "Adafruit_LiquidCrystal.h"
  Adafruit_LiquidCrystal lcd(0);                           // 0x20 is added inside driver
#else
  #include "LiquidCrystal_PCF8574.h"
  LiquidCrystal_PCF8574 lcd(0x27);                         // set the LCD address to 0x27 
#endif

// External Variables
extern unsigned long     yieldTime;           // Sensi
extern unsigned long     lastYield;           // Sensi
extern Settings          mySettings;          // Config
extern unsigned long     currentTime;         // Sensi
extern char              tmpStr[256];         // Sensi

extern bool              bme280_avail;        // bme280
extern float             bme280_pressure;
extern float             bme280_pressure24hrs;
extern float             bme280_temp;

extern bool              bme68x_avail;        // bme68x
extern Bme68x            bme68xSensor;
extern bme68xData        bme68x;      
extern float             bme68x_pressure24hrs;//

extern bool              scd30_avail;         // scd30
extern uint16_t          scd30_ppm; 
extern float             scd30_temp;
extern float             scd30_hum; 
extern float             scd30_ah;

extern bool              ccs811_avail;        // ccs811
extern CCS811            ccs811;    

extern bool              sgp30_avail;         // sgp30
extern SGP30             sgp30;     

extern bool              sps30_avail;         // sps30
extern sps30_measurement valSPS30;
extern volatile SensorStates stateSPS30;

extern bool              therm_avail;         // MLX
extern IRTherm           therm;
extern float             mlxOffset;

extern bool              weather_avail;
extern bool              weather_success;
extern volatile WiFiStates stateWeather;
extern clientData        weatherData;

extern bool              time_avail;
extern tm               *localTime; 

/******************************************************************************************************/
// Initialize LCD
/******************************************************************************************************/

bool initializeLCD() {
  bool success = true; // unfortuntely the LCD driver functions have no error checking
  switchI2C(lcd_port, lcd_i2c[0], lcd_i2c[1], lcd_i2cspeed, lcd_i2cClockStretchLimit);

#if defined(ADALCD)
  lcd.begin(20, 4, LCD_5x8DOTS, *lcd_port);
  if (mySettings.useBacklight == true) { lcd.setBacklight(HIGH); lastLCDInten = true; } else { lcd.setBacklight(LOW); lastLCDInten = false; }
#else
  lcd.begin(20, 4, *lcd_port);
  if (mySettings.useBacklight == true) { lcd.setBacklight(255);  lastLCDInten = true; } else { lcd.setBacklight(0);   lastLCDInten = false; }
#endif
  if (mySettings.debuglevel > 0) { R_printSerialTelnetLogln(F("LCD initialized")); }
  delay(50); lastYield = millis();

  return success;
} 

/**************************************************************************************/
// Update LCD
/**************************************************************************************/
// This code prints one whole LCD screen line at a time.
// It appears frequent lcd.setcursor() commands corrupt the display.
// A line is 20 characters long and terminated at the 21st character with null
// The first line is continuted at the 3rd line in the LCD driver and the 2nd line is continued at the 4th line.
// Display update takes 115ms

// Version 1: updateSinglePageLCDwTime() See outline in excel file
// Version 2: updateSinglePageLCD()
// Version 3: updateTwoPageLCD()
// Version 4: updateLCD()

bool updateSinglePageLCDwTime() {
  /**************************************************************************************/
  // Update Single Page LCD
  /**************************************************************************************/
  bool success = true;  // when ERROR recovery fails, success becomes false, no Error recovery with LCD
  
  float myTemp = -9999.;
  float myHum  = -9999.;
  float myCO2  = -9999.;
  float mydP   = -9999.;
  float mytVOC = -9999.;
  float myPM25 = -9999.;
  float myPM10 = -9999.;
  float myGAS  = -9999.;
  float myP    = -9999.;
  
  char myCO2_warning[]  = "N";
  char myTemp_warning[] = "N";
  char myHum_warning[]  = "N";
  char mydP_warning[]   = "N";
  char myPM25_warning[] = "N";
  char myPM10_warning[] = "N";
  char mytVOC_warning[] = "N";
  const char myNaN[]    = " ";

  char lcdbuf[21];
  const char  clearLine[] = "                    ";  // 20 spaces
  char qualityMessage[2];
    
  //////////////////////////////////////////////////////////////////////////////////////////////////
  // Collect Data
  //////////////////////////////////////////////////////////////////////////////////////////////////
  D_printSerialTelnet(F("D:LCD:CD.."));

  if (scd30_avail && mySettings.useSCD30 && (scd30_ppm != 0)) { // =CO2 Hum ===
    myTemp = scd30_temp;
    myHum  = scd30_hum;
    myCO2  = float(scd30_ppm);
  } else { 
    if (ccs811_avail && mySettings.useCCS811) { myCO2 = float(ccs811.getCO2()); } // scd30 not running use eCO2 from ccs811
    else { if (sgp30_avail && mySettings.useSGP30) { myCO2 = float(sgp30.CO2); } } // scd30 not running use eCO2 from sgp30
    if (bme68x_avail && mySettings.useBME68x) { myHum = bme68x.humidity; } 
  }
      
  if (bme68x_avail && mySettings.useBME68x) { // =Pressure Temp ===============
    mydP   = (bme68x.pressure - bme68x_pressure24hrs)/100.0;
    myTemp = bme68x.temperature;
    myP    = bme68x.pressure/100.0;   
    myGAS  = bme68x.gas_resistance;
  } else if (bme280_avail && mySettings.useBME280) { 
    mydP   = (bme280_pressure - bme280_pressure24hrs)/100.0;
    myTemp = bme280_temp;
    myP    = bme280_pressure/100.0;
  }
  
  if (ccs811_avail && mySettings.useCCS811) { // =tVOC ========================
    mytVOC = float(ccs811.getTVOC());
  } else if (sgp30_avail && mySettings.useSGP30) { 
    mytVOC = float(sgp30.TVOC);               // tVoc fall back
  }

  if (sps30_avail && mySettings.useSPS30) { // =Particles =====================
    if (stateSPS30 == HAS_ERROR) {
      myPM25 = -9999.;
      myPM10 = -9999.;
    } else {
      myPM25 = valSPS30.mc_2p5;
      myPM10 = valSPS30.mc_10p0;
    }
  }

  // my warnings // =================================================================================
  if (myCO2 > 0.) { checkCO2(myCO2, qualityMessage, 1); }                     else { strncpy(qualityMessage, myNaN, 1); }
  myCO2_warning[0] = qualityMessage[0];
    
  if (myTemp > -100.) { checkAmbientTemperature(myTemp, qualityMessage, 1); } else { strncpy(qualityMessage, myNaN, 1); }
  myTemp_warning[0] = qualityMessage[0];

  if (myHum >= 0.) { checkHumidity(myHum, qualityMessage, 1); }               else { strncpy(qualityMessage, myNaN, 1); }
  myHum_warning[0] = qualityMessage[0];
  
  if (mydP > -9999.0) {checkdP(mydP, qualityMessage, 1);}                     else { strncpy(qualityMessage, myNaN, 1); }
  mydP_warning[0] = qualityMessage[0];
    
  if (myPM25 >= 0.) { checkPM2(myPM25, qualityMessage, 1); }                  else { strncpy(qualityMessage, myNaN, 1); }
  myPM25_warning[0] = qualityMessage[0];

  if (myPM10 >= 0.) { checkPM10(myPM10, qualityMessage, 1); }                 else { strncpy(qualityMessage, myNaN, 1); }
  myPM10_warning[0] = qualityMessage[0];
  
  if (mytVOC >= 0.) { checkTVOC(mytVOC, qualityMessage, 1); }                 else { strncpy(qualityMessage, myNaN, 1); }
  mytVOC_warning[0] = qualityMessage[0];
  
  //////////////////////////////////////////////////////////////////////////////////////////////////
  // build display
  //////////////////////////////////////////////////////////////////////////////////////////////////

  D_printSerialTelnet(F("D:LCD:PF.."));

  // clear display buffer
  strncpy(&lcdDisplay[0][0], clearLine , 20); // we dont want Null character in the lcdDisplayBuffer
  strncpy(&lcdDisplay[1][0], clearLine , 20); // without Null char
  strncpy(&lcdDisplay[2][0], clearLine , 20); // without Null char
  strncpy(&lcdDisplay[3][0], clearLine , 20); // without Null char

  // Temperature
  if (myTemp > -100.) {
    if ( (myTemp < 100.0) && (myTemp > 0.0)) { 
      snprintf_P(lcdbuf, sizeof(lcdbuf), PSTR("%4.1fC"), myTemp);
      strncpy(&lcdDisplay[1][15], lcdbuf, 6); // without Null char
    } if (myTemp > 100.0) { 
      snprintf_P(lcdbuf, sizeof(lcdbuf), PSTR("%5.1fC"), myTemp);
      strncpy(&lcdDisplay[1][14], lcdbuf, 6); // without Null char
    } if (myTemp <0.0) {
      snprintf_P(lcdbuf, sizeof(lcdbuf), PSTR("%+5.1fC"), myTemp);
      strncpy(&lcdDisplay[1][14], lcdbuf, 6); // without Null char
    }
  } else {
    strncpy(&lcdDisplay[1][14], myNaN, 1); // without Null char
  }

  // Humidity
  if (myHum > 0.) {
    if (myHum <= 100.) { snprintf_P(lcdbuf, sizeof(lcdbuf), PSTR("%4.1f%%"), myHum); } 
    else               { snprintf_P(lcdbuf, sizeof(lcdbuf), PSTR(">100%%")); }
    strncpy(&lcdDisplay[0][15],lcdbuf, 5); // without Null char
    strncpy(&lcdDisplay[0][2], myHum_warning, 1); // without Null char
  } else {
    strncpy(&lcdDisplay[0][15],myNaN, 1); // without Null char
    strncpy(&lcdDisplay[0][2], myNaN, 1); // without Null char
  }
  
   // CO2
  if (myCO2 >= 0.) {
    snprintf_P(lcdbuf, sizeof(lcdbuf), PSTR("%4dppm"), myround(myCO2));
    strncpy(&lcdDisplay[0][7], lcdbuf, 7); // without Null char
    strncpy(&lcdDisplay[0][5], myCO2_warning, 1); // without Null char
  } else {
    strncpy(&lcdDisplay[0][7], myNaN, 1); // without Null char
    strncpy(&lcdDisplay[0][5], myNaN, 1); // without Null char
  }

  // delta Pressure in one day
  if (mydP < 10.0 && mydP > -10.0) {
    snprintf_P(lcdbuf, sizeof(lcdbuf), PSTR("%+4.1fmb"), mydP);
    strncpy(&lcdDisplay[2][14], lcdbuf, 6); // without Null char
    strncpy(&lcdDisplay[0][3], mydP_warning, 1); // without Null char
  } else {
    if (mydP >  10.0 && mydP != -9999.) { snprintf_P(lcdbuf, sizeof(lcdbuf), PSTR(">10mb"));  strncpy(&lcdDisplay[2][14], lcdbuf, 5); } // without Null char
    if (mydP < -10.0 && mydP != -9999.) { snprintf_P(lcdbuf, sizeof(lcdbuf), PSTR("<-10mb")); strncpy(&lcdDisplay[2][14], lcdbuf, 6);} // without Null char
    strncpy(&lcdDisplay[0][3], mydP_warning, 1); // without Null char
  }

  // Pressure
  if (myP !=-9999.) {
    snprintf_P(lcdbuf, sizeof(lcdbuf), PSTR("%4.0f"), myP);
    strncpy(&lcdDisplay[2][10], lcdbuf, 4); // without Null char
  } 

  // total Volatile Organic Compounds
  if (mytVOC >= 0.) {
    snprintf_P(lcdbuf, sizeof(lcdbuf), PSTR("%4.0fppb"), mytVOC);
    strncpy(&lcdDisplay[1][6], lcdbuf, 7); // without Null char
    strncpy(&lcdDisplay[0][4], mytVOC_warning, 1); // without Null char
  } else {
    strncpy(&lcdDisplay[1][6], myNaN, 1); // without Null char
    strncpy(&lcdDisplay[0][4], myNaN, 1); // without Null char
  }

  // Particulate Matter
  if ( (myPM25 >= 0.) && (myPM10 >= 0.) ) {
    if (myPM25<1000) { snprintf_P(lcdbuf, sizeof(lcdbuf), PSTR("%3du"), myround(myPM25)); } else { snprintf_P(lcdbuf, sizeof(lcdbuf), PSTR(">999")); }
    strncpy(&lcdDisplay[1][0], lcdbuf, 4); // without Null char
    if (myPM10<1000) { snprintf_P(lcdbuf, sizeof(lcdbuf), PSTR("%3du"), myround(myPM10)); } else { snprintf_P(lcdbuf, sizeof(lcdbuf), PSTR(">999")); }
    strncpy(&lcdDisplay[2][0], lcdbuf, 4);
    strncpy(&lcdDisplay[0][0], myPM25_warning, 1); // without Null char
    strncpy(&lcdDisplay[0][1], myPM10_warning, 1); // without Null char
  } else {
    strncpy(&lcdDisplay[1][0], myNaN, 1); // without Null char
    strncpy(&lcdDisplay[2][0], myNaN, 1); // without Null char
    strncpy(&lcdDisplay[0][0], myNaN, 1); // without Null char
    strncpy(&lcdDisplay[0][1], myNaN, 1); // without Null char
  }

  // 68x gas resistance
  if (myGAS >= 0.) {
    snprintf_P(lcdbuf, sizeof(lcdbuf), PSTR("%3dk"), myround(myGAS/1000.));
    strncpy(&lcdDisplay[2][5], lcdbuf, 4); // without Null char
  } else { 
    strncpy(&lcdDisplay[2][5], myNaN, 1); // without Null char
  }
  
  //  // time and date or weather]
  //  snprintf_P(tmpStr, sizeof(tmpStr), PSTR("Weather: avail ................ %s, %d"),  (weather_avail) ? FPSTR(mON) : FPSTR(mOFF), weather_avail); 
  //  printSerialTelnetLogln(tmpStr);
  //  snprintf_P(tmpStr, sizeof(tmpStr), PSTR("Weather: success .............. %s, %d"),  (weather_success) ? FPSTR(mON) : FPSTR(mOFF), weather_success); 
  //  printSerialTelnetLogln(tmpStr);
  //  snprintf_P(tmpStr, sizeof(tmpStr), PSTR("Weather: use .................. %s, %d"),  (mySettings.useWeather) ? FPSTR(mON) : FPSTR(mOFF), mySettings.useWeather); 
  //  printSerialTelnetLogln(tmpStr);
  //  snprintf_P(tmpStr, sizeof(tmpStr), PSTR("Alt Display: .................. %s, %d"),  (altDisplay>0) ? FPSTR(mON) : FPSTR(mOFF), altDisplay); 
  //  printSerialTelnetLogln(tmpStr);

  if (weather_avail && mySettings.useWeather && weather_success) { 
    altDisplay = (altDisplay+1) % 3;
  }

  if (altDisplay == 0) { // display time and date
    if (time_avail && mySettings.useNTP ) {
      // Data
      snprintf_P(lcdbuf, sizeof(lcdbuf),PSTR("%d%d.%d%d.%d"), 
              (localTime->tm_mon+1)/10, 
              (localTime->tm_mon+1)%10, 
              localTime->tm_mday/10, 
              localTime->tm_mday%10, 
              (localTime->tm_year+1900));  
      strncpy(&lcdDisplay[3][0], lcdbuf, 10); // without Null char
      // Time
      snprintf_P(lcdbuf, sizeof(lcdbuf),PSTR("%d%d:%d%d:%d%d"), 
             localTime->tm_hour/10 ? localTime->tm_hour/10 : 0 , 
             localTime->tm_hour%10,
             localTime->tm_min/10,    
             localTime->tm_min%10,
             localTime->tm_sec/10,    
             localTime->tm_sec%10);
      strncpy(&lcdDisplay[3][12], lcdbuf, 8); // without Null char
    }
    
  } else if (altDisplay == 1) { // display weather description and time if enough space
    int wl = strlen(weatherData.description);
    strncpy(&lcdDisplay[3][0], weatherData.description, wl >= 20 ? 20 : wl  ); // without Null char
    if (wl <=12) { 
      snprintf_P(lcdbuf, sizeof(lcdbuf),PSTR("%d%d:%d%d:%d%d"), 
             localTime->tm_hour/10 ? localTime->tm_hour/10 : 0 , 
             localTime->tm_hour%10,
             localTime->tm_min/10,    
             localTime->tm_min%10,
             localTime->tm_sec/10,    
             localTime->tm_sec%10);
      strncpy(&lcdDisplay[3][12], lcdbuf, 8); // without Null char
    } else if (wl <=15) {
      snprintf_P(lcdbuf, sizeof(lcdbuf),PSTR("%d%d:%d%d"), 
             localTime->tm_min/10,    
             localTime->tm_min%10,
             localTime->tm_sec/10,    
             localTime->tm_sec%10);
      strncpy(&lcdDisplay[3][15], lcdbuf, 5); // without Null char
    } else if (wl <=18) {
      snprintf_P(lcdbuf, sizeof(lcdbuf),PSTR("%d%d"), 
             localTime->tm_sec/10,    
             localTime->tm_sec%10);
      strncpy(&lcdDisplay[3][18], lcdbuf, 2); // without Null char
    }
    
  } else if (altDisplay == 2) { // display max and min weather temperature
    snprintf_P(lcdbuf, sizeof(lcdbuf),PSTR("%5.1f %5.1f"), weatherData.tempMin, weatherData.tempMax);
    strncpy(&lcdDisplay[3][0], lcdbuf, 11); // without Null char
    snprintf_P(lcdbuf, sizeof(lcdbuf),PSTR("%d%d:%d%d:%d%d"), 
           localTime->tm_hour/10 ? localTime->tm_hour/10 : 0 , 
           localTime->tm_hour%10,
           localTime->tm_min/10,    
           localTime->tm_min%10,
           localTime->tm_sec/10,    
           localTime->tm_sec%10);
    strncpy(&lcdDisplay[3][12], lcdbuf, 8); // without Null char
  }
    
  D_printSerialTelnet(F("D:LCD:US.."));

  switchI2C(lcd_port, lcd_i2c[0], lcd_i2c[1], lcd_i2cspeed, lcd_i2cClockStretchLimit);
  
  // lcd.clear();
  lcd.setCursor(0, 0); 
  
  strncpy(lcdbuf, &lcdDisplay[0][0], 20);    lcdbuf[20] = '\0'; if (strlen(lcdbuf) == 20) { lcd.print(lcdbuf); } // since display has no Null char we need to terminate lcdbuf
  yieldTime += yieldOS(); 
  strncpy(lcdbuf, &lcdDisplay[2][0], 20);    lcdbuf[20] = '\0'; if (strlen(lcdbuf) == 20) { lcd.print(lcdbuf); } // since display has no Null char we need to terminate lcdbuf
  yieldTime += yieldOS(); 
  strncpy(lcdbuf, &lcdDisplay[1][0], 20);    lcdbuf[20] = '\0'; if (strlen(lcdbuf) == 20) { lcd.print(lcdbuf); } // since display has no Null char we need to terminate lcdbuf
  yieldTime += yieldOS(); 
  strncpy(lcdbuf, &lcdDisplay[3][0], 20);    lcdbuf[20] = '\0'; if (strlen(lcdbuf) == 20) { lcd.print(lcdbuf); } // since display has no Null char we need to terminate lcdbuf
  yieldTime += yieldOS(); 

  if (mySettings.debuglevel == 11) { // if dbg, display the lines also on serial port
    strncpy(lcdbuf, &lcdDisplay[0][0], 20);    lcdbuf[20] = '\0'; printSerialTelnetLog("|");  printSerialTelnetLog(lcdbuf); printSerialTelnetLogln("|");
    yieldTime += yieldOS(); 
    strncpy(lcdbuf, &lcdDisplay[1][0], 20);    lcdbuf[20] = '\0'; printSerialTelnetLog("|");  printSerialTelnetLog(lcdbuf); printSerialTelnetLogln("|");
    yieldTime += yieldOS(); 
    strncpy(lcdbuf, &lcdDisplay[2][0], 20);    lcdbuf[20] = '\0'; printSerialTelnetLog("|");  printSerialTelnetLog(lcdbuf); printSerialTelnetLogln("|");
    yieldTime += yieldOS(); 
    strncpy(lcdbuf, &lcdDisplay[3][0], 20);    lcdbuf[20] = '\0'; printSerialTelnetLog("|");  printSerialTelnetLog(lcdbuf); printSerialTelnetLogln("|");
    yieldTime += yieldOS(); 
  }

  D_printSerialTelnet(F("D:LCD:D.."));

  return success;

} // update display 


bool updateSinglePageLCD() {
  /**************************************************************************************/
  // Update Single Page LCD
  /**************************************************************************************/
  bool success = true;
  
  float myTemp = -9999.;
  float myHum  = -9999.;
  float myCO2  = -9999.;
  float mydP   = -9999.;
  float mytVOC = -9999.;
  float myPM25 = -9999.;
  float myPM10 = -9999.;
  
  char myCO2_warning[]  = "nrm ";
  char myTemp_warning[] = "nrm ";
  char myHum_warning[]  = "nrm ";
  char mydP_warning[]   = "nrm ";
  char myPM_warning[]   = "nrm ";
  char mytVOC_warning[] = "nrm ";
  const char myNaN[]    = "na  ";
  
  char lcdbuf[21];
  const char  clearLine[] = "                    ";  // 20 spaces
  char qualityMessage[5];
    
  // Collect Data
  //////////////////////////////////////////////////////////////////////////////////////////////////

  if (scd30_avail && mySettings.useSCD30) { // ======================================================
    myTemp = scd30_temp;
    myHum  = scd30_hum;
    myCO2  = float(scd30_ppm);
  }
    
  if (bme68x_avail && mySettings.useBME68x) { // ====================================================
    mydP   = (bme68x.pressure - bme68x_pressure24hrs)/100.0;
    myTemp = bme68x.temperature;
  }

  if (bme280_avail && mySettings.useBME280) { // ====================================================
    mydP   = (bme280_pressure - bme280_pressure24hrs)/100.0;
    myTemp = bme280_temp;
  }
  
  if (sgp30_avail && mySettings.useSGP30) { // ======================================================
    mytVOC = float(sgp30.TVOC);    
  } 

  if (sps30_avail && mySettings.useSPS30) { // ======================================================
    myPM25 = valSPS30.mc_2p5;
    myPM10 = valSPS30.mc_10p0;
  }

  // my warnings // =================================================================================
  if (myCO2 > 0.) { checkCO2(float(myCO2), qualityMessage, 4); }              else { strncpy(qualityMessage, myNaN, 4); }
  strncpy(myCO2_warning, qualityMessage, 4);
  
  if (myTemp > -100.) { checkAmbientTemperature(myTemp, qualityMessage, 4); } else { strncpy(qualityMessage, myNaN, 4); }
  strncpy(myTemp_warning, qualityMessage, 4);

  if (myHum >= 0.) { checkHumidity(myHum, qualityMessage, 4); }               else { strncpy(qualityMessage, myNaN, 4); }
  strncpy(myHum_warning, qualityMessage, 4);
  
  checkdP(mydP, qualityMessage, 4);
  strncpy(mydP_warning, qualityMessage, 4);
    
  if (myPM10 >= 0.) { checkPM(myPM25, myPM10, qualityMessage, 4); }           else { strncpy(qualityMessage, myNaN, 4); }
  strncpy(myPM_warning, qualityMessage, 4);
  
  if (mytVOC >= 0.) { checkTVOC(mytVOC, qualityMessage, 4); }                 else { strncpy(qualityMessage, myNaN, 4); }
  strncpy(mytVOC_warning, qualityMessage, 4);
  
  // build display
  //////////////////////////////////////////////////////////////////////////////////////////////////
  strncpy(&lcdDisplay[0][0], clearLine , 20); // No Null char
  strncpy(&lcdDisplay[1][0], clearLine , 20); // No Null char
  strncpy(&lcdDisplay[2][0], clearLine , 20); // No Null char
  strncpy(&lcdDisplay[3][0], clearLine , 20); // No Null char

  if (myTemp > -100.) {
    if (myTemp < 100.0) { 
      snprintf_P(lcdbuf, sizeof(lcdbuf), PSTR("%+5.1fC"), myTemp);
      strncpy(&lcdDisplay[1][14], lcdbuf, 6);  // No Null char
    } else { 
      snprintf_P(lcdbuf, sizeof(lcdbuf), PSTR("%+6.1fC"),myTemp); 
      strncpy(&lcdDisplay[1][13], lcdbuf, 7); // No Null char
    }
  } else {
    strncpy(&lcdDisplay[1][14], myNaN, 4); // No Null char
  }

  if (myHum > 0.) {
    if (myHum <= 100.) { snprintf_P(lcdbuf, sizeof(lcdbuf), PSTR("%4.1f%%"), myHum); } else { snprintf_P(lcdbuf, sizeof(lcdbuf), PSTR(">100%%")); }
    strncpy(&lcdDisplay[0][15], lcdbuf, 5); // No Null char
    strncpy(&lcdDisplay[0][4], "rH", 2); // No Null char
    strncpy(&lcdDisplay[1][4], myHum_warning, 4); // No Null char
  } else {
    strncpy(&lcdDisplay[0][16], myNaN, 4); // No Null char
    strncpy(&lcdDisplay[0][4], "rH", 2); // No Null char
    strncpy(&lcdDisplay[1][4], myNaN, 4); // No Null char
  }
  
  if (myCO2 >= 0.) {
    snprintf_P(lcdbuf, sizeof(lcdbuf), PSTR("%4dppm"), myround(myCO2));
    strncpy(&lcdDisplay[3][13], lcdbuf, 7); // No Null char
    strncpy(&lcdDisplay[2][0], "CO2", 3); // No Null char
    strncpy(&lcdDisplay[3][0], myCO2_warning, 4); // No Null char
  } else {
    strncpy(&lcdDisplay[3][16], myNaN, 4); // No Null char
    strncpy(&lcdDisplay[2][0], "CO2", 3); // No Null char
    strncpy(&lcdDisplay[3][0], myNaN, 4); // No Null char
  }
  
  if (mydP < 10.0 && mydP > -10.0) {
    snprintf_P(lcdbuf, sizeof(lcdbuf), PSTR("%+4.1fmb"), mydP);
    strncpy(&lcdDisplay[2][14], lcdbuf, 6); // No Null char
    strncpy(&lcdDisplay[2][4], "dP", 2); // No Null char
    strncpy(&lcdDisplay[3][4], mydP_warning, 4); // No Null char
  } else {
    if (mydP >  10.0 && mydP != -9999) { snprintf_P(lcdbuf, sizeof(lcdbuf), PSTR(">10mb")); }
    if (mydP < -10.0 && mydP != -9999) { snprintf_P(lcdbuf, sizeof(lcdbuf), PSTR("<-10mb")); }
    strncpy(&lcdDisplay[2][14], lcdbuf, 6); // No Null char
    strncpy(&lcdDisplay[2][4], "dP", 2); // No Null char
    strncpy(&lcdDisplay[3][4], mydP_warning, 4); // No Null char
  }

  if (mytVOC >= 0.) {
    strncpy(&lcdDisplay[0][8], "tVOC", 4); // No Null char
    strncpy(&lcdDisplay[1][8], mytVOC_warning, 4); // No Null char
  } else {
    strncpy(&lcdDisplay[0][8], "tVOC", 4); // No Null char
    strncpy(&lcdDisplay[1][8], myNaN, 4); // No Null char
  }

  if (myPM25 >= 0.) {
    
    if (myPM25<1000) { snprintf_P(lcdbuf, sizeof(lcdbuf), PSTR("%3du"), myround(myPM25)); } else { snprintf_P(lcdbuf, sizeof(lcdbuf), PSTR(">999")); }
    strncpy(&lcdDisplay[2][8], lcdbuf, 4); // No Null char
    if (myPM10<1000) { snprintf_P(lcdbuf, sizeof(lcdbuf), PSTR("%3du"), myround(myPM10)); } else { snprintf_P(lcdbuf, sizeof(lcdbuf), PSTR(">999")); }
    strncpy(&lcdDisplay[3][8], lcdbuf, 4); // No Null char
    strncpy(&lcdDisplay[0][0], "PM", 2); // No Null char
    strncpy(&lcdDisplay[1][0], myPM_warning, 4); // No Null char
  } else {
    strncpy(&lcdDisplay[2][8], myNaN, 4); // No Null char
    strncpy(&lcdDisplay[3][8], myNaN, 4); // No Null char
    strncpy(&lcdDisplay[0][0], "PM", 2); // No Null char
    strncpy(&lcdDisplay[1][0], myNaN, 4); // No Null char
  }

  switchI2C(lcd_port, lcd_i2c[0], lcd_i2c[1], lcd_i2cspeed, lcd_i2cClockStretchLimit);
  
  // lcd.clear();
  lcd.setCursor(0, 0); 
  
  strncpy(lcdbuf, &lcdDisplay[0][0], 20);    lcdbuf[20] = '\0'; if (strlen(lcdbuf) == 20) { lcd.print(lcdbuf); }
  yieldTime += yieldOS(); 
  strncpy(lcdbuf, &lcdDisplay[2][0], 20);    lcdbuf[20] = '\0'; if (strlen(lcdbuf) == 20) { lcd.print(lcdbuf); }
  yieldTime += yieldOS(); 
  strncpy(lcdbuf, &lcdDisplay[1][0], 20);    lcdbuf[20] = '\0'; if (strlen(lcdbuf) == 20) { lcd.print(lcdbuf); }
  yieldTime += yieldOS(); 
  strncpy(lcdbuf, &lcdDisplay[3][0], 20);    lcdbuf[20] = '\0'; if (strlen(lcdbuf) == 20) { lcd.print(lcdbuf); } 
  yieldTime += yieldOS(); 

  if (mySettings.debuglevel == 11) { // if dbg, display the lines also on serial port
    strncpy(lcdbuf, &lcdDisplay[0][0], 20);    lcdbuf[20] = '\0'; 
    R_printSerialTelnetLog(lcdbuf); printSerialTelnetLogln("|");
    yieldTime += yieldOS(); 
    strncpy(lcdbuf, &lcdDisplay[1][0], 20);    lcdbuf[20] = '\0';   
    printSerialTelnetLog(lcdbuf); 
    printSerialTelnetLogln("|");
    yieldTime += yieldOS(); 
    strncpy(lcdbuf, &lcdDisplay[2][0], 20);    lcdbuf[20] = '\0';   
    printSerialTelnetLog(lcdbuf); 
    printSerialTelnetLogln("|");
    yieldTime += yieldOS(); 
    strncpy(lcdbuf, &lcdDisplay[3][0], 20);    lcdbuf[20] = '\0';   
    printSerialTelnetLog(lcdbuf); 
    printSerialTelnetLogln("|");
    yieldTime += yieldOS(); 
  }

  return success;

} // update display 

//////////////////////////////////////////////////////////////////

bool updateTwoPageLCD() {
  /**************************************************************************************/
  // Update Consumer LCD
  /**************************************************************************************/
  
  char lcdbuf[21];
  const char  clearLine[] = "                    ";  // 20 spaces
  bool success = true;;
  char qualityMessage[5];
  
  const char   firstLine[] = "P01     rH   TEMP   ";
  const char  secondLine[] = "P25     aH          ";
  const char   thirdLine[] = "P04      P   tVOC   ";
  const char  thirdLineN[] = "P04      P          ";
  const char   forthLine[] = "P10     dP    CO2   ";
  
  if (altDisplay>0) { // SHOW Measurands and Assessment
    
    strncpy(&lcdDisplayAlt[0][0],  firstLine , 20); // No Null char
    strncpy(&lcdDisplayAlt[1][0], secondLine , 20); // No Null char
    if (sgp30_avail && mySettings.useSGP30) { strncpy(&lcdDisplayAlt[2][0],   thirdLine , 20);} // No Null char
    else                                    { strncpy(&lcdDisplayAlt[2][0],  thirdLineN , 20);} // No Null char
    strncpy(&lcdDisplayAlt[3][0],  forthLine , 20); // No Null char

    if (scd30_avail && mySettings.useSCD30) { // =============================================================
            
	    checkCO2(float(scd30_ppm), qualityMessage, 1);
      lcdDisplayAlt[3][18] = qualityMessage[0];

      checkAmbientTemperature(scd30_temp, qualityMessage, 1);
      lcdDisplayAlt[0][18] = qualityMessage[0];

      checkHumidity(scd30_hum, qualityMessage, 1);
      lcdDisplayAlt[0][11] = qualityMessage[0];
    }

    if (bme68x_avail && mySettings.useBME68x) { // ====================================================

      checkdP( (abs(bme68x.pressure - bme68x_pressure24hrs)/100.0), qualityMessage, 1);
      lcdDisplayAlt[3][11] = qualityMessage[0];
    }

    if (bme280_avail && mySettings.useBME280) { // ====================================================
      checkdP( (abs(bme280_pressure - bme280_pressure24hrs)/100.0), qualityMessage, 1);
      lcdDisplayAlt[3][11] = qualityMessage[0];
    }

    if (sps30_avail && mySettings.useSPS30) { // ======================================================
      checkPM2(valSPS30.mc_2p5, qualityMessage, 1);
      lcdDisplayAlt[1][4]  = qualityMessage[0];
 
      checkPM10(valSPS30.mc_10p0, qualityMessage, 1);
      lcdDisplayAlt[3][4]  = qualityMessage[0];
    }

    if (sgp30_avail && mySettings.useSGP30) { // =======================================================
      checkTVOC(sgp30.TVOC, qualityMessage, 1);
      lcdDisplayAlt[2][18] = qualityMessage[0];
    }
    
  } else { // altDisplay SHOW DATA
    
    strncpy(&lcdDisplay[0][0], clearLine , 20); // No Null char
    strncpy(&lcdDisplay[1][0], clearLine , 20); // No Null char
    strncpy(&lcdDisplay[2][0], clearLine , 20); // No Null char
    strncpy(&lcdDisplay[3][0], clearLine , 20); // No Null char

    if (scd30_avail && mySettings.useSCD30) { // =======================================================
      snprintf_P(lcdbuf, sizeof(lcdbuf), PSTR("%4huppm"), scd30_ppm);
      strncpy(&lcdDisplay[3][13], lcdbuf, 7); // No Null char
      snprintf_P(lcdbuf, sizeof(lcdbuf), PSTR("%3d%%"), (int)scd30_hum);
      strncpy(&lcdDisplay[0][7], lcdbuf, 4); // No Null char
      snprintf_P(lcdbuf, sizeof(lcdbuf), PSTR("%+5.1fC"), scd30_temp);
      strncpy(&lcdDisplay[0][12], lcdbuf, 6); // No Null char
      float tmp = 273.15 + scd30_temp;
      scd30_ah = scd30_hum * 13.246 / tmp * exp(19.854 - 5423.0/tmp); // [gr/m^3]
      snprintf_P(lcdbuf, sizeof(lcdbuf), PSTR("%4.1fg/m3"), scd30_ah);
      strncpy(&lcdDisplay[1][6], lcdbuf, 8); // No Null char
    }

    if (bme68x_avail && mySettings.useBME68x) { // ====================================================
      snprintf_P(lcdbuf, sizeof(lcdbuf), PSTR("%4dmb"),(int)(bme68x.pressure/100.0));
      strncpy(&lcdDisplay[2][6], lcdbuf, 6); // No Null char
      snprintf_P(lcdbuf, sizeof(lcdbuf), PSTR("%2dmb"),(int)(abs((bme68x.pressure - bme68x_pressure24hrs)/100.0)));
      strncpy(&lcdDisplay[3][8], lcdbuf, 4); // No Null char
    }

    if (bme280_avail && mySettings.useBME280) { // ====================================================
      snprintf_P(lcdbuf, sizeof(lcdbuf), PSTR("%4dmb"),(int)(bme280_pressure/100.0));
      strncpy(&lcdDisplay[2][6], lcdbuf, 6); // No Null char
      snprintf_P(lcdbuf, sizeof(lcdbuf), PSTR("%+4.1fmb"),((bme280_pressure - bme280_pressure24hrs)/100.0));
      strncpy(&lcdDisplay[3][6], lcdbuf, 6); // No Null char
    }

    if (sps30_avail && mySettings.useSPS30) { // ====================================================
      snprintf_P(lcdbuf, sizeof(lcdbuf), PSTR("%3.0fug"),valSPS30.mc_1p0);
      strncpy(&lcdDisplay[0][0], lcdbuf, 5); // No Null char
      snprintf_P(lcdbuf, sizeof(lcdbuf), PSTR("%3.0fug"),valSPS30.mc_2p5);
      strncpy(&lcdDisplay[1][0], lcdbuf, 5); // No Null char
      snprintf_P(lcdbuf, sizeof(lcdbuf), PSTR("%3.0fug"),valSPS30.mc_4p0);
      strncpy(&lcdDisplay[2][0], lcdbuf, 5); // No Null char
      snprintf_P(lcdbuf, sizeof(lcdbuf), PSTR("%3.0fug"),valSPS30.mc_10p0);
      strncpy(&lcdDisplay[3][0], lcdbuf, 5); // No Null char
    }

    if (sgp30_avail && mySettings.useSGP30) { // ====================================================
      snprintf_P(lcdbuf, sizeof(lcdbuf), PSTR("%4dpbb"), sgp30.TVOC);
      strncpy(&lcdDisplay[2][13], lcdbuf, 7);   // No Null char
    }
  } // end alt display

  switchI2C(lcd_port, lcd_i2c[0], lcd_i2c[1], lcd_i2cspeed, lcd_i2cClockStretchLimit);
  
  //lcd.clear();
  lcd.setCursor(0, 0); 
  
  if (altDisplay>0) {
    // 1st line continues at 3d line
    // 2nd line continues at 4th line
    strncpy(lcdbuf, &lcdDisplayAlt[0][0], 20); lcdbuf[20] = '\0'; if (strlen(lcdbuf) == 20) { lcd.print(lcdbuf); }
    yieldTime += yieldOS(); 
    strncpy(lcdbuf, &lcdDisplayAlt[2][0], 20); lcdbuf[20] = '\0'; if (strlen(lcdbuf) == 20) { lcd.print(lcdbuf); } 
    yieldTime += yieldOS(); 
    strncpy(lcdbuf, &lcdDisplayAlt[1][0], 20); lcdbuf[20] = '\0'; if (strlen(lcdbuf) == 20) { lcd.print(lcdbuf); }
    yieldTime += yieldOS();   
    strncpy(lcdbuf, &lcdDisplayAlt[3][0], 20); lcdbuf[20] = '\0'; if (strlen(lcdbuf) == 20) { lcd.print(lcdbuf); } 
    yieldTime += yieldOS(); 
  } else {
    strncpy(lcdbuf, &lcdDisplay[0][0], 20);    lcdbuf[20] = '\0'; if (strlen(lcdbuf) == 20) { lcd.print(lcdbuf); }
    yieldTime += yieldOS(); 
    strncpy(lcdbuf, &lcdDisplay[2][0], 20);    lcdbuf[20] = '\0'; if (strlen(lcdbuf) == 20) { lcd.print(lcdbuf); }
    yieldTime += yieldOS();     
    strncpy(lcdbuf, &lcdDisplay[1][0], 20);    lcdbuf[20] = '\0'; if (strlen(lcdbuf) == 20) { lcd.print(lcdbuf); }
    yieldTime += yieldOS();   
    strncpy(lcdbuf, &lcdDisplay[3][0], 20);    lcdbuf[20] = '\0'; if (strlen(lcdbuf) == 20) { lcd.print(lcdbuf); } 
    yieldTime += yieldOS(); 
  }

  if (mySettings.debuglevel == 11) { // if dbg, display the lines also on serial port
    if (altDisplay>0) {
      strncpy(lcdbuf, &lcdDisplayAlt[0][0], 20); lcdbuf[20] = '\0'; 
      R_printSerialTelnetLog(lcdbuf); printSerialTelnetLogln("|");
      yieldTime += yieldOS(); 
      strncpy(lcdbuf, &lcdDisplayAlt[1][0], 20); lcdbuf[20] = '\0';   
      printSerialTelnetLog(lcdbuf); printSerialTelnetLogln("|");
      yieldTime += yieldOS(); 
      strncpy(lcdbuf, &lcdDisplayAlt[2][0], 20); lcdbuf[20] = '\0';   
      printSerialTelnetLog(lcdbuf); printSerialTelnetLogln("|");
      yieldTime += yieldOS(); 
      strncpy(lcdbuf, &lcdDisplayAlt[3][0], 20); lcdbuf[20] = '\0';   
      printSerialTelnetLog(lcdbuf); printSerialTelnetLogln("|");
      yieldTime += yieldOS(); 
    } else {
      strncpy(lcdbuf, &lcdDisplay[0][0], 20);    lcdbuf[20] = '\0'; 
      R_printSerialTelnetLog(lcdbuf); printSerialTelnetLogln("|");
      yieldTime += yieldOS(); 
      strncpy(lcdbuf, &lcdDisplay[1][0], 20);    lcdbuf[20] = '\0';   
      printSerialTelnetLog(lcdbuf); printSerialTelnetLogln("|");
      yieldTime += yieldOS(); 
      strncpy(lcdbuf, &lcdDisplay[2][0], 20);    lcdbuf[20] = '\0';   
      printSerialTelnetLog(lcdbuf); printSerialTelnetLogln("|");
      yieldTime += yieldOS(); 
      strncpy(lcdbuf, &lcdDisplay[3][0], 20);    lcdbuf[20] = '\0';   
      printSerialTelnetLog(lcdbuf); printSerialTelnetLogln("|");
      yieldTime += yieldOS();   
    }
  }
  
  // switch display
  if (altDisplay > 0) {
    altDisplay = 0;
  } else {
    altDisplay = 1;
  }

  return success;
} // update

//////////////////////////////////////////////////////////////////

bool updateLCD() {
  
  char lcdbuf[21];
  const char clearLine[] = "                    ";  // 20 spaces
  bool success = true;
  char qualityMessage[2];
  
  strncpy(&lcdDisplay[0][0], clearLine , 20); // No Null char
  strncpy(&lcdDisplay[1][0], clearLine , 20); // No Null char
  strncpy(&lcdDisplay[2][0], clearLine , 20); // No Null char
  strncpy(&lcdDisplay[3][0], clearLine , 20); // No Null char

  
  if (scd30_avail && mySettings.useSCD30) { // ====================================================
    if (scd30_ppm>0) { snprintf_P(lcdbuf, sizeof(lcdbuf), PSTR("%4hu"), scd30_ppm); } else { snprintf_P(lcdbuf, sizeof(lcdbuf), PSTR("   NA")); }
    strncpy(&lcdDisplay[CO2_Y][CO2_X], lcdbuf, 4); // No Null char
    
    if (scd30_hum>=0) { snprintf_P(lcdbuf, sizeof(lcdbuf), PSTR("%4.1f%%"), scd30_hum); } else {snprintf_P(lcdbuf, sizeof(lcdbuf), PSTR("  NA"));} 
    strncpy(&lcdDisplay[HUM1_Y][HUM1_X], lcdbuf, 5); // No Null char
    
    if (scd30_temp>-100.0) { snprintf_P(lcdbuf, sizeof(lcdbuf), PSTR("%+5.1fC"),scd30_temp); } else {snprintf_P(lcdbuf, sizeof(lcdbuf), PSTR("  NA"));}
    strncpy(&lcdDisplay[TEMP1_Y][TEMP1_X], lcdbuf, 6); // No Null char

    checkCO2(float(scd30_ppm), qualityMessage, 1);  
    if (mySettings.debuglevel == 11) { R_printSerialTelnetLog("SCD30 CO2: "); printSerialTelnetLog(qualityMessage); } // No Null char
    strncpy(&lcdDisplay[CO2_WARNING_Y][CO2_WARNING_X], qualityMessage, 1); // No Null char

    yieldTime += yieldOS(); 
  }  // end if avail scd30
  
  if (bme68x_avail && mySettings.useBME68x) { // ==================================================
    
    snprintf_P(lcdbuf, sizeof(lcdbuf), PSTR("%4d"),(int)(bme68x.pressure/100.0)); 
    strncpy(&lcdDisplay[PRESSURE_Y][PRESSURE_X], lcdbuf, 4); // No Null char
  
    snprintf_P(lcdbuf, sizeof(lcdbuf), PSTR("%4.1f%%"),bme68x.humidity);
    strncpy(&lcdDisplay[HUM2_Y][HUM2_X], lcdbuf, 5); // No Null char
  
    snprintf_P(lcdbuf, sizeof(lcdbuf), PSTR("%4.1fg"),bme68x_ah);
    strncpy(&lcdDisplay[HUM3_Y][HUM3_X], lcdbuf, 5); // No Null char

    checkHumidity(bme68x.humidity, qualityMessage, 1);
    if (mySettings.debuglevel == 11) { printSerialTelnetLog("BME68xHum: "); printSerialTelnetLog(qualityMessage); }
    // Where does it go on the display?
      
    snprintf_P(lcdbuf, sizeof(lcdbuf), PSTR("%+5.1fC"),bme68x.temperature);
    strncpy(&lcdDisplay[TEMP2_Y][TEMP2_X], lcdbuf, 6); // No Null char
  
    snprintf_P(lcdbuf, sizeof(lcdbuf), PSTR("%5.1f"),(bme68x.gas_resistance/1000.0));
    strncpy(&lcdDisplay[IAQ_Y][IAQ_X], lcdbuf, 5); // No Null char
  
    checkGasResistance(bme68x.gas_resistance, qualityMessage, 1);
    if (mySettings.debuglevel == 11) { printSerialTelnetLog("BME68x GasRes: "); printSerialTelnetLog(qualityMessage); }
    strncpy(&lcdDisplay[IAQ_WARNING_Y][IAQ_WARNING_X], qualityMessage, 1); // No Null char

    yieldTime += yieldOS(); 
    
  } // end if avail bme68x
  
  if (sgp30_avail && mySettings.useSGP30) { // ====================================================
    snprintf_P(lcdbuf, sizeof(lcdbuf), PSTR("%4hu"), sgp30.CO2);
    strncpy(&lcdDisplay[eCO2_Y][eCO2_X], lcdbuf, 4); // No Null char

    snprintf_P(lcdbuf, sizeof(lcdbuf), PSTR("%4hu"), sgp30.TVOC);
    strncpy(&lcdDisplay[TVOC_Y][TVOC_X], lcdbuf, 4); // No Null char

    checkCO2(float(sgp30.CO2), qualityMessage, 1);
    if (mySettings.debuglevel == 11) { printSerialTelnetLog("SGP30 CO2: "); printSerialTelnetLog(qualityMessage); }
    strncpy(&lcdDisplay[eCO2_WARNING_Y][eCO2_WARNING_X], qualityMessage, 1); // No Null char

    checkTVOC(sgp30.TVOC, qualityMessage, 1);
    if (mySettings.debuglevel == 11) { printSerialTelnetLog("SGP30 tVOC: "); printSerialTelnetLog(qualityMessage); }
    strncpy(&lcdDisplay[TVOC_WARNING_Y][TVOC_WARNING_X], qualityMessage, 1); // No Null char

    yieldTime += yieldOS(); 
    
  } // end if avail sgp30

  if (ccs811_avail && mySettings.useCCS811) { // ==================================================

    snprintf_P(lcdbuf, sizeof(lcdbuf), PSTR("%4hu"), ccs811.getCO2());
    strncpy(&lcdDisplay[eeCO2_Y][eeCO2_X], lcdbuf, 4); // No Null char

    snprintf_P(lcdbuf, sizeof(lcdbuf), PSTR("%4hu"), ccs811.getTVOC());
    strncpy(&lcdDisplay[TTVOC_Y][TTVOC_X], lcdbuf, 4); // No Null char

    checkCO2(float(ccs811.getCO2()), qualityMessage,1 );
    if (mySettings.debuglevel == 11) { printSerialTelnetLog("CCS811 CO2: "); printSerialTelnetLog(qualityMessage); }
    strncpy(&lcdDisplay[eeCO2_WARNING_Y][eeCO2_WARNING_X], qualityMessage, 1); // No Null char

    checkTVOC(ccs811.getTVOC(), qualityMessage, 1);
    strncpy(&lcdDisplay[TTVOC_WARNING_Y][TTVOC_WARNING_X], qualityMessage, 1); // No Null char

    yieldTime += yieldOS(); 
    
  } // end if avail ccs811
  
  if (sps30_avail && mySettings.useSPS30) { // ====================================================
    snprintf_P(lcdbuf, sizeof(lcdbuf), PSTR("%3.0f"),valSPS30.mc_1p0);
    strncpy(&lcdDisplay[PM1_Y][PM1_X], lcdbuf, 3); // No Null char
    
    snprintf_P(lcdbuf, sizeof(lcdbuf), PSTR("%3.0f"),valSPS30.mc_2p5);
    strncpy(&lcdDisplay[PM2_Y][PM2_X], lcdbuf, 3); // No Null char
    
    snprintf_P(lcdbuf, sizeof(lcdbuf), PSTR("%3.0f"),valSPS30.mc_4p0);
    strncpy(&lcdDisplay[PM4_Y][PM4_X], lcdbuf, 3); // No Null char
    
    snprintf_P(lcdbuf, sizeof(lcdbuf), PSTR("%3.0f"),valSPS30.mc_10p0);
    strncpy(&lcdDisplay[PM10_Y][PM10_X], lcdbuf, 3); // No Null char

    checkPM2(valSPS30.mc_2p5, qualityMessage, 1);
    if (mySettings.debuglevel == 11) { printSerialTelnetLog("SPS30 PM2: "); printSerialTelnetLog(qualityMessage); }
    strncpy(&lcdDisplay[PM2_WARNING_Y][PM2_WARNING_X], qualityMessage, 1);  // No Null char

    checkPM10(valSPS30.mc_10p0, qualityMessage,1 );
    if (mySettings.debuglevel == 11) { printSerialTelnetLog("SPS30 PM10: "); printSerialTelnetLog(qualityMessage); }
    strncpy(&lcdDisplay[PM10_WARNING_Y][PM10_WARNING_X], qualityMessage, 1); // No Null char

    yieldTime += yieldOS(); 
  }// end if avail SPS30

  if (therm_avail && mySettings.useMLX) { // ====================================================
    if (altDisplay > 0) {
      snprintf_P(lcdbuf, sizeof(lcdbuf), PSTR("%+5.1fC"),(therm.object()+mlxOffset));
      strncpy(&lcdDisplay[TEMP1_Y][TEMP1_X], lcdbuf, 6); // No Null char

      snprintf_P(lcdbuf, sizeof(lcdbuf), PSTR("%+5.1fC"),therm.ambient());
      strncpy(&lcdDisplay[TEMP2_Y][TEMP2_X], lcdbuf, 6); // No Null char

      checkFever((therm.object() + fhDelta + mlxOffset), qualityMessage, 1);
      if (mySettings.debuglevel == 11) { printSerialTelnetLog("MAX Temp: "); printSerialTelnetLog(qualityMessage); }
      strncpy(&lcdDisplay[MLX_WARNING_Y][MLX_WARNING_X], qualityMessage, 1); // No Null char

      yieldTime += yieldOS(); 
    }
  }// end if avail  MLX

  if (altDisplay > 0) { 
    altDisplay = 0; 
  } else { 
    altDisplay = 1; 
  }
  
  switchI2C(lcd_port, lcd_i2c[0], lcd_i2c[1], lcd_i2cspeed, lcd_i2cClockStretchLimit);
  
  //lcd.clear();
  lcd.setCursor(0, 0); 
  
  // 1st line continues at 3d line
  // 2nd line continues at 4th line
  strncpy(lcdbuf, &lcdDisplay[0][0], 20); lcdbuf[20] = '\0';  if (strlen(lcdbuf) == 20) { lcd.print(lcdbuf); }
  yieldTime += yieldOS(); 
  strncpy(lcdbuf, &lcdDisplay[2][0], 20); lcdbuf[20] = '\0';  if (strlen(lcdbuf) == 20) { lcd.print(lcdbuf); }
  yieldTime += yieldOS(); 
  strncpy(lcdbuf, &lcdDisplay[1][0], 20); lcdbuf[20] = '\0';  if (strlen(lcdbuf) == 20) { lcd.print(lcdbuf); }
  yieldTime += yieldOS(); 
  strncpy(lcdbuf, &lcdDisplay[3][0], 20); lcdbuf[20] = '\0';  if (strlen(lcdbuf) == 20) { lcd.print(lcdbuf); }
  yieldTime += yieldOS(); 

  if (mySettings.debuglevel == 11) {              // if dbg, display the lines also on serial port
    strncpy(lcdbuf, &lcdDisplay[0][0], 20); lcdbuf[20] = '\0'; 
    R_printSerialTelnetLog(lcdbuf); printSerialTelnetLogln("|");
    yieldTime += yieldOS(); 
    strncpy(lcdbuf, &lcdDisplay[1][0], 20); lcdbuf[20] = '\0';   
    printSerialTelnetLog(lcdbuf); printSerialTelnetLogln("|");
    yieldTime += yieldOS(); 
    strncpy(lcdbuf, &lcdDisplay[2][0], 20); lcdbuf[20] = '\0';   
    printSerialTelnetLog(lcdbuf); printSerialTelnetLogln("|");
    yieldTime += yieldOS(); 
    strncpy(lcdbuf, &lcdDisplay[3][0], 20); lcdbuf[20] = '\0';   
    printSerialTelnetLog(lcdbuf); printSerialTelnetLogln("|");
    yieldTime += yieldOS(); 
  }

  return success;
} // update display 
