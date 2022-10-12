/******************************************************************************************************/
// LCD Display
/******************************************************************************************************/
#ifndef LCD_H_
#define LCD_H_

// Power Consumption: ... not yet measured
// For display layout see excel file in project folder

// LCD setup uses long delays during initialization:
//  clear 2ms delay
//  setcursor no delay
//  print dont know delays, also dont know how this works as it uses arduino specific streaming
//  begin calls init and therfore has long delay
//  init >1000ms delay
//  backlight no delay
//
#define intervalLCDFast             5000                   // 5sec
#define intervalLCDSlow            60000                   // 1min
#define lcd_i2cspeed               I2C_REGULAR             
#define lcd_i2cClockStretchLimit   I2C_DEFAULTSTRETCH
#define myround(x) ((x)>=0?(int)((x)+0.5):(int)((x)-0.5))

bool initializeLCD(void);
bool updateLCD(void);
bool updateSinglePageLCD(void);
bool updateTwoPageLCD(void);
bool updateSinglePageLCDwTime(void);

#endif
