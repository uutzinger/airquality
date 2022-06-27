/******************************************************************************************************/
// LCD Display
/******************************************************************************************************/
// Power Consumption: ... not yet measured
// LCD setup uses long delays during initialization:
//  clear 2ms delay
//  setcursor no delay
//  print dont know delays, also dont know how this works as it uses arduino specific streaming
//  begin calls init and therfore has long delay
//  init >1000ms delay
//  backlight no delay
//
#define intervalLCDFast  5000                              // 5sec
#define intervalLCDSlow 60000                              // 1min
unsigned long intervalLCD = 0;                             // LCD refresh rate, is set depending on fastmode during setup
bool lcd_avail = false;                                    // is LCD attached?
unsigned long lastLCD;                                     // last time LCD was modified
unsigned long lastLCDReset;                                // last time LCD was reset
char lcdDisplay[4][20];                                    // 4 lines of 20 characters, Display 0
char lcdDisplayAlt[4][20];                                 // 4 lines of 20 characters, Display 1
bool altDisplay = false;                                   // Alternate between sensors, when not enough space on display
TwoWire *lcd_port = 0;                                     // Pointer to the i2c port
uint8_t lcd_i2c[2];                                        // the pins for the i2c port, set during initialization
// For display layout see excel file in project folder
bool initializeLCD(void);
bool updateLCD(void);
bool updateSinglePageLCD(void);
bool updateTwoPageLCD(void);
bool updateSinglePageLCDwTime(void);
