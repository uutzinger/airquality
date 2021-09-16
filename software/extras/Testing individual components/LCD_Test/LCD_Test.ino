#include <Wire.h>
#include <LiquidCrystal_I2C.h>

#if defined(ARDUINO) && ARDUINO >= 100
#define printByte(args)  write(args);
#else
#define printByte(args)  print(args,BYTE);
#endif

uint8_t bell[8]     = {0x4,0xe,0xe,0xe,0x1f,0x0,0x4};
uint8_t note[8]     = {0x2,0x3,0x2,0xe,0x1e,0xc,0x0};
uint8_t clk[8]      = {0x0,0xe,0x15,0x17,0x11,0xe,0x0};
uint8_t heart[8]    = {0x0,0xa,0x1f,0x1f,0xe,0x4,0x0};
uint8_t duck[8]     = {0x0,0xc,0x1d,0xf,0xf,0x6,0x0};
uint8_t chck[8]     = {0x0,0x1,0x3,0x16,0x1c,0x8,0x0};
uint8_t cross[8]    = {0x0,0x1b,0xe,0x4,0xe,0x1b,0x0};
uint8_t retarrow[8] = {	0x1,0x1,0x5,0x9,0x1f,0x8,0x4};
  
LiquidCrystal_I2C lcd(0x27,20,4);  // set the LCD address to 0x27 for a 20 chars and 4 line display

void setup()
{
  lcd.init();                      // initialize the lcd 
  lcd.backlight();
  
  lcd.createChar(0, bell);
  lcd.createChar(1, note);
  lcd.createChar(2, clk);
  lcd.createChar(3, heart);
  lcd.createChar(4, duck);
  lcd.createChar(5, chck);
  lcd.createChar(6, cross);
  lcd.createChar(7, retarrow);
  lcd.home();
  
  lcd.print("Hello world...");
  lcd.setCursor(0, 1);
  lcd.print("I ");
  lcd.printByte(3);
  lcd.print(" LCD!");
  delay(2000);
}

// display all keycodes
void displayKeyCodes(void) {
  for (uint8_t i = 0; i < 255; i+=20) {
    lcd.clear();
    lcd.print("Codes 0x"); lcd.print(i, HEX);
    lcd.print("-0x"); lcd.print(i+59, HEX);
    lcd.setCursor(0, 1);
    for (int j=0; j<20; j++) {
      lcd.printByte(i+j);
    }
    lcd.setCursor(0, 2);
    for (int j=20; j<40; j++) {
      lcd.printByte(i+j);
    }
    lcd.setCursor(0, 3);
    for (int j=40; j<60; j++) {
      lcd.printByte(i+j);
    }
    delay(500);
  }
}

void loop()
{
  displayKeyCodes();
  delay(1000);
}