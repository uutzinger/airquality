/******************************************************************************************************/
// Initialize LCD
/******************************************************************************************************/
#include "VSC.h"
#ifdef EDITVSC
#include "src/Sensi.h"
#include "src/LCD.h"
#endif

bool initializeLCD() {
  bool success = true; // unfortuntely the LCD driver functions have no error checking
  
  lcd_port->begin(lcd_i2c[0], lcd_i2c[1]);

#if defined(ADALCD)
  lcd.begin(20, 4, LCD_5x8DOTS, *lcd_port);
  if (mySettings.useBacklight == true) {lcd.setBacklight(HIGH); } else {lcd.setBacklight(LOW);}
#else
  lcd.begin(20, 4, *lcd_port);
  if (mySettings.useBacklight == true) {lcd.setBacklight(255); } else {lcd.setBacklight(0);}
#endif
  if (mySettings.debuglevel > 0) { printSerialTelnet(F("LCD initialized\r\n")); }
  delay(50);

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
  
  allGood = true;
  
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
    
  if (bme680_avail && mySettings.useBME680) { // ====================================================
    mydP   = (bme680.pressure - bme680_pressure24hrs)/100.0;
    myTemp = bme680.temperature;
  }

  if (bme280_avail && mySettings.useBME280) { // ====================================================
    mydP   = (bme280_pressure - bme280_pressure24hrs)/100.0;
    myTemp = bme280_temp;
  }
  
  if (sgp30_avail && mySettings.useSGP30) { // ======================================================
    mytVOC = float(sgp30.TVOC);    
  } 

  if (sps30_avail && mySettings.useSPS30) { // ======================================================
    myPM25 = valSPS30.MassPM2;
    myPM10 = valSPS30.MassPM10;
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
  strncpy(myPM_warning, "nrm ", 4);
  
  if (mytVOC >= 0.) { checkTVOC(mytVOC, qualityMessage, 4); }                 else { strncpy(qualityMessage, myNaN, 4); }
  strncpy(mytVOC_warning, qualityMessage, 4);
  
  // build display
  //////////////////////////////////////////////////////////////////////////////////////////////////
  strncpy(&lcdDisplay[0][0], clearLine , 20);
  strncpy(&lcdDisplay[1][0], clearLine , 20);
  strncpy(&lcdDisplay[2][0], clearLine , 20);
  strncpy(&lcdDisplay[3][0], clearLine , 20);

  if (myTemp > -100.) {
    if (myTemp < 100.0) { 
      sprintf(lcdbuf,"%+5.1fC", myTemp);
      strncpy(&lcdDisplay[1][14], lcdbuf, 6);
    } else { 
      sprintf(lcdbuf,"%+6.1fC",myTemp); 
      strncpy(&lcdDisplay[1][13], lcdbuf, 7);
    }
  } else {
    strncpy(&lcdDisplay[1][14], myNaN, 4);
  }

  if (myHum > 0.) {
    if (myHum <= 100.) { sprintf(lcdbuf, "%4.1f%%", myHum); } else { sprintf(lcdbuf, ">100%%"); }
    strncpy(&lcdDisplay[0][15], lcdbuf, 5);
    strncpy(&lcdDisplay[0][4], "rH", 2);
    strncpy(&lcdDisplay[1][4], myHum_warning, 4);
  } else {
    strncpy(&lcdDisplay[0][16], myNaN, 4);
    strncpy(&lcdDisplay[0][4], "rH", 2);
    strncpy(&lcdDisplay[1][4], myNaN, 4);
  }
  
  if (myCO2 >= 0.) {
    sprintf(lcdbuf, "%4dppm", int(myCO2));
    strncpy(&lcdDisplay[3][13], lcdbuf, 7);
    strncpy(&lcdDisplay[2][0], "CO2", 3);
    strncpy(&lcdDisplay[3][0], myCO2_warning, 4);
  } else {
    strncpy(&lcdDisplay[3][16], myNaN, 4);
    strncpy(&lcdDisplay[2][0], "CO2", 3);
    strncpy(&lcdDisplay[3][0], myNaN, 4);
  }
  
  if (mydP < 10.0 && mydP > -10.0) {
    sprintf(lcdbuf,"%+4.1fmb", mydP);
    strncpy(&lcdDisplay[2][14], lcdbuf, 6);
    strncpy(&lcdDisplay[2][4], "dP", 2);
    strncpy(&lcdDisplay[3][4], mydP_warning, 4);
  } else {
    if (mydP >  10.0 && mydP != -9999) { sprintf(lcdbuf,">10mb"); }
    if (mydP < -10.0 && mydP != -9999) { sprintf(lcdbuf,"<-10mb"); }
    strncpy(&lcdDisplay[2][14], lcdbuf, 6);
    strncpy(&lcdDisplay[2][4], "dP", 2);
    strncpy(&lcdDisplay[3][4], myNaN, 4);
  }

  if (mytVOC >= 0.) {
    strncpy(&lcdDisplay[0][8], "tVOC", 4);
    strncpy(&lcdDisplay[1][8], mytVOC_warning, 4);
  } else {
    strncpy(&lcdDisplay[0][8], "tVOC", 4);
    strncpy(&lcdDisplay[1][8], myNaN, 4);
  }

  if (myPM25 >= 0.) {
    
    if (myPM25<1000) { sprintf(lcdbuf, "%3du", int(myPM25)); } else { sprintf(lcdbuf, ">999"); }
    strncpy(&lcdDisplay[2][8], lcdbuf, 4);
    if (myPM10<1000) { sprintf(lcdbuf, "%3du", int(myPM10)); } else { sprintf(lcdbuf, ">999"); }
    strncpy(&lcdDisplay[3][8], lcdbuf, 4);
    strncpy(&lcdDisplay[0][0], "PM", 2);
    strncpy(&lcdDisplay[1][0], myPM_warning, 4);
  } else {
    strncpy(&lcdDisplay[2][8], myNaN, 4);
    strncpy(&lcdDisplay[3][8], myNaN, 4);
    strncpy(&lcdDisplay[0][0], "PM", 2);
    strncpy(&lcdDisplay[1][0], myNaN, 4);
  }

  lcd_port->begin(lcd_i2c[0], lcd_i2c[1]);
  // lcd.clear();
  lcd.setCursor(0, 0); 
  
  strncpy(lcdbuf, &lcdDisplay[0][0], 20);    lcdbuf[20] = '\0'; lcd.print(lcdbuf);
  strncpy(lcdbuf, &lcdDisplay[2][0], 20);    lcdbuf[20] = '\0'; lcd.print(lcdbuf); 
  strncpy(lcdbuf, &lcdDisplay[1][0], 20);    lcdbuf[20] = '\0'; lcd.print(lcdbuf); 
  strncpy(lcdbuf, &lcdDisplay[3][0], 20);    lcdbuf[20] = '\0'; lcd.print(lcdbuf); 

  if (mySettings.debuglevel == 2) { // if dbg, display the lines also on serial port
    strncpy(lcdbuf, &lcdDisplay[0][0], 20);    lcdbuf[20] = '\0'; printSerialTelnet(lcdbuf); printSerialTelnet("|\r\n");
    strncpy(lcdbuf, &lcdDisplay[1][0], 20);    lcdbuf[20] = '\0'; printSerialTelnet(lcdbuf); printSerialTelnet("|\r\n");
    strncpy(lcdbuf, &lcdDisplay[2][0], 20);    lcdbuf[20] = '\0'; printSerialTelnet(lcdbuf); printSerialTelnet("|\r\n");
    strncpy(lcdbuf, &lcdDisplay[3][0], 20);    lcdbuf[20] = '\0'; printSerialTelnet(lcdbuf); printSerialTelnet("|\r\n");
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
  
  if (altDisplay) { // SHOW Measurands and Assessment
    
    strncpy(&lcdDisplayAlt[0][0],  firstLine , 20);
    strncpy(&lcdDisplayAlt[1][0], secondLine , 20);
    if (sgp30_avail && mySettings.useSGP30) { strncpy(&lcdDisplayAlt[2][0],   thirdLine , 20);}
    else                                    { strncpy(&lcdDisplayAlt[2][0],  thirdLineN , 20);}
    strncpy(&lcdDisplayAlt[3][0],  forthLine , 20);

    allGood = true;

    if (scd30_avail && mySettings.useSCD30) { // =============================================================
            
	  checkCO2(float(scd30_ppm), qualityMessage, 1);
      lcdDisplayAlt[3][18] = qualityMessage[0];

      checkAmbientTemperature(scd30_temp, qualityMessage, 1);
      lcdDisplayAlt[0][18] = qualityMessage[0];

      checkHumidity(scd30_hum, qualityMessage, 1);
      lcdDisplayAlt[0][11] = qualityMessage[0];
    }

    if (bme680_avail && mySettings.useBME680) { // ====================================================

      checkdP( (abs(bme680.pressure - bme680_pressure24hrs)/100.0), qualityMessage, 1);
      lcdDisplayAlt[3][11] = qualityMessage[0];
    }

    if (bme280_avail && mySettings.useBME280) { // ====================================================
      checkdP( (abs(bme280_pressure - bme280_pressure24hrs)/100.0), qualityMessage, 1);
      lcdDisplayAlt[3][11] = qualityMessage[0];
    }

    if (sps30_avail && mySettings.useSPS30) { // ======================================================
      checkPM2(valSPS30.MassPM2, qualityMessage, 1);
      lcdDisplayAlt[1][4]  = qualityMessage[0];
 
      checkPM10(valSPS30.MassPM10, qualityMessage, 1);
      lcdDisplayAlt[3][4]  = qualityMessage[0];
    }

    if (sgp30_avail && mySettings.useSGP30) { // =======================================================
      checkTVOC(sgp30.TVOC, qualityMessage, 1);
      lcdDisplayAlt[2][18] = qualityMessage[0];
    }
    
  } else { // altDisplay SHOW DATA
    
    strncpy(&lcdDisplay[0][0], clearLine , 20);
    strncpy(&lcdDisplay[1][0], clearLine , 20);
    strncpy(&lcdDisplay[2][0], clearLine , 20);
    strncpy(&lcdDisplay[3][0], clearLine , 20);

    if (scd30_avail && mySettings.useSCD30) { // =======================================================
      sprintf(lcdbuf, "%4huppm", scd30_ppm);
      strncpy(&lcdDisplay[3][13], lcdbuf, 7);
      sprintf(lcdbuf, "%3d%%", (int)scd30_hum);
      strncpy(&lcdDisplay[0][7], lcdbuf, 4);
      sprintf(lcdbuf,"%+5.1fC", scd30_temp);
      strncpy(&lcdDisplay[0][12], lcdbuf, 6);
      float tmp = 273.15 + scd30_temp;
      scd30_ah = scd30_hum * 13.246 / tmp * exp(19.854 - 5423.0/tmp); // [gr/m^3]
      sprintf(lcdbuf, "%4.1fg/m3", scd30_ah);
      strncpy(&lcdDisplay[1][6], lcdbuf, 8);
    }

    if (bme680_avail && mySettings.useBME680) { // ====================================================
      sprintf(lcdbuf,"%4dmb",(int)(bme680.pressure/100.0));
      strncpy(&lcdDisplay[2][6], lcdbuf, 6);
      sprintf(lcdbuf,"%2dmb",(int)(abs((bme680.pressure - bme680_pressure24hrs)/100.0)));
      strncpy(&lcdDisplay[3][8], lcdbuf, 4);
    }

    if (bme280_avail && mySettings.useBME280) { // ====================================================
      sprintf(lcdbuf,"%4dmb",(int)(bme280_pressure/100.0));
      strncpy(&lcdDisplay[2][6], lcdbuf, 6);
      sprintf(lcdbuf,"%+4.1fmb",((bme280_pressure - bme280_pressure24hrs)/100.0));
      strncpy(&lcdDisplay[3][6], lcdbuf, 6);
    }

    if (sps30_avail && mySettings.useSPS30) { // ====================================================
      sprintf(lcdbuf,"%3.0fug",valSPS30.MassPM1);
      strncpy(&lcdDisplay[0][0], lcdbuf, 5);
      sprintf(lcdbuf,"%3.0fug",valSPS30.MassPM2);
      strncpy(&lcdDisplay[1][0], lcdbuf, 5);
      sprintf(lcdbuf,"%3.0fug",valSPS30.MassPM4);
      strncpy(&lcdDisplay[2][0], lcdbuf, 5);
      sprintf(lcdbuf,"%3.0fug",valSPS30.MassPM10);
      strncpy(&lcdDisplay[3][0], lcdbuf, 5);
    }

    if (sgp30_avail && mySettings.useSGP30) { // ====================================================
      sprintf(lcdbuf, "%4dpbb", sgp30.TVOC);
      strncpy(&lcdDisplay[2][13], lcdbuf, 7);  
    }
  } // end alt display

  lcd_port->begin(lcd_i2c[0], lcd_i2c[1]);
  lcd.clear();
  lcd.setCursor(0, 0); 
  
  if (altDisplay) {
    // 1st line continues at 3d line
    // 2nd line continues at 4th line
    strncpy(lcdbuf, &lcdDisplayAlt[0][0], 20); lcdbuf[20] = '\0'; lcd.print(lcdbuf);
    strncpy(lcdbuf, &lcdDisplayAlt[2][0], 20); lcdbuf[20] = '\0'; lcd.print(lcdbuf); 
    strncpy(lcdbuf, &lcdDisplayAlt[1][0], 20); lcdbuf[20] = '\0'; lcd.print(lcdbuf); 
    strncpy(lcdbuf, &lcdDisplayAlt[3][0], 20); lcdbuf[20] = '\0'; lcd.print(lcdbuf); 
  } else {
    strncpy(lcdbuf, &lcdDisplay[0][0], 20);    lcdbuf[20] = '\0'; lcd.print(lcdbuf);
    strncpy(lcdbuf, &lcdDisplay[2][0], 20);    lcdbuf[20] = '\0'; lcd.print(lcdbuf); 
    strncpy(lcdbuf, &lcdDisplay[1][0], 20);    lcdbuf[20] = '\0'; lcd.print(lcdbuf); 
    strncpy(lcdbuf, &lcdDisplay[3][0], 20);    lcdbuf[20] = '\0'; lcd.print(lcdbuf); 
  }

  if (mySettings.debuglevel == 2) { // if dbg, display the lines also on serial port
    if (altDisplay) {
      strncpy(lcdbuf, &lcdDisplayAlt[0][0], 20); lcdbuf[20] = '\0'; printSerialTelnet(lcdbuf); printSerialTelnet("|\r\n");
      strncpy(lcdbuf, &lcdDisplayAlt[1][0], 20); lcdbuf[20] = '\0'; printSerialTelnet(lcdbuf); printSerialTelnet("|\r\n");
      strncpy(lcdbuf, &lcdDisplayAlt[2][0], 20); lcdbuf[20] = '\0'; printSerialTelnet(lcdbuf); printSerialTelnet("|\r\n");
      strncpy(lcdbuf, &lcdDisplayAlt[3][0], 20); lcdbuf[20] = '\0'; printSerialTelnet(lcdbuf); printSerialTelnet("|\r\n");
      
    } else {
      strncpy(lcdbuf, &lcdDisplay[0][0], 20);    lcdbuf[20] = '\0'; printSerialTelnet(lcdbuf); printSerialTelnet("|\r\n");
      strncpy(lcdbuf, &lcdDisplay[1][0], 20);    lcdbuf[20] = '\0'; printSerialTelnet(lcdbuf); printSerialTelnet("|\r\n");
      strncpy(lcdbuf, &lcdDisplay[2][0], 20);    lcdbuf[20] = '\0'; printSerialTelnet(lcdbuf); printSerialTelnet("|\r\n");
      strncpy(lcdbuf, &lcdDisplay[3][0], 20);    lcdbuf[20] = '\0'; printSerialTelnet(lcdbuf); printSerialTelnet("|\r\n");
    }
  }
  
  altDisplay = !altDisplay; // flip display between analysis and values
  
  return success;
} // update

//////////////////////////////////////////////////////////////////

bool updateLCD() {
  char lcdbuf[21];
  const char clearLine[] = "                    ";  // 20 spaces
  bool success = true;
  char qualityMessage[2];
  
  strncpy(&lcdDisplay[0][0], clearLine , 20);
  strncpy(&lcdDisplay[1][0], clearLine , 20);
  strncpy(&lcdDisplay[2][0], clearLine , 20);
  strncpy(&lcdDisplay[3][0], clearLine , 20);

  
  if (scd30_avail && mySettings.useSCD30) { // ====================================================
    if (scd30_ppm>0) { sprintf(lcdbuf, "%4hu", scd30_ppm); } else { sprintf(lcdbuf, "   NA"); }
    strncpy(&lcdDisplay[CO2_Y][CO2_X], lcdbuf, 4);
    
    if (scd30_hum>=0) { sprintf(lcdbuf, "%4.1f%%", scd30_hum); } else {sprintf(lcdbuf, "  NA");}
    strncpy(&lcdDisplay[HUM1_Y][HUM1_X], lcdbuf, 5);
    
    if (scd30_temp>-100.0) { sprintf(lcdbuf,"%+5.1fC",scd30_temp); } else {sprintf(lcdbuf, "  NA");}
    strncpy(&lcdDisplay[TEMP1_Y][TEMP1_X], lcdbuf, 6);

    checkCO2(float(scd30_ppm), qualityMessage, 1);  
    if (mySettings.debuglevel == 2) { printSerialTelnet("SCD30 CO2: "); printSerialTelnet(qualityMessage); }
    strncpy(&lcdDisplay[CO2_WARNING_Y][CO2_WARNING_X], qualityMessage, 1);

  }  // end if avail scd30
  
  if (bme680_avail && mySettings.useBME680) { // ==================================================
    
    sprintf(lcdbuf,"%4d",(int)(bme680.pressure/100.0));
    strncpy(&lcdDisplay[PRESSURE_Y][PRESSURE_X], lcdbuf, 4);
  
    sprintf(lcdbuf,"%4.1f%%",bme680.humidity);
    strncpy(&lcdDisplay[HUM2_Y][HUM2_X], lcdbuf, 5);
  
    sprintf(lcdbuf,"%4.1fg",bme680_ah);
    strncpy(&lcdDisplay[HUM3_Y][HUM3_X], lcdbuf, 5);

    checkHumidity(bme680.humidity, qualityMessage, 1);
    if (mySettings.debuglevel == 2) { printSerialTelnet("BME680Hum: "); printSerialTelnet(qualityMessage); }
    // Where does it go on the display?
      
    sprintf(lcdbuf,"%+5.1fC",bme680.temperature);
    strncpy(&lcdDisplay[TEMP2_Y][TEMP2_X], lcdbuf, 6);
  
    sprintf(lcdbuf,"%5.1f",(float(bme680.gas_resistance)/1000.0));
    strncpy(&lcdDisplay[IAQ_Y][IAQ_X], lcdbuf, 5);
  
    checkGasResistance(bme680.gas_resistance, qualityMessage, 1);
    if (mySettings.debuglevel == 2) { printSerialTelnet("BME680 GasRes: "); printSerialTelnet(qualityMessage); }
    strncpy(&lcdDisplay[IAQ_WARNING_Y][IAQ_WARNING_X], qualityMessage, 1);
    
  } // end if avail bme680
  
  if (sgp30_avail && mySettings.useSGP30) { // ====================================================
    sprintf(lcdbuf, "%4hu", sgp30.CO2);
    strncpy(&lcdDisplay[eCO2_Y][eCO2_X], lcdbuf, 4);

    sprintf(lcdbuf, "%4hu", sgp30.TVOC);
    strncpy(&lcdDisplay[TVOC_Y][TVOC_X], lcdbuf, 4);

    checkCO2(float(sgp30.CO2), qualityMessage, 1);
    if (mySettings.debuglevel == 2) { printSerialTelnet("SGP30 CO2: "); printSerialTelnet(qualityMessage); }
    strncpy(&lcdDisplay[eCO2_WARNING_Y][eCO2_WARNING_X], qualityMessage, 1);

    checkTVOC(sgp30.TVOC, qualityMessage, 1);
    if (mySettings.debuglevel == 2) { printSerialTelnet("SGP30 tVOC: "); printSerialTelnet(qualityMessage); }
    strncpy(&lcdDisplay[TVOC_WARNING_Y][TVOC_WARNING_X], qualityMessage, 1);
  } // end if avail sgp30

  if (ccs811_avail && mySettings.useCCS811) { // ==================================================

    sprintf(lcdbuf, "%4hu", ccs811.getCO2());
    strncpy(&lcdDisplay[eeCO2_Y][eeCO2_X], lcdbuf, 4);

    sprintf(lcdbuf, "%4hu", ccs811.getTVOC());
    strncpy(&lcdDisplay[TTVOC_Y][TTVOC_X], lcdbuf, 4);

    checkCO2(float(ccs811.getCO2()), qualityMessage,1 );
    if (mySettings.debuglevel == 2) { printSerialTelnet("CCS811 CO2: "); printSerialTelnet(qualityMessage); }
    strncpy(&lcdDisplay[eeCO2_WARNING_Y][eeCO2_WARNING_X], qualityMessage, 1);

    checkTVOC(ccs811.getTVOC(), qualityMessage, 1);
    strncpy(&lcdDisplay[TTVOC_WARNING_Y][TTVOC_WARNING_X], qualityMessage, 1);
  } // end if avail ccs811
  
  if (sps30_avail && mySettings.useSPS30) { // ====================================================
    sprintf(lcdbuf,"%3.0f",valSPS30.MassPM1);
    strncpy(&lcdDisplay[PM1_Y][PM1_X], lcdbuf, 3);
    
    sprintf(lcdbuf,"%3.0f",valSPS30.MassPM2);
    strncpy(&lcdDisplay[PM2_Y][PM2_X], lcdbuf, 3);
    
    sprintf(lcdbuf,"%3.0f",valSPS30.MassPM4);
    strncpy(&lcdDisplay[PM4_Y][PM4_X], lcdbuf, 3);
    
    sprintf(lcdbuf,"%3.0f",valSPS30.MassPM10);
    strncpy(&lcdDisplay[PM10_Y][PM10_X], lcdbuf, 3);

    checkPM2(valSPS30.MassPM2, qualityMessage, 1);
    if (mySettings.debuglevel == 2) { printSerialTelnet("SPS30 PM2: "); printSerialTelnet(qualityMessage); }
    strncpy(&lcdDisplay[PM2_WARNING_Y][PM2_WARNING_X], qualityMessage, 1);

    checkPM10(valSPS30.MassPM10, qualityMessage,1 );
    if (mySettings.debuglevel == 2) { printSerialTelnet("SPS30 PM10: "); printSerialTelnet(qualityMessage); }
    strncpy(&lcdDisplay[PM10_WARNING_Y][PM10_WARNING_X], qualityMessage, 1);
  }// end if avail SPS30

  if (therm_avail && mySettings.useMLX) { // ====================================================
    if (altDisplay == true) {
      sprintf(lcdbuf,"%+5.1fC",(therm.object()+mlxOffset));
      strncpy(&lcdDisplay[TEMP1_Y][TEMP1_X], lcdbuf, 6);

      sprintf(lcdbuf,"%+5.1fC",therm.ambient());
      strncpy(&lcdDisplay[TEMP2_Y][TEMP2_X], lcdbuf, 6);

      checkFever((therm.object() + fhDelta + mlxOffset), qualityMessage, 1);
      if (mySettings.debuglevel == 2) { printSerialTelnet("MAX Temp: "); printSerialTelnet(qualityMessage); }
      strncpy(&lcdDisplay[MLX_WARNING_Y][MLX_WARNING_X], qualityMessage, 1);
    }
  }// end if avail  MLX

  altDisplay = !altDisplay;
  
  lcd_port->begin(lcd_i2c[0], lcd_i2c[1]);
  lcd.clear();
  lcd.setCursor(0, 0); 
  
  // 1st line continues at 3d line
  // 2nd line continues at 4th line
  strncpy(lcdbuf, &lcdDisplay[0][0], 20); lcdbuf[20] = '\0';  lcd.print(lcdbuf);
  strncpy(lcdbuf, &lcdDisplay[2][0], 20); lcdbuf[20] = '\0';  lcd.print(lcdbuf); 
  strncpy(lcdbuf, &lcdDisplay[1][0], 20); lcdbuf[20] = '\0';  lcd.print(lcdbuf); 
  strncpy(lcdbuf, &lcdDisplay[3][0], 20); lcdbuf[20] = '\0';  lcd.print(lcdbuf); 

  if (mySettings.debuglevel == 2) {              // if dbg, display the lines also on serial port
    strncpy(lcdbuf, &lcdDisplay[0][0], 20); lcdbuf[20] = '\0'; printSerialTelnet(lcdbuf); printSerialTelnet("|\r\n");
    strncpy(lcdbuf, &lcdDisplay[1][0], 20); lcdbuf[20] = '\0'; printSerialTelnet(lcdbuf); printSerialTelnet("|\r\n");
    strncpy(lcdbuf, &lcdDisplay[2][0], 20); lcdbuf[20] = '\0'; printSerialTelnet(lcdbuf); printSerialTelnet("|\r\n");
    strncpy(lcdbuf, &lcdDisplay[3][0], 20); lcdbuf[20] = '\0'; printSerialTelnet(lcdbuf); printSerialTelnet("|\r\n");
  }

  return success;
} // update display 
