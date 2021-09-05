# 1 "d:\\GitHub\\ESP Playground\\AirQuality\\software\\Unified Software\\Sensi\\Sensi.ino"
//////////////////////////////////////////////////////////////////////////////////////////////////////////
// Air Quality Sensor
//////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Supported:
//  - ESP8266 micro controller
//  - SPS30 Senserion particle, Paul Vah library
//  - SCD30 Senserion CO2, Sparkfun library, using interrupt from data ready pin
//  - SGP30 Senserion VOC, eCO2, Sparkfun library
//  - BME680 Bosch Temp, Humidity, Pressure, VOC, Adafruit library
//  - BM[E/P]280 Bosch Temp, [Humidity,] Pressure  Sparkfun library
//  - CCS811 Airquality eCO2 tVOC, Spakrfun library, using interrupt from data ready pin
//  - MLX90614 Melex temp contactless, Spakrfun library
//  - MAX30105 Maxim pulseox, not implemented yet, Spakfun library
//
// Data display through:
//  - LCD 20x4, LiquidCrystal_PCF8574 or Adafruit_LCD library
//  - MQTT publication to broker using PubSubClient library
//
// Operation:
//  Fast Mode: read often, minimal power saving, some sensors have higher sampling rate than others
//  Slow Mode: read at about 1 sample per minute, where possible enable power saving features,
//  Some sensorS require 1 sec sample interval regardless of Fast or Slow Mode.
//
//  Air quality sensors consume more power because they have heated sensos elements and are not
//  well suited for battery operation.
//
// Settings:
//  Check help-screen (send ? in terminal)
//  3 wifi passwords and 2 mqtt servers.
//  There are two places settings are stored. In the EEPRO and in Sensi.json.
//  During boot up settings are read form EEPROM in binary.
//  Settings are saved every couple of hours in EEPROM.
//  Settings are saved every couple of days in JSON file.
//  User can read settings from Sensi.json through console.
//  User can save settings trhough constole and it will generate JSON file and store in EEPROM.
//
// Wifi:
//  Scans network for known ssid, attempts connection with username password, if disconnected attempts reconnection.
//  MQTT server is contacted, if it can not be reached, a backup server is contected. username password is supported.
//  Network Time is obtained and adjusted for daylight savings time and timezone.
//  Over the Air programming can be enabled. 
//  HTTPUpdater at http://host:8890/firmware username admin password ....
//  mDNS responder and client are implemented.
//  HTML webserver is implemented. Sensors can be read useing hostnam/sensor. hostname/ provides self updating root page.
//  Large html files are streamed from LittleFS.
//
// Wire/I2C:
//  Multiple i2c pins are supported, ESP8266 Arduino supports only one active wire interface, however the pins can
//  be changed prior to each communication.
//  At startup, all pin combinations are scanned for possible scl & sda connections of the attached sensors.
//  Pins of supported sensors are registered.
//
//  Some breakout boards do not work well together with others.
//  Senosors might engage in excessive clockstretching and some have pull up resistor requirements beyond the one provided 
//  by the microcontroller.
//  For example CCS911 and SCD30 require long clock stretching. They are "slow" sensors. Its best to place them on separate i2c bus.
//  The LCD display requires 5V logic and corrupts regulalry when the i2c bus is shared, it should be placed on separate bus.
//  The MLX sensor might report negative or excessive high temperature when combined with arbitrary sensors.
//
// Improvements:
//  Some sensor drivers were modified to have no or almost no delay functions in the frequently called code sections.
//  Some drivers were modified to allow for user specified i2c port instead of standard "Wire" port.
//
// Software implementation:
//  The sensor driver is divided into intializations and updates.
//  The updates are written as state machines and called on regular basis through the main loop.

// Quality Assessments:
//  The LCD screen background light is blinking when a sensor is outside its recommended range.
//  Pressure:
//    A change of 5mbar within in 24hrs can cause headaches in suseptible subjects
//    The program computes the avaraging filter coefficients based on the sensor sample interval for a 24hr smoothing filter y = (1-a)*y + a*x
//  CO2:
//    >1000ppm is poor
//  Temperature:
//    20-25.5C is normal range
//  Humidity:
//    30-60% is normal range
//  Particles
//    P2.5: >25ug/m3 is poor
//    P10: >50ug/m3 is poor
//  tVOC:
//    >660ppb is poor
//////////////////////////////////////////////////////////////////////////////////////////////////////////
// This software is provided as is, no warranty to its proper operation is implied. Use it at your own risk.
// Urs Utzinger
// Sometime: MAX pulseox software (requires proximit detection and reqritting signal processing code)
// 2021 June: Webserver, NTP
// 2021 February, March: display update, mDNS, OTA
// 2021 January, i2c scanning and pin switching, OTA flashing
// 2020 Winter, fixing the i2c issues
// 2020 Fall, MQTT and Wifi
// 2020 Summer, first release
//////////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Wiring configuration of the sensors
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Wemos D1_mini ESP8266
// PIN CONFIGURATION
// ---------------------
// There are a limitted number of pins available and some affect the boot behaviour of the system.
// https://randomnerdtutorials.com/esp8266-pinout-reference-gpios/
// Symbols below:
//  2 can be used for i2c, pin is usually pulled high
//  l indictes pin is usually low,
//  c can be used for control,
//  i can be used for interrupt.
//  RX,TX, is needed if you want serial debug and USB terminal (serial.begin()), therefore they are not available if you user serial user input over the terminal.
//  There is no over the air serial output for ESP8266.
// Code, Pinnumber, Pinname, Usual Function, behaviour:
// lc ,         16,    "D0",               , no pull up, wake from sleep, no PWM or I2C, no interrupt, high at boot
// 2  ,          5,    "D1",           SCL , avail
// 2  ,          4,    "D2",           SDA , avail
// 2  ,          0,    "D3",               , pulled up, connected to flash, boot fails if pulled low
// 2  ,          2,    "D4",               , pulled up, high at boot, boot fails if pulled low
// 2  ,         14,    "D5",           SCK , avail
// 2  ,         12,    "D6",           MISO, avail
// 2  ,         13,    "D7",           MOSI, avail
// li ,         15,    "D8",             SS, pulled low, cannot use pull up, boot fails if pulled high
// 2  ,          3,    "RX",               , high at boot, goes to USB
// 2  ,          1,    "TX",               , high at boot, goes to USB, boot failure if pulled low
//
// Sensor characteristics
// NAME  Power Sup, Clock Speed, Clock Stretch, Pull ups on breakout board, Board name
// ------------------------------------------------------------------------------------------
// LCD20x4      5V,      100kHz,             ?                           ?, LCD display
// LEVEL    3.3&5V             ,           No,                          5k, Level Shifter
// SPS30    3.3&5V,      100kHz,           No,                         Inf, SPS30 Senserion particle, not compatible with LCD driver, power supply needs 5V, signal can be 3.3V (with pullup to 3.3V) or 5V (with pullup to 5V)
// SCD30    3.3-5V,      100kHz,          150,                         Inf, SCD30 Senserion CO2, output high level is 2.4V (TTL standard is 2.7V)
// SGP30  1.8-3.3V,      400kHz,           No,                         10k, SGP30 Senserion VOC, eCO2
// BME680 1.8-3.3V,      400kHz,           No,                         10k, BME680 Bosch Temp, Humidity, Pressure, VOC
// CCS811 1.8-3.6V,      100kHz,          Yes,                          5k, CCS811 Airquality eCO2 tVOC
// MLX       3or5V,      100kHz,           No,                         Inf, MLX90614 Melex temp contactless
// MAX    1.8&3.3V,      400kHz,           No,                          5k, MAX30105 Maxim pulseox
//
// Sensirion sensors without pullups need an external 10k pullup on SDA and SCL according the driver documentation from Paul Vah.
// ESP8266 has builtin pullups on standard i2c pins, but it appears external ones are needed in addition.
// If another sensor board on the same bus aledady has pullup resistors, there would no additional pullups be needed.
// All pullups will add in parallel and lower the total pullup resistor value, therfore requiring more current to operate the i2c bus.
//
// I2C interfaces
// -------------------------------------------------
// I2C_1 D1 D2 400kHz no clock stretch
//   SGP30     58
//   BME680    77
//   MAX30105  57
//
// I2C_2 D3 D4 100kHz with extra clock stretch
//   SPS30     69, with 5V power and pullup to 3.3V
//   SCD30     61
//   CCS811    5B
//
// I2C_3 D5 D6 100kHz no extra clock stretch
//   LCD       27, after 5V Level Shifter, this sensor affects operation of other sensors
//   MLX90614  5A, to 3.3V side, this sensor might be affected by other breakout boards and erroniously reporting negative temp and temp above 500C.
//
// Wake and Interrupt Pin assignements
// ------------------------------------
// D8: SCD30_RDY interrupt, goes high when data available, remains high until data is read,
//   this requires the sensors to be power switched after programming,
//   if the sensor has data available, the pin is high and the mcu can not be programmed
//
// D0: CCS811_Wake, needs to be low when communicating with sensors, most of the time its high
//
// D7: CCS811_INT, goes low when data is available, most of the time its high, needs pull up configuration via software
//
// CCS811 address needs to be set to high which is accomplished with connecting adress pin to 3.3V signal
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////////
//
const bool fastMode = true;
// true:  Measure with about 5 sec intervals,
// false: Measure in about 1 min intervals and enable sleeping and low energy modes if available
// slow mode has not been tested thoroughly
// currently can not switch between slow and fast during runtime


// #define ADALCD                                          // we have Adafruit i2c to LCD driver


//#define DEBUG true  // enabling this will pin point where program crashed

/******************************************************************************************************/
// Blinking / Warning
/******************************************************************************************************/
//
/// Blinking settings due to sensors warnings, this allows off/on intervals to be different



/******************************************************************************************************/
// I2C
/******************************************************************************************************/
//
# 203 "d:\\GitHub\\ESP Playground\\AirQuality\\software\\Unified Software\\Sensi\\Sensi.ino" 2
TwoWire myWire = TwoWire(); //  inintialize the two wire system.
// We will set multiple i2c ports for each sensor and we will switch pins each time we communicate to an other i2c bus.
// Unfortunately ESP8266 Arduino supports only one i2c instance because the wire library calls twi.h for which only one instance can run.
// Each time we commounicte with a device we need to set SDA and SCL pins.
# 208 "d:\\GitHub\\ESP Playground\\AirQuality\\software\\Unified Software\\Sensi\\Sensi.ino" 2

/******************************************************************************************************/
// Main Program Definitions
/******************************************************************************************************/
# 213 "d:\\GitHub\\ESP Playground\\AirQuality\\software\\Unified Software\\Sensi\\Sensi.ino" 2

/******************************************************************************************************/
// Time and Timezone
/******************************************************************************************************/
//#include <Time.h>

/******************************************************************************************************/
// Settings
/******************************************************************************************************/
// Use EEPROM and LittleFS to store settings in binary as well as JSON file
// Check src/Config.h for the available settings
# 225 "d:\\GitHub\\ESP Playground\\AirQuality\\software\\Unified Software\\Sensi\\Sensi.ino" 2
Settings mySettings; // the settings
File uploadFile;

/******************************************************************************************************/
// WiFi, MQTT, NTP, OTA
/******************************************************************************************************/
// Enables most web services available for ESP8266
// It is advisable to initialize only the ones you need in the configuration file.
// It is not advised to run webserver and mqtt client concurrenlty
# 235 "d:\\GitHub\\ESP Playground\\AirQuality\\software\\Unified Software\\Sensi\\Sensi.ino" 2
# 236 "d:\\GitHub\\ESP Playground\\AirQuality\\software\\Unified Software\\Sensi\\Sensi.ino" 2
# 237 "d:\\GitHub\\ESP Playground\\AirQuality\\software\\Unified Software\\Sensi\\Sensi.ino" 2
# 238 "d:\\GitHub\\ESP Playground\\AirQuality\\software\\Unified Software\\Sensi\\Sensi.ino" 2

ESP8266WiFiMulti wifiMulti; // 

WebSocketsServer webSocket = WebSocketsServer(81);

WiFiClient WiFiClient; // The WiFi interface
PubSubClient mqttClient(WiFiClient); // The MQTT interface

/******************************************************************************************************/
// WebServer
/******************************************************************************************************/
// Enable webserver and web firmware updated (this alows user to upload firmware via the web)
# 251 "d:\\GitHub\\ESP Playground\\AirQuality\\software\\Unified Software\\Sensi\\Sensi.ino" 2
ESP8266WebServer httpServer(80); // Server on port 80

# 254 "d:\\GitHub\\ESP Playground\\AirQuality\\software\\Unified Software\\Sensi\\Sensi.ino" 2
ESP8266WebServer httpUpdateServer(8890);
ESP8266HTTPUpdateServer httpUpdater; // Code update server


/******************************************************************************************************/
// SCD30; Sensirion CO2 sensor
/******************************************************************************************************/
// Likely  most accurate sensor for CO2
# 263 "d:\\GitHub\\ESP Playground\\AirQuality\\software\\Unified Software\\Sensi\\Sensi.ino" 2

/******************************************************************************************************/
// SPS30; Senserion Particle Sensor
/******************************************************************************************************/
// Likely most accurate sensor for PM2. PM110 might not be accurate though.
# 269 "d:\\GitHub\\ESP Playground\\AirQuality\\software\\Unified Software\\Sensi\\Sensi.ino" 2

/******************************************************************************************************/
// SGP30; Senserion TVOC, eCO2
/******************************************************************************************************/
// Accuracy is not known
# 275 "d:\\GitHub\\ESP Playground\\AirQuality\\software\\Unified Software\\Sensi\\Sensi.ino" 2

/******************************************************************************************************/
// CCS811; Airquality CO2 tVOC
/******************************************************************************************************/
// Traditional make comunity sensor
# 281 "d:\\GitHub\\ESP Playground\\AirQuality\\software\\Unified Software\\Sensi\\Sensi.ino" 2

/******************************************************************************************************/
// BME680; Bosch Temp, Humidity, Pressure, VOC
/******************************************************************************************************/
// More features than 280 sensor, dont need both (280 and 680)
# 287 "d:\\GitHub\\ESP Playground\\AirQuality\\software\\Unified Software\\Sensi\\Sensi.ino" 2

/******************************************************************************************************/
// BME280; Bosch Temp, Humidity, Pressure
/******************************************************************************************************/
// There is BME and BMP version.  One is lacking hymidity sensor.
// Has no user defined settings
# 294 "d:\\GitHub\\ESP Playground\\AirQuality\\software\\Unified Software\\Sensi\\Sensi.ino" 2

/******************************************************************************************************/
// MLX contact less tempreture sensor
/******************************************************************************************************/
// Thermal sensor for forehead readings. Likely not accurate.
# 300 "d:\\GitHub\\ESP Playground\\AirQuality\\software\\Unified Software\\Sensi\\Sensi.ino" 2

/******************************************************************************************************/
// MAX30105; pulseoximeter
/******************************************************************************************************/
// Not implemented yet
//
const long intervalMAX30 = 10; // readout intervall in ms
# 308 "d:\\GitHub\\ESP Playground\\AirQuality\\software\\Unified Software\\Sensi\\Sensi.ino" 2
//

/******************************************************************************************************/
// LCD Display
/******************************************************************************************************/
// Classic display, uses "old" driver software. Update is slow.
// Maybe replace later with other display.




# 320 "d:\\GitHub\\ESP Playground\\AirQuality\\software\\Unified Software\\Sensi\\Sensi.ino" 2
  LiquidCrystal_PCF8574 lcd(0x27); // set the LCD address to 0x27 

# 323 "d:\\GitHub\\ESP Playground\\AirQuality\\software\\Unified Software\\Sensi\\Sensi.ino" 2
# 324 "d:\\GitHub\\ESP Playground\\AirQuality\\software\\Unified Software\\Sensi\\Sensi.ino" 2

/******************************************************************************************************/
// Signal Interpretation
/******************************************************************************************************/
// See inside source code for the assessments and thresholds.
# 330 "d:\\GitHub\\ESP Playground\\AirQuality\\software\\Unified Software\\Sensi\\Sensi.ino" 2

//////////////////////////////////////////////////////////////////////////////////////////////////////////
// Lets get started
//////////////////////////////////////////////////////////////////////////////////////////////////////////
void setup() {

  Serial.begin(115200 /* serial communicaiton speed, terminal settings need to match*/);
  Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "337" "." "159" "\", \"aSM\", @progbits, 1 #"))) = (""); &__c[0];}))))));;

  const char waitmsg[] = "Waiting 10 seconds, skip by hitting enter"; // Allows user to open serial terminal to observe the debug output
  serialTrigger((char *) waitmsg, 10000);

  // Initialize wire interface
  // These settings might get changed inside the sensor drivers and should be reasserted after initialization.
  myWire.begin(D1, D2);
  myWire.setClock(100000); // 100kHz or 400kHz speed
  myWire.setClockStretchLimit(200000);

  /******************************************************************************************************/
  // Configuration setup and read
  /******************************************************************************************************/
  tmpTime = millis();
  EEPROM.begin(2048 /* make sure this value is larger than the space required by the settings below and lowwer than the max Settings of the microcontroller*/);
  EEPROM.get(eepromAddress, mySettings);

  /******************************************************************************************************/
  // Inject hard coded values into the settungs if needed. Useful when expanding settings.
  mySettings.debuglevel = 1; // enable debug regardless of eeprom setting
  //mySettings.useLCD    = true;  // enable/disable sensors even if they are connected
  //mySettings.useWiFi   = true;  //
  //mySettings.useSCD30  = true;  //
  //mySettings.useSPS30  = true;  //
  //mySettings.useSGP30  = true;  //
  //mySettings.useMAX30  = true;  //
  //mySettings.useMLX    = true;  //
  //mySettings.useBME680 = true;  //
  //mySettings.useBME280 = true;  //
  //mySettings.useCCS811 = true;  //
  //mySettings.consumerLCD = true;  //
  //mySettings.sendMQTTimmediate = true; //
  //mySettings.useBacklight = true; //
  //strcpy(mySettings.mqtt_mainTopic, "Sensi");
  //mySettings.useMQTT = false;
  //mySettings.useNTP  = true;
  //mySettings.usemDNS = false;
  //mySettings.useHTTP = false;
  //mySettings.useOTA  = false;
  //strcpy(mySettings.ntpServer, "time.nist.gov");
  //mySettings.utcOffset = -7;
  //mySettings.enableDST = false;
  strcpy(mySettings.timeZone, "MST7"); //https://raw.githubusercontent.com/nayarsystems/posix_tz_db/master/zones.csv

  if (mySettings.debuglevel > 0) { Serial.print(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "382" "." "160" "\", \"aSM\", @progbits, 1 #"))) = ("EEPROM read in: "); &__c[0];})))))); Serial.print((millis() - tmpTime)); Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "382" "." "161" "\", \"aSM\", @progbits, 1 #"))) = ("ms"); &__c[0];})))))); }

  /******************************************************************************************************/
  // Set debug output level
  /******************************************************************************************************/
  dbglevel = mySettings.debuglevel;
  if (dbglevel > 0) {
    Serial.print(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "389" "." "162" "\", \"aSM\", @progbits, 1 #"))) = ("Debug level is: "); &__c[0];}))))));
    Serial.println(dbglevel);
  }

  /******************************************************************************************************/
  // Start file system and list content
  /******************************************************************************************************/
  if(!LittleFS.begin()){ Serial.println("LittleFS mount failed!"); }
  if (dbglevel > 0) {
    Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "398" "." "163" "\", \"aSM\", @progbits, 1 #"))) = ("LittleFS started. Contents:"); &__c[0];}))))));
    Dir dir = LittleFS.openDir("/");
    while (dir.next()) { // List the file system contents
      String fileName = dir.fileName();
      size_t fileSize = dir.fileSize();
      Serial.printf("\tName: %s, Size: %s\r\n", fileName.c_str(), formatBytes(fileSize).c_str());
    }
    Serial.printf("\n");
  }

  //File myFile = LittleFS.open("/Sensi.config", "r");
  //myFile.read((byte *)&mySettings, sizeof(mySettings));
  //myFile.close();

  /******************************************************************************************************/
  // When EEPROM content is used for the first time, it has random content.
  // A boolen uses one byte of memory in the EEPROM and when you read from EEPROM to a boolen variable 
  // it can become uint_8. Setting a boolen true to false will convert from 255 to 254 and not to 0.
  // At boot time we  need to ensure that any non zero value for boolean settings are forced to true.
  if (mySettings.useLCD > 0) { mySettings.useLCD = true; }
  if (mySettings.useWiFi > 0) { mySettings.useWiFi = true; }
  if (mySettings.useSCD30 > 0) { mySettings.useSCD30 = true; }
  if (mySettings.useSPS30 > 0) { mySettings.useSPS30 = true; }
  if (mySettings.useMAX30 > 0) { mySettings.useMAX30 = true; }
  if (mySettings.useMLX > 0) { mySettings.useMLX = true; }
  if (mySettings.useBME680 > 0) { mySettings.useBME680 = true; }
  if (mySettings.useCCS811 > 0) { mySettings.useCCS811 = true; }
  if (mySettings.sendMQTTimmediate > 0) { mySettings.sendMQTTimmediate = true; }
  if (mySettings.consumerLCD > 0) { mySettings.consumerLCD = true; }
  if (mySettings.useBME280 > 0) { mySettings.useBME280 = true; }
  if (mySettings.useBacklight > 0) { mySettings.useBacklight = true; }
  if (mySettings.useHTTP > 0) { mySettings.useHTTP = true; }
  if (mySettings.useHTTPUpdater > 0) { mySettings.useHTTPUpdater = true; }
  if (mySettings.useNTP > 0) { mySettings.useNTP = true; }
  if (mySettings.useMQTT > 0) { mySettings.useMQTT = true; }
  if (mySettings.useOTA > 0) { mySettings.useOTA = true; }
  if (mySettings.usemDNS > 0) { mySettings.usemDNS = true; }

  /******************************************************************************************************/
  // Check which devices are attached to the I2C pins, this self configures our connections to the sensors
  /******************************************************************************************************/
  // Wemos 
  // uint8_t portArray[] = {16, 5, 4, 0, 2, 14, 12, 13};
  // String portMap[] = {"D0", "D1", "D2", "D3", "D4", "D5", "D6", "D7"};

  // dont search on ports used for data ready signaling
  uint8_t portArray[] = {5, 4, 0, 2, 14, 12};
  String portMap[] = {"D1", "D2", "D3", "D4", "D5", "D6"};
  byte error, address;
  char tmpStr[48];

  // lets turn on CCS811 so we can find it
  pinMode(D0 /* normal high, active low to wake up sensor, might need to pull low and high w resistor*/, 0x01); // CCS811 not Wake Pin
  digitalWrite(D0 /* normal high, active low to wake up sensor, might need to pull low and high w resistor*/, 0x0); // set CCS811 to wake up

  for (uint8_t i = 0; i < sizeof(portArray); i++) {
    for (uint8_t j = 0; j < sizeof(portArray); j++) {
      if (i != j) {
        if (dbglevel > 0) { Serial.println("Scanning (SDA : SCL) - " + portMap[i] + " : " + portMap[j] + " - "); }
        Wire.begin(portArray[i], portArray[j]);
        for (address = 1; address < 127; address++ ) {
          Wire.beginTransmission(address);
          error = Wire.endTransmission();
          if (error == 0) {
            // found a device
            if (address == 0x20) {
              lcd_avail = true; // LCD display Adafruit
              lcd_port = &myWire;
              lcd_i2c[0] = portArray[i];
              lcd_i2c[1] = portArray[j];
            }
            else if (address == 0x27) {
              lcd_avail = true; // LCD display PCF8574
              lcd_port = &myWire;
              lcd_i2c[0] = portArray[i];
              lcd_i2c[1] = portArray[j];
            }
            else if (address == 0x57) {
              max_avail = true; // MAX 30105 Pulseox & Particle
              max_port = &myWire;
              max_i2c[0] = portArray[i];
              max_i2c[1] = portArray[j];
            }
            else if (address == 0x58) {
              sgp30_avail = true; // Senserion TVOC eCO2
              sgp30_port = &myWire;
              sgp30_i2c[0] = portArray[i];
              sgp30_i2c[1] = portArray[j];
            }
            else if (address == 0x5A) {
              therm_avail = true; // MLX IR sensor
              mlx_port = &myWire;
              mlx_i2c[0] = portArray[i];
              mlx_i2c[1] = portArray[j];
            }
            else if (address == 0x5B) {
              ccs811_avail = true; // Airquality CO2 tVOC
              ccs811_port = &myWire;
              ccs811_i2c[0] = portArray[i];
              ccs811_i2c[1] = portArray[j];
            }
            else if (address == 0x61) {
              scd30_avail = true; // Senserion CO2
              scd30_port = &myWire;
              scd30_i2c[0] = portArray[i];
              scd30_i2c[1] = portArray[j];
            }
            else if (address == 0x69) {
              sps30_avail = true; // Senserion Particle
              sps30_port = &myWire;
              sps30_i2c[0] = portArray[i];
              sps30_i2c[1] = portArray[j];
            }
            else if (address == 0x76) {
              bme280_avail = true; // Bosch Temp, Humidity, Pressure
              bme280_port = &myWire;
              bme280_i2c[0] = portArray[i];
              bme280_i2c[1] = portArray[j];
            }
            else if (address == 0x77) {
              bme680_avail = true; // Bosch Temp, Humidity, Pressure, VOC
              bme680_port = &myWire;
              bme680_i2c[0] = portArray[i];
              bme680_i2c[1] = portArray[j];
            }
            else if (dbglevel > 0) {
              Serial.print("Found unknonw device - "); Serial.print(address); Serial.println(" !");
            }
          }
        }
      }
    }
  }

  // List the devices we found
  if (dbglevel > 0) {
    Serial.print(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "534" "." "164" "\", \"aSM\", @progbits, 1 #"))) = ("LCD                  "); &__c[0];}))))));
    if (lcd_avail) { sprintf(tmpStr, "available:  %d SDA %d SCL %d", uint32_t(lcd_port), lcd_i2c[0], lcd_i2c[1]); Serial.println(tmpStr); } else { Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "535" "." "165" "\", \"aSM\", @progbits, 1 #"))) = ("not available"); &__c[0];})))))); }
    Serial.print(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "536" "." "166" "\", \"aSM\", @progbits, 1 #"))) = ("MAX30105             "); &__c[0];}))))));
    if (max_avail) { sprintf(tmpStr, "available:  %d SDA %d SCL %d", uint32_t(max_port), max_i2c[0], max_i2c[1]); Serial.println(tmpStr); } else { Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "537" "." "167" "\", \"aSM\", @progbits, 1 #"))) = ("not available"); &__c[0];})))))); }
    Serial.print(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "538" "." "168" "\", \"aSM\", @progbits, 1 #"))) = ("CCS811 eCO2, tVOC    "); &__c[0];}))))));
    if (ccs811_avail) { sprintf(tmpStr, "available:  %d SDA %d SCL %d", uint32_t(ccs811_port), ccs811_i2c[0], ccs811_i2c[1]); Serial.println(tmpStr); } else { Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "539" "." "169" "\", \"aSM\", @progbits, 1 #"))) = ("not available"); &__c[0];})))))); }
    Serial.print(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "540" "." "170" "\", \"aSM\", @progbits, 1 #"))) = ("SGP30 eCO2, tVOC     "); &__c[0];}))))));
    if (sgp30_avail) { sprintf(tmpStr, "available:  %d SDA %d SCL %d", uint32_t(sgp30_port), sgp30_i2c[0], sgp30_i2c[1]); Serial.println(tmpStr); } else { Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "541" "." "171" "\", \"aSM\", @progbits, 1 #"))) = ("not available"); &__c[0];})))))); }
    Serial.print(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "542" "." "172" "\", \"aSM\", @progbits, 1 #"))) = ("MLX temp             "); &__c[0];}))))));
    if (therm_avail) { sprintf(tmpStr, "available:  %d SDA %d SCL %d", uint32_t(mlx_port), mlx_i2c[0], mlx_i2c[1]); Serial.println(tmpStr); } else { Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "543" "." "173" "\", \"aSM\", @progbits, 1 #"))) = ("not available"); &__c[0];})))))); }
    Serial.print(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "544" "." "174" "\", \"aSM\", @progbits, 1 #"))) = ("SCD30 CO2, rH        "); &__c[0];}))))));
    if (scd30_avail) { sprintf(tmpStr, "available:  %d SDA %d SCL %d", uint32_t(scd30_port), scd30_i2c[0], scd30_i2c[1]); Serial.println(tmpStr); } else { Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "545" "." "175" "\", \"aSM\", @progbits, 1 #"))) = ("not available"); &__c[0];})))))); }
    Serial.print(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "546" "." "176" "\", \"aSM\", @progbits, 1 #"))) = ("SPS30 PM             "); &__c[0];}))))));
    if (sps30_avail) { sprintf(tmpStr, "available:  %d SDA %d SCL %d", uint32_t(sps30_port), sps30_i2c[0], sps30_i2c[1]); Serial.println(tmpStr); } else { Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "547" "." "177" "\", \"aSM\", @progbits, 1 #"))) = ("not available"); &__c[0];})))))); }
    Serial.print(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "548" "." "178" "\", \"aSM\", @progbits, 1 #"))) = ("BM[E/P]280 T, P[, rH]"); &__c[0];}))))));
    if (bme280_avail) { sprintf(tmpStr, "available:  %d SDA %d SCL %d", uint32_t(bme280_port), bme280_i2c[0], bme280_i2c[1]); Serial.println(tmpStr); } else { Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "549" "." "179" "\", \"aSM\", @progbits, 1 #"))) = ("not available"); &__c[0];})))))); }
    Serial.print(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "550" "." "180" "\", \"aSM\", @progbits, 1 #"))) = ("BME680 T, rH, P tVOC "); &__c[0];}))))));
    if (bme680_avail) { sprintf(tmpStr, "available:  %d SDA %d SCL %d", uint32_t(bme680_port), bme680_i2c[0], bme680_i2c[1]); Serial.println(tmpStr); } else { Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "551" "." "181" "\", \"aSM\", @progbits, 1 #"))) = ("not available"); &__c[0];})))))); }
  }

  // serialTrigger((char *) waitmsg, 10000);

  /******************************************************************************************************/
  // Time Intervals for Loop, Display and MQTT publishing, keeping track of runtime
  /******************************************************************************************************/
  // Software is designed to run either with frequent updates or in slow mode with updates in the minutes

  if (fastMode == true) { // fast mode -----------------------------------
    intervalLoop = 100; // 0.1 sec, main loop runs 10 times per second
    intervalLCD = 5000 /* 5sec*/; // LCD display is updated every 10 seconds
    intervalMQTT = 500 /* check for MQTT conenction and incoming data every*/; //
    intervalRuntime = 60000; // 1 minute, uptime is updted every minute
    intervalBaseline = 600000 /* 10 minutes*/; // 10 minutes
  } else { // slow mode -----------------------------------
    intervalLoop = 1000; // 1 sec, main loop runs once a second
    intervalLCD = 60000 /* 1min*/; // 1 minute
    intervalMQTT = 5000 /* */; //
    intervalRuntime = 600000; // 10 minutes
    intervalBaseline = 3600000 /* 1 hour*/; // 1 hr
  }

  /******************************************************************************************************/
  // Initialize all devices
  /******************************************************************************************************/

  if (lcd_avail && mySettings.useLCD) { if (initializeLCD() == false) { lcd_avail = false; } } else { lcd_avail = false; } // Initialize LCD Screen
  if (false) { Serial.println("DBG:INI: LCD"); }
  if (scd30_avail && mySettings.useSCD30) { if (initializeSCD30() == false) { scd30_avail = false; } } else { scd30_avail = false; } // Initialize SCD30 CO2 sensor
  if (false) { Serial.println("DBG:INI: SCD30"); }
  if (sgp30_avail && mySettings.useSGP30) { if (initializeSGP30() == false) { sgp30_avail = false; } } else { sgp30_avail = false; } // SGP30 Initialize
  if (false) { Serial.println("DBG:INI: SGP30"); }
  if (ccs811_avail && mySettings.useCCS811) { if (initializeCCS811() == false) { ccs811_avail = false; } } else { ccs811_avail = false; } // CCS811 Initialize
  if (false) { Serial.println("DBG:INI: CCS811"); }
  if (sps30_avail && mySettings.useSPS30) { if (initializeSPS30() == false) { sps30_avail = false; } } else { sps30_avail = false; } // SPS30 Initialize Particle Sensor
  if (false) { Serial.println("DBG:INI: SPS30"); }
  if (bme680_avail && mySettings.useBME680) { if (initializeBME680() == false) { bme680_avail = false; } } else { bme680_avail = false; } // Initialize BME680
  if (false) { Serial.println("DBG:INI: BME680"); }
  if (bme280_avail && mySettings.useBME280) { if (initializeBME280() == false) { bme280_avail = false; } } else { bme280_avail = false; } // Initialize BME280
  if (false) { Serial.println("DBG:INI: BME280"); }
  if (therm_avail && mySettings.useMLX) { if (initializeMLX() == false) { therm_avail = false; } } else { therm_avail = false; } // Initialize MLX Sensor
  if (false) { Serial.println("DBG:INI: MLX"); }
  if (max_avail && mySettings.useMAX30) { } else { max_avail = false; } // Initialize MAX Pulse OX Sensor, N.A.
  if (false) { Serial.println("DBG:INI: MAX30"); }

  /******************************************************************************************************/
  // Make sure i2c bus was not changed during above's initializations.
  // These settings will work for all sensors supported.
  // Since one has only one wire instance available on EPS8266, it does not make sense to operate the
  // pins at differnt clock speeds.
  /******************************************************************************************************/
  myWire.setClock(100000);
  myWire.setClockStretchLimit(200000);

  /******************************************************************************************************/
  // Initialize Timing System
  /******************************************************************************************************/
  currentTime = millis();
  lastcurrentTime = currentTime;
  lastTime = currentTime;
  lastSCD30 = currentTime;
  lastLCD = currentTime;
  lastPressureSCD30 = currentTime;
  lastSaveSettings = currentTime;
  lastSaveSettingsJSON= currentTime;
  lastMAX = currentTime;
  lastMLX = currentTime;
  lastCCS811 = currentTime;
  lastCCS811Humidity = currentTime;
  lastCCS811Interrupt = currentTime;
  lastSCD30 = currentTime;
  lastSCD30Busy = currentTime;
  lastSPS30 = currentTime;
  lastBME280 = currentTime;
  lastBME680 = currentTime;
  lastSGP30 = currentTime;
  lastSGP30Humidity = currentTime;
  lastSGP30Baseline = currentTime;
  lastLCDReset = currentTime;
  lastWiFi = currentTime;
  lastMQTT = currentTime;
  lastHTTP = currentTime;
  lastMDNS = currentTime;
  lastHTTP = currentTime;
  lastMQTTPublish = currentTime;
  lastBlink = currentTime;
  lastBaseline = currentTime;
  lastWebSocket = currentTime;
  lastWarning = currentTime;

  /******************************************************************************************************/
  // Populate LCD screen, start with cleared LCD
  /******************************************************************************************************/
  if (lcd_avail && mySettings.useLCD) {
    tmpTime = millis();
    if (mySettings.consumerLCD) { updateSinglePageLCD(); } else { updateLCD(); }
    lastLCD = currentTime;
    if (dbglevel > 0) { Serial.print(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "650" "." "182" "\", \"aSM\", @progbits, 1 #"))) = ("LCD updated in "); &__c[0];})))))); Serial.print((millis() - tmpTime)); Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "650" "." "183" "\", \"aSM\", @progbits, 1 #"))) = ("ms"); &__c[0];})))))); }
  }
  if (false) { Serial.println("DBG:SET: LCD"); }

  /******************************************************************************************************/
  // Connect to WiFi
  /******************************************************************************************************/
  initializeWiFi(); // if user settings are set to off, functions depebding on wifi will not be enbaled
  if (false) { Serial.println("DBG:INI: WiFi"); }

  /******************************************************************************************************/
  // System is initialized
  /******************************************************************************************************/
  if (dbglevel > 0) { Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "663" "." "184" "\", \"aSM\", @progbits, 1 #"))) = ("System initialized"); &__c[0];})))))); }
  if (false) { Serial.println("DBG:INI: COMPLETED"); }

  scheduleReboot = false;

  serialTrigger((char *) waitmsg, 10000);

} // end setup

//////////////////////////////////////////////////////////////////////////////////////////////////////////
// The never ending story
//////////////////////////////////////////////////////////////////////////////////////////////////////////
void loop() {
  //////////////////////////////////////////////////////////////////////////////////////////////////////////
  //////////////////////////////////////////////////////////////////////////////////////////////////////////
  currentTime = millis(); // keep track of local time

  if (otaInProgress) { // when OTA is in progress we do not do anything else
    updateOTA();
  } else { // OTA is not in progress, we update the subsystems

    // update current time every second
    if ( (currentTime - lastcurrentTime) >= 1000 ) {
      startUpdate = millis(); // how long does this code segment take?
      if (false) { Serial.println("DBG:UPDATE: DATE&TIME"); }
      lastcurrentTime = currentTime;

      actualTime = time(__null);
      localTime = localtime(&actualTime); // convert to localtime with daylight saving time

      // every minute, broadcast time, window to occur is 0..29 secs
      // we need to give window because some service routines of ESP might block execution for several seconds
      // and we can not guarantee that we reach this code at exactly specified second
      if (localTime->tm_sec < 30) {
        if (updateTime == true) {
          timeNewDataWS = true; //brodcast time on websocket 
          timeNewData = true; //brodcast time on mqtt
          updateTime = false;
        }
      } else {
        if (updateTime == false) { updateTime = true; }
      }

      // at midnight update date, window to occur is one minute
      if (localTime->tm_hour == 0) {
        if (localTime->tm_min == 0) {
          if (updateDate == true) {
            dateNewDataWS = true; // broadcast date on websocket
            dateNewData = true; // broadcast date on mqtt
            updateDate = false;
          }
        } else {
          if ( (localTime->tm_min >= 1) && (updateDate==false) ) { updateDate = true; }
        }
      }

      if (false) { Serial.printf("DBG:NTP:TIME ntp: %u local: %u, internal: %u\r\n", NTP.millis()/1000, (unsigned long) actualTime, currentTime/1000); }
      if (false) { Serial.printf("DBG:NTP:DATE %d:%d:%d %d,%d,%d\r\n",localTime->tm_hour, localTime->tm_min, localTime->tm_sec, localTime->tm_mon, localTime->tm_mday, localTime->tm_year);}

      deltaUpdate = millis() - startUpdate; // this code segment took delta ms
      if (maxUpdateTime < deltaUpdate) { maxUpdateTime = deltaUpdate; } // current max updatetime for this code segment
      if (AllmaxUpdateTime < deltaUpdate) { AllmaxUpdateTime = deltaUpdate; } // longest global updatetime for this code segement

    }

    /******************************************************************************************************/
    // Update the State Machines for all Devices and System Processes
    /******************************************************************************************************/
    // Update Wireless Services
    if (false) { Serial.println("DBG:UPDATE: WiFi"); }
    if (wifi_avail && mySettings.useWiFi) {
      startUpdate = millis();
      updateWiFi(); //<<<<<<<<<<<<<<<<<<<<
      deltaUpdate = millis() - startUpdate;
      if (maxUpdateWifi < deltaUpdate) {maxUpdateWifi = deltaUpdate;} ;
      if (AllmaxUpdateWifi < deltaUpdate) {AllmaxUpdateWifi = deltaUpdate;} ;
    } // -------------------------------------------------------------- WiFi update, this will reconnect if disconnected
    if (false) { Serial.println("DBG:UPDATE: OTA"); }
    if (wifi_avail && mySettings.useWiFi && mySettings.useOTA) {
      startUpdate = millis();
      updateOTA(); //<<<<<<<<<<<<<<<<<<<<
      deltaUpdate = millis() - startUpdate;
      if (maxUpdateOTA < deltaUpdate) { maxUpdateOTA = deltaUpdate; }
      if (AllmaxUpdateOTA < deltaUpdate) { AllmaxUpdateOTA = deltaUpdate; }
    } // -------------------------------------------------------------- Update OTA
    if (false) { Serial.println("DBG:UPDATE: NTP"); }
    if (wifi_avail && mySettings.useWiFi && mySettings.useNTP) {
      startUpdate = millis();
      updateNTP(); //<<<<<<<<<<<<<<<<<<<<
      deltaUpdate = millis() - startUpdate;
      if (maxUpdateNTP < deltaUpdate) { maxUpdateNTP = deltaUpdate; }
      if (AllmaxUpdateNTP < deltaUpdate) { AllmaxUpdateNTP = deltaUpdate; }
    } // -------------------------------------------------------------- Update network time
    if (false) { Serial.println("DBG:UPDATE: MQTT"); }
    if (wifi_avail && mySettings.useWiFi && mySettings.useMQTT) {
      startUpdate = millis();
      updateMQTT(); //<<<<<<<<<<<<<<<<<<<<
      deltaUpdate = millis() - startUpdate;
      if (maxUpdateMQTT < deltaUpdate) { maxUpdateMQTT = deltaUpdate; }
      if (AllmaxUpdateMQTT < deltaUpdate) { AllmaxUpdateMQTT = deltaUpdate; }
    } // -------------------------------------------------------------- MQTT update server connection
    if (false) { Serial.println("DBG:UPDATE: WS"); }
    if (wifi_avail && mySettings.useWiFi && mySettings.useHTTP) {
      startUpdate = millis();
      updateWebSocket(); //<<<<<<<<<<<<<<<<<<<<
      deltaUpdate = millis() - startUpdate;
      if (maxUpdateWS < deltaUpdate) { maxUpdateWS = deltaUpdate; }
      if (AllmaxUpdateWS < deltaUpdate) { AllmaxUpdateWS = deltaUpdate; }
    } // -------------------------------------------------------------- Websocket update server connection
    if (false) { Serial.println("DBG:UPDATE: HTTP"); }
    if (wifi_avail && mySettings.useWiFi && mySettings.useHTTP) {
      startUpdate = millis();
      updateHTTP(); //<<<<<<<<<<<<<<<<<<<<
      deltaUpdate = millis() - startUpdate;
      if (maxUpdateHTTP < deltaUpdate) { maxUpdateHTTP = deltaUpdate; }
      if (AllmaxUpdateHTTP < deltaUpdate) { AllmaxUpdateHTTP = deltaUpdate; }
    } // -------------------------------------------------------------- Update web server
    if (false) { Serial.println("DBG:UPDATE: HTTPUPDATER"); }
    if (wifi_avail && mySettings.useWiFi && mySettings.useHTTPUpdater) {
      startUpdate = millis();
      updateHTTPUpdater(); //<<<<<<<<<<<<<<<<<<<<
      deltaUpdate = millis() - startUpdate;
      if (maxUpdateHTTPUPDATER < deltaUpdate) { maxUpdateHTTPUPDATER = deltaUpdate; }
      if (AllmaxUpdateHTTPUPDATER < deltaUpdate) { AllmaxUpdateHTTPUPDATER = deltaUpdate; }
    } // -------------------------------------------------------------- Update firmware web server
    if (false) { Serial.println("DBG:UPDATE: MDSN"); }
    if (wifi_avail && mySettings.useWiFi && mySettings.usemDNS) {
      startUpdate = millis();
      updateMDNS(); //<<<<<<<<<<<<<<<<<<<<
      deltaUpdate = millis() - startUpdate;
      if (maxUpdatemDNS < deltaUpdate) { maxUpdatemDNS = deltaUpdate; }
      if (AllmaxUpdatemDNS < deltaUpdate) { AllmaxUpdatemDNS = deltaUpdate; }
    } // -------------------------------------------------------------- Update mDNS
    // Update Sensor Readings
    if (false) { Serial.println("DBG:UPDATE: SCD30"); }
    if (scd30_avail && mySettings.useSCD30) {
      startUpdate = millis();
      if (updateSCD30() == false) {scheduleReboot = true;} //<<<<<<<<<<<<<<<<<<<<
      deltaUpdate = millis() - startUpdate;
      if (maxUpdateSCD30 < deltaUpdate) { maxUpdateSCD30 = deltaUpdate; }
      if (AllmaxUpdateSCD30 < deltaUpdate) { AllmaxUpdateSCD30 = deltaUpdate; }
    } // -------------------------------------------------------------- SCD30 Sensirion CO2 sensor
    if (false) { Serial.println("DBG:UPDATE: SGP30"); }
    if (sgp30_avail && mySettings.useSGP30) {
      startUpdate = millis();
      if (updateSGP30() == false) {scheduleReboot = true;} //<<<<<<<<<<<<<<<<<<<<
      deltaUpdate = millis() - startUpdate;
      if (maxUpdateSGP30 < deltaUpdate) { maxUpdateSGP30 = deltaUpdate; }
      if (AllmaxUpdateSGP30 < deltaUpdate) { AllmaxUpdateSGP30 = deltaUpdate; }
    } // -------------------------------------------------------------- SGP30 Sensirion eCO2 sensor
    if (false) { Serial.println("DBG:UPDATE: CCS811"); }
    if (ccs811_avail && mySettings.useCCS811) {
      startUpdate = millis();
      if (updateCCS811() == false) {scheduleReboot = true;} //<<<<<<<<<<<<<<<<<<<<
      deltaUpdate = millis() - startUpdate;
      if (maxUpdateCCS811 < deltaUpdate) { maxUpdateCCS811 = deltaUpdate; }
      if (AllmaxUpdateCCS811 < deltaUpdate) { AllmaxUpdateCCS811 = deltaUpdate; }
    } // CCS811
    if (false) { Serial.println("DBG:UPDATE: SPS30"); }
    if (sps30_avail && mySettings.useSPS30) {
      startUpdate = millis();
      if (updateSPS30() == false) {scheduleReboot = true;} //<<<<<<<<<<<<<<<<<<<<
      deltaUpdate = millis() - startUpdate;
      if (maxUpdateSPS30 < deltaUpdate) { maxUpdateSPS30 = deltaUpdate; }
      if (AllmaxUpdateSPS30 < deltaUpdate) { AllmaxUpdateSPS30 = deltaUpdate; }
    } // -------------------------------------------------------------- SPS30 Sensirion Particle Sensor State Machine
    if (false) { Serial.println("DBG:UPDATE: BME280"); }
    if (bme280_avail && mySettings.useBME280) {
      startUpdate = millis();
      if (updateBME280() == false) {scheduleReboot = true;} //<<<<<<<<<<<<<<<<<<<<
      deltaUpdate = millis() - startUpdate;
      if (maxUpdateBME280 < deltaUpdate) { maxUpdateBME280 = deltaUpdate; }
      if (AllmaxUpdateBME280 < deltaUpdate) { AllmaxUpdateBME280 = deltaUpdate; }
    } // -------------------------------------------------------------- BME280, Hum, Press, Temp
    if (false) { Serial.println("DBG:UPDATE: BME680"); }
    if (bme680_avail && mySettings.useBME680) {
      startUpdate = millis();
      if (updateBME680() == false) {scheduleReboot = true;} //<<<<<<<<<<<<<<<<<<<<
      deltaUpdate = millis() - startUpdate;
      if (maxUpdateBME680 < deltaUpdate) { maxUpdateBME680 = deltaUpdate; }
      if (AllmaxUpdateBME680 < deltaUpdate) { AllmaxUpdateBME680 = deltaUpdate; }
    } // -------------------------------------------------------------- BME680, Hum, Press, Temp, Gasresistance
    if (false) { Serial.println("DBG:UPDATE: MLX"); }
    if (therm_avail && mySettings.useMLX) {
      startUpdate = millis();
      if (updateMLX() == false) {scheduleReboot = true;} //<<<<<<<<<<<<<<<<<<<<
      deltaUpdate = millis() - startUpdate;
      if (maxUpdateMLX < deltaUpdate) { maxUpdateMLX = deltaUpdate; }
      if (AllmaxUpdateMLX < deltaUpdate) { AllmaxUpdateMLX = deltaUpdate; }
    } // -------------------------------------------------------------- MLX Contactless Thermal Sensor
    if (false) { Serial.println("DBG:UPDATE: MAX30"); }
    if (max_avail && mySettings.useMAX30) {
      startUpdate = millis();
      deltaUpdate = millis() - startUpdate;
      if (maxUpdateMAX < deltaUpdate) { maxUpdateMAX = deltaUpdate; } //<<<<<<<<<<<<<<<<<<<<
      if (AllmaxUpdateMAX < deltaUpdate) { AllmaxUpdateMAX = deltaUpdate; }
    } // -------------------------------------------------------------- MAX Pulse Ox Sensor

    /******************************************************************************************************/
    // Broadcast MQTT and WebSocket Message
    /******************************************************************************************************/
    if (false) { Serial.println("DBG:UPDATE: MQTTMESSAGE"); }
    if (mqtt_connected) {
      startUpdate = millis();
      updateMQTTMessage(); //<<<<<<<<<<<<<<<<<<<<
      deltaUpdate = millis() - startUpdate;
      if (maxUpdateMQTTMESSAGE < deltaUpdate) { maxUpdateMQTTMESSAGE = deltaUpdate; }
      if (AllmaxUpdateMQTTMESSAGE < deltaUpdate) { AllmaxUpdateMQTTMESSAGE = deltaUpdate; }
    } // -------------------------------------------------------------- MQTT send sensor data
    if (false) { Serial.println("DBG:UPDATE: WSMESSAGE"); }
    if (ws_connected) {
      startUpdate = millis();
      updateWebSocketMessage(); //<<<<<<<<<<<<<<<<<<<<
      deltaUpdate = millis() - startUpdate;
      if (maxUpdateWSMESSAGE < deltaUpdate) { maxUpdateWSMESSAGE = deltaUpdate; }
      if (AllmaxUpdateWSMESSAGE < deltaUpdate) { AllmaxUpdateWSMESSAGE = deltaUpdate; }
    } // -------------------------------------------------------------- WebSocket send sensor data
    /******************************************************************************************************/

    /******************************************************************************************************/
    // LCD: Display Sensor Data
    /******************************************************************************************************/
    if (lcd_avail && mySettings.useLCD) {
      if ((currentTime - lastLCD) >= intervalLCD) {
        lastLCD = currentTime;
        if (false) { Serial.println("DBG:UPDATE: LCD"); }
        tmpTime = millis();
        if (mySettings.consumerLCD) { updateSinglePageLCD(); } else { updateLCD(); } //<<<<<<<<<<<<<<<<<<<<
        deltaUpdate = millis() - tmpTime;
        if (dbglevel > 0) { Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "892" "." "185" "\", \"aSM\", @progbits, 1 #"))) = ("LCD updated"); &__c[0];})))))); }
        if (maxUpdateLCD < deltaUpdate) { maxUpdateLCD = deltaUpdate; }
        if (AllmaxUpdateLCD < deltaUpdate) { AllmaxUpdateLCD = deltaUpdate; }
      }
    }

    /******************************************************************************************************/
    // Serial System Status Display
    /******************************************************************************************************/
    if ((currentTime - lastSYS) >= 2000 /* rate at which system information (CPU usage, Heap size) is displayed on serial port*/) {
      lastSYS = currentTime;
      if (dbglevel > 0) {
        startUpdate = millis();
        if (false) { Serial.println("DBG:UPDATE: SYS"); }
        clearAndHome();
        Serial.println("-----------------------------------------------------------------------------------------------------------------------------------------------------");
        // Updating these values over serial display takes longer than the average loop time, therefore the minimum loop delay is always negative (meaning there is not enough time to complete a loop every 1/10 second).
        // However average loop delay is calculated only from positive values.
        Serial.print(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "910" "." "186" "\", \"aSM\", @progbits, 1 #"))) = ("Loop Average Delay: "); &__c[0];})))))); Serial.print(myDelayAvg); Serial.print(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "910" "." "187" "\", \"aSM\", @progbits, 1 #"))) = ("[ms] Min: "); &__c[0];})))))); Serial.print(myDelayMin); Serial.print(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "910" "." "188" "\", \"aSM\", @progbits, 1 #"))) = ("[ms] Current: "); &__c[0];})))))); Serial.print(myDelay); Serial.println("[ms]");
        Serial.print(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "911" "." "189" "\", \"aSM\", @progbits, 1 #"))) = ("Free Heap Size: "); &__c[0];})))))); Serial.print(ESP.getFreeHeap(),10); Serial.print(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "911" "." "190" "\", \"aSM\", @progbits, 1 #"))) = ("bytes Heap Fragmentation: "); &__c[0];})))))); Serial.print(ESP.getHeapFragmentation()); Serial.print(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "911" "." "191" "\", \"aSM\", @progbits, 1 #"))) = ("% Max Block Size: "); &__c[0];})))))); Serial.print(ESP.getMaxFreeBlockSize()); Serial.println("bytes");
        // This displays the amount of time it took complete the update routines.
        // The currentMax/alltimeMax is displayed. Alltime max is reset when network services are started up in their update routines.
        Serial.printf("Update Time&Date: %u/%u \tmDNS:  %u/%u \tHTTPupd: %u/%u \tHTTP:    %u/%u \tMQTT:    %u/%u \tWS:     %u/%u \tNTP: %u/%u \tOTA: %u/%u \tWifi:  %u/%u \r\n",
          maxUpdateTime, AllmaxUpdateTime, maxUpdatemDNS, AllmaxUpdatemDNS, maxUpdateHTTPUPDATER, AllmaxUpdateHTTPUPDATER, maxUpdateHTTP, AllmaxUpdateHTTP, maxUpdateMQTT, AllmaxUpdateMQTT,
          maxUpdateWS, AllmaxUpdateWS, maxUpdateNTP, AllmaxUpdateNTP, maxUpdateOTA, AllmaxUpdateOTA, maxUpdateWifi, AllmaxUpdateWifi);
        Serial.printf("Update SCD30:     %u/%u \tSGP30: %u/%u \tCCS811:  %u/%u \tSPS30:   %u/%u \tBME280:  %u/%u \tBME680: %u/%u \tMLX: %u/%u \tMAX: %u/%u \r\n", maxUpdateSCD30, AllmaxUpdateSCD30,
          maxUpdateSGP30, AllmaxUpdateSGP30, maxUpdateCCS811, AllmaxUpdateCCS811, maxUpdateSPS30, AllmaxUpdateSPS30, maxUpdateBME280, AllmaxUpdateBME280, maxUpdateBME680, AllmaxUpdateBME680, maxUpdateMLX, AllmaxUpdateMLX, maxUpdateMAX, AllmaxUpdateMAX);
        Serial.printf("Update MQTTmsg:   %u/%u \tWSmsg: %u/%u \tLCD:     %u/%u \tINPUT:   %u/%u \tRunTime: %u/%u \tEEPROM: %u/%u \tJS:  %u/%u \r\n", maxUpdateMQTTMESSAGE, AllmaxUpdateMQTTMESSAGE, maxUpdateWSMESSAGE, AllmaxUpdateWSMESSAGE,
          maxUpdateLCD, AllmaxUpdateLCD, maxUpdateINPUT, AllmaxUpdateINPUT, maxUpdateRT, AllmaxUpdateRT, maxUpdateEEPROM, AllmaxUpdateEEPROM, maxUpdateJS, AllmaxUpdateJS);
        Serial.printf("Update Baseline:  %u/%u \tBlink: %u/%u \tReboot:  %u/%u \tAllGood: %u/%u \r\n", maxUpdateBASE, AllmaxUpdateBASE, AllmaxUpdateBLINK, maxUpdateBLINK, AllmaxUpdateREBOOT, maxUpdateREBOOT, AllmaxUpdateALLGOOD, maxUpdateALLGOOD);
        Serial.printf("Update this display:  %u \t all values in [ms]\r\n", maxUpdateSYS);
        Serial.println("-----------------------------------------------------------------------------------------------------------------------------------------------------");

        if (scd30_avail && mySettings.useSCD30) { Serial.printf("SCD30 CO2 %4.1f[ppm], rH %4.1f[%%], T %4.1f[C] T_offset %4.1f[C}\r\n", scd30_ppm, scd30_hum, scd30_temp, scd30.getTemperatureOffset() ); }
        if (bme680_avail && mySettings.useBME680) { Serial.printf("BME680 T %+5.1f[C] P %4.1f[mbar] P_ave %4.1f[mbar] rH %4.1f[%%] aH %4.1f[g/m^3] Gas resistance %d[Ohm]\r\n", bme680.temperature, bme680.pressure/100., bme680_pressure24hrs/100., bme680.humidity, bme680_ah, bme680.gas_resistance); }
        if (bme280_avail && mySettings.useBME280) { Serial.printf("BME280 T %+5.1f[C] P %4.1f[mbar] P_ave %4.1f[mbar]", bme280_temp, bme280_pressure/100., bme280_pressure24hrs/100.);
                                if (BMEhum_avail) { Serial.printf(" rH%4.1f[%%] aH%4.1f[g/m^3]\r\n", bme280_hum, bme280_ah);} else Serial.println();
        }
        if (sgp30_avail && mySettings.useSGP30) { Serial.printf("SGP30 CO2 %d[ppm] tVOC %d[ppb] H2 %d[ppb] Ethanol %d[ppm]\r\n", sgp30.CO2, sgp30.TVOC, sgp30.H2, sgp30.ethanol); }
        if (ccs811_avail && mySettings.useCCS811) {
          uint16_t CO2uI = ccs811.getCO2();
          uint16_t TVOCuI = ccs811.getTVOC();
          Serial.printf("CCS811 CO2 %d tVOC %d\r\n", CO2uI, TVOCuI);
        }
        if (sps30_avail && mySettings.useSPS30) {
          Serial.printf("SPS30 P1.0 %4.1f[μg/m3] P2.5 %4.1f[μg/m3] P4.0 %4.1f[μg/m3] P10 %4.1f[μg/m3]\r\n", valSPS30.MassPM1, valSPS30.MassPM2, valSPS30.MassPM4, valSPS30.MassPM10);
          Serial.printf("SPS30 P1.0 %4.1f[#/cm3] P2.5 %4.1f[#/cm3] P4.0 %4.1f[#/cm3] P10 %4.1f[#/cm3]\r\n", valSPS30.NumPM0, valSPS30.NumPM2, valSPS30.NumPM4, valSPS30.NumPM10);
          Serial.printf("SPS30 Average %4.1f[μm]\r\n", valSPS30.PartSize);
        }
        if (therm_avail && mySettings.useMLX) {
          float tmpOF = therm.object();
          float tmpAF = therm.ambient();
          Serial.printf("MLX Object %+5.1f[C] MLX Ambient %+5.1f[C]", tmpOF, tmpAF);
          Serial.printf(" MLX Offset %4.1f\r\n", mlxOffset);
        }
        if (max_avail && mySettings.useMAX30) {
        }

      // Member	  Type	Meaning	Range
      // tm_sec	  int	  seconds after the minute	0-61*
      // tm_min	  int	  minutes after the hour	  0-59
      // tm_hour	int	  hours since midnight	    0-23
      // tm_mday	int	  day of the month	        1-31
      // tm_mon	  int	  months since January	    0-11
      // tm_year	int	  years since 1900	
      // tm_wday	int	  days since Sunday	        0-6
      // tm_yday	int	  days since January 1	    0-365
      // tm_isdst	int	  Daylight Saving Time flag	

        Serial.println("-----------------------------------------------------------------------------------------------------------------------------------------------------");
        Serial.printf("NTP is: %s and time is %s\r\n", ntp_avail ? "available" : "not available", timeSynced ? "synchronized" : "not synchronized");
        String dateString = "Date: ";
        dateString+= weekDays[localTime->tm_wday];
        dateString+= " ";
        dateString+= months[localTime->tm_mon];
        dateString+= " ";
        dateString+= localTime->tm_mday;
        dateString+= " ";
        dateString+= localTime->tm_year;
        Serial.print(dateString);

        String timeString = " Time: ";
        timeString+= localTime->tm_hour/10 ? localTime->tm_hour/10 : 0;
        timeString+= localTime->tm_hour%10;
        timeString+= ":";
        timeString+= localTime->tm_min/10;
        timeString+= localTime->tm_min%10;
        timeString+= ":";
        timeString+= localTime->tm_sec/10;
        timeString+= localTime->tm_sec%10;
        Serial.println(timeString);
        Serial.printf("Daylight saving time is %s\r\n", (localTime->tm_isdst>0) ? "observed" : "not observed");
        Serial.println("-----------------------------------------------------------------------------------------------------------------------------------------------------");

        if (wifi_avail && mySettings.useWiFi) {
          Serial.printf("WiFi status: %s\r\n", WiFi.status() ? "on" : "off");
          Serial.printf("WiFi hostname: %s\r\n", hostName);
          if (wifi_connected) {
            Serial.printf("Connected to:   %s\r\n", WiFi.SSID().c_str());
            Serial.printf("IP address:     %s\r\n", WiFi.localIP().toString().c_str());
          } else {
            Serial.println("WiFi not connected");
          }
          Serial.print("WiFi State:         ");
          if (stateWiFi == START_UP) { Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "996" "." "192" "\", \"aSM\", @progbits, 1 #"))) = ("Starting Up"); &__c[0];}))))));}
          else if (stateWiFi == CHECK_CONNECTION) { Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "997" "." "193" "\", \"aSM\", @progbits, 1 #"))) = ("Monitoring connection"); &__c[0];})))))); }
          else if (stateWiFi == IS_SCANNING) { Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "998" "." "194" "\", \"aSM\", @progbits, 1 #"))) = ("Scanning for known network"); &__c[0];})))))); }
          else if (stateWiFi == IS_CONNECTING) { Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "999" "." "195" "\", \"aSM\", @progbits, 1 #"))) = ("Connecting"); &__c[0];})))))); }
          else if (stateWiFi == IS_WAITING) { Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "1000" "." "196" "\", \"aSM\", @progbits, 1 #"))) = ("Waiting"); &__c[0];})))))); }

          Serial.print("MDNS State:         ");
          if (stateMDNS == START_UP) { Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "1003" "." "197" "\", \"aSM\", @progbits, 1 #"))) = ("Starting Up"); &__c[0];})))))); }
          else if (stateMDNS == CHECK_CONNECTION) { Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "1004" "." "198" "\", \"aSM\", @progbits, 1 #"))) = ("Monitoring connection"); &__c[0];})))))); }
          else if (stateMDNS == IS_SCANNING) { Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "1005" "." "199" "\", \"aSM\", @progbits, 1 #"))) = ("Scanning for known network"); &__c[0];})))))); }
          else if (stateMDNS == IS_CONNECTING) { Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "1006" "." "200" "\", \"aSM\", @progbits, 1 #"))) = ("Connecting"); &__c[0];})))))); }
          else if (stateMDNS == IS_WAITING) { Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "1007" "." "201" "\", \"aSM\", @progbits, 1 #"))) = ("Waiting"); &__c[0];})))))); }

          Serial.print("MQTT State:         ");
          if (stateMQTT == START_UP) { Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "1010" "." "202" "\", \"aSM\", @progbits, 1 #"))) = ("Starting Up"); &__c[0];}))))));}
          else if (stateMQTT == CHECK_CONNECTION) { Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "1011" "." "203" "\", \"aSM\", @progbits, 1 #"))) = ("Monitoring connection"); &__c[0];})))))); }
          else if (stateMQTT == IS_SCANNING) { Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "1012" "." "204" "\", \"aSM\", @progbits, 1 #"))) = ("Scanning for known network"); &__c[0];})))))); }
          else if (stateMQTT == IS_CONNECTING) { Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "1013" "." "205" "\", \"aSM\", @progbits, 1 #"))) = ("Connecting"); &__c[0];})))))); }
          else if (stateMQTT == IS_WAITING) { Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "1014" "." "206" "\", \"aSM\", @progbits, 1 #"))) = ("Waiting"); &__c[0];})))))); }

          Serial.print("WebSocket State:    ");
          if (stateWebSocket == START_UP) { Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "1017" "." "207" "\", \"aSM\", @progbits, 1 #"))) = ("Starting Up"); &__c[0];}))))));}
          else if (stateWebSocket == CHECK_CONNECTION) { Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "1018" "." "208" "\", \"aSM\", @progbits, 1 #"))) = ("Monitoring connection"); &__c[0];})))))); }
          else if (stateWebSocket == IS_SCANNING) { Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "1019" "." "209" "\", \"aSM\", @progbits, 1 #"))) = ("Scanning for known network"); &__c[0];})))))); }
          else if (stateWebSocket == IS_CONNECTING) { Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "1020" "." "210" "\", \"aSM\", @progbits, 1 #"))) = ("Connecting"); &__c[0];})))))); }
          else if (stateWebSocket == IS_WAITING) { Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "1021" "." "211" "\", \"aSM\", @progbits, 1 #"))) = ("Waiting"); &__c[0];})))))); }

          Serial.print("OTA State:          ");
          if (stateOTA == START_UP) { Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "1024" "." "212" "\", \"aSM\", @progbits, 1 #"))) = ("Starting Up"); &__c[0];})))))); }
          else if (stateOTA == CHECK_CONNECTION) { Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "1025" "." "213" "\", \"aSM\", @progbits, 1 #"))) = ("Monitoring connection"); &__c[0];})))))); }
          else if (stateOTA == IS_SCANNING) { Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "1026" "." "214" "\", \"aSM\", @progbits, 1 #"))) = ("Scanning for known network"); &__c[0];})))))); }
          else if (stateOTA == IS_CONNECTING) { Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "1027" "." "215" "\", \"aSM\", @progbits, 1 #"))) = ("Connecting"); &__c[0];})))))); }
          else if (stateOTA == IS_WAITING) { Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "1028" "." "216" "\", \"aSM\", @progbits, 1 #"))) = ("Waiting"); &__c[0];})))))); }

          Serial.print("HTTP State:         ");
          if (stateHTTP == START_UP) { Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "1031" "." "217" "\", \"aSM\", @progbits, 1 #"))) = ("Starting Up"); &__c[0];})))))); }
          else if (stateHTTP == CHECK_CONNECTION) { Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "1032" "." "218" "\", \"aSM\", @progbits, 1 #"))) = ("Monitoring connection"); &__c[0];})))))); }
          else if (stateHTTP == IS_SCANNING) { Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "1033" "." "219" "\", \"aSM\", @progbits, 1 #"))) = ("Scanning for known network"); &__c[0];})))))); }
          else if (stateHTTP == IS_CONNECTING) { Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "1034" "." "220" "\", \"aSM\", @progbits, 1 #"))) = ("Connecting"); &__c[0];})))))); }
          else if (stateHTTP == IS_WAITING) { Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "1035" "." "221" "\", \"aSM\", @progbits, 1 #"))) = ("Waiting"); &__c[0];})))))); }

          Serial.print("HTTP Updater State: ");
          if (stateHTTP == START_UP) { Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "1038" "." "222" "\", \"aSM\", @progbits, 1 #"))) = ("Starting Up"); &__c[0];})))))); }
          else if (stateHTTP == CHECK_CONNECTION) { Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "1039" "." "223" "\", \"aSM\", @progbits, 1 #"))) = ("Monitoring connection"); &__c[0];})))))); }
          else if (stateHTTP == IS_SCANNING) { Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "1040" "." "224" "\", \"aSM\", @progbits, 1 #"))) = ("Scanning for known network"); &__c[0];})))))); }
          else if (stateHTTP == IS_CONNECTING) { Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "1041" "." "225" "\", \"aSM\", @progbits, 1 #"))) = ("Connecting"); &__c[0];})))))); }
          else if (stateHTTP == IS_WAITING) { Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "1042" "." "226" "\", \"aSM\", @progbits, 1 #"))) = ("Waiting"); &__c[0];})))))); }

          Serial.print("NTP State:          ");
          if (stateNTP == START_UP) { Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "1045" "." "227" "\", \"aSM\", @progbits, 1 #"))) = ("Starting Up"); &__c[0];})))))); }
          else if (stateNTP == CHECK_CONNECTION) { Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "1046" "." "228" "\", \"aSM\", @progbits, 1 #"))) = ("Monitoring connection"); &__c[0];})))))); }
          else if (stateNTP == IS_SCANNING) { Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "1047" "." "229" "\", \"aSM\", @progbits, 1 #"))) = ("Scanning for known network"); &__c[0];})))))); }
          else if (stateNTP == IS_CONNECTING) { Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "1048" "." "230" "\", \"aSM\", @progbits, 1 #"))) = ("Connecting"); &__c[0];})))))); }
          else if (stateNTP == IS_WAITING) { Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "1049" "." "231" "\", \"aSM\", @progbits, 1 #"))) = ("Waiting"); &__c[0];})))))); }

          Serial.println("-----------------------------------------------------------------------------------------------------------------------------------------------------");
        }
        deltaUpdate = millis() - startUpdate;
        if (maxUpdateSYS < deltaUpdate) { maxUpdateSYS = deltaUpdate; }
      }
      // reset max values and measure them again until next display
      // AllmaxUpdate... are the runtime maxima that are not reset between each display update
      myDelayMin = 2000 /* rate at which system information (CPU usage, Heap size) is displayed on serial port*/;
      maxUpdateTime = maxUpdateWifi = maxUpdateOTA = maxUpdateNTP = maxUpdateMQTT = maxUpdateHTTP = maxUpdateHTTPUPDATER = maxUpdatemDNS = 0;
      maxUpdateSCD30 = maxUpdateSGP30 = maxUpdateCCS811 = maxUpdateSPS30 = maxUpdateBME280 = maxUpdateBME680 = maxUpdateMLX = maxUpdateMAX = 0;
      maxUpdateMQTTMESSAGE = maxUpdateWSMESSAGE = maxUpdateLCD = maxUpdateINPUT = maxUpdateRT = maxUpdateEEPROM = maxUpdateJS = maxUpdateBASE = maxUpdateBLINK = maxUpdateREBOOT = maxUpdateALLGOOD = 0;
    }

    /******************************************************************************************************/
    // Serial User Input
    /******************************************************************************************************/
    if (false) { Serial.println("DBG:UPDATE: SERIALINPUT"); }
    startUpdate = millis();
    inputHandle(); // Serial input handling
    deltaUpdate = millis() - startUpdate;
    if (maxUpdateINPUT < deltaUpdate) { maxUpdateINPUT = deltaUpdate; }
    if (AllmaxUpdateINPUT < deltaUpdate) { AllmaxUpdateINPUT = deltaUpdate; }

    /******************************************************************************************************/
    // Other Time Managed Events such as runtime, saving baseline, rebooting, blinking LCD for warning
    /******************************************************************************************************/

    // Update runtime every minute -------------------------------------
    if ((currentTime - lastTime) >= intervalRuntime) {
      startUpdate = millis();
      if (false) { Serial.println("DBG:UPDATE: RUNTIME"); }
      mySettings.runTime = mySettings.runTime + ((currentTime - lastTime) / 1000);
      lastTime = currentTime;
      if (dbglevel > 0) { Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "1084" "." "232" "\", \"aSM\", @progbits, 1 #"))) = ("Runtime updated"); &__c[0];})))))); }
      deltaUpdate = millis() - startUpdate;
      if (maxUpdateRT < deltaUpdate) { maxUpdateRT = deltaUpdate; }
      if (AllmaxUpdateRT < deltaUpdate) { AllmaxUpdateRT = deltaUpdate; }
    }

    // Safe Configuration infrequently ---------------------------------------
    if ((currentTime - lastSaveSettings) >= 43200000 /* 12 hrs, min write interval is 6,650,000 for 20 year liftime: 100,000/20yrs/365days = 13 writes per day*/) {
      startUpdate = millis();
      if (false) { Serial.println("DBG:UPDATE: EEPROM"); }
      EEPROM.put(0, mySettings);
      if (EEPROM.commit()) {
        lastSaveSettings = currentTime;
        if (dbglevel > 0) { Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "1097" "." "233" "\", \"aSM\", @progbits, 1 #"))) = ("EEPROM updated"); &__c[0];})))))); }
      }
      lastSaveSettings = currentTime;
      deltaUpdate = millis() - startUpdate;
      if (maxUpdateEEPROM < deltaUpdate) { maxUpdateEEPROM = deltaUpdate; }
      if (AllmaxUpdateEEPROM < deltaUpdate) { AllmaxUpdateEEPROM = deltaUpdate; }
    }

    if ((currentTime - lastSaveSettingsJSON) >= 604800000 /* 7 days*/) {
      startUpdate = millis();
      if (false) { Serial.println("DBG:UPDATE: JSON config file"); }
      saveConfiguration("/Sensi.json", mySettings);
      lastSaveSettingsJSON = currentTime;
      if (dbglevel > 0) { Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "1110" "." "234" "\", \"aSM\", @progbits, 1 #"))) = ("Sensi.json updated"); &__c[0];})))))); }
      deltaUpdate = millis() - startUpdate;
      if (maxUpdateJS < deltaUpdate) { maxUpdateJS = deltaUpdate; }
      if (AllmaxUpdateJS < deltaUpdate) { AllmaxUpdateJS = deltaUpdate; }
    }

    // Obtain basline from sensors that create internal baseline --------
    if ((currentTime - lastBaseline) >= intervalBaseline) {
      startUpdate = millis();
      lastBaseline = currentTime;
      if (false) { Serial.println("DBG:UPDATE: BASELINE"); }
      // Copy CCS811 basline to settings when warmup is finished
      if (ccs811_avail && mySettings.useCCS811) {
        if (currentTime >= warmupCCS811) {
          ccs811_port->begin(ccs811_i2c[0], ccs811_i2c[1]);
          mySettings.baselineCCS811 = ccs811.getBaseline();
          mySettings.baselineCCS811_valid = 0xF0;
          if (dbglevel == 10) { Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "1127" "." "235" "\", \"aSM\", @progbits, 1 #"))) = ("CCS811 baseline placed into settings"); &__c[0];})))))); }
          // warmupCCS811 = warmupCCS811 + stablebaseCCS811;
        }
      }
      // Copy SGP30 basline to settings when warmup is finished
      if (sgp30_avail && mySettings.useSGP30) {
        if (currentTime >= warmupSGP30) {
          mySettings.baselinetVOC_SGP30 = sgp30.baselineTVOC;
          mySettings.baselineeCO2_SGP30 = sgp30.baselineCO2;
          mySettings.baselineSGP30_valid = 0xF0;
          if (dbglevel == 6) { Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "1137" "." "236" "\", \"aSM\", @progbits, 1 #"))) = ("SGP30 baseline setting updated"); &__c[0];})))))); }
          // warmupSGP30 = warmupSGP30 + intervalSGP30Baseline;
        }
      }
      deltaUpdate = millis() - startUpdate;
      if (maxUpdateBASE < deltaUpdate) { maxUpdateBASE = deltaUpdate; }
      if (AllmaxUpdateBASE < deltaUpdate) { AllmaxUpdateBASE = deltaUpdate; }
    }

    // Update AirQuality Warning --------------------------------------------------
    if ((currentTime - lastWarning) > 60000 /* Check sensors every minute wether to create warning message/blinking*/) { // warning interval
      startUpdate = millis();
      if (false) { Serial.println("DBG:UPDATE: ALL GOOD"); }
      lastWarning = currentTime;
      allGood = sensorsWarning();
      deltaUpdate = millis() - startUpdate;
      if (maxUpdateALLGOOD < deltaUpdate) { maxUpdateALLGOOD = deltaUpdate; }
      if (AllmaxUpdateALLGOOD < deltaUpdate) { AllmaxUpdateALLGOOD = deltaUpdate; }
    }
    // Warning ----------------------------------------------------------    
    if ((currentTime - lastBlink) > intervalBlink) { // blink interval
      startUpdate = millis();
      if (false) { Serial.println("DBG:UPDATE: BLINK"); }
      bool blinkLCD = false;
      // do we want to blink the LCD background ?
      if (mySettings.useBacklight && (allGood == false)) {
        if (timeSynced) { // is it night and we dont want to blink?
          if ( ( localTime->tm_hour*60+localTime->tm_min < mySettings.nightBegin ) && ( localTime->tm_hour*60+localTime->tm_min > mySettings.nightEnd ) ) {
            blinkLCD = true;
          }
        } else { // no network time
          blinkLCD = true;
        }
      } else { // just make sure the back light is on when we dont blink
        if (lastBlinkInten == false) { // make sure backlight is on
          lcd_port->begin(lcd_i2c[0], lcd_i2c[1]);



          if (mySettings.useBacklight) { lcd.setBacklight(255); } else { lcd.setBacklight(0); }

          lastBlinkInten = true;
          intervalBlink = 9500 /* blink on time in ms*/;
        }
      }

      // blink LCD background
      if (blinkLCD) {
        // if (dbglevel > 0) { Serial.println(F("Sensors out of normal range!")); }
        lcd_port->begin(lcd_i2c[0], lcd_i2c[1]);
        lastBlink = currentTime;
        lastBlinkInten = !lastBlinkInten;
# 1198 "d:\\GitHub\\ESP Playground\\AirQuality\\software\\Unified Software\\Sensi\\Sensi.ino"
        if (lastBlinkInten) {
          lcd.setBacklight(255);
          intervalBlink = 9500 /* blink on time in ms*/;
        } else {
          lcd.setBacklight(0);
          intervalBlink = 500 /* blink off time in ms*/;
        }

      } // toggel the LCD background
      deltaUpdate = millis() - startUpdate;
      if (maxUpdateBLINK < deltaUpdate) { maxUpdateBLINK = deltaUpdate; }
      if (AllmaxUpdateBLINK < deltaUpdate) { AllmaxUpdateBLINK = deltaUpdate; }
    } // blinking interval

    // Deal with request to reboot --------------------------------------
    // Reboot at preset time
    if (scheduleReboot == true) {
      startUpdate = millis();
      if (false) { Serial.println("DBG:UPDATE: REBOOTCHECK"); }
      if ( (mySettings.rebootMinute>=0) && (mySettings.rebootMinute<=1440) ) { // do not reboot if rebootMinute is -1 or bigger than 24hrs
        if (timeSynced) {
          if (localTime->tm_hour*60+localTime->tm_min == mySettings.rebootMinute) {
            if (dbglevel > 0) { Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "1220" "." "237" "\", \"aSM\", @progbits, 1 #"))) = ("Rebooting..."); &__c[0];})))))); }
            Serial.flush() ;
            ESP.reset();
          }
        } else {
          if (dbglevel > 0) { Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "1225" "." "238" "\", \"aSM\", @progbits, 1 #"))) = ("Rebooting..."); &__c[0];}))))));}
          Serial.flush() ;
          ESP.reset();
        }
      }
      deltaUpdate = millis() - startUpdate;
      if (maxUpdateREBOOT < deltaUpdate) { maxUpdateREBOOT = deltaUpdate; }
      if (AllmaxUpdateREBOOT < deltaUpdate) { AllmaxUpdateREBOOT = deltaUpdate; }
    }

    /******************************************************************************************************/
    // Free up some processor time for ESP background services
    /******************************************************************************************************/
    //  The main loop is expected to occur every 1000/intervalLoop seconds.
    //  If at this location in the program, there is time left, we call delay function which allows ESP to its background tasks 
    //  We keep track of how much time in average is left.
    myDelay = long(currentTime + intervalLoop) - long(millis()); // how much time is left until system loop expires
    if (myDelay < myDelayMin) { myDelayMin = myDelay; }
    if (myDelay > 0) { // there is time left, so initiate a delay, could also go to sleep here
      if (false) { Serial.println("DBG:UPDATE: LOOP DELAY"); }
      myDelayAvg = 0.9 * myDelayAvg + 0.1 * float(myDelay);
      delay(myDelay);
    }

  } // OTA not in progress
} // loop

//////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////
// Support functions
//////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////

// Scan the I2C bus whether device is attached at address
bool checkI2C(byte address, TwoWire *myWire) {
  myWire->beginTransmission(address);
  if (myWire->endTransmission() == 0) { return true; } else { return false; }
}

String formatBytes(size_t bytes) {
  if (bytes < 1024) {
    return String(bytes) + "B";
  } else if (bytes < (1024 * 1024)) {
    return String(bytes / 1024.0) + "KB";
  } else if (bytes < (1024 * 1024 * 1024)) {
    return String(bytes / 1024.0 / 1024.0) + "MB";
  } else {
    return String(bytes / 1024.0 / 1024.0 / 1024.0) + "GB";
  }
}

// Boot helper
// Prints message and waits until timeout or user send character on serial terminal
void serialTrigger(char* mess, int timeout) {
  unsigned long startTime = millis();
  bool clearSerial = true;
  Serial.println();
  Serial.println(mess);
  while (!Serial.available()) {
    delay(1000);
    if (millis() - startTime >= timeout) {
      clearSerial = false;
      break;
    }
  }
  if (clearSerial == true) {
    while (Serial.available()) {
      Serial.read();
    }
  }
}

void clearAndHome()
{
  Serial.write(27); // ESC command
  Serial.print("[2J"); // clear screen command
  Serial.write(27);
  Serial.print("[H");
}

/**************************************************************************************/
// JSON: Support Routines, JSON for the senosors is in their code segment
/**************************************************************************************/
// Member	  Type	Meaning	Range
// tm_sec	  int	  seconds after the minute	0-61*
// tm_min	  int	  minutes after the hour	  0-59
// tm_hour	int	  hours since midnight	    0-23
// tm_mday	int	  day of the month	        1-31
// tm_mon	  int	  months since January	    0-11
// tm_year	int	  years since 1900	
// tm_wday	int	  days since Sunday	        0-6
// tm_yday	int	  days since January 1	    0-365
// tm_isdst	int	  Daylight Saving Time flag	
void timeJSON(char *payLoad) {
      sprintf(payLoad, "{\"time\":{\"hour\":%d,\"minute\":%d,\"second\":%d}}",
      localTime->tm_hour,
      localTime->tm_min,
      localTime->tm_sec);
}

void dateJSON(char *payLoad) {
      sprintf(payLoad, "{\"date\":{\"day\":%d,\"month\":%d,\"year\":%d}}",
      localTime->tm_mday,
      localTime->tm_mon,
      localTime->tm_year);
}

/**************************************************************************************/
// Serial Input: Support Routines
/**************************************************************************************/

void helpMenu() {
  Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "1337" "." "239" "\", \"aSM\", @progbits, 1 #"))) = ("================================================================================="); &__c[0];}))))));
  Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "1338" "." "240" "\", \"aSM\", @progbits, 1 #"))) = ("| Sensi, 2020, 2021, Urs Utzinger                                               |"); &__c[0];}))))));
  Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "1339" "." "241" "\", \"aSM\", @progbits, 1 #"))) = ("================================================================================="); &__c[0];}))))));
  Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "1340" "." "242" "\", \"aSM\", @progbits, 1 #"))) = ("Supports........................................................................."); &__c[0];}))))));
  Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "1341" "." "243" "\", \"aSM\", @progbits, 1 #"))) = (".........................................................................LCD 20x4"); &__c[0];}))))));
  Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "1342" "." "244" "\", \"aSM\", @progbits, 1 #"))) = (".........................................................SPS30 Senserion Particle"); &__c[0];}))))));
  Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "1343" "." "245" "\", \"aSM\", @progbits, 1 #"))) = ("..............................................................SCD30 Senserion CO2"); &__c[0];}))))));
  Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "1344" "." "246" "\", \"aSM\", @progbits, 1 #"))) = (".......................................................SGP30 Senserion tVOC, eCO2"); &__c[0];}))))));
  Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "1345" "." "247" "\", \"aSM\", @progbits, 1 #"))) = ("...........................BME680/280 Bosch Temperature, Humidity, Pressure, tVOC"); &__c[0];}))))));
  Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "1346" "." "248" "\", \"aSM\", @progbits, 1 #"))) = ("....................................................CCS811 eCO2 tVOC, Air Quality"); &__c[0];}))))));
  Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "1347" "." "249" "\", \"aSM\", @progbits, 1 #"))) = ("...........................................MLX90614 Melex Temperature Contactless"); &__c[0];}))))));
  Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "1348" "." "250" "\", \"aSM\", @progbits, 1 #"))) = ("==All Sensors===========================|========================================"); &__c[0];}))))));
  Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "1349" "." "251" "\", \"aSM\", @progbits, 1 #"))) = ("| z: print all sensor data              | n: this device Name, nSensi           |"); &__c[0];}))))));
  Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "1350" "." "252" "\", \"aSM\", @progbits, 1 #"))) = ("| p: print current settings             | s: save settings (JSON & binary)      |"); &__c[0];}))))));
  Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "1351" "." "253" "\", \"aSM\", @progbits, 1 #"))) = ("| d: create default seetings            | r: read settings (JSON)               |"); &__c[0];}))))));
  Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "1352" "." "254" "\", \"aSM\", @progbits, 1 #"))) = ("==Network===============================|==MQTT=================================="); &__c[0];}))))));
  Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "1353" "." "255" "\", \"aSM\", @progbits, 1 #"))) = ("| 1: SSID 1, 1myssid                    | u: mqtt username, umqtt or empty      |"); &__c[0];}))))));
  Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "1354" "." "256" "\", \"aSM\", @progbits, 1 #"))) = ("| 2: SSID 2, 2myssid                    | w: mqtt password, ww1ldc8ts or empty  |"); &__c[0];}))))));
  Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "1355" "." "257" "\", \"aSM\", @progbits, 1 #"))) = ("| 3: SSID 3, 3myssid                    | q: send mqtt immediatly, q            |"); &__c[0];}))))));
  Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "1356" "." "258" "\", \"aSM\", @progbits, 1 #"))) = ("| 4: password SSID 1, 4mypas or empty   |                                       |"); &__c[0];}))))));
  Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "1357" "." "259" "\", \"aSM\", @progbits, 1 #"))) = ("| 5: password SSID 2, 5mypas or empty   | 8: mqtt server, 8my,mqtt.com          |"); &__c[0];}))))));
  Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "1358" "." "260" "\", \"aSM\", @progbits, 1 #"))) = ("| 6: password SSID 3, 6mypas or empty   | 9: mqtt fall back server              |"); &__c[0];}))))));
  Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "1359" "." "261" "\", \"aSM\", @progbits, 1 #"))) = ("|-Network Time--------------------------|---------------------------------------|"); &__c[0];}))))));
  Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "1360" "." "262" "\", \"aSM\", @progbits, 1 #"))) = ("| o: set time zone oMST7                | N: ntp server, Ntime.nist.gov         |"); &__c[0];}))))));
  Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "1361" "." "263" "\", \"aSM\", @progbits, 1 #"))) = ("| https://raw.githubusercontent.com/nayarsystems/posix_tz_db/master/zones.csv   |"); &__c[0];}))))));
  Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "1362" "." "264" "\", \"aSM\", @progbits, 1 #"))) = ("| a: night start in min after midnight  | A: night end                          |"); &__c[0];}))))));
  Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "1363" "." "265" "\", \"aSM\", @progbits, 1 #"))) = ("| R: reboot time in min after midnight: -1=off                                  |"); &__c[0];}))))));
  Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "1364" "." "266" "\", \"aSM\", @progbits, 1 #"))) = ("==Sensors======================================================================="); &__c[0];}))))));
  Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "1365" "." "267" "\", \"aSM\", @progbits, 1 #"))) = ("|-SGP30-eCO2----------------------------|-MAX-----------------------------------|"); &__c[0];}))))));
  Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "1366" "." "268" "\", \"aSM\", @progbits, 1 #"))) = ("| e: force eCO2, e400                   |                                       |"); &__c[0];}))))));
  Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "1367" "." "269" "\", \"aSM\", @progbits, 1 #"))) = ("| v: force tVOC, t3000                  |                                       |"); &__c[0];}))))));
  Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "1368" "." "270" "\", \"aSM\", @progbits, 1 #"))) = ("| g: get baselines                      |                                       |"); &__c[0];}))))));
  Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "1369" "." "271" "\", \"aSM\", @progbits, 1 #"))) = ("|-SCD30=CO2-----------------------------|--CCS811-eCO2--------------------------|"); &__c[0];}))))));
  Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "1370" "." "272" "\", \"aSM\", @progbits, 1 #"))) = ("| f: force CO2, f400.0 in ppm           | c: force basline, c400                |"); &__c[0];}))))));
  Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "1371" "." "273" "\", \"aSM\", @progbits, 1 #"))) = ("| t: force temperature offset, t5.0 in C| b: get baseline                       |"); &__c[0];}))))));
  Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "1372" "." "274" "\", \"aSM\", @progbits, 1 #"))) = ("|-MLX Temp------------------------------|-LCD-----------------------------------|"); &__c[0];}))))));
  Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "1373" "." "275" "\", \"aSM\", @progbits, 1 #"))) = ("| m: set temp offset, m1.4              | i: simplified display                 |"); &__c[0];}))))));
  Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "1374" "." "276" "\", \"aSM\", @progbits, 1 #"))) = ("==Disable===============================|==Disable==============================="); &__c[0];}))))));
  Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "1375" "." "277" "\", \"aSM\", @progbits, 1 #"))) = ("| x: 99 reset microcontroller           | x: 10 MAX30 on/off                    |"); &__c[0];}))))));
  Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "1376" "." "278" "\", \"aSM\", @progbits, 1 #"))) = ("| x: 2 LCD on/off                       | x: 11 MLX on/off                      |"); &__c[0];}))))));
  Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "1377" "." "279" "\", \"aSM\", @progbits, 1 #"))) = ("| x: 3 WiFi on/off                      | x: 12 BME680 on/off                   |"); &__c[0];}))))));
  Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "1378" "." "280" "\", \"aSM\", @progbits, 1 #"))) = ("| x: 4 SCD30 on/off                     | x: 13 BME280 on/off                   |"); &__c[0];}))))));
  Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "1379" "." "281" "\", \"aSM\", @progbits, 1 #"))) = ("| x: 5 SPS30 on/off                     | x: 14 CCS811 on/off                   |"); &__c[0];}))))));
  Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "1380" "." "282" "\", \"aSM\", @progbits, 1 #"))) = ("| x: 6 SGP30 on/off                     | x: 15 LCD backlight on/off            |"); &__c[0];}))))));
  Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "1381" "." "283" "\", \"aSM\", @progbits, 1 #"))) = ("| x: 7 MQTT on/off                      | x: 16 HTML server on/off              |"); &__c[0];}))))));
  Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "1382" "." "284" "\", \"aSM\", @progbits, 1 #"))) = ("| x: 8 NTP on/off                       | x: 17 OTA on/off                      |"); &__c[0];}))))));
  Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "1383" "." "285" "\", \"aSM\", @progbits, 1 #"))) = ("| x: 9 mDNS on/off                      | x: 18                                 |"); &__c[0];}))))));
  Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "1384" "." "286" "\", \"aSM\", @progbits, 1 #"))) = ("|---------------------------------------|---------------------------------------|"); &__c[0];}))))));
  Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "1385" "." "287" "\", \"aSM\", @progbits, 1 #"))) = ("| You will need to x99 to initialize the sensor                                 |"); &__c[0];}))))));
  Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "1386" "." "288" "\", \"aSM\", @progbits, 1 #"))) = ("==Debug Level===========================|==Debug Level==========================="); &__c[0];}))))));
  Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "1387" "." "289" "\", \"aSM\", @progbits, 1 #"))) = ("| l: 0 ALL off                          |                                       |"); &__c[0];}))))));
  Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "1388" "." "290" "\", \"aSM\", @progbits, 1 #"))) = ("| l: 1 ALL minimal                      | l: 6 SGP30 max level                  |"); &__c[0];}))))));
  Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "1389" "." "291" "\", \"aSM\", @progbits, 1 #"))) = ("| l: 2 LCD max level                    | l: 7 MAX30 max level                  |"); &__c[0];}))))));
  Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "1390" "." "292" "\", \"aSM\", @progbits, 1 #"))) = ("| l: 3 WiFi max level                   | l: 8 MLX max level                    |"); &__c[0];}))))));
  Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "1391" "." "293" "\", \"aSM\", @progbits, 1 #"))) = ("| l: 4 SCD30 max level                  | l: 9 BME680/280 max level             |"); &__c[0];}))))));
  Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "1392" "." "294" "\", \"aSM\", @progbits, 1 #"))) = ("| l: 5 SPS30 max level                  | l: 10 CCS811 max level                |"); &__c[0];}))))));
  Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "1393" "." "295" "\", \"aSM\", @progbits, 1 #"))) = ("================================================================================="); &__c[0];}))))));
  Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "1394" "." "296" "\", \"aSM\", @progbits, 1 #"))) = ("  Dont forget to save settings"); &__c[0];}))))));
  Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "1395" "." "297" "\", \"aSM\", @progbits, 1 #"))) = ("================================================================================="); &__c[0];}))))));
}

/******************************************************************************************************/
// This deals with the user input from serial console and allows configuration the system during runtime
/******************************************************************************************************/
void inputHandle() {
  char inBuff[64];
  uint16_t tmpuI;
  int16_t tmpI;
  float tmpF;
  int bytesread;
  String value;
  String command;

  if (Serial.available()) {
    Serial.setTimeout(1 ); // Serial read timeout
    bytesread = Serial.readBytesUntil('\n', inBuff, 33); // Read from serial until CR is read or timeout exceeded
    inBuff[bytesread] = '\0';
    String instruction = String(inBuff);
    command = instruction.substring(0, 1);
    if (bytesread > 1) { // we have also a value
      value = instruction.substring(1, bytesread);
      // Serial.println(value);
    }

    if (command == "a") { // set start of night in minutes after midnight
      tmpuI = value.toInt();
      if ((tmpuI >= 0) && (tmpuI <= 1440) && (tmpuI >= mySettings.nightEnd)) {
        mySettings.nightBegin = (uint16_t)tmpuI;
        Serial.print(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "1425" "." "298" "\", \"aSM\", @progbits, 1 #"))) = ("Night begin set to:  "); &__c[0];}))))));
        Serial.println(mySettings.nightBegin);
      } else {
        Serial.print(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "1428" "." "299" "\", \"aSM\", @progbits, 1 #"))) = ("Night start out of valid range: "); &__c[0];}))))));
        Serial.print(mySettings.nightEnd);
        Serial.println("...1440");
     }
    }

    else if (command == "A") { // set end of night in minutes after midnight
      tmpuI = value.toInt();
      if ((tmpuI >= 0) && (tmpuI <= 1440) && (tmpuI <= mySettings.nightBegin)) {
        mySettings.nightEnd = (uint16_t)tmpuI;
        Serial.print(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "1438" "." "300" "\", \"aSM\", @progbits, 1 #"))) = ("Night end set to:  "); &__c[0];}))))));
        Serial.println(mySettings.nightEnd);
      } else {
        Serial.print(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "1441" "." "301" "\", \"aSM\", @progbits, 1 #"))) = ("Night end out of valid range: 0..."); &__c[0];}))))));
        Serial.println(mySettings.nightBegin);
      }
    }

    else if (command == "R") { // set end of night in minutes after midnight
      tmpI = value.toInt();
      if ((tmpI >= -1) && (tmpI <= 1440)) {
        mySettings.rebootMinute = (int16_t)tmpI;
        Serial.print(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "1450" "." "302" "\", \"aSM\", @progbits, 1 #"))) = ("Reboot minute set to:  "); &__c[0];}))))));
        Serial.println(mySettings.rebootMinute);
      } else {
        Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "1453" "." "303" "\", \"aSM\", @progbits, 1 #"))) = ("Reboot time out of valid range: -1(off)...1440"); &__c[0];}))))));
      }
    }

    else if (command == "l") { // set debug level
      tmpuI = value.toInt();
      if ((tmpuI >= 0) && (tmpuI <= 10)) {
        dbglevel = (unsigned int)tmpuI;
        mySettings.debuglevel = dbglevel;
        Serial.print(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "1462" "." "304" "\", \"aSM\", @progbits, 1 #"))) = ("Debug level set to:  "); &__c[0];}))))));
        Serial.println(mySettings.debuglevel);
      } else {
        Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "1465" "." "305" "\", \"aSM\", @progbits, 1 #"))) = ("Debug level out of valid Range"); &__c[0];}))))));
      }
    }

    else if (command == "z") { // dump all sensor data
      printSensors();
    }

    ///////////////////////////////////////////////////////////////////
    // SGP30
    ///////////////////////////////////////////////////////////////////
    else if (command == "e") { // forced CO2 set setpoint
      tmpuI = value.toInt();
      if ((tmpuI >= 400) && (tmpuI <= 2000)) {
        if (sgp30_avail && mySettings.useSGP30) {
          sgp30_port->begin(sgp30_i2c[0], sgp30_i2c[1]);
          sgp30.getBaseline();
          mySettings.baselinetVOC_SGP30 = sgp30.baselineTVOC;
          sgp30.setBaseline((uint16_t)tmpuI, (uint16_t)mySettings.baselinetVOC_SGP30);
          mySettings.baselineSGP30_valid = 0xF0;
          mySettings.baselineeCO2_SGP30 = (uint16_t)tmpuI;
          Serial.print(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "1486" "." "306" "\", \"aSM\", @progbits, 1 #"))) = ("Calibration point is:  "); &__c[0];}))))));
          Serial.println(tmpuI);
        }
      } else {
        Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "1490" "." "307" "\", \"aSM\", @progbits, 1 #"))) = ("Calibration point out of valid Range"); &__c[0];}))))));
      }
    }

    else if (command == "v") { // forced tVOC set setpoint
      tmpuI = value.toInt();
      if ((tmpuI > 400) && (tmpuI < 2000)) {
        if (sgp30_avail && mySettings.useSGP30) {
          sgp30_port->begin(sgp30_i2c[0], sgp30_i2c[1]);
          sgp30.getBaseline();
          mySettings.baselineeCO2_SGP30 = sgp30.baselineCO2;
          sgp30.setBaseline((uint16_t)mySettings.baselineeCO2_SGP30, (uint16_t)tmpuI);
          mySettings.baselineSGP30_valid = 0xF0;
          mySettings.baselinetVOC_SGP30 = (uint16_t)tmpuI;
          Serial.print(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "1504" "." "308" "\", \"aSM\", @progbits, 1 #"))) = ("Calibration point is:  "); &__c[0];}))))));
          Serial.println(tmpuI);
        }
      } else {
        Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "1508" "." "309" "\", \"aSM\", @progbits, 1 #"))) = ("Calibration point out of valid Range"); &__c[0];}))))));
      }
    }

    else if (command == "g") { // forced tVOC set setpoint
      if (sgp30_avail && mySettings.useSGP30) {
        sgp30_port->begin(sgp30_i2c[0], sgp30_i2c[1]);
        sgp30.getBaseline();
        mySettings.baselineSGP30_valid = 0xF0;
        mySettings.baselinetVOC_SGP30 = sgp30.baselineTVOC;
        mySettings.baselineeCO2_SGP30 = sgp30.baselineCO2;
      }
    }

    ///////////////////////////////////////////////////////////////////
    // SCD30
    ///////////////////////////////////////////////////////////////////
    else if (command == "f") { // forced CO2 set setpoint
      tmpuI = value.toInt();
      if ((tmpuI >= 400) && (tmpuI <= 2000)) {
        if (scd30_avail && mySettings.useSCD30) {
          scd30_port->begin(scd30_i2c[0], scd30_i2c[1]);
          scd30.setForcedRecalibrationFactor((uint16_t)tmpuI);
          Serial.print(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "1531" "." "310" "\", \"aSM\", @progbits, 1 #"))) = ("Calibration point is:  "); &__c[0];}))))));
          Serial.println(tmpuI);
        }
      } else {
        Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "1535" "." "311" "\", \"aSM\", @progbits, 1 #"))) = ("Calibration point out of valid Range"); &__c[0];}))))));
      }
    }

    else if (command == "t") { // forced CO2 set setpoint
      tmpF = value.toFloat();
      if ((tmpF >= 0.0) && (tmpF <= 10.0)) {
        if (scd30_avail && mySettings.useSCD30) {
          scd30_port->begin(scd30_i2c[0], scd30_i2c[1]);
          scd30.setTemperatureOffset(tmpF);
          mySettings.tempOffset_SCD30_valid = 0xF0;
          mySettings.tempOffset_SCD30 = scd30.getTemperatureOffset();
          Serial.print(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "1547" "." "312" "\", \"aSM\", @progbits, 1 #"))) = ("Temperature offset set to:  "); &__c[0];}))))));
          Serial.println(mySettings.tempOffset_SCD30);
        }
      } else {
        Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "1551" "." "313" "\", \"aSM\", @progbits, 1 #"))) = ("Offset point out of valid Range"); &__c[0];}))))));
      }
    }

    ///////////////////////////////////////////////////////////////////
    // CCS811
    ///////////////////////////////////////////////////////////////////
    else if (command == "c") { // forced baseline
      tmpuI = value.toInt();
      if ((tmpuI >= 0) && (tmpuI <= 100000)) {
        if (ccs811_avail && mySettings.useCCS811) {
          ccs811_port->begin(ccs811_i2c[0], ccs811_i2c[1]);
          ccs811.setBaseline((uint16_t)tmpuI);
          mySettings.baselineCCS811_valid = 0xF0;
          mySettings.baselineCCS811 = (uint16_t)tmpuI;
          Serial.print(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "1566" "." "314" "\", \"aSM\", @progbits, 1 #"))) = ("Calibration point is:  "); &__c[0];}))))));
          Serial.println(tmpuI);
        }
      } else {
        Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "1570" "." "315" "\", \"aSM\", @progbits, 1 #"))) = ("Calibration point out of valid Range"); &__c[0];}))))));
      }
    }

    else if (command == "b") { // getbaseline
      if (ccs811_avail && mySettings.useCCS811) {
        ccs811_port->begin(ccs811_i2c[0], ccs811_i2c[1]);
        mySettings.baselineCCS811 = ccs811.getBaseline();
        Serial.print(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "1578" "." "316" "\", \"aSM\", @progbits, 1 #"))) = ("CCS811: baseline is  "); &__c[0];}))))));
        Serial.println(mySettings.baselineCCS811);
      }
    }

    ///////////////////////////////////////////////////////////////////
    // MLX settings
    ///////////////////////////////////////////////////////////////////
    else if (command == "m") { // forced MLX temp offset
      tmpF = value.toFloat();
      if ((tmpF >= -20.0) && (tmpF <= 20.0)) {
        mlxOffset = tmpF;
        mySettings.tempOffset_MLX_valid = 0xF0;
        mySettings.tempOffset_MLX = tmpF;
        Serial.print(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "1592" "." "317" "\", \"aSM\", @progbits, 1 #"))) = ("Temperature offset set to:  "); &__c[0];}))))));
        Serial.println(mySettings.tempOffset_MLX);
      } else {
        Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "1595" "." "318" "\", \"aSM\", @progbits, 1 #"))) = ("Offset point out of valid Range"); &__c[0];}))))));
      }
    }

    ///////////////////////////////////////////////////////////////////
    // EEPROM settings
    ///////////////////////////////////////////////////////////////////
    else if (command == "s") { // save EEPROM
      tmpTime = millis();
      if (false) { Serial.println("DBG:SAVE: EEPROM"); }
      EEPROM.put(0, mySettings);
      if (EEPROM.commit()) { Serial.printf("EEPROM saved in: %d ms\n\r", millis() - tmpTime); }

      if (false) { Serial.println("DBG:SAVE: JSON/LittleFS"); }
      tmpTime = millis();
      saveConfiguration("/Sensi.json",mySettings);
      Serial.printf("Settings in JSON saved in: %d ms\n\r", millis() - tmpTime);
    }

    else if (command == "r") { // read EEPROM
      tmpTime = millis();
      loadConfiguration("/Sensi.json", mySettings);
      Serial.printf("Settings json read in: %d ms", millis() - tmpTime);
      printSettings();
    }

    else if (command == "p") { // print display settings
      printSettings();
    }

    else if (command == "d") { // load default settings
      defaultSettings();
      printSettings();
    }

    else if (command == "1") { // SSID 1
      value.toCharArray(mySettings.ssid1, value.length() + 1);
      Serial.print(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "1632" "." "319" "\", \"aSM\", @progbits, 1 #"))) = ("SSID 1 is:  "); &__c[0];}))))));
      Serial.println(mySettings.ssid1);
    }

    else if (command == "2") { // SSID 2
      value.toCharArray(mySettings.ssid2, value.length() + 1);
      Serial.print(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "1638" "." "320" "\", \"aSM\", @progbits, 1 #"))) = ("SSID 2 is:  "); &__c[0];}))))));
      Serial.println(mySettings.ssid2);
    }

    else if (command == "3") { // SSID 3
      value.toCharArray(mySettings.ssid3, value.length() + 1);
      Serial.print(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "1644" "." "321" "\", \"aSM\", @progbits, 1 #"))) = ("SSID 3 is:  "); &__c[0];}))))));
      Serial.println(mySettings.ssid3);
    }

    else if (command == "4") { // Password 1
      value.toCharArray(mySettings.pw1, value.length() + 1);
      Serial.print(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "1650" "." "322" "\", \"aSM\", @progbits, 1 #"))) = ("Password 1 is:  "); &__c[0];}))))));
      Serial.println(mySettings.pw1);
    }

    else if (command == "5") { // Password 2
      value.toCharArray(mySettings.pw2, value.length() + 1);
      Serial.print(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "1656" "." "323" "\", \"aSM\", @progbits, 1 #"))) = ("Password 2 is:  "); &__c[0];}))))));
      Serial.println(mySettings.pw2);
    }

    else if (command == "6") { // Password 3
      value.toCharArray(mySettings.pw3, value.length() + 1);
      Serial.print(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "1662" "." "324" "\", \"aSM\", @progbits, 1 #"))) = ("Password 3 is:  "); &__c[0];}))))));
      Serial.println(mySettings.pw3);
    }

    else if (command == "8") { // MQTT Server
      value.toCharArray(mySettings.mqtt_server, value.length() + 1);
      Serial.print(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "1668" "." "325" "\", \"aSM\", @progbits, 1 #"))) = ("MQTT server is:  "); &__c[0];}))))));
      Serial.println(mySettings.mqtt_server);
    }

    else if (command == "9") { // MQTT fallback Server
      value.toCharArray(mySettings.mqtt_fallback, value.length() + 1);
      Serial.print(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "1674" "." "326" "\", \"aSM\", @progbits, 1 #"))) = ("MQTT fallback server is:  "); &__c[0];}))))));
      Serial.println(mySettings.mqtt_fallback);
    }

    else if (command == "u") { // MQTT Server
      value.toCharArray(mySettings.mqtt_username, value.length() + 1);
      Serial.print(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "1680" "." "327" "\", \"aSM\", @progbits, 1 #"))) = ("MQTT username is:  "); &__c[0];}))))));
      Serial.println(mySettings.mqtt_username);
    }

    else if (command == "w") { // MQTT Server
      value.toCharArray(mySettings.mqtt_password, value.length() + 1);
      Serial.print(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "1686" "." "328" "\", \"aSM\", @progbits, 1 #"))) = ("MQTT password is:  "); &__c[0];}))))));
      Serial.println(mySettings.mqtt_password);
    }

    else if (command == "q") { // MQTT Server
      mySettings.sendMQTTimmediate = !bool(mySettings.sendMQTTimmediate);
      Serial.print(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "1692" "." "329" "\", \"aSM\", @progbits, 1 #"))) = ("MQTT is send immediatly:  "); &__c[0];}))))));
      Serial.println(mySettings.sendMQTTimmediate);
    }

    else if (command == "n") { // MQTT main topic name (device Name)
      value.toCharArray(mySettings.mqtt_mainTopic, value.length() + 1);
      Serial.print(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "1698" "." "330" "\", \"aSM\", @progbits, 1 #"))) = ("MQTT mainTopic is:  "); &__c[0];}))))));
      Serial.println(mySettings.mqtt_mainTopic);
    }

    else if (command == "N") { // NTP server
      value.toCharArray(mySettings.ntpServer, value.length() + 1);
      Serial.print(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "1704" "." "331" "\", \"aSM\", @progbits, 1 #"))) = ("NTP server is:  "); &__c[0];}))))));
      Serial.println(mySettings.ntpServer);
      stateNTP = START_UP;
    }

    else if (command == "o") { // Time Zone Offset
      value.toCharArray(mySettings.timeZone, value.length() + 1);
      Serial.print(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "1711" "." "332" "\", \"aSM\", @progbits, 1 #"))) = ("Time Zone is:  "); &__c[0];}))))));
      NTP.setTimeZone(mySettings.timeZone);
      //tmpI = value.toInt();
      //if ( (tmpI >= -12) && (tmpI <= 14) ) {
      //  mySettings.utcOffset = tmpI;
      //  Serial.print(F("UTC offset is:  "));
      //  Serial.println(mySettings.utcOffset);
      //  stateNTP = START_UP;
      //} else {
      //  Serial.print(F("UTC offset neets to be -12..14"));
      //}
    }

    //else if (command == "D") {                                       // Daylight Saving enable/disable
    //  mySettings.enableDST = !bool(mySettings.enableDST);
    //  if (mySettings.enableDST)      {     Serial.println(F("Daylight Saving Time is observed"));  } else { Serial.println(F("Daylight Saving Time is not observed")); }
    //}

    else if (command == "x") { // enable/disable equipment
      tmpuI = value.toInt();
      if ((tmpuI > 0) && (tmpuI <= 99)) {
        switch (tmpuI) {
          case 2:
            mySettings.useLCD = !bool(mySettings.useLCD);
            if (mySettings.useLCD) { Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "1735" "." "333" "\", \"aSM\", @progbits, 1 #"))) = ("LCD is used"); &__c[0];})))))); } else { Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "1735" "." "334" "\", \"aSM\", @progbits, 1 #"))) = ("LCD is not used"); &__c[0];})))))); }
            break;
          case 3:
            mySettings.useWiFi = !bool(mySettings.useWiFi);
            if (mySettings.useWiFi) { Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "1739" "." "335" "\", \"aSM\", @progbits, 1 #"))) = ("WiFi is used"); &__c[0];})))))); } else { Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "1739" "." "336" "\", \"aSM\", @progbits, 1 #"))) = ("WiFi is not used"); &__c[0];})))))); }
            break;
          case 4:
            mySettings.useSCD30 = !bool(mySettings.useSCD30);
            if (mySettings.useSCD30) { Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "1743" "." "337" "\", \"aSM\", @progbits, 1 #"))) = ("SCD30 is used"); &__c[0];})))))); } else { Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "1743" "." "338" "\", \"aSM\", @progbits, 1 #"))) = ("SCD30 is not used"); &__c[0];})))))); }
            break;
          case 5:
            mySettings.useSPS30 = !bool(mySettings.useSPS30);
            if (mySettings.useSPS30) { Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "1747" "." "339" "\", \"aSM\", @progbits, 1 #"))) = ("SPS30 is used"); &__c[0];})))))); } else { Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "1747" "." "340" "\", \"aSM\", @progbits, 1 #"))) = ("SPS30 is not used"); &__c[0];})))))); }
            break;
          case 6:
            mySettings.useSGP30 = !bool(mySettings.useSGP30);
            if (mySettings.useSGP30) { Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "1751" "." "341" "\", \"aSM\", @progbits, 1 #"))) = ("SGP30 is used"); &__c[0];})))))); } else { Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "1751" "." "342" "\", \"aSM\", @progbits, 1 #"))) = ("SGP30 is not used"); &__c[0];})))))); }
            break;
          case 7:
            // if (mySettings.useHTTP && !bool(mySettings.useMQTT)) { Serial.println(F("Plese turn off WebServer first!")); } 
            // else { }
            mySettings.useMQTT = !bool(mySettings.useMQTT);
            if (mySettings.useMQTT) { Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "1757" "." "343" "\", \"aSM\", @progbits, 1 #"))) = ("MQTT is used"); &__c[0];})))))); } else { Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "1757" "." "344" "\", \"aSM\", @progbits, 1 #"))) = ("MQTT is not used"); &__c[0];})))))); }
            break;
          case 8:
            mySettings.useNTP = !bool(mySettings.useNTP);
            if (mySettings.useNTP) { Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "1761" "." "345" "\", \"aSM\", @progbits, 1 #"))) = ("NTP is used"); &__c[0];})))))); } else { Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "1761" "." "346" "\", \"aSM\", @progbits, 1 #"))) = ("NTP is not used"); &__c[0];})))))); }
            break;
          case 9:
            mySettings.usemDNS = !bool(mySettings.usemDNS);
            if (mySettings.usemDNS) { Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "1765" "." "347" "\", \"aSM\", @progbits, 1 #"))) = ("mDNS is used"); &__c[0];})))))); } else { Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "1765" "." "348" "\", \"aSM\", @progbits, 1 #"))) = ("mDNS is not used"); &__c[0];})))))); }
            break;
          case 10:
            mySettings.useMAX30 = !bool(mySettings.useMAX30);
            if (mySettings.useMAX30) { Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "1769" "." "349" "\", \"aSM\", @progbits, 1 #"))) = ("MAX30 is used"); &__c[0];})))))); } else { Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "1769" "." "350" "\", \"aSM\", @progbits, 1 #"))) = ("MAX30 is not used"); &__c[0];})))))); }
            break;
          case 11:
            mySettings.useMLX = !bool(mySettings.useMLX);
            if (mySettings.useMLX) { Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "1773" "." "351" "\", \"aSM\", @progbits, 1 #"))) = ("MLX is used"); &__c[0];})))))); } else { Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "1773" "." "352" "\", \"aSM\", @progbits, 1 #"))) = ("MLX is not used"); &__c[0];})))))); }
            break;
          case 12:
            mySettings.useBME680 = !bool(mySettings.useBME680);
            if (mySettings.useBME680) { Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "1777" "." "353" "\", \"aSM\", @progbits, 1 #"))) = ("BME680 is used"); &__c[0];})))))); } else { Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "1777" "." "354" "\", \"aSM\", @progbits, 1 #"))) = ("BME680 is not used"); &__c[0];})))))); }
            break;
          case 13:
            mySettings.useBME280 = !bool(mySettings.useBME280);
            if (mySettings.useBME280) { Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "1781" "." "355" "\", \"aSM\", @progbits, 1 #"))) = ("BME280 is used"); &__c[0];})))))); } else { Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "1781" "." "356" "\", \"aSM\", @progbits, 1 #"))) = ("BME280 is not used"); &__c[0];})))))); }
            break;
          case 14:
            mySettings.useCCS811 = !bool(mySettings.useCCS811);
            if (mySettings.useCCS811) { Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "1785" "." "357" "\", \"aSM\", @progbits, 1 #"))) = ("CCS811 is used"); &__c[0];})))))); } else { Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "1785" "." "358" "\", \"aSM\", @progbits, 1 #"))) = ("CCS811 is not used"); &__c[0];})))))); }
            break;
          case 15:
            mySettings.useBacklight = !bool(mySettings.useBacklight);
            if (mySettings.useBacklight) { Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "1789" "." "359" "\", \"aSM\", @progbits, 1 #"))) = ("Baclkight is on"); &__c[0];})))))); } else { Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "1789" "." "360" "\", \"aSM\", @progbits, 1 #"))) = ("Backlight is off"); &__c[0];})))))); }
            break;
          case 16:
            // if (bool(mySettings.useMQTT) && !bool(mySettings.useHTTP)) { Serial.println(F("Plese turn off MQTT first!")); } 
            // else { }
            mySettings.useHTTP = !bool(mySettings.useHTTP);
            if (mySettings.useHTTP) { Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "1795" "." "361" "\", \"aSM\", @progbits, 1 #"))) = ("HTTP webserver is on"); &__c[0];})))))); } else { Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "1795" "." "362" "\", \"aSM\", @progbits, 1 #"))) = ("HTTP webserver is off"); &__c[0];})))))); }
            break;
          case 17:
            mySettings.useOTA = !bool(mySettings.useOTA);
            if (mySettings.useOTA) { Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "1799" "." "363" "\", \"aSM\", @progbits, 1 #"))) = ("OTA server is on"); &__c[0];})))))); } else { Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "1799" "." "364" "\", \"aSM\", @progbits, 1 #"))) = ("OTA server is off"); &__c[0];})))))); }
            break;
          case 18:
            break;
          case 99:
            Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "1804" "." "365" "\", \"aSM\", @progbits, 1 #"))) = ("ESP is going to restart. Bye"); &__c[0];}))))));
            Serial.flush();
            ESP.restart();
            break;
          default:
            break;
        }
      }
    }

    else if (command == "j") { // test json output
      char payLoad[256];
      timeJSON(payLoad); Serial.print(payLoad); Serial.printf(" len: %u\r\n", strlen(payLoad)); // ok
      dateJSON(payLoad); Serial.print(payLoad); Serial.printf(" len: %u\r\n", strlen(payLoad)); // ok
      bme280JSON(payLoad); Serial.print(payLoad); Serial.printf(" len: %u\r\n", strlen(payLoad)); // ok
      bme680JSON(payLoad); Serial.print(payLoad); Serial.printf(" len: %u\r\n", strlen(payLoad)); // ok
      ccs811JSON(payLoad); Serial.print(payLoad); Serial.printf(" len: %u\r\n", strlen(payLoad)); // ok
      mlxJSON(payLoad); Serial.print(payLoad); Serial.printf(" len: %u\r\n", strlen(payLoad)); // ok
      scd30JSON(payLoad); Serial.print(payLoad); Serial.printf(" len: %u\r\n", strlen(payLoad)); // ok
      sgp30JSON(payLoad); Serial.print(payLoad); Serial.printf(" len: %u\r\n", strlen(payLoad)); // not ok
      sps30JSON(payLoad); Serial.print(payLoad); Serial.printf(" len: %u\r\n", strlen(payLoad));
      if (false) { Serial.println("DBG:DISPLAY: JSON"); }
    }

    //{"time":{"hour":0,"minute":0,"second":15}} len: 42
    //{"date":{"day":1,"month":1,"year":1970}} len: 40
    //{"bme280":{"avail":false,"p": 0.0,"pavg": 0.0,"rH": 0.0,"aH": 0.0,"T":  0.0,"dp_airquality":"Normal","rH_airquality":"Excessive","T_airquality":"Cold"}} len: 152
    //{"bme680":{"avail":true,"p":927.4,"pavg":927.4,"rH":32.3,"aH": 8.3,"T": 26.6,"resistance":0,"dp_airquality":"Normal","rH_airquality":"Threshold Low","resistance_airquality":"?","T_airquality":"Hot"}} len: 199
    //{"ccs811":{"avail":true,"eCO2":0,"tVOC":0,"eCO2_airquality":"Normal","tVOC_airquality":"Normal"}} len: 97
    //{"mlx":{"avail":true,"To": 26.4,"Ta": 27.0,"fever":"Low ","T_airquality":"Hot"}} len: 80
    //{"scd30":{"avail":true,"CO2":0,"rH":-1.0,"aH":-1.0,"T":-999.0,"CO2_airquality":"Normal","rH_airquality":"?","T_airquality":"?"}} len: 128
    //{"sgp30":{"avail":true,"eCO2":400,"tVOC":0,"eCO2_airquality":"Normal","tVOC_airquality":"Normal"}} len: 98
    //{"sps30":{"avail":false,"PM1": 0.0,"PM2": 0.0,"PM4": 0.0,"PM10": 0.0,"nPM0": 0.0,"nPM1": 0.0,"nPM2": 0.0,"nPM4": 0.0,"nPM10": 0.0,"PartSize": 0.0,"PM2_airquality":"Normal","PM10_airquality":"Normal"}} len: 200

    else if (command == "i") { // simpler display on LCD
      mySettings.consumerLCD = !bool(mySettings.consumerLCD);
      if (mySettings.consumerLCD) {
        Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "1841" "." "366" "\", \"aSM\", @progbits, 1 #"))) = ("Simpler LCD display"); &__c[0];}))))));
      } else {
        Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "1843" "." "367" "\", \"aSM\", @progbits, 1 #"))) = ("Engineering LCD display"); &__c[0];}))))));
      }
    }

    else { // unrecognized command
      helpMenu();
    } // end if

  } // end serial available
} // end Input

void printSettings() {
  Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "1855" "." "368" "\", \"aSM\", @progbits, 1 #"))) = ("=System=========================================================================="); &__c[0];}))))));
  Serial.print(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "1856" "." "369" "\", \"aSM\", @progbits, 1 #"))) = ("Debug level: .................. "); &__c[0];})))))); Serial.println((unsigned int)mySettings.debuglevel);
  Serial.print(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "1857" "." "370" "\", \"aSM\", @progbits, 1 #"))) = ("Reboot Minute: ................ "); &__c[0];})))))); Serial.println(mySettings.rebootMinute);
  Serial.print(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "1858" "." "371" "\", \"aSM\", @progbits, 1 #"))) = ("Runtime [min]: ................ "); &__c[0];})))))); Serial.println((unsigned long)mySettings.runTime / 60);
  Serial.print(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "1859" "." "372" "\", \"aSM\", @progbits, 1 #"))) = ("Simpler Display: .............. "); &__c[0];})))))); Serial.println((mySettings.consumerLCD) ? "on" : "off");
  Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "1860" "." "373" "\", \"aSM\", @progbits, 1 #"))) = ("=Network==========================================================================="); &__c[0];}))))));
  Serial.print(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "1861" "." "374" "\", \"aSM\", @progbits, 1 #"))) = ("WiFi: ......................... "); &__c[0];})))))); Serial.println((mySettings.useWiFi) ? "on" : "off");
  Serial.print(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "1862" "." "375" "\", \"aSM\", @progbits, 1 #"))) = ("HTTP: ......................... "); &__c[0];})))))); Serial.println((mySettings.useHTTP) ? "on" : "off");
  Serial.print(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "1863" "." "376" "\", \"aSM\", @progbits, 1 #"))) = ("MQTT: ......................... "); &__c[0];})))))); Serial.println((mySettings.useMQTT) ? "on" : "off");
  Serial.print(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "1864" "." "377" "\", \"aSM\", @progbits, 1 #"))) = ("OTA: .......................... "); &__c[0];})))))); Serial.println((mySettings.useOTA) ? "on" : "off");
  Serial.print(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "1865" "." "378" "\", \"aSM\", @progbits, 1 #"))) = ("mDNS: ......................... "); &__c[0];})))))); Serial.println((mySettings.usemDNS) ? "on" : "off");
  Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "1866" "." "379" "\", \"aSM\", @progbits, 1 #"))) = ("-Time-------------------------------------------------------------------------------"); &__c[0];}))))));
  Serial.print(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "1867" "." "380" "\", \"aSM\", @progbits, 1 #"))) = ("NTP: .......................... "); &__c[0];})))))); Serial.println((mySettings.useNTP) ? "on" : "off");
  Serial.print(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "1868" "." "381" "\", \"aSM\", @progbits, 1 #"))) = ("NTP Server: ................... "); &__c[0];})))))); Serial.println(mySettings.ntpServer);
  Serial.print(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "1869" "." "382" "\", \"aSM\", @progbits, 1 #"))) = ("Time Zone: .................... "); &__c[0];})))))); Serial.println(mySettings.timeZone);
  Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "1870" "." "383" "\", \"aSM\", @progbits, 1 #"))) = ("-Credentials------------------------------------------------------------------------"); &__c[0];}))))));
  Serial.print(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "1871" "." "384" "\", \"aSM\", @progbits, 1 #"))) = ("SSID: ......................... "); &__c[0];})))))); Serial.println(mySettings.ssid1);
  Serial.print(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "1872" "." "385" "\", \"aSM\", @progbits, 1 #"))) = ("PW: ........................... "); &__c[0];})))))); Serial.println(mySettings.pw1);
  Serial.print(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "1873" "." "386" "\", \"aSM\", @progbits, 1 #"))) = ("SSID: ......................... "); &__c[0];})))))); Serial.println(mySettings.ssid2);
  Serial.print(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "1874" "." "387" "\", \"aSM\", @progbits, 1 #"))) = ("PW: ........................... "); &__c[0];})))))); Serial.println(mySettings.pw2);
  Serial.print(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "1875" "." "388" "\", \"aSM\", @progbits, 1 #"))) = ("SSID: ......................... "); &__c[0];})))))); Serial.println(mySettings.ssid3);
  Serial.print(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "1876" "." "389" "\", \"aSM\", @progbits, 1 #"))) = ("PW: ........................... "); &__c[0];})))))); Serial.println(mySettings.pw3);
  Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "1877" "." "390" "\", \"aSM\", @progbits, 1 #"))) = ("-MQTT-------------------------------------------------------------------------------"); &__c[0];}))))));
  Serial.print(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "1878" "." "391" "\", \"aSM\", @progbits, 1 #"))) = ("MQTT: ......................... "); &__c[0];})))))); Serial.println(mySettings.mqtt_server);
  Serial.print(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "1879" "." "392" "\", \"aSM\", @progbits, 1 #"))) = ("MQTT fallback: ................ "); &__c[0];})))))); Serial.println(mySettings.mqtt_fallback);
  Serial.print(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "1880" "." "393" "\", \"aSM\", @progbits, 1 #"))) = ("MQTT user: .................... "); &__c[0];})))))); Serial.println(mySettings.mqtt_username);
  Serial.print(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "1881" "." "394" "\", \"aSM\", @progbits, 1 #"))) = ("MQTT password: ................ "); &__c[0];})))))); Serial.println(mySettings.mqtt_password);
  Serial.print(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "1882" "." "395" "\", \"aSM\", @progbits, 1 #"))) = ("MQTT send immediatly: ......... "); &__c[0];})))))); Serial.println((mySettings.sendMQTTimmediate) ? "on" : "off");
  Serial.print(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "1883" "." "396" "\", \"aSM\", @progbits, 1 #"))) = ("MQTT main topic: .............. "); &__c[0];})))))); Serial.println(mySettings.mqtt_mainTopic);
  Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "1884" "." "397" "\", \"aSM\", @progbits, 1 #"))) = ("=Sensors============================================================================"); &__c[0];}))))));
  Serial.print(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "1885" "." "398" "\", \"aSM\", @progbits, 1 #"))) = ("SCD30: ........................ "); &__c[0];})))))); Serial.println((mySettings.useSCD30) ? "on" : "off");
  Serial.print(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "1886" "." "399" "\", \"aSM\", @progbits, 1 #"))) = ("SPS30: ........................ "); &__c[0];})))))); Serial.println((mySettings.useSPS30) ? "on" : "off");
  Serial.print(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "1887" "." "400" "\", \"aSM\", @progbits, 1 #"))) = ("SGP30: ........................ "); &__c[0];})))))); Serial.println((mySettings.useSGP30) ? "on" : "off");
  Serial.print(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "1888" "." "401" "\", \"aSM\", @progbits, 1 #"))) = ("MAX30: ........................ "); &__c[0];})))))); Serial.println((mySettings.useMAX30) ? "on" : "off");
  Serial.print(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "1889" "." "402" "\", \"aSM\", @progbits, 1 #"))) = ("MLX: .......................... "); &__c[0];})))))); Serial.println((mySettings.useMLX) ? "on" : "off");
  Serial.print(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "1890" "." "403" "\", \"aSM\", @progbits, 1 #"))) = ("BME680: ....................... "); &__c[0];})))))); Serial.println((mySettings.useBME680) ? "on" : "off");
  Serial.print(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "1891" "." "404" "\", \"aSM\", @progbits, 1 #"))) = ("BME280: ....................... "); &__c[0];})))))); Serial.println((mySettings.useBME280) ? "on" : "off");
  Serial.print(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "1892" "." "405" "\", \"aSM\", @progbits, 1 #"))) = ("CCS811: ....................... "); &__c[0];})))))); Serial.println((mySettings.useCCS811) ? "on" : "off");
  Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "1893" "." "406" "\", \"aSM\", @progbits, 1 #"))) = ("-Offsets&Baselines-------------------------------------------------------------------"); &__c[0];}))))));
  Serial.print(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "1894" "." "407" "\", \"aSM\", @progbits, 1 #"))) = ("Base SGP30 valid: ............. "); &__c[0];})))))); Serial.println((int)mySettings.baselineSGP30_valid);
  Serial.print(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "1895" "." "408" "\", \"aSM\", @progbits, 1 #"))) = ("Base eCO2 SGP30: [ppm]......... "); &__c[0];})))))); Serial.println((int)mySettings.baselineeCO2_SGP30);
  Serial.print(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "1896" "." "409" "\", \"aSM\", @progbits, 1 #"))) = ("Base tVOC SGP30: [ppb]......... "); &__c[0];})))))); Serial.println((int)mySettings.baselinetVOC_SGP30);
  Serial.print(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "1897" "." "410" "\", \"aSM\", @progbits, 1 #"))) = ("Base CCS811 valid: ............ "); &__c[0];})))))); Serial.println((int)mySettings.baselineCCS811_valid);
  Serial.print(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "1898" "." "411" "\", \"aSM\", @progbits, 1 #"))) = ("Base CCS811: [Ohm]............. "); &__c[0];})))))); Serial.println((int)mySettings.baselineCCS811);
  Serial.print(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "1899" "." "412" "\", \"aSM\", @progbits, 1 #"))) = ("Temp Offset SCD30 valid: ...... "); &__c[0];})))))); Serial.println((int)mySettings.tempOffset_SCD30_valid);
  Serial.print(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "1900" "." "413" "\", \"aSM\", @progbits, 1 #"))) = ("Temp Offset SCD30: [C]......... "); &__c[0];})))))); Serial.println((float)mySettings.tempOffset_SCD30);
  //Serial.print(F("Forced Calibration SCD30 valid: ")); Serial.println((int)mySettings.forcedCalibration_SCD30_valid);
  //Serial.print(F("Forced Calibration SCD30:[ppm]. ")); Serial.println((float)mySettings.forcedCalibration_SCD30);
  Serial.print(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "1903" "." "414" "\", \"aSM\", @progbits, 1 #"))) = ("MLX Temp Offset valid: ........ "); &__c[0];})))))); Serial.println((int)mySettings.tempOffset_MLX_valid);
  Serial.print(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "1904" "." "415" "\", \"aSM\", @progbits, 1 #"))) = ("MLX Temp Offset: .............. "); &__c[0];})))))); Serial.println((float)mySettings.tempOffset_MLX);
  Serial.print(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "1905" "." "416" "\", \"aSM\", @progbits, 1 #"))) = ("Average Pressure: ............. "); &__c[0];})))))); Serial.println(mySettings.avgP);
  Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "1906" "." "417" "\", \"aSM\", @progbits, 1 #"))) = ("=LCD============================================================================="); &__c[0];}))))));
  Serial.print(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "1907" "." "418" "\", \"aSM\", @progbits, 1 #"))) = ("LCD: .......................... "); &__c[0];})))))); Serial.println((mySettings.useLCD) ? "on" : "off");
  Serial.print(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "1908" "." "419" "\", \"aSM\", @progbits, 1 #"))) = ("LCD Backlight: ................ "); &__c[0];})))))); Serial.println((mySettings.useBacklight) ? "on" : "off");
  Serial.print(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "1909" "." "420" "\", \"aSM\", @progbits, 1 #"))) = ("Night Start: .................. "); &__c[0];})))))); Serial.println(mySettings.nightBegin);
  Serial.print(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "1910" "." "421" "\", \"aSM\", @progbits, 1 #"))) = ("Night End: .................... "); &__c[0];})))))); Serial.println(mySettings.nightEnd);
  Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "1911" "." "422" "\", \"aSM\", @progbits, 1 #"))) = ("================================================================================="); &__c[0];}))))));
}

void defaultSettings() {
  mySettings.runTime = (unsigned long) 0;
  mySettings.debuglevel = (unsigned int) 1;
  mySettings.baselineSGP30_valid = (byte) 0x00;
  mySettings.baselineeCO2_SGP30 = (uint16_t) 0;
  mySettings.baselinetVOC_SGP30 = (uint16_t) 0;
  mySettings.baselineCCS811_valid = (byte) 0x00;
  mySettings.baselineCCS811 = (uint16_t) 0;
  mySettings.tempOffset_SCD30_valid = (byte) 0x00;
  mySettings.tempOffset_SCD30 = (float) 0.01;
  mySettings.forcedCalibration_SCD30_valid = (byte) 0x00; // not used
  mySettings.forcedCalibration_SCD30 = (float) 0.0; // not used
  mySettings.tempOffset_MLX_valid = (byte) 0x00;
  mySettings.tempOffset_MLX = (float) 0.0;
  strcpy(mySettings.ssid1, "meddev");
  strcpy(mySettings.pw1, "");
  strcpy(mySettings.ssid2, "UAGUest");
  strcpy(mySettings.pw2, "");
  strcpy(mySettings.ssid3, "MuchoCoolioG");
  strcpy(mySettings.pw3, "");
  strcpy(mySettings.mqtt_server, "my.mqqtt.server.org");
  strcpy(mySettings.mqtt_fallback, "192.168.1.1");
  strcpy(mySettings.mqtt_username, "mqtt");
  strcpy(mySettings.mqtt_password, "");
  strcpy(mySettings.mqtt_mainTopic, "Sensi");
  mySettings.sendMQTTimmediate = true;
  mySettings.useLCD = true;
  mySettings.useWiFi = true;
  mySettings.useSCD30 = true;
  mySettings.useSPS30 = true;
  mySettings.useSGP30 = true;
  mySettings.useMAX30 = true;
  mySettings.useMLX = true;
  mySettings.useBME680 = true;
  mySettings.useBME280 = true;
  mySettings.useCCS811 = true;
  mySettings.consumerLCD = true;
  mySettings.avgP = (float) 100000.0;
  mySettings.useBacklight = true;
  mySettings.nightBegin = (uint16_t) 1320;
  mySettings.nightEnd = (uint16_t) 420;
  mySettings.rebootMinute = (int16_t) -1;
  mySettings.useHTTP = false;
  mySettings.useNTP = false;
  mySettings.useMQTT = false;
  mySettings.useOTA = false;
  mySettings.usemDNS = false;
  mySettings.useHTTPUpdater = false;
  strcpy(mySettings.ntpServer, "time.nist.gov");
  strcpy(mySettings.timeZone, "MST7");
}

void printSensors() {

  Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "1968" "." "423" "\", \"aSM\", @progbits, 1 #"))) = ("================================================================================="); &__c[0];}))))));
  Serial.printf("Loop Delay Average: %f Min: %ld Current: %ld\r\n", myDelayAvg, myDelayMin, myDelay);
  Serial.printf("Free Heap Size: %d Fragmentation: %d Max Block Size: %d\r\n", ESP.getFreeHeap(), ESP.getHeapFragmentation(), ESP.getMaxFreeBlockSize());
  Serial.printf("Free Sketch Size: %d\r\n", ESP.getFreeSketchSpace());
  Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "1972" "." "424" "\", \"aSM\", @progbits, 1 #"))) = ("---------------------------------------------------------------------------------"); &__c[0];}))))));

  if (wifi_avail && mySettings.useWiFi) {
    Serial.printf("WiFi status: %s\r\n", WiFi.status() ? "on" : "off");
    Serial.printf("WiFi hostname: %s\r\n", hostName);
    if (wifi_connected) {
      Serial.printf("Connected to: %s\r\n", WiFi.SSID().c_str());
      Serial.printf("IP address: %s\r\n", WiFi.localIP().toString().c_str());
    } else {
      Serial.println("WiFi not connected");
    }
    Serial.print("WiFi State: ");
    if (stateWiFi == START_UP) { Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "1984" "." "425" "\", \"aSM\", @progbits, 1 #"))) = ("Starting Up"); &__c[0];}))))));}
    else if (stateWiFi == CHECK_CONNECTION) { Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "1985" "." "426" "\", \"aSM\", @progbits, 1 #"))) = ("Monitoring connection"); &__c[0];})))))); }
    else if (stateWiFi == IS_SCANNING) { Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "1986" "." "427" "\", \"aSM\", @progbits, 1 #"))) = ("Scanning for known network"); &__c[0];})))))); }
    else if (stateWiFi == IS_CONNECTING) { Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "1987" "." "428" "\", \"aSM\", @progbits, 1 #"))) = ("Connecting"); &__c[0];})))))); }
    else if (stateWiFi == IS_WAITING) { Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "1988" "." "429" "\", \"aSM\", @progbits, 1 #"))) = ("Waiting"); &__c[0];})))))); }

    Serial.print("MDNS State: ");
    if (stateMDNS == START_UP) { Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "1991" "." "430" "\", \"aSM\", @progbits, 1 #"))) = ("Starting Up"); &__c[0];})))))); }
    else if (stateMDNS == CHECK_CONNECTION) { Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "1992" "." "431" "\", \"aSM\", @progbits, 1 #"))) = ("Monitoring connection"); &__c[0];})))))); }
    else if (stateMDNS == IS_SCANNING) { Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "1993" "." "432" "\", \"aSM\", @progbits, 1 #"))) = ("Scanning for known network"); &__c[0];})))))); }
    else if (stateMDNS == IS_CONNECTING) { Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "1994" "." "433" "\", \"aSM\", @progbits, 1 #"))) = ("Connecting"); &__c[0];})))))); }
    else if (stateMDNS == IS_WAITING) { Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "1995" "." "434" "\", \"aSM\", @progbits, 1 #"))) = ("Waiting"); &__c[0];})))))); }

    Serial.print("MQTT State: ");
    if (stateMQTT == START_UP) { Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "1998" "." "435" "\", \"aSM\", @progbits, 1 #"))) = ("Starting Up"); &__c[0];}))))));}
    else if (stateMQTT == CHECK_CONNECTION) { Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "1999" "." "436" "\", \"aSM\", @progbits, 1 #"))) = ("Monitoring connection"); &__c[0];})))))); }
    else if (stateMQTT == IS_SCANNING) { Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "2000" "." "437" "\", \"aSM\", @progbits, 1 #"))) = ("Scanning for known network"); &__c[0];})))))); }
    else if (stateMQTT == IS_CONNECTING) { Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "2001" "." "438" "\", \"aSM\", @progbits, 1 #"))) = ("Connecting"); &__c[0];})))))); }
    else if (stateMQTT == IS_WAITING) { Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "2002" "." "439" "\", \"aSM\", @progbits, 1 #"))) = ("Waiting"); &__c[0];})))))); }

    Serial.print("WebSocket State: ");
    if (stateWebSocket == START_UP) { Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "2005" "." "440" "\", \"aSM\", @progbits, 1 #"))) = ("Starting Up"); &__c[0];}))))));}
    else if (stateWebSocket == CHECK_CONNECTION) { Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "2006" "." "441" "\", \"aSM\", @progbits, 1 #"))) = ("Monitoring connection"); &__c[0];})))))); }
    else if (stateWebSocket == IS_SCANNING) { Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "2007" "." "442" "\", \"aSM\", @progbits, 1 #"))) = ("Scanning for known network"); &__c[0];})))))); }
    else if (stateWebSocket == IS_CONNECTING) { Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "2008" "." "443" "\", \"aSM\", @progbits, 1 #"))) = ("Connecting"); &__c[0];})))))); }
    else if (stateWebSocket == IS_WAITING) { Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "2009" "." "444" "\", \"aSM\", @progbits, 1 #"))) = ("Waiting"); &__c[0];})))))); }

    Serial.print("OTA State: ");
    if (stateOTA == START_UP) { Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "2012" "." "445" "\", \"aSM\", @progbits, 1 #"))) = ("Starting Up"); &__c[0];})))))); }
    else if (stateOTA == CHECK_CONNECTION) { Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "2013" "." "446" "\", \"aSM\", @progbits, 1 #"))) = ("Monitoring connection"); &__c[0];})))))); }
    else if (stateOTA == IS_SCANNING) { Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "2014" "." "447" "\", \"aSM\", @progbits, 1 #"))) = ("Scanning for known network"); &__c[0];})))))); }
    else if (stateOTA == IS_CONNECTING) { Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "2015" "." "448" "\", \"aSM\", @progbits, 1 #"))) = ("Connecting"); &__c[0];})))))); }
    else if (stateOTA == IS_WAITING) { Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "2016" "." "449" "\", \"aSM\", @progbits, 1 #"))) = ("Waiting"); &__c[0];})))))); }

    Serial.print("HTTP State: ");
    if (stateHTTP == START_UP) { Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "2019" "." "450" "\", \"aSM\", @progbits, 1 #"))) = ("Starting Up"); &__c[0];})))))); }
    else if (stateHTTP == CHECK_CONNECTION) { Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "2020" "." "451" "\", \"aSM\", @progbits, 1 #"))) = ("Monitoring connection"); &__c[0];})))))); }
    else if (stateHTTP == IS_SCANNING) { Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "2021" "." "452" "\", \"aSM\", @progbits, 1 #"))) = ("Scanning for known network"); &__c[0];})))))); }
    else if (stateHTTP == IS_CONNECTING) { Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "2022" "." "453" "\", \"aSM\", @progbits, 1 #"))) = ("Connecting"); &__c[0];})))))); }
    else if (stateHTTP == IS_WAITING) { Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "2023" "." "454" "\", \"aSM\", @progbits, 1 #"))) = ("Waiting"); &__c[0];})))))); }

    Serial.print("NTP State: ");
    if (stateNTP == START_UP) { Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "2026" "." "455" "\", \"aSM\", @progbits, 1 #"))) = ("Starting Up"); &__c[0];})))))); }
    else if (stateNTP == CHECK_CONNECTION) { Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "2027" "." "456" "\", \"aSM\", @progbits, 1 #"))) = ("Monitoring connection"); &__c[0];})))))); }
    else if (stateNTP == IS_SCANNING) { Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "2028" "." "457" "\", \"aSM\", @progbits, 1 #"))) = ("Scanning for known network"); &__c[0];})))))); }
    else if (stateNTP == IS_CONNECTING) { Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "2029" "." "458" "\", \"aSM\", @progbits, 1 #"))) = ("Connecting"); &__c[0];})))))); }
    else if (stateNTP == IS_WAITING) { Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "2030" "." "459" "\", \"aSM\", @progbits, 1 #"))) = ("Waiting"); &__c[0];})))))); }

    Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "2032" "." "460" "\", \"aSM\", @progbits, 1 #"))) = ("---------------------------------------------------------------------------------"); &__c[0];}))))));
  }

  if (lcd_avail && mySettings.useLCD) {
    Serial.printf("LCD interval: %d Port: %d SDA %d SCL %d\r\n", intervalLCD, uint32_t(lcd_port), lcd_i2c[0], lcd_i2c[1]);
    Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "2037" "." "461" "\", \"aSM\", @progbits, 1 #"))) = ("---------------------------------------------------------------------------------"); &__c[0];}))))));
  }

  if (scd30_avail && mySettings.useSCD30) {
    char qualityMessage[16];
    scd30_port->begin(scd30_i2c[0], scd30_i2c[1]);
    Serial.printf("SCD30 CO2 %4.1f[ppm], rH %4.1f[%%], T %4.1f[C] T_offset %4.1f[C}\r\n", scd30_ppm, scd30_hum, scd30_temp, scd30.getTemperatureOffset() );
    checkCO2(scd30_ppm,qualityMessage,15);
    Serial.printf("SCD30 CO2 is: %s\r\n", qualityMessage);
    Serial.printf("SCD30 interval: %d Port: %d SDA %d SCL %d \r\n", intervalSCD30, uint32_t(scd30_port), scd30_i2c[0], scd30_i2c[1]);
    Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "2047" "." "462" "\", \"aSM\", @progbits, 1 #"))) = ("---------------------------------------------------------------------------------"); &__c[0];}))))));
  }

  if (bme680_avail && mySettings.useBME680) {
    char qualityMessage[16];
    Serial.printf("BME680 T %+5.1f[C] P %4.1f[mbar] P_ave %4.1f[mbar] rH%4.1f[%%] aH%4.1f[g/m^3] Gas resistance %d[Ohm]\r\n", bme680.temperature, bme680.pressure/100., bme680_pressure24hrs/100., bme680.humidity, bme680_ah, bme680.gas_resistance);
    checkAmbientTemperature(bme680.temperature,qualityMessage, 15);
    Serial.printf("Temperature is %s ",qualityMessage);
    checkdP((bme680.pressure - bme680_pressure24hrs)/100.0,qualityMessage,15),
    Serial.printf("Change in pressure is %s ",qualityMessage);
    checkHumidity(bme680.humidity,qualityMessage,15);
    Serial.printf("Humidity is %s\r\n",qualityMessage);
    checkGasResistance(bme680.gas_resistance,qualityMessage,15);
    Serial.printf("BME680 interval: %d Port: %d SDA %d SCL %d \r\n", intervalBME680, uint32_t(bme680_port), bme680_i2c[0], bme680_i2c[1]);
    Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "2061" "." "463" "\", \"aSM\", @progbits, 1 #"))) = ("---------------------------------------------------------------------------------"); &__c[0];}))))));
  }

  if (bme280_avail && mySettings.useBME280) {
    char qualityMessage[16];
    Serial.printf("BME280 T %+5.1f[C] P %4.1f[mbar] P_ave %4.1f[mbar]", bme280_temp, bme280_pressure/100., bme280_pressure24hrs/100.);
    if (BMEhum_avail) { Serial.printf(" rH%4.1f[%%] aH%4.1f[g/m^3]\r\n", bme280_hum, bme280_ah);} else Serial.println();
    checkAmbientTemperature(bme280_temp,qualityMessage,15);
    Serial.printf("Temperature is %s ",qualityMessage);
    checkdP((bme280_pressure - bme280_pressure24hrs)/100.0,qualityMessage,15);
    Serial.printf("Change in pressure is %s",qualityMessage);
    if (BMEhum_avail) {
      checkHumidity(bme280_hum,qualityMessage,15);
      Serial.printf(" Humidity is %s\r\n",qualityMessage);
    } else { Serial.println(); }
    Serial.printf("BME280 interval: %d Port: %d SDA %d SCL %d \r\n", intervalBME280, uint32_t(bme280_port), bme280_i2c[0], bme280_i2c[1]);
    Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "2077" "." "464" "\", \"aSM\", @progbits, 1 #"))) = ("--------------------------------------------------------------------------------"); &__c[0];}))))));
  }

  if (sgp30_avail && mySettings.useSGP30) {
    char qualityMessage[16];
    Serial.printf("SGP30 CO2 %d[ppm] tVOC%d[ppb] baseline_CO2 %d baseline_tVOC %6d H2 %d[ppb] Ethanol %d[ppm]", sgp30.CO2, sgp30.TVOC, sgp30.baselineCO2, sgp30.baselineTVOC,sgp30.H2, sgp30.ethanol);
    checkCO2(sgp30.CO2,qualityMessage,15);
    Serial.printf("CO2 conentration is %s",qualityMessage);
    checkTVOC(sgp30.TVOC,qualityMessage,15);
    Serial.printf(" tVOC conentration is %s\r\n",qualityMessage);
    Serial.printf("SGP30 interval: %d Port: %d SDA %d SCL %d \r\n", intervalSGP30, uint32_t(sgp30_port), sgp30_i2c[0], sgp30_i2c[1]);
    Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "2088" "." "465" "\", \"aSM\", @progbits, 1 #"))) = ("---------------------------------------------------------------------------------"); &__c[0];}))))));
  }

  if (ccs811_avail && mySettings.useCCS811) {
    char qualityMessage[16];
    uint16_t CO2uI = ccs811.getCO2();
    uint16_t TVOCuI = ccs811.getTVOC();
    ccs811_port->begin(ccs811_i2c[0], ccs811_i2c[1]);
    uint16_t BaselineuI = ccs811.getBaseline();
    Serial.printf("CCS811 CO2 %d tVOC %d CCS811_baseline %d\r\n", CO2uI, TVOCuI, BaselineuI);
    checkCO2(CO2uI,qualityMessage,15);
    Serial.printf("CO2 concentration is %s", qualityMessage);
    checkTVOC(TVOCuI,qualityMessage,15);
    Serial.printf(" tVOC concentration is %s\r\n", qualityMessage);
    Serial.printf("CCS811 mode: %d Port: %d SDA %d SCL %d \r\n", ccs811Mode, uint32_t(sgp30_port), sgp30_i2c[0], sgp30_i2c[1]);
    Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "2103" "." "466" "\", \"aSM\", @progbits, 1 #"))) = ("[4=0.25s, 3=60s, 2=10sec, 1=1sec]"); &__c[0];}))))));
    Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "2104" "." "467" "\", \"aSM\", @progbits, 1 #"))) = ("---------------------------------------------------------------------------------"); &__c[0];}))))));
  }

  if (sps30_avail && mySettings.useSPS30) {
    char qualityMessage[16];
    Serial.printf("SPS30 P1.0 %4.1f[μg/m3] P2.5 %4.1f[μg/m3] P4.0 %4.1f[μg/m3] P10 %4.1f[μg/m3]\r\n", valSPS30.MassPM1, valSPS30.MassPM2, valSPS30.MassPM4, valSPS30.MassPM10);
    Serial.printf("SPS30 P1.0 %4.1f[#/cm3] P2.5 %4.1f[#/cm3] P4.0 %4.1f[#/cm3] P10 %4.1f[#/cm3]\r\n", valSPS30.NumPM0, valSPS30.NumPM2, valSPS30.NumPM4, valSPS30.NumPM10);
    Serial.printf("SPS30 Average %4.1f[μm]\r\n", valSPS30.PartSize);
    checkPM2(valSPS30.MassPM2,qualityMessage,15);
    Serial.printf("PM2.5 is: %s ", qualityMessage);
    checkPM10(valSPS30.MassPM10,qualityMessage,15);
    Serial.printf("PM10 is %s\r\n", qualityMessage);
    Serial.printf("SPS30 interval: %d Port: %d SDA %d SCL %d \r\n", intervalSPS30, uint32_t(sps30_port), sps30_i2c[0], sps30_i2c[1]);
    Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "2117" "." "468" "\", \"aSM\", @progbits, 1 #"))) = ("---------------------------------------------------------------------------------"); &__c[0];}))))));
  }

  if (therm_avail && mySettings.useMLX) {
    char qualityMessage[16];
    mlx_port->begin(mlx_i2c[0], mlx_i2c[1]);
    float tmpOF = therm.object();
    float tmpAF = therm.ambient();
    Serial.printf("MLX Object %+5.1f[C] MLX Ambient %+5.1f[C]\r\n", tmpOF, tmpAF);
    float tmpEF = therm.readEmissivity();
    Serial.printf("MLX Emissivity %4.1f MLX Offset %4.1f\r\n", tmpEF, mlxOffset);
    checkFever(tmpOF,qualityMessage, 15);
    Serial.printf("Object temperature is %s ", qualityMessage);
    checkAmbientTemperature(tmpOF,qualityMessage,15);
    Serial.printf("Ambient temperature is %s\r\n", qualityMessage);
    Serial.printf("MLX interval: %d Port: %d SDA %d SCL %d \r\n", intervalMLX, uint32_t(mlx_port), mlx_i2c[0], mlx_i2c[1]);
    Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "2133" "." "469" "\", \"aSM\", @progbits, 1 #"))) = ("---------------------------------------------------------------------------------"); &__c[0];}))))));
  }

  if (max_avail && mySettings.useMAX30) {
    char qualityMessage[16];
    // max_port->begin(max_i2c[0], max_i2c[1]);
    Serial.printf("MAX interval: %d Port: %d SDA %d SCL %d \r\n", 100 /* fix this once code is available*/, uint32_t(max_port), max_i2c[0], max_i2c[1]);
    Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "2140" "." "470" "\", \"aSM\", @progbits, 1 #"))) = ("---------------------------------------------------------------------------------"); &__c[0];}))))));
  }

  Serial.printf("NTP is: %s and time is %s\r\n", ntp_avail ? "available" : "not available", timeSynced ? "synchronized" : "not synchronized");
  String dateString = "Date: ";
  dateString+= weekDays[localTime->tm_wday];
  dateString+= " ";
  dateString+= months[localTime->tm_mon];
  dateString+= " ";
  dateString+= localTime->tm_mday;
  dateString+= " ";
  dateString+= localTime->tm_year;
  Serial.print(dateString);

  String timeString = " Time: ";
  timeString+= localTime->tm_hour/10 ? localTime->tm_hour/10 : 0;
  timeString+= localTime->tm_hour%10;
  timeString+= ":";
  timeString+= localTime->tm_min/10;
  timeString+= localTime->tm_min%10;
  timeString+= ":";
  timeString+= localTime->tm_sec/10;
  timeString+= localTime->tm_sec%10;
  Serial.println(timeString);
  Serial.printf("Daylight saving time is %s\r\n", (localTime->tm_isdst>0) ? "observed" : "not observed");

  Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Sensi.ino" "." "2166" "." "471" "\", \"aSM\", @progbits, 1 #"))) = ("================================================================================="); &__c[0];}))))));

}
# 1 "d:\\GitHub\\ESP Playground\\AirQuality\\software\\Unified Software\\Sensi\\BME280.ino"
/******************************************************************************************************/
// Initialize BME280
/******************************************************************************************************/
bool initializeBME280() {
  bool success = true;
  bme280_port->begin(bme280_i2c[0], bme280_i2c[1]); // switch to BME280 i2c port  
  bme280.settings.commInterface = 0;
  bme280.settings.I2CAddress = 0x76;

  if (dbglevel > 0) { Serial.print(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "BME280.ino" "." "10" "." "472" "\", \"aSM\", @progbits, 1 #"))) = ("BME280: setting oversampling for sensors\r\n"); &__c[0];})))))); }
  if (fastMode == true) {
    intervalBME280 = 0 /* this will use the calculated interval time */;
    bme280.settings.runMode = 0b11 /* run autonomously*/;
    bme280.settings.tStandby = 5 /* 1000ms*/;
    bme280.settings.filter = 4 /* 16 data poinrts*/;
    bme280.settings.tempOverSample = 5 /* 16x*/;
    bme280.settings.pressOverSample = 5 /* 16x*/;
    bme280.settings.humidOverSample = 5 /* 16x*/;
  } else {
    intervalBME280 = 60000 /* 1 minute*/;
    bme280.settings.runMode = 0b01 /* measure manually*/;
    bme280.settings.tStandby = 0 /* is not relevant*/;
    bme280.settings.filter = 4 /* 16 data points*/;
    bme280.settings.tempOverSample = 1 /* 1x*/;
    bme280.settings.pressOverSample = 1 /* 1x*/;
    bme280.settings.humidOverSample = 1 /* 1x*/;
  }

  if (dbglevel >0) { Serial.print(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "BME280.ino" "." "29" "." "473" "\", \"aSM\", @progbits, 1 #"))) = ("BM[E/P]280: interval: "); &__c[0];})))))); Serial.println(intervalBME280); }

  uint8_t chipID=bme280.beginI2C(*bme280_port);
  if (chipID == 0x58) {
    BMEhum_avail = false;
    if (dbglevel > 0) { Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "BME280.ino" "." "34" "." "474" "\", \"aSM\", @progbits, 1 #"))) = ("BMP280: initialized"); &__c[0];})))))); }
  } else if(chipID == 0x60) {
    BMEhum_avail = true;
    if (dbglevel > 0) { Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "BME280.ino" "." "37" "." "475" "\", \"aSM\", @progbits, 1 #"))) = ("BME280: initialized"); &__c[0];})))))); }
  } else {
    BMEhum_avail = false;
    if (dbglevel > 0) { Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "BME280.ino" "." "40" "." "476" "\", \"aSM\", @progbits, 1 #"))) = ("BMx280: sensor not detected, please check wiring"); &__c[0];})))))); }
    stateBME280 = HAS_ERROR;
    bme280_avail = false;
    success = false;
  }

  // Calculate the time it takes to complete a measurement cycle
  // -----------------------------------------------------------
  float tmp = 1.0;
  if (bme280.settings.tempOverSample == 1) {tmp = tmp + 2.;}
  else if (bme280.settings.tempOverSample == 2) {tmp = tmp + 4.;}
  else if (bme280.settings.tempOverSample == 3) {tmp = tmp + 8.;}
  else if (bme280.settings.tempOverSample == 4) {tmp = tmp + 16.;}
  else if (bme280.settings.tempOverSample == 5) {tmp = tmp + 32.;}

  if (bme280.settings.pressOverSample == 1) {tmp = tmp + 2.5;}
  else if (bme280.settings.pressOverSample == 2) {tmp = tmp + 4.5;}
  else if (bme280.settings.pressOverSample == 3) {tmp = tmp + 8.5;}
  else if (bme280.settings.pressOverSample == 4) {tmp = tmp + 16.5;}
  else if (bme280.settings.pressOverSample == 5) {tmp = tmp + 32.5;}

  if (BMEhum_avail) {
    if (bme280.settings.humidOverSample == 1) {tmp = tmp + 2.5;}
    else if (bme280.settings.humidOverSample == 2) {tmp = tmp + 4.5;}
    else if (bme280.settings.humidOverSample == 3) {tmp = tmp + 8.5;}
    else if (bme280.settings.humidOverSample == 4) {tmp = tmp + 16.5;}
    else if (bme280.settings.humidOverSample == 5) {tmp = tmp + 32.5;}
  }

  if (bme280.settings.runMode == 3) {
    if (bme280.settings.tStandby == 0) { tmp = tmp + 0.5; }
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
  // filter cut off frequency is 1/day
  float w_c = 2.0 * 3.141 / (24.0 * 3600.0); // = cut off frequency [radians / seconds]
  float f_s = 1.0 / (intervalBME280 / 1000.0); // = sampling frenecy [1/s] 
          w_c = w_c / f_s; // = normalize cut off frequency [radians]
  float y = 1 - cos(w_c); // compute alpha for 3dB attenuation at cut off frequency
  alphaBME280 = -y + sqrt( y*y + 2 * y); // 

  if ((chipID == 0x58) || (chipID == 0x60)) { //
    if (bme280.settings.runMode == 0b11) { // for normal mode we obtain readings periodically
        bme280.setMode(0b11); //
        stateBME280 = IS_BUSY; //
    } else if (bme280.settings.runMode == 0b01) { // for forced mode we initiate reading manually
      bme280.setMode(0b00); // sleep for now
      stateBME280 = IS_SLEEPING; //
    } else if (bme280.settings.runMode == 0b00){ //
       bme280.setMode(0b00); //
       stateBME280 = IS_SLEEPING; //
    }
  }

  if ((mySettings.avgP > 30000.0) && (mySettings.avgP <= 200000.0)) { bme280_pressure24hrs = mySettings.avgP; }

  if (dbglevel > 0) { Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "BME280.ino" "." "108" "." "477" "\", \"aSM\", @progbits, 1 #"))) = ("BME280: initialized"); &__c[0];})))))); }
  delay(50);
  return success;

} // end bme280


/******************************************************************************************************/
// Update BME280
/******************************************************************************************************/
bool updateBME280() {
  bool success = true;
  switch(stateBME280) {

    case IS_SLEEPING: { // ------------------ Slow and Energy Saving Mode: Wait until time to start measurement
      if ((currentTime - lastBME280) >= intervalBME280) {
        bme280_port->begin(bme280_i2c[0], bme280_i2c[1]);
        bme280.setMode(0b01); // Start reading
        lastBME280 = currentTime;
        stateBME280 = IS_BUSY;
      }
      break;
    }

    case IS_BUSY: { // ---------------------- Slow and Energy Saving Mode: Wait until measurement complete
      if ((currentTime - lastBME280) >= bme280_measuretime) { // wait until measurement completed
        bme280_port->begin(bme280_i2c[0], bme280_i2c[1]);
        // check if measurement is actually completed, if not wait some longer
        if (bme280.isMeasuring() == true) {
          if ((currentTime - lastBME280) >= 2 * bme280_measuretime)
            if (dbglevel == 9) { Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "BME280.ino" "." "138" "." "478" "\", \"aSM\", @progbits, 1 #"))) = ("BM[E/P]280: failed to complete reading"); &__c[0];})))))); }
            stateBME280 = HAS_ERROR;
            success = false;
        } else {
          stateBME280 = DATA_AVAILABLE;
        } // yes its completed
      }
      break;
    }

    case IS_MEASURING: { // ------------------ Fast and Accurate Mode: Wait until mesaurement complete
      if ((currentTime - lastBME280) >= intervalBME280) {
        stateBME280 = DATA_AVAILABLE;
      }
      break;
    }

    case DATA_AVAILABLE : { //--------------- Data is available either from slow or fast mode
      bme280_port->begin(bme280_i2c[0], bme280_i2c[1]);
      bme280_temp = bme280.readTempC();
      bme280_pressure = bme280.readFloatPressure();
      if (BMEhum_avail) {
        bme280_hum = bme280.readFloatHumidity();
        float tmp = 273.15 + bme280_temp;
        bme280_ah = bme280_hum * 13.246 / tmp * exp(19.854 - 5423.0/tmp); // [gr/m^3]
      } else {
        bme280_hum = -1.0;
        bme280_ah =-1.0;
      }
      if (dbglevel == 9) { Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "BME280.ino" "." "167" "." "479" "\", \"aSM\", @progbits, 1 #"))) = ("BM[E/P]280: readout completed"); &__c[0];})))))); }
      bme280NewData = true;
      bme280NewDataWS = true;
      lastBME280 = currentTime;

      if (bme280_pressure24hrs == 0.0) {bme280_pressure24hrs = bme280_pressure;}
      else { bme280_pressure24hrs = (1.0-alphaBME280) * bme280_pressure24hrs + alphaBME280 * bme280_pressure; }
      mySettings.avgP = bme280_pressure24hrs;

      if (fastMode) { stateBME280 = IS_MEASURING; }
      else { stateBME280 = IS_SLEEPING; }
      break;
    }

    case HAS_ERROR : {
      // What are we going to do about that?
      if (dbglevel > 9) { Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "BME280.ino" "." "183" "." "480" "\", \"aSM\", @progbits, 1 #"))) = ("BME280: has error!"); &__c[0];})))))); }
      success = false;
      break;
    }

  } // end cases
  return success;
}

void bme280JSON(char *payload){
  //{"bme280":{ "avail":true, "p":123.4, "pavg":1234.5, "rH":123.4,"aH":123.4,"T":-25.0,"dp_airquality":"normal", "rh_airquality":"normal"}}
  // about 150 Ccharacters
  char qualityMessage1[16];
  char qualityMessage2[16];
  char qualityMessage3[16];
  checkdP((bme280_pressure-bme280_pressure24hrs)/100.0, qualityMessage1, 15);
  checkHumidity(bme280_hum, qualityMessage2, 15);
  checkAmbientTemperature(bme280_temp, qualityMessage3, 15);
  sprintf(payload, "{\"bme280\":{\"avail\":%s,\"p\":%4.1f,\"pavg\":%4.1f,\"rH\":%4.1f,\"aH\":%4.1f,\"T\":%5.1f,\"dp_airquality\":\"%s\",\"rH_airquality\":\"%s\",\"T_airquality\":\"%s\"}}",
                       bme280_avail ? "true" : "false",
                       bme280_pressure/100.0,
                       bme280_pressure24hrs/100.0,
                       bme280_hum,
                       bme280_ah,
                       bme280_temp,
                       qualityMessage1,
                       qualityMessage2,
                       qualityMessage3);
}
# 1 "d:\\GitHub\\ESP Playground\\AirQuality\\software\\Unified Software\\Sensi\\BME680.ino"
/******************************************************************************************************/
// Initialize BME680
/******************************************************************************************************/
bool initializeBME680() {
  bool success = true;
  bme680_port->begin(bme680_i2c[0], bme680_i2c[1]);

  if (bme680.begin(0x77, true, bme680_port) == true) {
    stateBME680 = IS_IDLE;
    if (dbglevel > 0) { Serial.print(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "BME680.ino" "." "10" "." "481" "\", \"aSM\", @progbits, 1 #"))) = ("BME680: setting oversampling for sensors\r\n"); &__c[0];})))))); }
    if (fastMode == true) {
      intervalBME680 = 5000 /* 5sec, mimimum 1s */;
      bme680.setTemperatureOversampling(5 /* 1,2,4,8*,16x*/);
      bme680.setHumidityOversampling(5 /* 1,2*,4,8,16x*/);
      bme680.setPressureOversampling(5 /* 1,2,4*,8,16x*/);
      bme680.setIIRFilterSize(4 /* 0,1,3*,7,15,31,63,127 */);
      if (dbglevel > 0) { Serial.print(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "BME680.ino" "." "17" "." "482" "\", \"aSM\", @progbits, 1 #"))) = ("BME680: IIR filter set for fast measurements\r\n"); &__c[0];})))))); }
    } else {
      intervalBME680 = 60000 /* 60sec*/;
      bme680.setTemperatureOversampling(1 /* 1,2,4,8*,16x*/);
      bme680.setHumidityOversampling(1 /* 1,2*,4,8,16x*/);
      bme680.setPressureOversampling(1 /* 1,2,4*,8,16x*/);
      bme680.setIIRFilterSize(0 /* 0,1,3*,7,15,31,63,127 */);
      if (dbglevel > 0) { Serial.print(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "BME680.ino" "." "24" "." "483" "\", \"aSM\", @progbits, 1 #"))) = ("BME680: IIR filter set for slow measurements\r\n"); &__c[0];})))))); }
    }
    if (dbglevel >0) { Serial.print(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "BME680.ino" "." "26" "." "484" "\", \"aSM\", @progbits, 1 #"))) = ("BME680: interval: "); &__c[0];})))))); Serial.println(intervalBME680); }

    bme680.setGasHeater(320 /* C*/,150 /* ms, time it takes for stable reading*/);
    if (dbglevel > 0) { Serial.print(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "BME680.ino" "." "29" "." "485" "\", \"aSM\", @progbits, 1 #"))) = ("BME680: gas measurement set to 320\xC2\xB0\x43 for 150ms\r\n"); &__c[0];})))))); }
    BMEhum_avail = true;
    if (dbglevel > 0) { Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "BME680.ino" "." "31" "." "486" "\", \"aSM\", @progbits, 1 #"))) = ("BME680: initialized"); &__c[0];})))))); }
  } else {
    if (dbglevel > 0) { Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "BME680.ino" "." "33" "." "487" "\", \"aSM\", @progbits, 1 #"))) = ("BME680: sensor not detected, please check wiring"); &__c[0];})))))); }
    stateBME680 = HAS_ERROR;
    bme680_avail = false;
    success = false;
  }

  // Construct poor man s lowpass filter y = (1-alpha) * y + alpha * x
  // https://dsp.stackexchange.com/questions/54086/single-pole-iir-low-pass-filter-which-is-the-correct-formula-for-the-decay-coe
  // filter cut off frequency is 1/day
  float w_c = 2.0 * 3.141 / (24.0 * 3600.0); // = cut off frequency [radians / seconds]
  float f_s = 1.0 / (intervalBME680 / 1000.0); // = sampling frenecy [1/s] 
          w_c = w_c / f_s; // = normalize cut off frequency [radians]
  float y = 1 - cos(w_c); //
  alphaBME680 = -y + sqrt( y*y + 2 * y); // 

  if ((mySettings.avgP > 30000.0) && (mySettings.avgP <= 200000.0)) { bme680_pressure24hrs = mySettings.avgP; }
  delay(50);
  return success;
}

/******************************************************************************************************/
// Update BME680
/******************************************************************************************************/
bool updateBME680() {
  bool success = true;

  switch(stateBME680) {

    case IS_IDLE : { //---------------------
      if ((currentTime - lastBME680) >= intervalBME680) {
        bme680_port->begin(bme680_i2c[0], bme680_i2c[1]);
        tmpTime = millis();
        endTimeBME680 = bme680.beginReading();
        lastBME680 = currentTime;
        if (endTimeBME680 == 0) {
          if (dbglevel > 0) { Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "BME680.ino" "." "68" "." "488" "\", \"aSM\", @progbits, 1 #"))) = ("BME680: failed to begin reading"); &__c[0];})))))); }
          stateBME680 = HAS_ERROR;
          success = false;
        } else {
          stateBME680 = IS_BUSY;
        }
        if (dbglevel == 9) { Serial.print(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "BME680.ino" "." "74" "." "489" "\", \"aSM\", @progbits, 1 #"))) = ("BME680: reading started. Completes in "); &__c[0];})))))); Serial.print(endTimeBME680-tmpTime); Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "BME680.ino" "." "74" "." "490" "\", \"aSM\", @progbits, 1 #"))) = ("ms"); &__c[0];})))))); }
      }
      break;
    }

    case IS_BUSY : { //---------------------
      if (currentTime > endTimeBME680) {
        stateBME680 = DATA_AVAILABLE;
      }
      break;
    }

    case DATA_AVAILABLE : { //---------------------
      bme680_port->begin(bme680_i2c[0], bme680_i2c[1]);
      if (bme680.endReading() == false) {
        if (dbglevel == 9) { Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "BME680.ino" "." "89" "." "491" "\", \"aSM\", @progbits, 1 #"))) = ("BME680: Failed to complete reading, timeout"); &__c[0];})))))); }
        stateBME680 = HAS_ERROR;
        success = false;
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
        // Saturation Vapor Pressure: es = 611.3 exp (5423 [K] *(1/273.15[K] - 1/T[K]))) [P]
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

        if (dbglevel == 9) { Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "BME680.ino" "." "125" "." "492" "\", \"aSM\", @progbits, 1 #"))) = ("BME680: readout completed"); &__c[0];})))))); }
        bme680NewData = true;
        bme680NewDataWS = true;

        stateBME680 = IS_IDLE;
      }
      break;
    }

    case HAS_ERROR : {
      // What are we going to do about that?
      if (dbglevel > 0) { Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "BME680.ino" "." "136" "." "493" "\", \"aSM\", @progbits, 1 #"))) = ("BME680: has error"); &__c[0];})))))); }
      bme680_avail = false;
      success = false;
      break;
    }

  } // end cases
  return success;
}

void bme680JSON(char *payload){
  // {"bme680":{"avail":true, "p":1234.5, "rH":12.3, "ah":123.4, "T":-25.1, "resistance":1234, "rH_airquality":"normal", "resistance_airquality":"normal"}}
  char qualityMessage1[16];
  char qualityMessage2[16];
  char qualityMessage3[16];
  char qualityMessage4[16];
  checkdP((bme680.pressure-bme680_pressure24hrs)/100.0, qualityMessage1, 15);
  checkHumidity(bme680.humidity, qualityMessage2, 15);
  checkGasResistance(bme680.gas_resistance, qualityMessage3, 15);
  checkAmbientTemperature(bme680.temperature, qualityMessage4, 15);
  sprintf(payload, "{\"bme680\":{\"avail\":%s,\"p\":%4.1f,\"pavg\":%4.1f,\"rH\":%4.1f,\"aH\":%4.1f,\"T\":%5.1f,\"resistance\":%d,\"dp_airquality\":\"%s\",\"rH_airquality\":\"%s\",\"resistance_airquality\":\"%s\",\"T_airquality\":\"%s\"}}",
                       bme680_avail ? "true" : "false",
                       bme680.pressure/100.0,
                       bme680_pressure24hrs/100.0,
                       bme680.humidity,
                       bme680_ah,
                       bme680.temperature,
                       bme680.gas_resistance,
                       qualityMessage1,
                       qualityMessage2,
                       qualityMessage3,
                       qualityMessage4);
}
# 1 "d:\\GitHub\\ESP Playground\\AirQuality\\software\\Unified Software\\Sensi\\CCS811.ino"
# 2 "d:\\GitHub\\ESP Playground\\AirQuality\\software\\Unified Software\\Sensi\\CCS811.ino" 2

/******************************************************************************************************/
// Interrupt Handler CCS811
/******************************************************************************************************/
void __attribute__((section("\".iram.text." "CCS811.ino" "." "6" "." "494" "\""))) handleCCS811Interrupt() { // interrupt service routine to handle data ready signal
    if (fastMode == true) {
      stateCCS811 = DATA_AVAILABLE; // update the sensor state
    } else {
      digitalWrite(D0 /* normal high, active low to wake up sensor, might need to pull low and high w resistor*/, 0x0); // set CCS811 to wake up 
      stateCCS811 = IS_WAKINGUP; // update the sensor state
      lastCCS811Interrupt = millis();
    }
    if (dbglevel == 10) {Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "CCS811.ino" "." "14" "." "495" "\", \"aSM\", @progbits, 1 #"))) = ("CCS811: interrupt occured"); &__c[0];})))))); }
}

/******************************************************************************************************/
// INITIALIZE CCS811
/******************************************************************************************************/
bool initializeCCS811(){
  bool success = true;
  CCS811Core::CCS811_Status_e css811Ret;

  if (fastMode == true) {
    ccs811Mode = 2 /* 4=0.25s, 3=60s, 2=10sec, 1=1sec.*/; // 
    intervalCCS811Baseline = 300000 /* 5 mins*/;
    intervalCCS811Humidity = 120000 /* 2 min, update humitidy from other sensor to improve accuracy*/;
  } else {
    ccs811Mode = 3 /* */; // 
    intervalCCS811Baseline = 3600000 /* 1 hour*/;
    intervalCCS811Humidity = 300000 /* 5 mins, update humidity from other sensor to improve accuracy*/;
    if (dbglevel > 0) { Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "CCS811.ino" "." "32" "." "496" "\", \"aSM\", @progbits, 1 #"))) = ("CCS811: it will take about 5 minutes until readings are non-zero"); &__c[0];})))))); }
  }
  warmupCCS811 = currentTime + 43200000 /* sensor needs 12hr until baseline stable*/;

  // 4=0.25s, 3=60s, 2=10sec, 1=1sec.
  if ( ccs811Mode == 1 ) {
    intervalCCS811 = 1000;
  } else if ( ccs811Mode == 2 ) {
    intervalCCS811 = 10000;
  } else if ( ccs811Mode == 3 ) {
    intervalCCS811 = 60000;
  } else {
    intervalCCS811 = 250;
  }
  if (dbglevel >0) { Serial.print(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "CCS811.ino" "." "46" "." "497" "\", \"aSM\", @progbits, 1 #"))) = ("CCS811: Interval: "); &__c[0];})))))); Serial.println(intervalCCS811); }

  pinMode(D0 /* normal high, active low to wake up sensor, might need to pull low and high w resistor*/, 0x01); // CCS811 not Wake Pin
  pinMode(CCS811interruptPin, 0x02); // CCS811 not Interrupt Pin
  attachInterrupt((((CCS811interruptPin) < 16)? (CCS811interruptPin) : -1), handleCCS811Interrupt, 0x02);
  if (dbglevel > 0) { Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "CCS811.ino" "." "51" "." "498" "\", \"aSM\", @progbits, 1 #"))) = ("CCS811: interrupt configured"); &__c[0];})))))); }

  digitalWrite(D0 /* normal high, active low to wake up sensor, might need to pull low and high w resistor*/, 0x0); // Set CCS811 to wake
  delay(1); // wakeup takes 50 microseconds
  if (dbglevel > 0) { Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "CCS811.ino" "." "55" "." "499" "\", \"aSM\", @progbits, 1 #"))) = ("CCS811: sensor waking up"); &__c[0];})))))); }

  ccs811_port->begin(ccs811_i2c[0], ccs811_i2c[1]);

  css811Ret = ccs811.beginWithStatus(*ccs811_port); // has delays and wait loops
  if (css811Ret != CCS811Core::CCS811_Stat_SUCCESS) {
      if (dbglevel > 0) { Serial.print(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "CCS811.ino" "." "61" "." "500" "\", \"aSM\", @progbits, 1 #"))) = ("CCS811: error opening port - "); &__c[0];})))))); Serial.println(ccs811.statusString(css811Ret)); }
      ccs811_avail = false;
      success = false;
  }

  if (dbglevel > 0) { Serial.print(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "CCS811.ino" "." "66" "." "501" "\", \"aSM\", @progbits, 1 #"))) = ("CCS811: begin - "); &__c[0];})))))); Serial.println(ccs811.statusString(css811Ret)); }
  css811Ret = ccs811.setDriveMode(ccs811Mode);
  if (css811Ret != CCS811Core::CCS811_Stat_SUCCESS) {
      if (dbglevel > 0) { Serial.print(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "CCS811.ino" "." "69" "." "502" "\", \"aSM\", @progbits, 1 #"))) = ("CCS811: error setting drive mode - "); &__c[0];})))))); Serial.println(ccs811.statusString(css811Ret)); }
      ccs811_avail = false;
      success = false;
  }

  if (dbglevel > 0) { Serial.print(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "CCS811.ino" "." "74" "." "503" "\", \"aSM\", @progbits, 1 #"))) = ("CCS811: mode request set - "); &__c[0];})))))); Serial.println(ccs811.statusString(css811Ret)); }
  css811Ret = ccs811.enableInterrupts(); // Configure and enable the interrupt line, then print error status
  if (css811Ret != CCS811Core::CCS811_Stat_SUCCESS) {
      if (dbglevel > 0) { Serial.print(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "CCS811.ino" "." "77" "." "504" "\", \"aSM\", @progbits, 1 #"))) = ("CCS811: error enable interrupts - "); &__c[0];})))))); Serial.println(ccs811.statusString(css811Ret)); }
      ccs811_avail = false;
      success = false;
  }

  if (dbglevel > 0) { Serial.print(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "CCS811.ino" "." "82" "." "505" "\", \"aSM\", @progbits, 1 #"))) = ("CCS811: interrupt configuation - "); &__c[0];})))))); Serial.println(ccs811.statusString(css811Ret)); }
  if (mySettings.baselineCCS811_valid == 0xF0) {
    CCS811Core::CCS811_Status_e errorStatus = ccs811.setBaseline(mySettings.baselineCCS811);
    if (errorStatus == CCS811Core::CCS811_Stat_SUCCESS) {
      if (dbglevel > 0) { Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "CCS811.ino" "." "86" "." "506" "\", \"aSM\", @progbits, 1 #"))) = ("CCS811: baseline programmed"); &__c[0];})))))); }
    } else {
      if (dbglevel > 0) { Serial.print(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "CCS811.ino" "." "88" "." "507" "\", \"aSM\", @progbits, 1 #"))) = ("CCS811: error writing baseline - "); &__c[0];})))))); Serial.println(ccs811.statusString(errorStatus)); }
      ccs811_avail = false;
      success = false;
    }
  }
  if (dbglevel > 0) { Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "CCS811.ino" "." "93" "." "508" "\", \"aSM\", @progbits, 1 #"))) = ("CCS811: initialized"); &__c[0];})))))); }
  stateCCS811 = IS_IDLE;
  delay(50);
  return success;
}

/******************************************************************************************************/
// UPDATE CCS811
/******************************************************************************************************/
bool updateCCS811() {
  // Operation:
  //  Setup 
  //   turn on sensor and starts reading
  //  When measurement complete, interrupt is asserted and ISR is called
  //  if fastMode = false
  //    ISR wakes up i2c communication on wakeup pin
  //    Sensor is given 1ms to wake up
  //    Senor data is read and sensor logic is switched back to sleep mode
  //  if fastMode = true
  //    Sensor data is read
  //  Status is sleeping
  //    Waiting until data interrupt occurs

  bool success = true;

  switch(stateCCS811) {

    case IS_WAKINGUP : { // ISR will activate this when getting out of sleeping
      if ((currentTime - lastCCS811Interrupt) >= 1) { // wakeup time is 50micro seconds
        stateCCS811 = DATA_AVAILABLE;
        if (dbglevel == 10) { Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "CCS811.ino" "." "123" "." "509" "\", \"aSM\", @progbits, 1 #"))) = ("CCS811: wake up completed"); &__c[0];})))))); }
      }
      break;
    }

    case DATA_AVAILABLE : { // executed either after sleeping or ideling
      tmpTime = millis();
      ccs811_port->begin(ccs811_i2c[0], ccs811_i2c[1]);
      if (ccs811.readAlgorithmResults() != CCS811Core::CCS811_Stat_SUCCESS) { // Calling this function updates the global tVOC and CO2 variables
        stateCCS811 = HAS_ERROR;
        ccs811_avail = false;
        success = false;
      } else {

        if (dbglevel == 10) { Serial.print(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "CCS811.ino" "." "137" "." "510" "\", \"aSM\", @progbits, 1 #"))) = ("CCS811: readAlgorithmResults completed in "); &__c[0];})))))); Serial.print((millis()-tmpTime)); Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "CCS811.ino" "." "137" "." "511" "\", \"aSM\", @progbits, 1 #"))) = ("ms"); &__c[0];})))))); }
        ccs811NewData=true;
        ccs811NewDataWS=true;
        uint8_t error = ccs811.getErrorRegister();
        if (dbglevel > 0) {
          if (error == 0xFF) { Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "CCS811.ino" "." "142" "." "512" "\", \"aSM\", @progbits, 1 #"))) = ("CCS811: failed to read ERROR_ID register"); &__c[0];})))))); }
          else {
            if (error & 1 << 5) Serial.print(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "CCS811.ino" "." "144" "." "513" "\", \"aSM\", @progbits, 1 #"))) = ("CCS811: error HeaterSupply"); &__c[0];}))))));
            if (error & 1 << 4) Serial.print(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "CCS811.ino" "." "145" "." "514" "\", \"aSM\", @progbits, 1 #"))) = ("CCS811: error HeaterFault"); &__c[0];}))))));
            if (error & 1 << 3) Serial.print(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "CCS811.ino" "." "146" "." "515" "\", \"aSM\", @progbits, 1 #"))) = ("CCS811: error MaxResistance"); &__c[0];}))))));
            if (error & 1 << 2) Serial.print(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "CCS811.ino" "." "147" "." "516" "\", \"aSM\", @progbits, 1 #"))) = ("CCS811: error MeasModeInvalid"); &__c[0];}))))));
            if (error & 1 << 1) Serial.print(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "CCS811.ino" "." "148" "." "517" "\", \"aSM\", @progbits, 1 #"))) = ("CCS811: error ReadRegInvalid"); &__c[0];}))))));
            if (error & 1 << 0) Serial.print(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "CCS811.ino" "." "149" "." "518" "\", \"aSM\", @progbits, 1 #"))) = ("CCS811: error MsgInvalid"); &__c[0];}))))));
          }
        }

        if ((currentTime - lastCCS811Baseline) >= intervalCCS811Baseline ) {
          tmpTime = millis();
          mySettings.baselineCCS811 = ccs811.getBaseline();
          lastCCS811Baseline = currentTime;
          if (dbglevel == 10) { Serial.print(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "CCS811.ino" "." "157" "." "519" "\", \"aSM\", @progbits, 1 #"))) = ("CCS811: baseline obtained in"); &__c[0];})))))); Serial.print((millis()-tmpTime)); Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "CCS811.ino" "." "157" "." "520" "\", \"aSM\", @progbits, 1 #"))) = ("ms"); &__c[0];})))))); }
        }

        if ((currentTime - lastCCS811Humidity) > intervalCCS811Humidity ) {
          lastCCS811Humidity = currentTime;
          if (bme680_avail && mySettings.useBME680) {
            tmpTime = millis();
            ccs811.setEnvironmentalData(bme680.humidity, bme680.temperature);
            if (dbglevel == 10) { Serial.print(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "CCS811.ino" "." "165" "." "521" "\", \"aSM\", @progbits, 1 #"))) = ("CCS811: humidity and temperature compensation updated in "); &__c[0];})))))); Serial.print((millis()-tmpTime)); Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "CCS811.ino" "." "165" "." "522" "\", \"aSM\", @progbits, 1 #"))) = ("ms"); &__c[0];})))))); }
          }
        }

        if (fastMode == false) {
          digitalWrite(D0 /* normal high, active low to wake up sensor, might need to pull low and high w resistor*/, 0x1); // put CCS811 to sleep
          if (dbglevel == 10) { Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "CCS811.ino" "." "171" "." "523" "\", \"aSM\", @progbits, 1 #"))) = ("CCS811: puttting sensor to sleep"); &__c[0];})))))); }
          stateCCS811 = IS_SLEEPING;
        } else {
          stateCCS811 = IS_IDLE;
        }

      }
      lastCCS811 = currentTime;
      break;
    }

    case IS_IDLE : { //---------------------
      // We want to continue ideling as we are waiting for interrupt      
      // However if if interrupt timed out, we obtain data manually
      if ( (currentTime - lastCCS811) > 2*intervalCCS811 ) {
        if (dbglevel == 10) { Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "CCS811.ino" "." "186" "." "524" "\", \"aSM\", @progbits, 1 #"))) = ("CCS811: interrupt timeout occured"); &__c[0];})))))); }
        ccs811_port->begin(ccs811_i2c[0], ccs811_i2c[1]);
        if ( ccs811.dataAvailable() ) {
          if (fastMode == true) {
            stateCCS811 = DATA_AVAILABLE; // update the sensor state
          } else {
            digitalWrite(D0 /* normal high, active low to wake up sensor, might need to pull low and high w resistor*/, 0x0); // set CCS811 to wake up 
            stateCCS811 = IS_WAKINGUP; // update the sensor state
            lastCCS811Interrupt = millis();
          }
        } else {
          stateCCS811 = HAS_ERROR;
          ccs811_avail = false;
          success = false;
        }
      }

      if (dbglevel == 10) { Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "CCS811.ino" "." "203" "." "525" "\", \"aSM\", @progbits, 1 #"))) = ("CCS811: is idle"); &__c[0];})))))); }
      // if we dont use ISR method use this:
      //if (digitalRead(CCS811_INT) == 0) {
      //  stateCCS811 = DATA_AVAILABLE;          // update the sensor state
      //  if (dbglevel == 10) {Serial.println(F("CCS811: interrupt occured")); }
      //} // end if not use ISR
      break;
    }

    case IS_SLEEPING : { //---------------------
      // Continue Sleeping, we are waiting for interrupt
      if (dbglevel == 10) { Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "CCS811.ino" "." "214" "." "526" "\", \"aSM\", @progbits, 1 #"))) = ("CCS811: is sleeping"); &__c[0];})))))); }
      // if we dont want to use ISR method use this:
      //if (digitalRead(CCS811_INT) == 0) {
      //  digitalWrite(CCS811_WAKE, LOW);        // set CCS811 to wake up 
      //  stateCCS811 = IS_WAKINGUP;             // update the sensor state
      //  lastCCS811Interrupt = millis();
      //  if (dbglevel == 10) {Serial.println(F("CCS811: interrupt occured")); }
      //} // end if not use ISR
      break;
    }

    case HAS_ERROR : { //-----------------------
      if (dbglevel > 0) { Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "CCS811.ino" "." "226" "." "527" "\", \"aSM\", @progbits, 1 #"))) = ("CCS811: error"); &__c[0];})))))); }
      success = false;
      break;
    }

  } // end switch

  return success;
}

void ccs811JSON(char *payload){
  char qualityMessage1[16];
  char qualityMessage2[16];
  checkCO2(float(ccs811.getCO2()), qualityMessage1, 15);
  checkTVOC(float(ccs811.getTVOC()), qualityMessage2, 15);
  sprintf(payload, "{\"ccs811\":{\"avail\":%s,\"eCO2\":%d,\"tVOC\":%d,\"eCO2_airquality\":\"%s\",\"tVOC_airquality\":\"%s\"}}",
                       ccs811_avail ? "true" : "false",
                       ccs811.getCO2(),
                       ccs811.getTVOC(),
                       qualityMessage1,
                       qualityMessage2);
}
# 1 "d:\\GitHub\\ESP Playground\\AirQuality\\software\\Unified Software\\Sensi\\Config.ino"
// Save and Read Settings
// Uses JSON structure on LittleFS Filespace
// These routines are not used for bootup and regular intervals

void saveConfiguration(const char *filename, Settings &config) {
  // Delete existing file, otherwise the configuration is appended to the file
  LittleFS.remove(filename);

  // Open file for writing
  File file = LittleFS.open(filename, "w+");
  if (!file) {
    Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Config.ino" "." "12" "." "528" "\", \"aSM\", @progbits, 1 #"))) = ("Failed to create file"); &__c[0];}))))));
    return;
  }

  // Allocate a temporary JsonDocument
  // Use arduinojson.org/assistant to compute the capacity.
  StaticJsonDocument<4096 /* crashes program if too small, use online tool to check recommended size*/> doc;

  // Set the values in the document
  doc["runTime"] = config.runTime; // keep track of total sensor run time
  doc["debuglevel"] = config.debuglevel; // amount of debug output on serial port

  doc["baselineSGP30_valid"] = config.baselineSGP30_valid; // 0xF0 = valid
  doc["baselineeCO2_SGP30"] = config.baselineeCO2_SGP30; //
  doc["baselinetVOC_SGP30"] = config.baselinetVOC_SGP30; //
  doc["baselineCCS811_valid"] = config.baselineCCS811_valid; // 0xF0 = valid
  doc["baselineCCS811"] = config.baselineCCS811; // baseline is an internal value, not ppm
  doc["tempOffset_SCD30_valid"] = config.tempOffset_SCD30_valid; // 0xF0 = valid
  doc["tempOffset_SCD30"] = config.tempOffset_SCD30; // in C
  doc["forcedCalibration_SCD30_valid"]= config.forcedCalibration_SCD30_valid; // 0xF0 = valid, not used as the sensor is not designed to prepoluate its internal calibration
  doc["forcedCalibration_SCD30"] = config.forcedCalibration_SCD30; // not used
  doc["tempOffset_MLX_valid"] = config.tempOffset_MLX_valid; // 0xF0 = valid
  doc["tempOffset_MLX"] = config.tempOffset_MLX; // in C

  doc["useWiFi"] = config.useWiFi; // use/not use WiFi and MQTT

  doc["ssid1"] = config.ssid1; // WiFi SSID 32 bytes max
  doc["pw1"] = config.pw1; // WiFi passwrod 64 chars max
  doc["ssid2"] = config.ssid2; // 2nd set of WiFi credentials
  doc["pw2"] = config.pw2; //
  doc["ssid3"] = config.ssid3; // 3rd set of WiFi credentials
  doc["pw3"] = config.pw3; //

  doc["useMQTT"] = config.useMQTT; // provide MQTT data
  doc["mqtt_server"] = config.mqtt_server; // your mqtt server
  doc["mqtt_username"] = config.mqtt_username; // username for MQTT server, leave blank if no password
  doc["mqtt_password"] = config.mqtt_password; // password for MQTT server
  doc["sendMQTTimmediate"] = config.sendMQTTimmediate; // true: update MQTT right away when new data is availablk, otherwise send one unified message
  doc["mqtt_fallback"] = config.mqtt_fallback; // your fallback mqtt server if initial server fails, useful when on private home network
  doc["mqtt_mainTopic"] = config.mqtt_mainTopic; // name of this sensing device for mqtt broker

  doc["useLCD"] = config.useLCD; // use/not use LCD even if it is connected
  doc["consumerLCD"] = config.consumerLCD; // simplified display
  doc["useBacklight"] = config.useBacklight; // backlight on/off
  doc["nightBegin"] = config.nightBegin; // minutes from midnight when to stop changing backlight because of low airquality
  doc["nightEnd"] = config.nightEnd; // minutes from midnight when to start changing backight because of low airquality

  doc["useSCD30"] = config.useSCD30; // ...
  doc["useSPS30"] = config.useSPS30; // ...
  doc["useSGP30"] = config.useSGP30; // ...
  doc["useMAX30"] = config.useMAX30; // ...
  doc["useMLX"] = config.useMLX; // ...
  doc["useBME680"] = config.useBME680; // ...
  doc["useCCS811"] = config.useCCS811; // ...
  doc["useBME280"] = config.useBME280; // ...
  doc["avgP"] = config.avgP; // averagePressure

  doc["useHTTP"] = config.useHTTP; // provide webserver
  doc["useOTA"] = config.useOTA; // porivude over the air programming
  doc["usemDNS"] = config.usemDNS; // provide mDNS
  doc["useHTTPUpdater"] = config.useHTTPUpdater; // use HTTP updating 

  doc["useNTP"] = config.useNTP; // want network time
  doc["ntpServer"] = config.ntpServer; // ntp server
  //doc["utcOffset"]                    = config.utcOffset;                                 // time offset from GMT in hours 
  //doc["enableDST"]                    = config.enableDST;                                 // enable day light sving time=
  doc["timeZone"] = config.timeZone; // time zone according second column in https://raw.githubusercontent.com/nayarsystems/posix_tz_db/master/zones.csv
  doc["rebootMinute"] = config.rebootMinute; // if sensor error, when should we attempt rebooting in minutes after midnight?

  // Serialize JSON to file
  if (serializeJsonPretty(doc, file) == 0) {
    Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Config.ino" "." "83" "." "529" "\", \"aSM\", @progbits, 1 #"))) = ("Failed to write to file"); &__c[0];}))))));
  }

  // serializeJsonPretty(doc, Serial);

  // Close the file
  file.close();
}

void loadConfiguration(const char *filename, Settings &config) {
  // Open file for reading
  File file = LittleFS.open(filename, "r");

  // Allocate a temporary JsonDocument
  // Use arduinojson.org/v6/assistant to compute the capacity.
  StaticJsonDocument<4096 /* crashes program if too small, use online tool to check recommended size*/> doc;

  // Deserialize the JSON document
  DeserializationError error = deserializeJson(doc, file);
  if (error) { Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "Config.ino" "." "102" "." "530" "\", \"aSM\", @progbits, 1 #"))) = ("Failed to read file, using default configuration"); &__c[0];})))))); }

  config.runTime = doc["runTime"] | 0;
  config.debuglevel = doc["debuglevel"] | 1;

  config.baselineSGP30_valid = doc["baselineSGP30_valid"] | 0x00;
  config.baselineeCO2_SGP30 = doc["baselineeCO2_SGP30"] | 0;
  config.baselinetVOC_SGP30 = doc["baselinetVOC_SGP30"] | 0;
  config.baselineCCS811_valid = doc["baselineCCS811_valid"] | 0x00;
  config.baselineCCS811 = doc["baselineCCS811"] | 0;
  config.tempOffset_SCD30_valid = doc["tempOffset_SCD30_valid"] | 0x00;
  config.tempOffset_SCD30 = doc["tempOffset_SCD30"] | 0.01;
  config.forcedCalibration_SCD30_valid = doc["forcedCalibration_SCD30_valid"] | 0x00;
  config.forcedCalibration_SCD30 = doc["forcedCalibration_SCD30"] | 0.0;
  config.tempOffset_MLX_valid = doc["tempOffset_MLX_valid"] | 0x00;
  config.tempOffset_MLX = doc["tempOffset_MLX"] | 0.0;

  config.useWiFi = doc["useWiFi"] | false;
  strlcpy(config.ssid1, doc["ssid1"] | "MEDDEV", sizeof(config.ssid1));
  strlcpy(config.pw1, doc["pw1"] | "", sizeof(config.pw1));
  strlcpy(config.ssid2, doc["ssid2"] | "UAGUest", sizeof(config.ssid2));
  strlcpy(config.pw2, doc["pw2"] | "", sizeof(config.pw3));
  strlcpy(config.ssid3, doc["ssid3"] | "MuchoCoolioG", sizeof(config.ssid3));
  strlcpy(config.pw3, doc["pw3"] | "", sizeof(config.pw3));

  config.useMQTT = doc["useMQTT"] | false;
  strlcpy(config.mqtt_server, doc["mqtt_server"] | "my.mqqtt.server.org", sizeof(config.mqtt_server));
  strlcpy(config.mqtt_username, doc["mqtt_username"] | "mqtt", sizeof(config.mqtt_username));
  strlcpy(config.mqtt_password, doc["mqtt_password"] | "", sizeof(config.mqtt_password));
  strlcpy(config.mqtt_fallback, doc["mqtt_fallback"] | "192.168.1.1", sizeof(config.mqtt_password));
  strlcpy(config.mqtt_mainTopic, doc["mqtt_mainTopic"] | "Senso", sizeof(config.mqtt_mainTopic));
  config.sendMQTTimmediate = doc["sendMQTTimmediate"] | true;

  config.useLCD = doc["useLCD"] | true;
  config.consumerLCD = doc["consumerLCD"] | true;
  config.useBacklight = doc["useBacklight"] | true;
  config.nightBegin = doc["nightBegin"] | 1320;
  config.nightEnd = doc["nightEnd"] | 420;

  config.useSCD30 = doc["useSCD30"] | true;
  config.useSPS30 = doc["useSPS30"] | true;
  config.useSGP30 = doc["useSGP30"] | true;
  config.useMAX30 = doc["useMAX30"] | true;
  config.useMLX = doc["useMLX"] | true;
  config.useBME680 = doc["useBME680"] | true;
  config.useCCS811 = doc["useCCS811"] | true;
  config.useBME280 = doc["useBME280"] | true;
  config.avgP = doc["avgP"] | 90000.0;

  config.useHTTP = doc["useHTTP"] | true;
  config.useOTA = doc["useOTA"] | false;
  config.usemDNS = doc["usemDNS"] | true;
  config.useHTTPUpdater = doc["useHTTPUpdater"] | false;

  config.useNTP = doc["useNTP"] | true;
  strlcpy(config.ntpServer, doc["ntpServer"] | "time.nist.gov", sizeof(config.ntpServer));
  strlcpy(config.timeZone, doc["timeZone"] | "MST7", sizeof(config.timeZone));
  //config.utcOffset              = doc["utcOffset"]                            | -7;
  //config.enableDST              = doc["enableDST"]                            | false;
  config.rebootMinute = doc["rebootMinute"] | -1;

  file.close(); // Close the file 
}
# 1 "d:\\GitHub\\ESP Playground\\AirQuality\\software\\Unified Software\\Sensi\\HTTP.ino"
/******************************************************************************************************/
// HTTP Server
/******************************************************************************************************/
// https://www.mischianti.org/2020/05/24/rest-server-on-esp8266-and-esp32-get-and-json-formatter-part-2/
// https://tttapa.github.io/ESP8266/Chap16%20-%20Data%20Logging.html
// https://gist.github.com/maditnerd/c08c50c8eb2bb841bfd993f4f2beee7b
// https://github.com/Links2004/arduinoWebSockets

void updateHTTP() {
  // Operation:

  switch(stateHTTP) {

    case IS_WAITING : { //---------------------
      // just wait...
      if ((currentTime - lastHTTP) >= 1000 /* We check if WiFi is connected every 1 sec, if disconnected we attempted reconnection every 1 sec*/) {
        lastHTTP = currentTime;
        if ((dbglevel == 3) && mySettings.useHTTP) { Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "HTTP.ino" "." "18" "." "531" "\", \"aSM\", @progbits, 1 #"))) = ("HTTP server: is waiting for network to come up"); &__c[0];})))))); }
      }
       break;
    }

    case START_UP : { //---------------------
      if ((currentTime - lastHTTP) >= 1000 /* We check if WiFi is connected every 1 sec, if disconnected we attempted reconnection every 1 sec*/) {
        lastHTTP = currentTime;
        if (false) { Serial.println("DBG:STARTUP: HTTP"); }
        httpServer.on("/", handleRoot); //Which routine to handle at root location. This is display page
        httpServer.on("/animation",handleAnimation);
        httpServer.on("/bme280", handleBME280);
        httpServer.on("/bme680", handleBME680);
        httpServer.on("/ccs811", handleCCS811);
        httpServer.on("/max", handleMAX30);
        httpServer.on("/mlx", handleMLX);
        httpServer.on("/scd30", handleSCD30);
        httpServer.on("/sgp30", handleSGP30);
        httpServer.on("/sps30", handleSPS30);
        httpServer.on("/date", handleDate);
        httpServer.on("/time", handleTime);
        httpServer.on("/hostname", handleHostname);
        httpServer.on("/ip", handleIP);
        httpServer.on("/edit.htm", HTTP_POST, []() { // If a POST request is sent to the /edit.html address,
        httpServer.send(200, "text/plain", "");
        }, handleFileUpload); // go to 'handleFileUpload'          httpServer.on()
        httpServer.onNotFound( handleNotFound); // When a client requests an unknown URI (i.e. something other than "/"), call function "handleNotFound"
        httpServer.begin(); // Start server
        if (dbglevel > 0) { Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "HTTP.ino" "." "46" "." "532" "\", \"aSM\", @progbits, 1 #"))) = ("HTTP Server: initialized"); &__c[0];})))))); }
        stateHTTP = CHECK_CONNECTION;
        AllmaxUpdateHTTP = 0;
      }
      break;
    }

    case CHECK_CONNECTION : { //---------------------
      //if ((currentTime - lastHTTP) >= intervalHTTP) {
      //  lastHTTP = currentTime;
        httpServer.handleClient();
      //}
      break;
    }

  } // end switch state
} // http

//Working with JSON:
// https://en.wikipedia.org/wiki/JSON
// https://circuits4you.com/2019/01/11/nodemcu-esp8266-arduino-json-parsing-example/

//Creating Webcontent on ESP
// GET & POST https://randomnerdtutorials.com/esp8266-nodemcu-http-get-post-arduino/
// JSON & autoupdate https://circuits4you.com/2019/03/22/esp8266-weather-station-arduino/
// JSON & tabulated data https://circuits4you.com/2019/01/25/esp8266-dht11-humidity-temperature-data-logging/
// Simple autorefresh https://circuits4you.com/2018/02/04/esp8266-ajax-update-part-of-web-page-without-refreshing/

void handleRoot() {
  handleFileRead("/index.htm");
  // String s = MAIN_page;                 // MAIN_Page is defined in HTTP include file
  // httpServer.send(200, "text/html", s); // Send web page
  if (dbglevel == 3) { Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "HTTP.ino" "." "78" "." "533" "\", \"aSM\", @progbits, 1 #"))) = ("HTTP: root request received"); &__c[0];}))))));}
}

void handleAnimation() {
  handleFileRead("/animation.htm");
 // String s = MAIN_page;                 // MAIN_Page is defined in HTTP include file
 // httpServer.send(200, "text/html", s); // Send web page
 if (dbglevel == 3) { Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "HTTP.ino" "." "85" "." "534" "\", \"aSM\", @progbits, 1 #"))) = ("HTTP: animation request received"); &__c[0];}))))));}
}

void handleNotFound(){
  if (!handleFileRead(httpServer.uri())) { // check if the file exists in the flash memory (SPIFFS), if so, send it
   String message= "File Not Found \r\n\n";
   message += "URI: ";
   message += httpServer.uri();
   message += "\r\nMethod: ";
   message += ( httpServer.method() == HTTP_GET ) ? "GET" : "POST";
   message += httpServer.args();
   message += "\r\n";
   for (int8_t i=0; i < httpServer.args(); i++ ) {
    message += " " + httpServer.argName(i) + ": " + httpServer.arg(i) + "\r\n";
   }
   httpServer.send(404, "text/plain", message); // Send HTTP status 404
  }
  if (dbglevel == 3) { Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "HTTP.ino" "." "102" "." "535" "\", \"aSM\", @progbits, 1 #"))) = ("HTTP: invalid request received"); &__c[0];}))))));}
}

void handleTime() {
  char HTTPpayloadStr[64]; // String allocated for HTTP message  
  timeJSON(HTTPpayloadStr);
  httpServer.send(200, "text/json", HTTPpayloadStr);
  if (dbglevel == 3) { Serial.print(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "HTTP.ino" "." "109" "." "536" "\", \"aSM\", @progbits, 1 #"))) = ("HTTP: time request received. Sent: "); &__c[0];})))))); Serial.println(strlen(HTTPpayloadStr)); }
}

void handleDate() {
  char HTTPpayloadStr[64]; // String allocated for HTTP message  
  dateJSON(HTTPpayloadStr);
  httpServer.send(200, "text/json", HTTPpayloadStr);
  if (dbglevel == 3) { Serial.print(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "HTTP.ino" "." "116" "." "537" "\", \"aSM\", @progbits, 1 #"))) = ("HTTP: date request received. Sent: "); &__c[0];})))))); Serial.println(strlen(HTTPpayloadStr)); }
}

void handleHostname() {
  //Create JSON data
  String response = "{\"hostname\":\"" + String(hostName) +"\"}";
  httpServer.send(200, "text/json", response);
  if (dbglevel == 3) { Serial.print(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "HTTP.ino" "." "123" "." "538" "\", \"aSM\", @progbits, 1 #"))) = ("HTTP: hostname request received. Sent: "); &__c[0];})))))); Serial.println(response.length()); }
}

void handleIP() {
  //Create JSON data
  String response = "{\"ip\":\"" + WiFi.localIP().toString() +"\"}";
  httpServer.send(200, "text/json", response);
  if (dbglevel == 3) { Serial.print(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "HTTP.ino" "." "130" "." "539" "\", \"aSM\", @progbits, 1 #"))) = ("HTTP: ip request received. Sent: "); &__c[0];})))))); Serial.println(response.length());}
}

void handleBME280() {
  //Create JSON data
  //{"Sensi":{"bme280":{ "avail":true, "p":123.4,"rH":123.4,"aH":123.4,"T":+25.0,"rh_airquality":"normal"}}}
  char HTTPpayloadStr[256]; // String allocated for HTTP message  
  bme280JSON(HTTPpayloadStr);
  httpServer.send(200, "text/json", HTTPpayloadStr);
  if (dbglevel == 3) { Serial.print(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "HTTP.ino" "." "139" "." "540" "\", \"aSM\", @progbits, 1 #"))) = ("HTTP: bme280 request received. Sent: "); &__c[0];})))))); Serial.println(strlen(HTTPpayloadStr));}
}

void handleBME680() {
  //Create JSON data
  //{"Sensi":{"bme680":{"p":123.4,"rH":12.3,"ah":123.4,"T":+25.1,"resistance":1234,"rH_airquality":"normal","resistance_airquality":"normal"}}}
  char HTTPpayloadStr[256]; // String allocated for HTTP message  
  bme680JSON(HTTPpayloadStr);
  httpServer.send(200, "text/json", HTTPpayloadStr);
  if (dbglevel == 3) { Serial.print(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "HTTP.ino" "." "148" "." "541" "\", \"aSM\", @progbits, 1 #"))) = ("HTTP: bme680 request received. Sent: "); &__c[0];})))))); Serial.println(strlen(HTTPpayloadStr));}
}

void handleCCS811() {
  //Create JSON data
  char HTTPpayloadStr[256]; // String allocated for HTTP message  
  ccs811JSON(HTTPpayloadStr);
  httpServer.send(200, "text/json", HTTPpayloadStr);
  if (dbglevel == 3) { Serial.print(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "HTTP.ino" "." "156" "." "542" "\", \"aSM\", @progbits, 1 #"))) = ("HTTP: ccs811 request received. Sent: "); &__c[0];})))))); Serial.println(strlen(HTTPpayloadStr));}
}

void handleSCD30() {
  //Create JSON data
  char HTTPpayloadStr[256]; // String allocated for HTTP message  
  scd30JSON(HTTPpayloadStr);
  httpServer.send(200, "text/json", HTTPpayloadStr);
  if (dbglevel == 3) { Serial.print(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "HTTP.ino" "." "164" "." "543" "\", \"aSM\", @progbits, 1 #"))) = ("HTTP: scd30 request received. Sent: "); &__c[0];})))))); Serial.println(strlen(HTTPpayloadStr));}
}

void handleSGP30() {
  //Create JSON data
  char HTTPpayloadStr[256]; // String allocated for HTTP message  
  sgp30JSON(HTTPpayloadStr);
  httpServer.send(200, "text/json", HTTPpayloadStr);
  if (dbglevel == 3) { Serial.print(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "HTTP.ino" "." "172" "." "544" "\", \"aSM\", @progbits, 1 #"))) = ("HTTP: sgp30 request received. Sent: "); &__c[0];})))))); Serial.println(strlen(HTTPpayloadStr));}
}

void handleSPS30() {
  //Create JSON data
  char HTTPpayloadStr[512]; // String allocated for HTTP message  
  sps30JSON(HTTPpayloadStr);
  httpServer.send(200, "text/json", HTTPpayloadStr);
  if (dbglevel == 3) { Serial.print(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "HTTP.ino" "." "180" "." "545" "\", \"aSM\", @progbits, 1 #"))) = ("HTTP: sps30 request received. Sent: "); &__c[0];})))))); Serial.println(strlen(HTTPpayloadStr));}
}

void handleMLX() {
  //Create JSON data
  char HTTPpayloadStr[256]; // String allocated for HTTP message  
  mlxJSON(HTTPpayloadStr);
  httpServer.send(200, "text/json", HTTPpayloadStr);
  if (dbglevel == 3) { Serial.print(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "HTTP.ino" "." "188" "." "546" "\", \"aSM\", @progbits, 1 #"))) = ("HTTP: mlx request received. Sent: "); &__c[0];})))))); Serial.println(strlen(HTTPpayloadStr));}
}

void handleMAX30() {
  //Create JSON data
  // Not Implemented Yet
  String response = "{\"max\":{\"avail\":" +String(max_avail) +",";
  if (max_avail) {
    response += "\"HR\":-1.0,";
    response += "\"O2Sat\":-1.0}}";
  } else {
    response += "\"HR\":-1.0,";
    response += "\"O2Sat\":-1.0}}";
  }
  httpServer.send(200, "text/json", response); //
  if (dbglevel == 3) { Serial.print(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "HTTP.ino" "." "203" "." "547" "\", \"aSM\", @progbits, 1 #"))) = ("HTTP: max request received. Sent: "); &__c[0];})))))); Serial.println(response.length());}
}

String getContentType(String filename){
  if (httpServer.hasArg("download")) return "application/octet-stream";
  else if(filename.endsWith(".htm")) return "text/html";
  else if(filename.endsWith(".html")) return "text/html";
  else if(filename.endsWith(".css")) return "text/css";
  else if(filename.endsWith(".js")) return "application/javascript";
  else if(filename.endsWith(".png")) return "image/png";
  else if(filename.endsWith(".gif")) return "image/gif";
  else if(filename.endsWith(".jpg")) return "image/jpeg";
  else if(filename.endsWith(".ico")) return "image/x-icon";
  else if(filename.endsWith(".xml")) return "text/xml";
  else if(filename.endsWith(".json")) return "text/json";
  else if(filename.endsWith(".pdf")) return "application/x-pdf";
  else if(filename.endsWith(".zip")) return "application/x-zip";
  else if(filename.endsWith(".gz")) return "application/x-gzip";
  return "text/plain";
}

// send the right file to the client (if it exists)
bool handleFileRead(String path) {
  if (dbglevel == 3) { Serial.println("handleFileRead: " + path); }
  if (path.endsWith("/")) path += "index.htm"; // If a folder is requested, send the index file
  String contentType = getContentType(path); // Get the MIME type
  String pathWithGz = path + ".gz"; // Also check for compressed version
  if (LittleFS.exists(pathWithGz) || LittleFS.exists(path)) { // If either file exists
    if(LittleFS.exists(pathWithGz)) {path = pathWithGz;} // If there's a compressed version available, se the compressed version
    File file = LittleFS.open(path, "r"); // Open it
    size_t sent = httpServer.streamFile(file, contentType); // And send it to the client
    if (sent != file.size()) { if (dbglevel > 0) { Serial.printf("HTTP: Error sent less data than expected: %d of %d\r\n", sent, file.size()); } }
    file.close(); // Then close the file again
    return true;
  } else {
    if (dbglevel > 0) { Serial.print("HTTP: File Not Found: "); Serial.println(path); }
    return false; // If the file doesn't exist, return false
  }
}

void handleFileUpload() { // upload a new file to the SPIFFS
  HTTPUpload& upload = httpServer.upload();
  String path;
  if (upload.status == UPLOAD_FILE_START) {
    path = upload.filename;
    if (!path.startsWith("/")) path = "/" + path;
    if (!path.endsWith(".gz")) { // The file server always prefers a compressed version of a file
      String pathWithGz = path + ".gz"; // So if an uploaded file is not compressed, the existing compressed
      if (LittleFS.exists(pathWithGz)) // version of that file must be deleted (if it exists)
        LittleFS.remove(pathWithGz);
    }
    Serial.print("handleFileUpload Name: "); Serial.println(path);
    uploadFile = LittleFS.open(path, "w"); // Open the file for writing in SPIFFS (create if it doesn't exist)
    path = String();
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    if (uploadFile)
      uploadFile.write(upload.buf, upload.currentSize); // Write the received bytes to the file
  } else if (upload.status == UPLOAD_FILE_END) {
    if (uploadFile) { // If the file was successfully created
      uploadFile.close(); // Close the file again
      Serial.print("handleFileUpload Size: "); Serial.println(upload.totalSize);
      httpServer.sendHeader("Location", "/success.htm"); // Redirect the client to the success page
      httpServer.send(303);
    } else {
      httpServer.send(500, "text/plain", "500: couldn't create file");
    }
  }
}
# 1 "d:\\GitHub\\ESP Playground\\AirQuality\\software\\Unified Software\\Sensi\\HTTPUpdater.ino"
/******************************************************************************************************/
// HTTP Updae Server / Upload Code
/******************************************************************************************************/
// https://www.hackster.io/s-wilson/nodemcu-home-weather-station-with-websocket-7c77a3

void updateHTTPUpdater() {
  // Operation:

  switch(stateHTTPUpdater) {

    case IS_WAITING : { //---------------------
      // just wait...
      if ((currentTime - lastHTTPUpdater) >= 1000 /* We check if WiFi is connected every 1 sec, if disconnected we attempted reconnection every 1 sec*/) {
        lastHTTPUpdater = currentTime;
        if (dbglevel == 3) { Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "HTTPUpdater.ino" "." "15" "." "548" "\", \"aSM\", @progbits, 1 #"))) = ("HTTP Update Server: is waiting for network to come up"); &__c[0];})))))); }
      }
       break;
    }

    case START_UP : { //---------------------
      if ((currentTime - lastHTTPUpdater) >= 1000 /* We check if WiFi is connected every 1 sec, if disconnected we attempted reconnection every 1 sec*/) {
        lastHTTPUpdater = currentTime;
        if (false) { Serial.println("DBG:STARTUP: HTTP UPDATER"); }
        httpUpdater.setup(&httpUpdateServer, update_path, update_username, update_password);
        httpUpdateServer.begin(); // Start server
        if (dbglevel > 0) { Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "HTTPUpdater.ino" "." "26" "." "549" "\", \"aSM\", @progbits, 1 #"))) = ("HTTP Update Server: initialized"); &__c[0];})))))); }
        stateHTTPUpdater = CHECK_CONNECTION;
        maxUpdateHTTPUPDATER = 0;
      }
      break;
    }

    case CHECK_CONNECTION : { //---------------------
      //if ((currentTime - lastHTTPUpdater) >= intervalHTTPUpdater) {
      //  lastHTTPUpdater = currentTime;
        httpUpdateServer.handleClient();
      //}
      break;
    }

  } // end switch state
} // http
# 1 "d:\\GitHub\\ESP Playground\\AirQuality\\software\\Unified Software\\Sensi\\LCD.ino"
/******************************************************************************************************/
// Initialize LCD
/******************************************************************************************************/
bool initializeLCD() {
  bool success = true; // unfortuntely the LCD driver functions have no error checking

  lcd_port->begin(lcd_i2c[0], lcd_i2c[1]);





  lcd.begin(20, 4, *lcd_port);
  if (mySettings.useBacklight == true) {lcd.setBacklight(255); } else {lcd.setBacklight(0);}

  if (dbglevel > 0) { Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "LCD.ino" "." "16" "." "550" "\", \"aSM\", @progbits, 1 #"))) = ("LCD initialized"); &__c[0];})))))); }
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
bool updateLCD() {
  char lcdbuf[21];
  const char clearLine[] = "                    "; // 20 spaces
  bool success = true;
  char qualityMessage[2];

  strncpy(&lcdDisplay[0][0], clearLine , 20);
  strncpy(&lcdDisplay[1][0], clearLine , 20);
  strncpy(&lcdDisplay[2][0], clearLine , 20);
  strncpy(&lcdDisplay[3][0], clearLine , 20);


  if (scd30_avail && mySettings.useSCD30) { // ====================================================

    sprintf(lcdbuf, "%4d", int(scd30_ppm));
    strncpy(&lcdDisplay[0][4], lcdbuf, 4);

    sprintf(lcdbuf, "%4.1f%%", scd30_hum);
    strncpy(&lcdDisplay[0][9], lcdbuf, 5);

    sprintf(lcdbuf,"%+5.1fC",scd30_temp);
    strncpy(&lcdDisplay[0][14], lcdbuf, 6);

    checkCO2(scd30_ppm, qualityMessage, 1);
    strncpy(&lcdDisplay[0][8], qualityMessage, 1);

  } // end if avail scd30

  if (bme680_avail && mySettings.useBME680) { // ==================================================

    sprintf(lcdbuf,"%4d",(int)(bme680.pressure/100.0));
    strncpy(&lcdDisplay[3][4], lcdbuf, 4);

    sprintf(lcdbuf,"%4.1f%%",bme680.humidity);
    strncpy(&lcdDisplay[1][9], lcdbuf, 5);

    sprintf(lcdbuf,"%4.1fg",bme680_ah);
    strncpy(&lcdDisplay[2][9], lcdbuf, 5);

    checkHumidity(bme680.humidity, qualityMessage, 1);
 // Where does it go on the display?

    sprintf(lcdbuf,"%+5.1fC",bme680.temperature);
    strncpy(&lcdDisplay[1][14], lcdbuf, 6);

    sprintf(lcdbuf,"%5.1f",(float(bme680.gas_resistance)/1000.0));
    strncpy(&lcdDisplay[3][9], lcdbuf, 5);

    checkGasResistance(bme680.gas_resistance, qualityMessage, 1);
    strncpy(&lcdDisplay[3][14], qualityMessage, 1);

  } // end if avail bme680

  if (sgp30_avail && mySettings.useSGP30) { // ====================================================
    sprintf(lcdbuf, "%4d", sgp30.CO2);
    strncpy(&lcdDisplay[1][4], lcdbuf, 4);

    sprintf(lcdbuf, "%4d", sgp30.TVOC);
    strncpy(&lcdDisplay[3][15], lcdbuf, 4);

    checkCO2(sgp30.CO2, qualityMessage, 1);
    strncpy(&lcdDisplay[1][8], qualityMessage, 1);

    checkTVOC(sgp30.TVOC, qualityMessage, 1);
    strncpy(&lcdDisplay[3][19], qualityMessage, 1);
  } // end if avail sgp30

  if (ccs811_avail && mySettings.useCCS811) { // ==================================================

    sprintf(lcdbuf, "%4d", ccs811.getCO2());
    strncpy(&lcdDisplay[2][4], lcdbuf, 4);

    sprintf(lcdbuf, "%4d", ccs811.getTVOC());
    strncpy(&lcdDisplay[2][15], lcdbuf, 4);

    checkCO2(ccs811.getCO2(), qualityMessage,1 );
    strncpy(&lcdDisplay[2][8], qualityMessage, 1);

    checkTVOC(ccs811.getTVOC(), qualityMessage, 1);
    strncpy(&lcdDisplay[2][19], qualityMessage, 1);
  } // end if avail ccs811

  if (sps30_avail && mySettings.useSPS30) { // ====================================================
    sprintf(lcdbuf,"%3.0f",valSPS30.MassPM1);
    strncpy(&lcdDisplay[0][0], lcdbuf, 3);

    sprintf(lcdbuf,"%3.0f",valSPS30.MassPM2);
    strncpy(&lcdDisplay[1][0], lcdbuf, 3);

    sprintf(lcdbuf,"%3.0f",valSPS30.MassPM4);
    strncpy(&lcdDisplay[2][0], lcdbuf, 3);

    sprintf(lcdbuf,"%3.0f",valSPS30.MassPM10);
    strncpy(&lcdDisplay[3][0], lcdbuf, 3);

    checkPM2(valSPS30.MassPM2, qualityMessage, 1);
    strncpy(&lcdDisplay[1][3], qualityMessage, 1);

    checkPM10(valSPS30.MassPM10, qualityMessage,1 );
    strncpy(&lcdDisplay[3][3], qualityMessage, 1);
  }// end if avail SPS30

  if (therm_avail && mySettings.useMLX) { // ====================================================
    if (altDisplay == true) {
      sprintf(lcdbuf,"%+5.1fC",(therm.object()+mlxOffset));
      strncpy(&lcdDisplay[0][14], lcdbuf, 6);

      sprintf(lcdbuf,"%+5.1fC",therm.ambient());
      strncpy(&lcdDisplay[1][14], lcdbuf, 6);

      checkFever((therm.object() + 0.5 /* difference from forehead to oral temperature: https://www.mottchildren.org/health-library/tw9223*/ + mlxOffset), qualityMessage, 1);
      strncpy(&lcdDisplay[0][14], qualityMessage, 1);
    }
  }// end if avail  MLX

  altDisplay = !altDisplay;

  lcd_port->begin(lcd_i2c[0], lcd_i2c[1]);
  lcd.clear();
  lcd.setCursor(0, 0);

  // 1st line continues at 3d line
  // 2nd line continues at 4th line
  strncpy(lcdbuf, &lcdDisplay[0][0], 20); lcdbuf[20] = '\0'; lcd.print(lcdbuf);
  strncpy(lcdbuf, &lcdDisplay[2][0], 20); lcdbuf[20] = '\0'; lcd.print(lcdbuf);
  strncpy(lcdbuf, &lcdDisplay[1][0], 20); lcdbuf[20] = '\0'; lcd.print(lcdbuf);
  strncpy(lcdbuf, &lcdDisplay[3][0], 20); lcdbuf[20] = '\0'; lcd.print(lcdbuf);

  if (dbglevel == 2) { // if dbg, display the lines also on serial port
    strncpy(lcdbuf, &lcdDisplay[0][0], 20); lcdbuf[20] = '\0'; Serial.println(lcdbuf);
    strncpy(lcdbuf, &lcdDisplay[1][0], 20); lcdbuf[20] = '\0'; Serial.println(lcdbuf);
    strncpy(lcdbuf, &lcdDisplay[2][0], 20); lcdbuf[20] = '\0'; Serial.println(lcdbuf);
    strncpy(lcdbuf, &lcdDisplay[3][0], 20); lcdbuf[20] = '\0'; Serial.println(lcdbuf);
  }

  return success;
} // update display 

bool updateSinglePageLCD() {
  /**************************************************************************************/
  // Update Single Page LCD
  /**************************************************************************************/
  bool success = true;

  float myTemp = -9999.;
  float myHum = -9999.;
  float myCO2 = -9999.;
  float mydP = -9999.;
  float mytVOC = -9999.;
  float myPM25 = -9999.;
  float myPM10 = -9999.;

  char myCO2_warning[] = "nrm ";
  char myTemp_warning[] = "nrm ";
  char myHum_warning[] = "nrm ";
  char mydP_warning[] = "nrm ";
  char myPM_warning[] = "nrm ";
  char mytVOC_warning[] = "nrm ";
  const char myNaN[] = "  na";

  allGood = true;

  char lcdbuf[21];
  const char clearLine[] = "                    "; // 20 spaces
  char qualityMessage[5];

  // Collect Data
  //////////////////////////////////////////////////////////////////////////////////////////////////

  if (scd30_avail && mySettings.useSCD30) { // ======================================================
    myTemp = scd30_temp;
    myHum = scd30_hum;
    myCO2 = scd30_ppm;
  }

  if (bme680_avail && mySettings.useBME680) { // ====================================================
    mydP = (bme680.pressure - bme680_pressure24hrs)/100.0;
    myTemp = bme680.temperature;
  }

  if (bme280_avail && mySettings.useBME280) { // ====================================================
    mydP = (bme280_pressure - bme280_pressure24hrs)/100.0;
    myTemp = bme280_temp;
  }

  if (sgp30_avail && mySettings.useSGP30) { // ======================================================
    mytVOC = sgp30.TVOC;
  }

  if (sps30_avail && mySettings.useSPS30) { // ======================================================
    myPM25 = valSPS30.MassPM2;
    myPM10 = valSPS30.MassPM10;
  }

  // my warnings // =================================================================================
  checkCO2(myCO2, qualityMessage, 4);
  strncpy(myCO2_warning, qualityMessage, 4);

  checkAmbientTemperature(myTemp, qualityMessage, 4);
  strncpy(myTemp_warning, qualityMessage, 4);

  checkHumidity(myHum, qualityMessage, 4);
  strncpy(myHum_warning, qualityMessage, 4);

  checkdP(mydP, qualityMessage, 4);
  strncpy(mydP_warning, qualityMessage, 4);

  checkPM(myPM25, myPM10, qualityMessage, 4);
  strncpy(myPM_warning, "nrm ", 4);

  checkTVOC(mytVOC, qualityMessage, 4);
  strncpy(mytVOC_warning, qualityMessage, 4);

  // build display
  //////////////////////////////////////////////////////////////////////////////////////////////////
  strncpy(&lcdDisplay[0][0], clearLine , 20);
  strncpy(&lcdDisplay[1][0], clearLine , 20);
  strncpy(&lcdDisplay[2][0], clearLine , 20);
  strncpy(&lcdDisplay[3][0], clearLine , 20);

  if (myTemp != -9999.) {
    sprintf(lcdbuf,"%+5.1fC", myTemp);
    strncpy(&lcdDisplay[1][14], lcdbuf, 6);
    // strncpy(&lcdDisplay[2][8], "T", 1);
    // strncpy(&lcdDisplay[3][8], myTemp_warning, 4);
  } else {
    strncpy(&lcdDisplay[2][8], "Temp", 4);
    strncpy(&lcdDisplay[3][8], myNaN, 4);
  }

  if (myHum != -9999.) {
    sprintf(lcdbuf, "%4.1f%%", myHum);
    strncpy(&lcdDisplay[0][15], lcdbuf, 5);
    strncpy(&lcdDisplay[0][4], "rH", 2);
    strncpy(&lcdDisplay[1][4], myHum_warning, 4);
  } else {
    strncpy(&lcdDisplay[0][4], "rH", 2);
    strncpy(&lcdDisplay[1][4], myNaN, 4);
  }

  if (myCO2 != -9999.) {
    sprintf(lcdbuf, "%4dppm", int(myCO2));
    strncpy(&lcdDisplay[3][13], lcdbuf, 7);
    strncpy(&lcdDisplay[2][0], "CO2", 3);
    strncpy(&lcdDisplay[3][0], myCO2_warning, 4);
  } else {
    strncpy(&lcdDisplay[2][0], "CO2", 3);
    strncpy(&lcdDisplay[3][0], myNaN, 4);
  }

  if (mydP != -9999.) {
    sprintf(lcdbuf,"%+4.1fmb", mydP);
    strncpy(&lcdDisplay[2][14], lcdbuf, 6);
    strncpy(&lcdDisplay[2][4], "dP", 2);
    strncpy(&lcdDisplay[3][4], mydP_warning, 4);
  } else {
    strncpy(&lcdDisplay[2][4], "dP", 2);
    strncpy(&lcdDisplay[3][4], myNaN, 4);
  }

  if (mytVOC != -9999.) {
    strncpy(&lcdDisplay[0][8], "tVOC", 4);
    strncpy(&lcdDisplay[1][8], mytVOC_warning, 4);
  } else {
    strncpy(&lcdDisplay[0][8], "tVOC", 4);
    strncpy(&lcdDisplay[1][8], myNaN, 4);
  }

  if (myPM25 != -9999.) {
    sprintf(lcdbuf, "%3du", int(myPM25));
    strncpy(&lcdDisplay[2][8], lcdbuf, 4);
    sprintf(lcdbuf, "%3du", int(myPM10));
    strncpy(&lcdDisplay[3][8], lcdbuf, 4);
    strncpy(&lcdDisplay[0][0], "PM", 2);
    strncpy(&lcdDisplay[1][0], myPM_warning, 4);
  } else {
    strncpy(&lcdDisplay[0][0], "PM", 2);
    strncpy(&lcdDisplay[1][0], myNaN, 4);
  }

  lcd_port->begin(lcd_i2c[0], lcd_i2c[1]);
  // lcd.clear();
  lcd.setCursor(0, 0);

  strncpy(lcdbuf, &lcdDisplay[0][0], 20); lcdbuf[20] = '\0'; lcd.print(lcdbuf);
  strncpy(lcdbuf, &lcdDisplay[2][0], 20); lcdbuf[20] = '\0'; lcd.print(lcdbuf);
  strncpy(lcdbuf, &lcdDisplay[1][0], 20); lcdbuf[20] = '\0'; lcd.print(lcdbuf);
  strncpy(lcdbuf, &lcdDisplay[3][0], 20); lcdbuf[20] = '\0'; lcd.print(lcdbuf);

  if (dbglevel == 2) { // if dbg, display the lines also on serial port
    strncpy(lcdbuf, &lcdDisplay[0][0], 20); lcdbuf[20] = '\0'; Serial.println(lcdbuf);
    strncpy(lcdbuf, &lcdDisplay[1][0], 20); lcdbuf[20] = '\0'; Serial.println(lcdbuf);
    strncpy(lcdbuf, &lcdDisplay[2][0], 20); lcdbuf[20] = '\0'; Serial.println(lcdbuf);
    strncpy(lcdbuf, &lcdDisplay[3][0], 20); lcdbuf[20] = '\0'; Serial.println(lcdbuf);
  }

  return success;

} // update display 

bool updateTwoPageLCD() {
  /**************************************************************************************/
  // Update Consumer LCD
  /**************************************************************************************/
  char lcdbuf[21];
  const char clearLine[] = "                    "; // 20 spaces
  bool success = true;;
  char qualityMessage[5];

  const char firstLine[] = "P01     rH   TEMP   ";
  const char secondLine[] = "P25     aH          ";
  const char thirdLine[] = "P04      P   tVOC   ";
  const char thirdLineN[] = "P04      P          ";
  const char forthLine[] = "P10     dP    CO2   ";

  if (altDisplay) { // SHOW Measurands and Assessment

    strncpy(&lcdDisplayAlt[0][0], firstLine , 20);
    strncpy(&lcdDisplayAlt[1][0], secondLine , 20);
    if (sgp30_avail && mySettings.useSGP30) { strncpy(&lcdDisplayAlt[2][0], thirdLine , 20);}
    else { strncpy(&lcdDisplayAlt[2][0], thirdLineN , 20);}
    strncpy(&lcdDisplayAlt[3][0], forthLine , 20);

    allGood = true;

    if (scd30_avail && mySettings.useSCD30) { // =============================================================

   checkCO2(scd30_ppm, qualityMessage, 1);
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
      lcdDisplayAlt[1][4] = qualityMessage[0];

      checkPM10(valSPS30.MassPM10, qualityMessage, 1);
      lcdDisplayAlt[3][4] = qualityMessage[0];
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
      sprintf(lcdbuf, "%4dppm", int(scd30_ppm));
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
    strncpy(lcdbuf, &lcdDisplay[0][0], 20); lcdbuf[20] = '\0'; lcd.print(lcdbuf);
    strncpy(lcdbuf, &lcdDisplay[2][0], 20); lcdbuf[20] = '\0'; lcd.print(lcdbuf);
    strncpy(lcdbuf, &lcdDisplay[1][0], 20); lcdbuf[20] = '\0'; lcd.print(lcdbuf);
    strncpy(lcdbuf, &lcdDisplay[3][0], 20); lcdbuf[20] = '\0'; lcd.print(lcdbuf);
  }

  if (dbglevel == 2) { // if dbg, display the lines also on serial port
    if (altDisplay) {
      strncpy(lcdbuf, &lcdDisplayAlt[0][0], 20); lcdbuf[20] = '\0'; Serial.println(lcdbuf);
      strncpy(lcdbuf, &lcdDisplayAlt[1][0], 20); lcdbuf[20] = '\0'; Serial.println(lcdbuf);
      strncpy(lcdbuf, &lcdDisplayAlt[2][0], 20); lcdbuf[20] = '\0'; Serial.println(lcdbuf);
      strncpy(lcdbuf, &lcdDisplayAlt[3][0], 20); lcdbuf[20] = '\0'; Serial.println(lcdbuf);

    } else {
      strncpy(lcdbuf, &lcdDisplay[0][0], 20); lcdbuf[20] = '\0'; Serial.println(lcdbuf);
      strncpy(lcdbuf, &lcdDisplay[1][0], 20); lcdbuf[20] = '\0'; Serial.println(lcdbuf);
      strncpy(lcdbuf, &lcdDisplay[2][0], 20); lcdbuf[20] = '\0'; Serial.println(lcdbuf);
      strncpy(lcdbuf, &lcdDisplay[3][0], 20); lcdbuf[20] = '\0'; Serial.println(lcdbuf);
    }
  }

  altDisplay = !altDisplay; // flip display between analysis and values

  return success;
} // update consumer display 
# 1 "d:\\GitHub\\ESP Playground\\AirQuality\\software\\Unified Software\\Sensi\\MLX.ino"
/******************************************************************************************************/
// Initialize MLX
/******************************************************************************************************/
bool initializeMLX(){
  bool success = true;

  mlx_port->begin(mlx_i2c[0], mlx_i2c[1]);

  if (therm.begin(0x5A, *mlx_port) == true) {
    therm.setUnit(TEMP_C); // Set the library's units to Centigrade
    therm.setEmissivity((float)0.98 /* emissivity of skin*/);
    if (dbglevel > 0) { Serial.print(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "MLX.ino" "." "12" "." "551" "\", \"aSM\", @progbits, 1 #"))) = ("MLX: Emissivity: "); &__c[0];})))))); Serial.println(therm.readEmissivity()); }
    stateMLX = IS_MEASURING;
  } else {
    if (dbglevel > 0) { Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "MLX.ino" "." "15" "." "552" "\", \"aSM\", @progbits, 1 #"))) = ("MLX: sensor not detected. Please check wiring"); &__c[0];})))))); }
    stateMLX = HAS_ERROR;
    therm_avail = false;
  }
  sleepTimeMLX = intervalMLX - 250 /* time until stable internal readings in ms*/ - 50;
  if (dbglevel > 0) {
    Serial.print(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "MLX.ino" "." "21" "." "553" "\", \"aSM\", @progbits, 1 #"))) = ("MLX: sleep time is "); &__c[0];})))))); Serial.print(sleepTimeMLX); Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "MLX.ino" "." "21" "." "554" "\", \"aSM\", @progbits, 1 #"))) = ("ms"); &__c[0];}))))));
    Serial.print(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "MLX.ino" "." "22" "." "555" "\", \"aSM\", @progbits, 1 #"))) = ("MLX: interval time is "); &__c[0];})))))); Serial.print(intervalMLX); Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "MLX.ino" "." "22" "." "556" "\", \"aSM\", @progbits, 1 #"))) = ("ms"); &__c[0];}))))));
  }

  if (mySettings.tempOffset_MLX_valid == 0xF0) {
    mlxOffset = mySettings.tempOffset_MLX;
    if (dbglevel > 0) { Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "MLX.ino" "." "27" "." "557" "\", \"aSM\", @progbits, 1 #"))) = ("MLX: offset found"); &__c[0];}))))));}
  } else {
    if (dbglevel > 0) { Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "MLX.ino" "." "29" "." "558" "\", \"aSM\", @progbits, 1 #"))) = ("MLX: no offset found"); &__c[0];}))))));}
  }

  if (dbglevel > 0) { Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "MLX.ino" "." "32" "." "559" "\", \"aSM\", @progbits, 1 #"))) = ("MLX: initialized"); &__c[0];})))))); }
  delay(50);

  return success;
}

/******************************************************************************************************/
// Update MLX
/******************************************************************************************************/
bool updateMLX() {
  bool success = true;

  switch(stateMLX) {

    case IS_MEASURING : { //---------------------
      if ((currentTime - lastMLX) > intervalMLX) {
        mlx_port->begin(mlx_i2c[0], mlx_i2c[1]);
        if (therm.read()) {
          if (dbglevel == 8) { Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "MLX.ino" "." "50" "." "560" "\", \"aSM\", @progbits, 1 #"))) = ("MLX: temperature measured"); &__c[0];})))))); }
          lastMLX = currentTime;
          mlxNewData = true;
          mlxNewDataWS = true;
          if (fastMode == false) {
            therm.sleep();
            if (dbglevel == 8) { Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "MLX.ino" "." "56" "." "561" "\", \"aSM\", @progbits, 1 #"))) = ("MLX: sent to sleep"); &__c[0];})))))); }
            stateMLX = IS_SLEEPING;
          }
        } else {
          // read error
          stateMLX = HAS_ERROR;
          therm_avail = false;
          success = false;
        }
      }
      break;
    }

    case IS_SLEEPING : { //---------------------
      if ((currentTime - lastMLX) > sleepTimeMLX) {
        mlx_port->begin(mlx_i2c[0], mlx_i2c[1]);
        therm.wake(); // takes 250ms to wake up, sleepTime is shorter than intervalMLX to accomodate that
        if (dbglevel == 8) { Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "MLX.ino" "." "73" "." "562" "\", \"aSM\", @progbits, 1 #"))) = ("MLX: initiated wake up"); &__c[0];})))))); }
        stateMLX = IS_MEASURING;
      }
      break;
    }

    case HAS_ERROR : { // ----------------------
      success = false;
      break;
    }
  } // switch

  return success;
}

void mlxJSON(char *payload){
  char qualityMessage1[16];
  char qualityMessage2[16];
  checkFever(therm.object()+mlxOffset+0.5 /* difference from forehead to oral temperature: https://www.mottchildren.org/health-library/tw9223*/, qualityMessage1, 15);
  checkAmbientTemperature(therm.ambient(), qualityMessage2, 15);
  sprintf(payload, "{\"mlx\":{\"avail\":%s,\"To\":%5.1f,\"Ta\":%5.1f,\"fever\":\"%s\",\"T_airquality\":\"%s\"}}",
                       therm_avail ? "true" : "false",
                       therm.object()+mlxOffset,
                       therm.ambient(),
                       qualityMessage1,
                       qualityMessage2);
}
# 1 "d:\\GitHub\\ESP Playground\\AirQuality\\software\\Unified Software\\Sensi\\MQTT.ino"
/******************************************************************************************************/
// MQTT
/******************************************************************************************************/
// Andreas Spiess examples

void updateMQTT() {
// Operation:
//   Wait until WiFi connection is established
//   Set the MQTT server
//   Connect with either username or no username
//   If connection did not work, attempt connection to fallback server
//   Once connection is stablished, periodically check if still connected
//   If connection lost go back to setting up MQTT server
// -------------------------------------------------------
  switch(stateMQTT) {

    case IS_WAITING : { //---------------------
      // just wait...
      if ((currentTime - lastMQTT) >= 1000 /* We check if WiFi is connected every 1 sec, if disconnected we attempted reconnection every 1 sec*/) {
        if (dbglevel == 3) { Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "MQTT.ino" "." "20" "." "563" "\", \"aSM\", @progbits, 1 #"))) = ("MQTT: is waiting for network to come up"); &__c[0];})))))); }
        lastMQTT = currentTime;
      }
      break;
    }

    case START_UP : { //---------------------
      if ((currentTime - lastMQTT) >= 1000 /* We check if WiFi is connected every 1 sec, if disconnected we attempted reconnection every 1 sec*/) {
        lastMQTT = currentTime;
        if (false) { Serial.println("DBG:STARTUP: MQTT"); }
        mqttClient.setCallback(mqttCallback); // start listener
        mqttClient.setServer(mySettings.mqtt_server, 1883 /* default mqtt port*/); // start connection to server
        if (dbglevel > 0) { Serial.print(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "MQTT.ino" "." "32" "." "564" "\", \"aSM\", @progbits, 1 #"))) = ("Connecting to MQTT: "); &__c[0];})))))); Serial.println(mySettings.mqtt_server); }
        mqtt_connected = false;
        // connect to server with or without username password
        if (strlen(mySettings.mqtt_username) > 0) { mqtt_connected = mqttClient.connect("SensiEPS8266Client", mySettings.mqtt_username, mySettings.mqtt_password); }
        else { mqtt_connected = mqttClient.connect("SensiEPS8266Client"); }
        if (mqtt_connected == false) { // try fall back server
          mqttClient.setServer(mySettings.mqtt_fallback, 1883 /* default mqtt port*/);
          if (dbglevel > 0) { Serial.print(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "MQTT.ino" "." "39" "." "565" "\", \"aSM\", @progbits, 1 #"))) = ("Connecting to MQTT: "); &__c[0];})))))); Serial.println(mySettings.mqtt_fallback); }
          if (strlen(mySettings.mqtt_username) > 0) { mqtt_connected = mqttClient.connect("SensiEPS8266Client", mySettings.mqtt_username, mySettings.mqtt_password); }
          else { mqtt_connected = mqttClient.connect("SensiEPS8266Client"); }
        }
        // publish connection status
        if (mqtt_connected == true) {
          char MQTTtopicStr[64]; // String allocated for MQTT topic 
          sprintf(MQTTtopicStr,"%s/status",mySettings.mqtt_mainTopic);
          mqttClient.publish(MQTTtopicStr, "Sensi up");
          if (dbglevel > 0) { Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "MQTT.ino" "." "48" "." "566" "\", \"aSM\", @progbits, 1 #"))) = ("MQTT: connected successfully"); &__c[0];})))))); }
          stateMQTT = CHECK_CONNECTION; // advance to connection monitoring and publishing
          AllmaxUpdateMQTT = 0;
        } else { // connection failed
          if (dbglevel > 0) { Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "MQTT.ino" "." "52" "." "567" "\", \"aSM\", @progbits, 1 #"))) = ("MQTT: conenction failed"); &__c[0];})))))); }
        } // failed connectig, repeat starting up
      } //interval time
      break;
    }

    case CHECK_CONNECTION : { //---------------------
      if ((currentTime - lastMQTT) >= intervalMQTT) {
        lastMQTT = currentTime;
        if (mqttClient.loop() != true) {
          mqtt_connected = false;
          stateMQTT = START_UP;
          if (dbglevel == 3) { Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "MQTT.ino" "." "64" "." "568" "\", \"aSM\", @progbits, 1 #"))) = ("MQTT: is disconnected. Reconnecting to server"); &__c[0];})))))); }
        }
      }
      break;
    }

  } // end switch state
}

/******************************************************************************************************/
// Update MQTT Message
/******************************************************************************************************/
void updateMQTTMessage() {
  char MQTTtopicStr[64]; // String allocated for MQTT topic 
  mqtt_sent = false;

  // --------------------- this sends new data to mqtt server as soon as its available ----------------
  if (mySettings.sendMQTTimmediate) {
    char MQTTpayloadStr[512]; // String allocated for MQTT message
    if (scd30NewData) {
      sprintf(MQTTtopicStr,"%s/data",mySettings.mqtt_mainTopic);
      scd30JSON(MQTTpayloadStr);
      mqttClient.publish(MQTTtopicStr, MQTTpayloadStr);
      scd30NewData = false;
      if (dbglevel == 3) { Serial.println("SCD30 MQTT updated");}
      mqtt_sent = true;
      delay(1);
    }

    if (sgp30NewData) {
      sprintf(MQTTtopicStr,"%s/data",mySettings.mqtt_mainTopic);
      sgp30JSON(MQTTpayloadStr);
      mqttClient.publish(MQTTtopicStr, MQTTpayloadStr);
      sgp30NewData = false;
      if (dbglevel == 3) { Serial.println("SGP30 MQTT updated");}
      mqtt_sent = true;
      delay(1);
    }

    if (sps30NewData) {
      sprintf(MQTTtopicStr,"%s/data",mySettings.mqtt_mainTopic);
      sps30JSON(MQTTpayloadStr);
      mqttClient.publish(MQTTtopicStr, MQTTpayloadStr);
      sps30NewData = false;
      if (dbglevel == 3) { Serial.println("SPS30 MQTT updated");}
      mqtt_sent = true;
      delay(1);
    }

    if (ccs811NewData) {
      sprintf(MQTTtopicStr,"%s/data",mySettings.mqtt_mainTopic);
      ccs811JSON(MQTTpayloadStr);
      mqttClient.publish(MQTTtopicStr, MQTTpayloadStr);
      ccs811NewData = false;
      if (dbglevel == 3) { Serial.println("CCS811 MQTT updated");}
      mqtt_sent = true;
      delay(1);
    }

    if (bme680NewData) {
      sprintf(MQTTtopicStr,"%s/data",mySettings.mqtt_mainTopic);
      bme680JSON(MQTTpayloadStr);
      mqttClient.publish(MQTTtopicStr, MQTTpayloadStr);
      bme680NewData = false;
      if (dbglevel == 3) { Serial.println("BME680 MQTT updated");}
      mqtt_sent = true;
      delay(1);
    }

    if (bme280NewData) {
      sprintf(MQTTtopicStr,"%s/data",mySettings.mqtt_mainTopic);
      bme280JSON(MQTTpayloadStr);
      mqttClient.publish(MQTTtopicStr, MQTTpayloadStr);
      bme280NewData = false;
      if (dbglevel == 3) { Serial.println("BME280 MQTT updated");}
      mqtt_sent = true;
      delay(1);
    }


    if (mlxNewData) {
      sprintf(MQTTtopicStr,"%s/data",mySettings.mqtt_mainTopic);
      mlxJSON(MQTTpayloadStr);
      mqttClient.publish(MQTTtopicStr, MQTTpayloadStr);
      mlxNewData = false;
      if (dbglevel == 3) { Serial.println("MLX MQTT updated");}
      mqtt_sent = true;
      delay(1);
    }

    if (timeNewData) {
      sprintf(MQTTtopicStr,"%s/status",mySettings.mqtt_mainTopic);
      timeJSON(MQTTpayloadStr);
      mqttClient.publish(MQTTtopicStr, MQTTpayloadStr);
      mlxNewData = false;
      if (dbglevel == 3) { Serial.println("Time MQTT updated");}
      mqtt_sent = true;
      delay(1);
    }

    if (dateNewData) {
      sprintf(MQTTtopicStr,"%s/status",mySettings.mqtt_mainTopic);
      dateJSON(MQTTpayloadStr);
      mqttClient.publish(MQTTtopicStr, MQTTpayloadStr);
      mlxNewData = false;
      if (dbglevel == 3) { Serial.println("Data MQTT updated");}
      mqtt_sent = true;
      delay(1);
    }

    if (sendMQTTonce) { // send sensor polling once at boot up
      sprintf(MQTTtopicStr,"%s/status",mySettings.mqtt_mainTopic);
      sprintf(MQTTpayloadStr, "{\"mlx_interval\":%u,\"lcd_interval\":%u,\"ccs811_mode\":%u,\"sgp30_interval\":%u,\"scd30_interval\":%u,\"sps30_interval\":%u,\"bme680_interval\":%u,\"bme280_interval\":%u}",
                                  intervalMLX, intervalLCD, ccs811Mode, intervalSGP30, intervalSCD30, intervalSPS30, intervalBME680, intervalBME280);
      mqttClient.publish(MQTTtopicStr, MQTTpayloadStr);
      delay(1);
      sendMQTTonce = false; // do not update interval information again
    }

    if (currentTime - lastMQTTPublish > 5000 /* */) { // update sensor status every mninute
      sprintf(MQTTtopicStr,"%s/status",mySettings.mqtt_mainTopic);
      sprintf(MQTTpayloadStr, "{\"mlx_avail\":%u,\"lcd_avail\":%u,\"ccs811_avail\":%u,\"sgp30_avail\":%u,\"scd30_avail\":%u,\"sps30_avail\":%u,\"bme680_avail\":%u,\"bme280_avail\":%u,\"ntp_avail\":%u}",
                                  therm_avail, lcd_avail, ccs811_avail, sgp30_avail, scd30_avail, sps30_avail, bme680_avail, bme280_avail, ntp_avail);
      mqttClient.publish(MQTTtopicStr, MQTTpayloadStr);
      delay(1);

      lastMQTTPublish = currentTime;
      mqtt_sent = true;
    }
  }

  // --------------- this creates single message ---------------------------------------------------------
  else {
    if (currentTime - lastMQTTPublish > intervalMQTT) { // wait for interval time
      char MQTTpayloadStr[1024]; // String allocated for MQTT message
      updateMQTTpayload(MQTTpayloadStr); // creating payload String
      sprintf(MQTTtopicStr,"%s/data/all",mySettings.mqtt_mainTopic);
      if (dbglevel == 3) { Serial.println(MQTTpayloadStr);}
      mqttClient.publish(MQTTtopicStr, MQTTpayloadStr);
      lastMQTTPublish = currentTime;
      mqtt_sent = true;
    } // end interval
  } // end send immidiate

  // --------------- dubg status ---------------------------------------------------------
  if (mqtt_sent == true) {
    if (dbglevel == 3) { Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "MQTT.ino" "." "210" "." "569" "\", \"aSM\", @progbits, 1 #"))) = ("MQTT: published data"); &__c[0];}))))));}
  }
}

/**************************************************************************************/
// Create single MQTT payload message
/**************************************************************************************/

  // NEED TO FIX THIS TO MATCH MQTT ENTRIES OF SINGLE SENSOR UPDATES

void updateMQTTpayload(char *payload) {
  char tmpbuf[24];
  strcpy(payload, "{");
  if (scd30_avail && mySettings.useSCD30) { // ==CO2===========================================================
    strcat(payload,"\"scd30_CO2\":"); sprintf(tmpbuf, "%4dppm", int(scd30_ppm)); strcat(payload,tmpbuf); strcat(payload,", ");
    strcat(payload,"\"scd30_rH\":"); sprintf(tmpbuf, "%4.1f%%", scd30_hum); strcat(payload,tmpbuf); strcat(payload,", ");
    strcat(payload,"\"scd30_T\":"); sprintf(tmpbuf, "%+5.1fC",scd30_temp); strcat(payload,tmpbuf); strcat(payload,", ");
  } // end if avail scd30
  if (bme680_avail && mySettings.useBME680) { // ===rH,T,aQ=================================================
    strcat(payload,"\"bme680_p\":"); sprintf(tmpbuf, "%4dbar",(int)(bme680.pressure/100.0)); strcat(payload,tmpbuf); strcat(payload,", ");
    strcat(payload,"\"bme680_rH\":"); sprintf(tmpbuf, "%4.1f%%",bme680.humidity); strcat(payload,tmpbuf); strcat(payload,", ");
    strcat(payload,"\"bme680_aH\":"); sprintf(tmpbuf, "%4.1fg",bme680_ah); strcat(payload,tmpbuf); strcat(payload,", ");
    strcat(payload,"\"bme680_T\":"); sprintf(tmpbuf, "%+5.1fC",bme680.temperature); strcat(payload,tmpbuf); strcat(payload,", ");
    strcat(payload,"\"bme680_aq\":"); sprintf(tmpbuf, "%dOhm",bme680.gas_resistance); strcat(payload,tmpbuf); strcat(payload,", ");
  } // end if avail bme680
  if (bme280_avail && mySettings.useBME280) { // ===rH,T,aQ=================================================
    strcat(payload,"\"bme280_p\":"); sprintf(tmpbuf, "%4dbar",(int)(bme280_pressure/100.0)); strcat(payload,tmpbuf); strcat(payload,", ");
    strcat(payload,"\"bme280_rH\":"); sprintf(tmpbuf, "%4.1f%%",bme280_hum); strcat(payload,tmpbuf); strcat(payload,", ");
    strcat(payload,"\"bme280_aH\":"); sprintf(tmpbuf, "%4.1fg",bme280_ah); strcat(payload,tmpbuf); strcat(payload,", ");
    strcat(payload,"\"bme280_T\":"); sprintf(tmpbuf, "%+5.1fC",bme280_temp); strcat(payload,tmpbuf); strcat(payload,", ");
  } // end if avail bme680
  if (sgp30_avail && mySettings.useSGP30) { // ===CO2,tVOC=================================================
    strcat(payload,"\"sgp30_CO2\":"); sprintf(tmpbuf, "%4dppm", sgp30.CO2); strcat(payload,tmpbuf); strcat(payload,", ");
    strcat(payload,"\"sgp30_tVOC\":"); sprintf(tmpbuf, "%4dppb", sgp30.TVOC); strcat(payload,tmpbuf); strcat(payload,", ");
  } // end if avail sgp30
  if (ccs811_avail && mySettings.useCCS811) { // ===CO2,tVOC=================================================
    strcat(payload,"\"ccs811_CO2\":"); sprintf(tmpbuf, "%4dppm", ccs811.getCO2()); strcat(payload,tmpbuf); strcat(payload,", ");
    strcat(payload,"\"ccs811_tVOC\":"); sprintf(tmpbuf, "%4dppb", ccs811.getTVOC()); strcat(payload,tmpbuf); strcat(payload,", ");
  } // end if avail ccs811
  if (sps30_avail && mySettings.useSPS30) { // ===Particle=================================================
    strcat(payload,"\"sps30_PM1\":"); sprintf(tmpbuf, "%3.0fµg/m3",valSPS30.MassPM1); strcat(payload,tmpbuf); strcat(payload,", ");
    strcat(payload,"\"sps30_PM2\":"); sprintf(tmpbuf, "%3.0fµg/m3",valSPS30.MassPM2); strcat(payload,tmpbuf); strcat(payload,", ");
    strcat(payload,"\"sps30_PM4\":"); sprintf(tmpbuf, "%3.0fµg/m3",valSPS30.MassPM4); strcat(payload,tmpbuf); strcat(payload,", ");
    strcat(payload,"\"sps30_nPM10\":"); sprintf(tmpbuf, "%3.0fµg/m3",valSPS30.MassPM10); strcat(payload,tmpbuf); strcat(payload,", ");
    strcat(payload,"\"sps30_nPM0\":"); sprintf(tmpbuf, "%3.0f#/m3", valSPS30.NumPM0); strcat(payload,tmpbuf); strcat(payload,", ");
    strcat(payload,"\"sps30_nPM2\":"); sprintf(tmpbuf, "%3.0f#/m3", valSPS30.NumPM2); strcat(payload,tmpbuf); strcat(payload,", ");
    strcat(payload,"\"sps30_nPM4\":"); sprintf(tmpbuf, "%3.0f#/m3", valSPS30.NumPM4); strcat(payload,tmpbuf); strcat(payload,", ");
    strcat(payload,"\"sps30_nPM10\":"); sprintf(tmpbuf, "%3.0f#/m3", valSPS30.NumPM10); strcat(payload,tmpbuf); strcat(payload,", ");
    strcat(payload,"\"sps30_PartSize\":"); sprintf(tmpbuf, "%3.0fµm", valSPS30.PartSize); strcat(payload,tmpbuf); strcat(payload,", ");
  }// end if avail SPS30
  if (therm_avail && mySettings.useMLX) { // ====To,Ta================================================
    strcat(payload,"\"MLX_To\":"); sprintf(tmpbuf, "%+5.1fC",(therm.object()+mlxOffset)); strcat(payload,tmpbuf); strcat(payload,", ");
    strcat(payload,"\"MLX_Ta\":"); sprintf(tmpbuf, "%+5.1fC",therm.ambient()); strcat(payload,tmpbuf); strcat(payload,", ");
  }// end if avail  MLX
  strcat(payload, "}");
} // update MQTT


/******************************************************************************************************/
// MQTT Call Back in case we receive MQTT message from network
/******************************************************************************************************/

void mqttCallback(char* topic, byte* payload, unsigned int len) {
  if (dbglevel > 0) {
    Serial.println("MQTT: Message received");
    Serial.print("MQTT: topic: ");
    Serial.println(topic);
    Serial.print("MQTT: payload: ");
    Serial.print("] ");
    for (int i = 0; i < len; i++) {
      Serial.print((char)payload[i]);
    }
    Serial.println();
  }
}
# 1 "d:\\GitHub\\ESP Playground\\AirQuality\\software\\Unified Software\\Sensi\\NTP.ino"
//******************************************************************************************************//
// Network Time Protocol
//******************************************************************************************************//
// https://tttapa.github.io/ESP8266/Chap15%20-%20NTP.html
void updateNTP() {

  switch(stateNTP) {

    case IS_WAITING : { //---------------------
      if ((currentTime - lastNTP) >= 1000 /* We check if WiFi is connected every 1 sec, if disconnected we attempted reconnection every 1 sec*/) {
        lastNTP = currentTime;
        if ((dbglevel == 3) && mySettings.useNTP) { Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "NTP.ino" "." "12" "." "570" "\", \"aSM\", @progbits, 1 #"))) = ("NTP: is waiting for network to come up"); &__c[0];})))))); }
      }
      break;
    }

    case START_UP : { //---------------------

      if (false) { Serial.println("DBG:STARTUP: NTP"); }
      if (dbglevel == 3) { Serial.printf("NTP: connecting to %s \r\n", mySettings.ntpServer); }

      NTP.onNTPSyncEvent ([] (NTPEvent_t event) {
          ntpEvent = event;
          syncEventTriggered = true;
      });

      ntpSetupError = false;
      // setting up NTP client
      NTP.setTimeZone (mySettings.timeZone);
      if (!NTP.setInterval (600 /* default 1800 seconds*/)) {ntpSetupError = true;}
      if (!NTP.setNTPTimeout (5000 /* default 5000 milliseconds*/)) {ntpSetupError = true;}
      NTP.setMinSyncAccuracy (5000 /* default 5000*/);
      NTP.settimeSyncThreshold (2500 /* default 2500*/);
      NTP.setMaxNumSyncRetry(3 /* default 3*/);
      NTP.setnumAveRounds(1 /* default 1*/);
      // setup completed, now start it
      if (!ntpSetupError) {
        if (NTP.begin (mySettings.ntpServer)) {
          stateNTP = CHECK_CONNECTION; // move on
          if (dbglevel > 0) { Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "NTP.ino" "." "40" "." "571" "\", \"aSM\", @progbits, 1 #"))) = ("NTP: client initialized"); &__c[0];})))))); }
        }
      } else {
          if (dbglevel > 0) { Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "NTP.ino" "." "43" "." "572" "\", \"aSM\", @progbits, 1 #"))) = ("NTP: client setup error"); &__c[0];})))))); }
      }
      break;
    }

    case CHECK_CONNECTION : { //---------------------

      if (syncEventTriggered==true) {
          syncEventTriggered = false;
          processSyncEvent (ntpEvent);
      }

      break;
    }

  } // end switch state
} // ntp


void processSyncEvent (NTPEvent_t ntpEvent) {
    if (dbglevel > 0) { Serial.printf ("NTP: %s\n", NTP.ntpEvent2str(ntpEvent)); }
    switch (ntpEvent.event) {
        case timeSyncd:
          // got NTP time
          ntp_avail = true;
          timeSynced = true;
          break;
        case partlySync:
          // partial sync
          ntp_avail = true;
          timeSynced = true;
          break;
        case syncNotNeeded:
          // sync not needed
          ntp_avail = true;
          timeSynced = true;
          break;
        case accuracyError:
          // accuracy error
          ntp_avail = true;
          timeSynced = false;
          break;
        case noResponse:
          // no response from NTP server
          ntp_avail = false;
          break;
        case invalidAddress:
          // invalid server address
          ntp_avail = false;
          break;
        case invalidPort:
          // invalid port
          ntp_avail = false;
          break;
        case errorSending:
          // error sending ntp request
          ntp_avail = false;
          break;
        case responseError:
          // NTP response error
          ntp_avail = false;
          break;
        case syncError:
          // error applyting sync
          ntp_avail = false;
          break;
        case requestSent:
          // NTP request sent
          break;
        default:
          // unknown error
          break;
    }
}

/* old approach



void updateNTP() {



  switch(stateNTP) {

    

    case IS_WAITING : { //---------------------

      if ((currentTime - lastNTP) >= intervalWiFi) {

        if ((dbglevel == 3) && mySettings.useNTP) { Serial.println(F("NTP: is waiting for network to come up")); }          

        lastNTP = currentTime;

      }

      break;

    }

    

    case START_UP : { //---------------------

      if ((currentTime - lastNTP) >= intervalWiFi) {

        lastNTP = currentTime;



        if (DEBUG) { Serial.println("DBG:STARTUP: NTP"); }

        if (dbglevel == 3) { Serial.printf("NTP: connecting to %s \r\n", mySettings.ntpServer); }



        ntpUDP.begin(123);

        if(!WiFi.hostByName(mySettings.ntpServer, timeServerIP)) { // Get the IP address of the NTP server

          if (dbglevel  > 0) { Serial.println("NTP: DNS lookup failed"); }

          ntp_avail = false;

        } else {

          ntp_avail = true;

          sendNTPpacket(timeServerIP);

          stateNTP = CHECK_CONNECTION;        

          AllmaxUpdateNTP = 0;

          if (dbglevel  > 0) { Serial.println(F("NTP: client initialized")); }

        }

        

      }

      break;

    }

    

    case CHECK_CONNECTION : { //---------------------



      if ((currentTime - lastNTP) >= intervalNTP) {

        lastNTP = currentTime;

        while (ntpUDP.parsePacket() != 0) { // flush any existing packets

          ntpUDP.flush(); 

        }

        sendNTPpacket(timeServerIP);

        lastNTPSent = currentTime; // we need to record when we sent the packet

        if (dbglevel == 3) {Serial.println("NTP: Request sent"); }

      }



      uint32_t time = getTime();                   // Check if an NTP response has arrived and get the (UNIX) time

      if (time) {                                  // If a new timestamp has been received

        ntpTime = time;

        if (dbglevel  == 3) { Serial.print("NTP response: "); Serial.println(ntpTime); }

        lastNTPResponse = lastNTPSent;             // we need to update when packet was sent

        timeSynced = true;

      } else if ((currentTime - lastNTPResponse) > 3600000) {

        if (dbglevel  > 0) {Serial.println("NTP: More than 1 hour since last NTP response"); }

      }



      break;          

    }



  } // end switch state

} // ntp



uint32_t getTime() {

  if (ntpUDP.parsePacket() == 0) { // If there's no response (yet)

    return 0;

  }

  ntpUDP.read(NTPBuffer, NTP_PACKET_SIZE); // read the packet into the buffer

  // Combine the 4 timestamp bytes into one 32-bit number

  uint32_t NTPTime = (NTPBuffer[40] << 24) | (NTPBuffer[41] << 16) | (NTPBuffer[42] << 8) | NTPBuffer[43];

  // Convert NTP time to a UNIX timestamp:

  // Unix time starts on Jan 1 1970. That's 2208988800 seconds in NTP time:

  const uint32_t seventyYears = 2208988800UL;

  // subtract seventy years:

  uint32_t UNIXTime = NTPTime - seventyYears;

  return UNIXTime;

}



void sendNTPpacket(IPAddress& address) {

  memset(NTPBuffer, 0, NTP_PACKET_SIZE);  // set all bytes in the buffer to 0

  // Initialize values needed to form NTP request

  NTPBuffer[0] = 0b11100011;   // LI, Version, Mode

  NTPBuffer[1] = 0;            // Strati, or type clock

  NTPBuffer[2] = 6;            // Polling Interval

  NTPBuffer[3] = 0xEC;         // Peer clock precision

  // 8 bytes of zero for Root Delay & Root Dispersion

  NTPBuffer[12]  = 49;

  NTPBuffer[13]  = 0x4E;

  NTPBuffer[14]  = 49;

  NTPBuffer[15]  = 52;  

  // send a packet requesting a timestamp:

  ntpUDP.beginPacket(address, 123); // NTP requests are to port 123

  ntpUDP.write(NTPBuffer, NTP_PACKET_SIZE);

  ntpUDP.endPacket();

}

*/
# 1 "d:\\GitHub\\ESP Playground\\AirQuality\\software\\Unified Software\\Sensi\\OTA.ino"
/******************************************************************************************************/
// OTA
/******************************************************************************************************/
// Over the air program, standard functions from many web tutorials
void onstartOTA() {
  if (dbglevel > 0) {
    String type;
    if (ArduinoOTA.getCommand() == 0)
      type = "sketch";
    else // U_SPIFFS, LittleFS
      type = "filesystem";
    Serial.print(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "OTA.ino" "." "12" "." "573" "\", \"aSM\", @progbits, 1 #"))) = ("OTA: starting "); &__c[0];})))))); Serial.println(type);
    otaInProgress = true;
 }
}

void onendOTA() {
  if (dbglevel > 0) { Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "OTA.ino" "." "18" "." "574" "\", \"aSM\", @progbits, 1 #"))) = ("\r\nOTA: End"); &__c[0];})))))); }
  otaInProgress = false;
}

void onprogressOTA(unsigned int progress, unsigned int total) {
  if (dbglevel > 0) { Serial.printf("OTA: progress: %u%%\r", (progress / (total / 100))); }
}

void onerrorOTA(ota_error_t error) {
  if (dbglevel > 0) {
    Serial.printf("OTA: error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "OTA.ino" "." "29" "." "575" "\", \"aSM\", @progbits, 1 #"))) = (" Auth Failed"); &__c[0];}))))));
    else if (error == OTA_BEGIN_ERROR) Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "OTA.ino" "." "30" "." "576" "\", \"aSM\", @progbits, 1 #"))) = (" Begin Failed"); &__c[0];}))))));
    else if (error == OTA_CONNECT_ERROR) Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "OTA.ino" "." "31" "." "577" "\", \"aSM\", @progbits, 1 #"))) = (" Connect Failed"); &__c[0];}))))));
    else if (error == OTA_RECEIVE_ERROR) Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "OTA.ino" "." "32" "." "578" "\", \"aSM\", @progbits, 1 #"))) = (" Receive Failed"); &__c[0];}))))));
    else if (error == OTA_END_ERROR) Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "OTA.ino" "." "33" "." "579" "\", \"aSM\", @progbits, 1 #"))) = (" End Failed"); &__c[0];}))))));
  }
  otaInProgress = false;
}

void updateOTA() {

  switch(stateOTA) {

    case IS_WAITING : { //---------------------
      // just wait...
      if ((currentTime - lastOTA) >= 1000 /* We check if WiFi is connected every 1 sec, if disconnected we attempted reconnection every 1 sec*/) {
        if ((dbglevel == 3) && mySettings.useOTA) { Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "OTA.ino" "." "45" "." "580" "\", \"aSM\", @progbits, 1 #"))) = ("OTA: waiting for network to come up"); &__c[0];})))))); }
        lastOTA = currentTime;
      }
      break;
    }

    case START_UP : { //---------------------
      if ((currentTime - lastOTA) >= 1000 /* We check if WiFi is connected every 1 sec, if disconnected we attempted reconnection every 1 sec*/) {
        lastOTA = currentTime;
        if (false) { Serial.println("DBG:STARTUP: OTA"); }
        ArduinoOTA.setPort(8266); // OTA port
        ArduinoOTA.setHostname(hostName); // hostname
        ArduinoOTA.setPassword("w1ldc8ts"); // That should be set by programmer and not user 
        // call back functions
        ArduinoOTA.onStart( onstartOTA );
        ArduinoOTA.onEnd( onendOTA );
        ArduinoOTA.onProgress( onprogressOTA );
        ArduinoOTA.onError( onerrorOTA );
        // get it going
        ArduinoOTA.begin(true); //start and use  mDNS
        if (dbglevel > 0) { Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "OTA.ino" "." "65" "." "581" "\", \"aSM\", @progbits, 1 #"))) = ("OTA: ready"); &__c[0];})))))); }
        stateOTA = CHECK_CONNECTION;
        AllmaxUpdateOTA = 0;
      } // currentTime
      break;
    } // startup

    case CHECK_CONNECTION : { //---------------------
      //if (otaInProgress) {
      //  ArduinoOTA.handle(); 
      //} else if ((currentTime - lastOTA) >= intervalOTA) {
      //  lastOTA = currentTime;
          ArduinoOTA.handle();
      //} 
      break;
    }
  } // end switch
} // end update OTA
# 1 "d:\\GitHub\\ESP Playground\\AirQuality\\software\\Unified Software\\Sensi\\Quality.ino"
//////////////////////////////////////////////////////////////////////////////////////////////////////////
// Checks values to expected range and provides assessment
// Longet returned message is 15 characters long
//////////////////////////////////////////////////////////////////////////////////////////////////////////

bool checkCO2(float co2, char *message, int len) {
  // Space station CO2 is about 2000ppbm
  // Global outdoor CO2 is well published
  // Indoor: https://www.dhs.wisconsin.gov/chemical/carbondioxide.htm
  bool ok = true;
  if (len > 0) {
    if ( (co2 < 0.0) || co2 > 2.0e9 ) { // outside reasonable values
      strcpy(message, "?");
    } else if (len == 1) {
      if (co2 < 1000.) { strcpy(message, "N"); }
      else if (co2 < 2000.) { strcpy(message, "T"); }
      else if (co2 < 5000.) { strcpy(message, "P"); ok = false; }
      else { strcpy(message, "!"); ok = false; }

    } else if (len <= 4) {
      if (co2 < 1000.) { strncpy(message, "nrm ", len); }
      else if (co2 < 2000.) { strncpy(message, "thr ", len); }
      else if (co2 < 5000.) { strncpy(message, "poor", len); ok = false; }
      else { strncpy(message, "exc ", len); ok = false; }

    } else {
      if (co2 < 1000.) { strncpy(message, "Normal", len); }
      else if (co2 < 2000.) { strncpy(message, "Threshold", len); }
      else if (co2 < 5000.) { strncpy(message, "Poor", len); ok = false; }
      else { strncpy(message, "Excessive", len); ok = false; }
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
      if ((rH >= 45.) && (rH <= 55.)) { strcpy(message, "N"); }
      else if ((rH >= 25.) && (rH < 45.)) { strcpy(message, "l"); }
      else if ((rH >= 55.) && (rH < 65.)) { strcpy(message, "h"); }
      else if ((rH >= 65.) && (rH < 80.)) { strcpy(message, "H"); }
      else if ((rH > 15.) && (rH < 25.)) { strcpy(message, "L"); }
      else { strcpy(message, "!"); ok = false; }

    } else if (len <= 4) {
      if ((rH >= 45.) && (rH <= 55.)) { strncpy(message, "nrm ", len); }
      else if ((rH >= 25.) && (rH < 45.)) { strncpy(message, "thr ", len); }
      else if ((rH >= 55.) && (rH < 65.)) { strncpy(message, "thr ", len); }
      else if ((rH >= 65.) && (rH < 80.)) { strncpy(message, "high", len); }
      else if ((rH > 15.) && (rH < 25.)) { strncpy(message, "low ", len); }
      else { strncpy(message, "!   ", len); ok = false; }

    } else {
      if ((rH >= 45.) && (rH <= 55.)) { strncpy(message, "Normal", len); }
      else if ((rH >= 25.) && (rH < 45.)) { strncpy(message, "Threshold Low", len); }
      else if ((rH >= 55.) && (rH < 65.)) { strncpy(message, "Threshold High", len); }
      else if ((rH >= 65.) && (rH < 80.)) { strncpy(message, "High", len); }
      else if ((rH > 15.) && (rH < 25.)) { strncpy(message, "Low", len); }
      else { strncpy(message, "Excessive", len); ok = false; }
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
  bool ok = true;
  if (len > 0) {
    if (res == 0) {
      strcpy(message, "?");
    } else if (len == 1) {
      if (res < 5000) { strcpy(message, "N"); }
      else if (res < 10000) { strcpy(message, "T"); }
      else if (res < 300000) { strcpy(message, "P"); }
      else { strcpy(message, "!"); ok = false; }

    } else if (len <= 4) {
      if (res < 5000) { strncpy(message, "nrm ", len); }
      else if (res < 10000) { strncpy(message, "thr ", len); }
      else if (res < 300000) { strncpy(message, "poor", len); }
      else { strncpy(message, "!   ", len); ok = false; }

    } else {
      if (res < 5000) { strncpy(message, "Normal", len ); }
      else if (res < 10000) { strncpy(message, "Threshold", len ); }
      else if (res < 300000) { strncpy(message, "Poor", len ); }
      else { strncpy(message, "Excessive", len ); ok = false; }
    }
    message[len] = '\0';
  } else {
    if (res >= 300000) { ok = false; }
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
      if (tVOC < 220.) { strcpy(message, "N"); }
      else if (tVOC < 660.) { strcpy(message, "T"); }
      else if (tVOC < 2200.) { strcpy(message, "P"); }
      else { strcpy(message, "!"); ok = false;}

    } else if (len <= 4) {
      if (tVOC < 220.) { strncpy(message, "nrm ", len); }
      else if (tVOC < 660.) { strncpy(message, "thr ", len); }
      else if (tVOC < 2200.) { strncpy(message, "poor", len); }
      else { strncpy(message, "!   ", len); ok = false; }

    } else {
      if (tVOC < 220.) { strncpy(message, "Normal", len); }
      else if (tVOC < 660.) { strncpy(message, "Threshold", len); }
      else if (tVOC < 2200.) { strncpy(message, "Poor", len); }
      else { strncpy(message, "Excessive", len); ok = false;}
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
      if (PM2 < 10.0) { strcpy(message, "N"); }
      else if (PM2 < 25.0) { strcpy(message, "T"); }
      else if (PM2 < 65.0) { strcpy(message, "P"); }
      else { strcpy(message, "!"); ok = false; }

    } else if (len <= 4) {
      if (PM2 < 10.0) { strncpy(message, "nrm ", len); }
      else if (PM2 < 25.0) { strncpy(message, "thr ", len); }
      else if (PM2 < 65.0) { strncpy(message, "poor", len); }
      else { strncpy(message, "!   ", len); ok = false; }

    } else {
      if (PM2 < 10.0) { strncpy(message, "Normal", len); }
      else if (PM2 < 25.0) { strncpy(message, "Threshold", len); }
      else if (PM2 < 65.0) { strncpy(message, "Poor", len); }
      else { strncpy(message, "Excessive", len); ok = false; }
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
      if (PM10 < 20.0) { strcpy(message, "N"); }
      else if (PM10 < 50.0) { strcpy(message, "T"); }
      else if (PM10 < 150.0) { strcpy(message, "P"); }
      else { strcpy(message, "!"); ok = false; }

    } else if (len <= 4) {
      if (PM10 < 20.0) { strncpy(message, "nrm ", len); }
      else if (PM10 < 50.0) { strncpy(message, "thr ", len); }
      else if (PM10 < 150.0) { strncpy(message, "poor", len); }
      else { strncpy(message, "!   ", len); ok = false; }

    } else {
      if (PM10 < 20.0) { strncpy(message, "Normal", len); }
      else if (PM10 < 50.0) { strncpy(message, "Threshold", len); }
      else if (PM10 < 150.0) { strncpy(message, "Poor", len); }
      else { strncpy(message, "Excessive", len); ok = false; }
    }
    message[len] = '\0';
  } else {
    if (PM10 >= 150.0) { ok = false; }
  }
  return ok;
}

bool checkPM(float PM2, float PM10, char *message, int len) {
  bool ok = true;
  if (len > 0) {
    if ( (PM10 < 0.0) || (PM10 > 100000.0) || (PM2 < 0.0) || (PM2 > 100000.0) ) {
       strcpy(message, "?");
    } else if (len == 1) {
      if ((PM2 < 10.0) && (PM10 < 20.0)) { strcpy(message, "N"); }
      else if ((PM2 < 25.0) || (PM10 < 50.0)) { strcpy(message, "T"); }
      else if ((PM2 < 65.0) || (PM10 < 150.0)) { strcpy(message, "P"); }
      else { strcpy(message, "!"); ok = false; }

    } else if (len <= 4) {
      if ((PM2 < 10.0) && (PM10 < 20.0)) { strncpy(message, "nrm ", len); }
      else if ((PM2 < 25.0) || (PM10 < 50.0)) { strncpy(message, "thr ", len); }
      else if ((PM2 < 65.0) || (PM10 < 150.0)) { strncpy(message, "poor", len); }
      else { strncpy(message, "excs", len); ok = false; }

    } else {
      if ((PM2 < 10.0) && (PM10 < 20.0)) { strncpy(message, "Normal", len); }
      else if ((PM2 < 25.0) || (PM10 < 50.0)) { strncpy(message, "Threshold", len); }
      else if ((PM2 < 65.0) || (PM10 < 150.0)) { strncpy(message, "Poor", len); }
      else { strncpy(message, "Excessive", len); ok = false; }
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
      if (T < 35.0) { strcpy(message, "L"); ok = false;}
      else if (T <= 36.4) { strcpy(message, "T"); ok = false;}
      else if (T < 37.2) { strcpy(message, "N"); }
      else if (T < 38.3) { strcpy(message, "T"); ok = false;}
      else if (T < 41.5) { strcpy(message, "F"); ok = false;}
      else { strcpy(message, "!"); ok = false;}

    } else if (len <= 4) {
      if (T < 35.0) { strncpy(message, "low ", len); ok = false;}
      else if (T <= 36.4) { strncpy(message, "thr ", len); ok = false;}
      else if (T < 37.2) { strncpy(message, "nrm ", len); }
      else if (T < 38.3) { strncpy(message, "thr ", len); ok = false;}
      else if (T < 41.5) { strncpy(message, "f   ", len); ok = false;}
      else { strncpy(message, "!   ", len); ok = false;}

    } else {
      if (T < 35.0) { strncpy(message, "Low ", len); ok = false;}
      else if (T <= 36.4) { strncpy(message, "Threshold Low", len); ok = false;}
      else if (T < 37.2) { strncpy(message, "Normal", len); }
      else if (T < 38.3) { strncpy(message, "Threshold High", len); ok = false; }
      else if (T < 41.5) { strncpy(message, "Fever", len); ok = false;}
      else { strncpy(message, "Excessive Fever", len); ok = false;}
    }
    message[len] = '\0';
  } else {
      if (T <= 36.4) { ok = false; }
      else if (T >= 37.2) { ok = false; }
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
      strcpy(message, "?");
    } else if (len == 1) {
      if (T < 20.0) { strcpy(message, "C"); }
      else if (T <= 25.0) { strcpy(message, "N"); }
      else { strcpy(message, "H"); }

    } else if (len <= 4) {
      if (T < 20.0) { strncpy(message, "cld ", len); }
      else if (T <= 25.0) { strncpy(message, "nrm ", len); }
      else { strncpy(message, "hot ", len); }

    } else {
      if (T < 20.0) { strncpy(message, "Cold", len); }
      else if (T <= 25.0) { strncpy(message, "Normal", len); }
      else { strncpy(message, "Hot", len); }
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
    } else if (len == 1) {
      if (dP >= 5.0) { strcpy(message, "H"); ok = false; }
      else if (dP <= -5.0) { strcpy(message, "L"); ok = false; }
      else { strcpy(message, "N"); }

    } else if (len <= 4) {
      if (dP >= 5.0) { strncpy(message, "hig ", len); ok = false; }
      else if (dP <= -5.0) { strncpy(message, "low ", len); ok = false; }
      else { strncpy(message, "nrm ", len); }

    } else {
      if (dP >= 5.0) { strncpy(message, "High", len); ok = false; }
      else if (dP <= -5.0) { strncpy(message, "Low", len); ok = false; }
      else { strncpy(message, "Normal", len); }
    }
    message[len] = '\0';
  } else {
      if (dP >= 5.0) { ok = false; }
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
  if (scd30_avail && mySettings.useSCD30) {
    if ( checkCO2(float(scd30_ppm), qualityMessage, 0) == false ) { ok = false; }
  } else if (sgp30_avail && mySettings.useSGP30) {
      if ( checkCO2(float(scd30_ppm), qualityMessage, 0) == false ) { ok = false; }
  } else if (ccs811_avail && mySettings.useCCS811) {
    if ( checkCO2(ccs811.getCO2(), qualityMessage, 0) == false ) { ok = false; }
  }

  // Check Particle
  if (sps30_avail && mySettings.useSPS30) {
    if ( checkPM2(valSPS30.MassPM2, qualityMessage, 0) == false) { ok = false; }
    if ( checkPM10(valSPS30.MassPM10, qualityMessage, 0) == false) { ok = false; }
  } // SPS30 Sensirion Particle Sensor State Machine

  // Check tVOC
  if (ccs811_avail && mySettings.useCCS811) {
    if ( checkTVOC(ccs811.getTVOC(), qualityMessage, 0) == false ) { ok = false; }
  } else if (sgp30_avail && mySettings.useSGP30) {
    if ( checkTVOC(sgp30.TVOC, qualityMessage, 0) == false ) { ok = false; }
  }

  // Check Humidity
  if (bme280_avail && mySettings.useBME280) {
    if ( checkHumidity(bme280_hum, qualityMessage, 0) == false) { ok = false; }
  } else if (bme680_avail && mySettings.useBME680) {
    if ( checkHumidity(bme680.humidity, qualityMessage, 0) == false) { ok = false; }
  } else if (scd30_avail && mySettings.useSCD30) {
    if ( checkHumidity(scd30_hum, qualityMessage, 0) == false ) { ok = false; }
  }

  // Check dP
  if (bme280_avail && mySettings.useBME280) {
    if ( checkdP((bme280_pressure-bme280_pressure24hrs)/100.0, qualityMessage, 0) == false) { ok = false; }
  } else if (bme680_avail && mySettings.useBME680) {
    if ( checkdP((bme680.pressure-bme680_pressure24hrs)/100.0, qualityMessage, 0) == false){ ok = false; }
  }

  return ok;
}
# 1 "d:\\GitHub\\ESP Playground\\AirQuality\\software\\Unified Software\\Sensi\\SCD30.ino"
/******************************************************************************************************/
// Interrrupt Handler SCD30
/******************************************************************************************************/
void __attribute__((section("\".iram.text." "SCD30.ino" "." "4" "." "582" "\""))) handleSCD30Interrupt() { // Interrupt service routine when data ready is signaled
  stateSCD30 = DATA_AVAILABLE; // advance the sensor state
  if (dbglevel == 4) { // for debugging
    Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "SCD30.ino" "." "7" "." "583" "\", \"aSM\", @progbits, 1 #"))) = ("SCD30: interrupt occured"); &__c[0];}))))));
  }
}

/******************************************************************************************************/
// Initialize SCD30
/******************************************************************************************************/
bool initializeSCD30() {
  bool success = true;

  if (fastMode) { intervalSCD30 = 4000 /* measure every 2sec ... 30minutes, default is 4 */; } // set fast or slow mode
  else { intervalSCD30 = 60000 /* once a minute*/; }
  if (dbglevel >0) { Serial.print(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "SCD30.ino" "." "19" "." "584" "\", \"aSM\", @progbits, 1 #"))) = ("SCD30: Interval: "); &__c[0];})))))); Serial.println(intervalSCD30); }

  if (dbglevel > 0) { Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "SCD30.ino" "." "21" "." "585" "\", \"aSM\", @progbits, 1 #"))) = ("SCD30: configuring interrupt"); &__c[0];})))))); }
  pinMode(SCD30interruptPin , 0x00); // interrupt scd30
  attachInterrupt((((SCD30interruptPin) < 16)? (SCD30interruptPin) : -1), handleSCD30Interrupt, 0x01);
  scd30_port->begin(scd30_i2c[0], scd30_i2c[1]);
  if (scd30.begin(*scd30_port, true)) { // start with autocalibration
    scd30.setMeasurementInterval(uint16_t(intervalSCD30/1000));
    scd30.setAutoSelfCalibration(true);
    if (mySettings.tempOffset_SCD30_valid == 0xF0) { scd30.setTemperatureOffset(mySettings.tempOffset_SCD30); }
    mySettings.tempOffset_SCD30 = scd30.getTemperatureOffset();
    if (dbglevel > 0) { Serial.print(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "SCD30.ino" "." "30" "." "586" "\", \"aSM\", @progbits, 1 #"))) = ("SCD30: current temp offset: "); &__c[0];})))))); Serial.print(mySettings.tempOffset_SCD30,2); Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "SCD30.ino" "." "30" "." "587" "\", \"aSM\", @progbits, 1 #"))) = ("C"); &__c[0];})))))); }
    stateSCD30 = IS_BUSY;
  } else {
    scd30_avail = false;
    stateSCD30 = HAS_ERROR;
    success = false;
  }
  if (dbglevel > 0) { Serial.println("SCD30: initialized"); }
  delay(50);
  return success;
}

/******************************************************************************************************/
// Update SCD30
/******************************************************************************************************/
bool updateSCD30 () {
  // Operation:
  //   Sensor is initilized
  //     Measurement interval is set
  //     Autocalibrate is enabled
  //     Temperature offset to compensate self heating, improves humidity reading
  //     Pressure is set
  //   Begin Measureing
  //   If sensor has completed measurement ISR is triggered
  //   ISR advances statemachine
  //    Check if Data Available
  //     get CO2, Humidity, Temperature
  //    Update barometric pressure every once in a while if third party sensor is avaialble
  //    Wait until next ISR

  bool success = true;

  switch(stateSCD30) {

    case IS_MEASURING : { // used when RDY pin interrupt is not enabled
      if ((currentTime - lastSCD30) >= intervalSCD30) {
        scd30_port->begin(scd30_i2c[0], scd30_i2c[1]);
        if (scd30.dataAvailable()) {
          scd30_ppm = scd30.getCO2();
          scd30_temp = scd30.getTemperature();
          scd30_hum = scd30.getHumidity();
          lastSCD30 = currentTime;
          if (dbglevel == 4) { Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "SCD30.ino" "." "72" "." "588" "\", \"aSM\", @progbits, 1 #"))) = ("SCD30: data read"); &__c[0];})))))); }
          scd30NewData = true;
          scd30NewDataWS = true;
        } else {
          if (dbglevel == 4) { Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "SCD30.ino" "." "76" "." "589" "\", \"aSM\", @progbits, 1 #"))) = ("SCD30: data not yet available"); &__c[0];})))))); }
        }
      }
      break;
    } // is measuring

    case IS_BUSY: { // used to bootup sensor when RDY pin interrupt is enabled
      if ((currentTime - lastSCD30Busy) > 500 /* how frequently to read dataReady when sensor boots up, this is needed to clear the dataready signal for the interrupt*/) {
        scd30_port->begin(scd30_i2c[0], scd30_i2c[1]);
        if (scd30.dataAvailable()) {scd30.readMeasurement();} // without reading data, RDY pin will remain high and no interrupt will occur
        lastSCD30Busy = currentTime;
        if (dbglevel == 4) { Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "SCD30.ino" "." "87" "." "590" "\", \"aSM\", @progbits, 1 #"))) = ("SCD30: is busy"); &__c[0];})))))); }
      }
      break;
    }

    case DATA_AVAILABLE : { // used to obtain data when RDY pin interrupt is used
      scd30_port->begin(scd30_i2c[0], scd30_i2c[1]);
      scd30_ppm = scd30.getCO2();
      scd30_temp = scd30.getTemperature();
      scd30_hum = scd30.getHumidity();
      lastSCD30 = currentTime;
      scd30NewData = true;
      scd30NewDataWS = true;
      stateSCD30 = IS_IDLE;
      if (dbglevel == 4) { Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "SCD30.ino" "." "101" "." "591" "\", \"aSM\", @progbits, 1 #"))) = ("SCD30: data read"); &__c[0];})))))); }
      break;
    }

    case IS_IDLE : { // used when RDY pin is used with ISR

      // wait for intrrupt to occur, 
      // backup: if interrupt timed out, check the sensor
      if ( (currentTime - lastSCD30) > (2*intervalSCD30) ) {
        if (dbglevel == 4) { Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "SCD30.ino" "." "110" "." "592" "\", \"aSM\", @progbits, 1 #"))) = ("SCD30: interrupt timeout occured"); &__c[0];})))))); }
        scd30_port->begin(scd30_i2c[0], scd30_i2c[1]);
        if (scd30.dataAvailable()) {
          stateSCD30 = DATA_AVAILABLE;
        } else {
          stateSCD30 = HAS_ERROR;
          scd30_avail = false;
          success= false;
        }
      }

      // update pressure if available
      if (bme680_avail && mySettings.useBME680) { // update pressure settings
        if ((currentTime - lastPressureSCD30) >= 120000 /* if we have pressure data from other sensor we will provide it to the co2 sensor to improve accuracy in this interval*/) {
          scd30_port->begin(scd30_i2c[0], scd30_i2c[1]);
          scd30.setAmbientPressure(uint16_t(bme680.pressure/100.0)); // update with value from pressure sensor, needs to be mbar
          lastPressureSCD30 = currentTime;
          if (dbglevel == 4) { Serial.print(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "SCD30.ino" "." "127" "." "593" "\", \"aSM\", @progbits, 1 #"))) = ("SCD30: pressure updated to"); &__c[0];})))))); Serial.print(bme680.pressure/100.0); Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "SCD30.ino" "." "127" "." "594" "\", \"aSM\", @progbits, 1 #"))) = ("mbar"); &__c[0];})))))); }
        }
      } else if ((bme280_avail && mySettings.useBME280)) {
        if ((currentTime - lastPressureSCD30) >= 120000 /* if we have pressure data from other sensor we will provide it to the co2 sensor to improve accuracy in this interval*/) {
          scd30_port->begin(scd30_i2c[0], scd30_i2c[1]);
          scd30.setAmbientPressure(uint16_t(bme280_pressure/100.0)); // pressure is in Pa and scd30 needs mBar
          lastPressureSCD30 = currentTime;
          if (dbglevel == 4) { Serial.print(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "SCD30.ino" "." "134" "." "595" "\", \"aSM\", @progbits, 1 #"))) = ("SCD30: pressure updated to"); &__c[0];})))))); Serial.print(bme280_pressure/100.0); Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "SCD30.ino" "." "134" "." "596" "\", \"aSM\", @progbits, 1 #"))) = ("mbar"); &__c[0];})))))); }
        }
      }
      break;
    }

    case HAS_ERROR : {
      if (dbglevel > 0) { Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "SCD30.ino" "." "141" "." "597" "\", \"aSM\", @progbits, 1 #"))) = ("SCD30: error"); &__c[0];})))))); }
      success = false;
      break;
    }

  } // switch state

  return success;
}

void scd30JSON(char *payload){
  char qualityMessage1[16];
  char qualityMessage2[16];
  char qualityMessage3[16];
  checkCO2(float(scd30_ppm), qualityMessage1, 15);
  checkHumidity(scd30_hum, qualityMessage2, 15);
  checkAmbientTemperature(scd30_temp, qualityMessage3, 15);
  sprintf(payload, "{\"scd30\":{\"avail\":%s,\"CO2\":%d,\"rH\":%4.1f,\"aH\":%4.1f,\"T\":%4.1f,\"CO2_airquality\":\"%s\",\"rH_airquality\":\"%s\",\"T_airquality\":\"%s\"}}",
                       scd30_avail ? "true" : "false",
                       scd30_ppm,
                       scd30_hum,
                       scd30_ah,
                       scd30_temp,
                       qualityMessage1,
                       qualityMessage2,
                       qualityMessage3);
}
# 1 "d:\\GitHub\\ESP Playground\\AirQuality\\software\\Unified Software\\Sensi\\SGP30.ino"
/******************************************************************************************************/
// Initialize SGP30
/******************************************************************************************************/
bool initializeSGP30() {
  bool success = true;

  if (fastMode) { intervalSGP30 = 1000 /* as low as 10ms, ideal is 1s, gives best accuracy*/;}
  else { intervalSGP30 = 1000 /* recommended interval is 1s, no slow version*/;}
  if (dbglevel >0) { Serial.print(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "SGP30.ino" "." "9" "." "598" "\", \"aSM\", @progbits, 1 #"))) = ("SGP30: Interval: "); &__c[0];})))))); Serial.println(intervalSGP30); }
  sgp30_port->begin(sgp30_i2c[0], sgp30_i2c[1]);
  if (sgp30.begin(*sgp30_port) == false) {
    if (dbglevel > 0) { Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "SGP30.ino" "." "12" "." "599" "\", \"aSM\", @progbits, 1 #"))) = ("SGP30: No SGP30 Detected. Check connections"); &__c[0];})))))); }
    sgp30_avail = false;
    stateSGP30 = HAS_ERROR;
    success = false;
  }

  //Initializes sensor for air quality readings
  sgp30.initAirQuality();
  if (dbglevel > 0) { Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "SGP30.ino" "." "20" "." "600" "\", \"aSM\", @progbits, 1 #"))) = ("SGP30: measurements initialzed"); &__c[0];})))))); }
  stateSGP30 = IS_MEASURING;
  if (mySettings.baselineSGP30_valid == 0xF0) {
    sgp30.setBaseline((uint16_t)mySettings.baselineeCO2_SGP30, (uint16_t)mySettings.baselinetVOC_SGP30);
    if (dbglevel > 0) { Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "SGP30.ino" "." "24" "." "601" "\", \"aSM\", @progbits, 1 #"))) = ("SGP30: found valid baseline"); &__c[0];})))))); }
    warmupSGP30 = millis() + 3600000 /* 60min */;
  } else {
    warmupSGP30 = millis() + 43200000 /* 12hrs */;
  }

  if (dbglevel > 0) { Serial.println("SGP30: initialized"); }
  delay(50);
  return success;
}

/******************************************************************************************************/
// Update SGP30
/******************************************************************************************************/
bool updateSGP30() {
  // Operation Sequence:
  //  Setup
  //   Power up, puts the sensor in sleep mode
  //   Init Command (10ms to complete)
  //   Read baseline from EEPROM
  //   If valid set baseline
  //  Update
  //   Humidity interval exceeded?
  //     write humidity from third party sensors to sensor (10ms to complete)
  //   Start Measure Airquality Command 
  //   Unresponsive for 12ms
  //   Initiate Get Airquality Command
  //   Wait until measurement interval exceeded
  //
  // Read Baseline (10ms) periodically from sensors dynamic baseline calculations
  // Set Humidity from third party sensor periodically to increase accuracy

  bool success = true;
  SGP30ERR sgp30Error;

  switch(stateSGP30) {

    case IS_MEASURING : { //---------------------
      if ((currentTime - lastSGP30Humidity) > 60000 /* update humidity once a minute if available from other sensor*/) {
        if (bme680_avail && mySettings.useBME680) {
          sgp30_port->begin(sgp30_i2c[0], sgp30_i2c[1]);
          // Humidity correction, 8.8 bit number
          // 0x0F80 = 15.5 g/m^3
          // 0x0001 = 1/256 g/m^3
          // 0xFFFF = 256 +256/256 g/m^3
          sgp30.setHumidity(uint16_t(bme680_ah * 256.0 + 0.5));
          if (dbglevel == 6) { Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "SGP30.ino" "." "70" "." "602" "\", \"aSM\", @progbits, 1 #"))) = ("SGP30: humidity updated for eCO2"); &__c[0];})))))); }
        }
        lastSGP30Humidity = currentTime;
      } // end humidity update 

      if ((currentTime - lastSGP30Baseline) > 300000 /* obtain baseline every 5 minutes*/) {
        sgp30_port->begin(sgp30_i2c[0], sgp30_i2c[1]);
        sgp30Error = sgp30.startGetBaseline(); // this has 10ms delay
        if (sgp30Error != SUCCESS) {
           if (dbglevel > 0) { Serial.print(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "SGP30.ino" "." "79" "." "603" "\", \"aSM\", @progbits, 1 #"))) = ("SGP30: error obtaining baseline: "); &__c[0];})))))); Serial.println(sgp30Error); }
           stateSGP30 = HAS_ERROR;
           success = false;
           sgp30_avail = false;
        } else {
          if (dbglevel == 6) { Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "SGP30.ino" "." "84" "." "604" "\", \"aSM\", @progbits, 1 #"))) = ("SGP30: obtaining internal baseline"); &__c[0];})))))); }
          lastSGP30Baseline= millis();
          stateSGP30 = GET_BASELINE;
        }
        break;
      }

      if ((currentTime - lastSGP30) > intervalSGP30) {
        sgp30_port->begin(sgp30_i2c[0], sgp30_i2c[1]);
        sgp30Error = sgp30.startAirQuality();
        if (sgp30Error != SUCCESS) {
          if (dbglevel > 0) { Serial.print(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "SGP30.ino" "." "95" "." "605" "\", \"aSM\", @progbits, 1 #"))) = ("SGP30: error initiating airquality measurement: "); &__c[0];})))))); Serial.println(sgp30Error); }
          stateSGP30 = HAS_ERROR;
          success = false;
          sgp30_avail = false;
        } else {
          if (dbglevel == 6) { Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "SGP30.ino" "." "100" "." "606" "\", \"aSM\", @progbits, 1 #"))) = ("SGP30: airquality measurement initiated"); &__c[0];})))))); }
          stateSGP30 = IS_BUSY;
        }
        lastSGP30 = currentTime;
      }
      break;
    }

    case IS_BUSY : { //---------------------
      if ((currentTime - lastSGP30) >= 20) {
        sgp30_port->begin(sgp30_i2c[0], sgp30_i2c[1]);
        sgp30Error = sgp30.getAirQuality();
        if (sgp30Error != SUCCESS) {
          if (dbglevel > 0) {Serial.print(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "SGP30.ino" "." "113" "." "607" "\", \"aSM\", @progbits, 1 #"))) = ("SGP30: error obtaining measurements: "); &__c[0];})))))); Serial.println(sgp30Error); }
          stateSGP30 = HAS_ERROR;
          success = false;
          sgp30_avail = false;
        } else {
          if (dbglevel == 6) { Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "SGP30.ino" "." "118" "." "608" "\", \"aSM\", @progbits, 1 #"))) = ("SGP30: eCO2 & tVOC measured"); &__c[0];})))))); }
          sgp30NewData = true;
          sgp30NewDataWS = true;
          stateSGP30 = IS_MEASURING;
        }
        lastSGP30 = currentTime;
      }
      break;
    }

    case GET_BASELINE : { //---------------------
      if ((currentTime - lastSGP30Baseline) > 10) {
        sgp30_port->begin(sgp30_i2c[0], sgp30_i2c[1]);
        sgp30Error = sgp30.finishGetBaseline();
        if (sgp30Error == SUCCESS) {
          if (dbglevel == 6) { Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "SGP30.ino" "." "133" "." "609" "\", \"aSM\", @progbits, 1 #"))) = ("SGP30: baseline obtained"); &__c[0];})))))); }
          stateSGP30 = IS_MEASURING;
        } else {
          if (dbglevel > 0) { Serial.print(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "SGP30.ino" "." "136" "." "610" "\", \"aSM\", @progbits, 1 #"))) = ("SGP30: error baseline: "); &__c[0];})))))); Serial.println(sgp30Error); }
          stateSGP30 = HAS_ERROR;
          success = false;
          sgp30_avail = false;
        }
      }
      break;
    }

    case HAS_ERROR : { //------------------
      if (dbglevel > 0) { Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "SGP30.ino" "." "146" "." "611" "\", \"aSM\", @progbits, 1 #"))) = ("SGP30: error"); &__c[0];})))))); }
      success = false;
      break;
    }

  } // switch state

  return success;
}

void sgp30JSON(char *payload){
  char qualityMessage1[16];
  char qualityMessage2[16];
  checkCO2(float(sgp30.CO2), qualityMessage1, 15);
  checkTVOC(float(sgp30.TVOC), qualityMessage2, 15);
  sprintf(payload, "{\"sgp30\":{\"avail\":%s,\"eCO2\":%d,\"tVOC\":%d,\"eCO2_airquality\":\"%s\",\"tVOC_airquality\":\"%s\"}}",
                       sgp30_avail ? "true" : "false",
                       sgp30.CO2,
                       sgp30.TVOC,
                       qualityMessage1,
                       qualityMessage2);
}
# 1 "d:\\GitHub\\ESP Playground\\AirQuality\\software\\Unified Software\\Sensi\\SPS30.ino"
/******************************************************************************************************/
// Initialize SPS30
/******************************************************************************************************/
bool initializeSPS30() {
  bool success = true;

  if (fastMode == true) { intervalSPS30 = 5000 /* minimum is 1 sec*/; }
  else { intervalSPS30 = 60000 /* system will sleep for intervalSPS30Slow - timetoStable if sleep function is available*/; }

  if (dbglevel >0) { Serial.print(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "SPS30.ino" "." "10" "." "612" "\", \"aSM\", @progbits, 1 #"))) = ("SPS: Interval: "); &__c[0];})))))); Serial.println(intervalSPS30); }
  sps30_port->begin(sps30_i2c[0], sps30_i2c[1]);

  sps30.EnableDebugging(0 /* define driver debug: */);

  if (sps30.begin(sps30_port) == false) {
    if (dbglevel > 0) { Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "SPS30.ino" "." "16" "." "613" "\", \"aSM\", @progbits, 1 #"))) = ("SPS30: Sensor not detected in I2C. Please check wiring"); &__c[0];})))))); }
    stateSPS30 = HAS_ERROR;
    sps30_avail = false;
    success = false;
  }

  if (sps30.probe() == false) {
    // attempt reset and retry, I dont know how to restart sps30 without physical access.
    sps30.reset();
    delay(100);
    if (sps30.probe() == false) {
      if (dbglevel > 0) {
        Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "SPS30.ino" "." "28" "." "614" "\", \"aSM\", @progbits, 1 #"))) = ("SPS30: could not probe / connect"); &__c[0];}))))));
        Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "SPS30.ino" "." "29" "." "615" "\", \"aSM\", @progbits, 1 #"))) = ("SPS30: powercycle the system and reopen serial console"); &__c[0];}))))));
      }
      stateSPS30 = HAS_ERROR;
      sps30_avail = false;
      success = false;
    }
  } else {
    if (dbglevel > 0) { Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "SPS30.ino" "." "36" "." "616" "\", \"aSM\", @progbits, 1 #"))) = ("SPS30: detected"); &__c[0];})))))); }
  }

  if (sps30.reset() == false) {
    if (dbglevel > 0) { Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "SPS30.ino" "." "40" "." "617" "\", \"aSM\", @progbits, 1 #"))) = ("SPS30: could not reset"); &__c[0];})))))); }
    stateSPS30 = HAS_ERROR;
    sps30_avail = false;
    success = false;
  }

  // read device info
  if (dbglevel > 0) {
    Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "SPS30.ino" "." "48" "." "618" "\", \"aSM\", @progbits, 1 #"))) = ("SPS30: obtaining device information:"); &__c[0];}))))));
    ret = sps30.GetSerialNumber(buf, 32);
    if (ret == 0x00) {
      if (dbglevel > 0) {
        Serial.print(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "SPS30.ino" "." "52" "." "619" "\", \"aSM\", @progbits, 1 #"))) = ("SPS30: serial number : "); &__c[0];}))))));
        if(strlen(buf) > 0) { Serial.println(buf); }
        else { Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "SPS30.ino" "." "54" "." "620" "\", \"aSM\", @progbits, 1 #"))) = ("not available"); &__c[0];})))))); }
      }
    } else { Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "SPS30.ino" "." "56" "." "621" "\", \"aSM\", @progbits, 1 #"))) = ("SPS30: could not obtain serial number"); &__c[0];})))))); }

    ret = sps30.GetProductName(buf, 32);
    if (ret == 0x00) {
        Serial.print(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "SPS30.ino" "." "60" "." "622" "\", \"aSM\", @progbits, 1 #"))) = ("SPS30: product name  : "); &__c[0];}))))));
        if(strlen(buf) > 0) { Serial.println(buf); }
        else { Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "SPS30.ino" "." "62" "." "623" "\", \"aSM\", @progbits, 1 #"))) = ("not available"); &__c[0];})))))); }
    } else { if (dbglevel > 0) { Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "SPS30.ino" "." "63" "." "624" "\", \"aSM\", @progbits, 1 #"))) = ("SPS30: could not obtain product name"); &__c[0];})))))); } }
  }

  ret = sps30.GetVersion(&v);
  if (ret != 0x00) { // I have sensor that reports v.minor = 255 and CRC error when reading version information
    if (dbglevel >0 ) { Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "SPS30.ino" "." "68" "." "625" "\", \"aSM\", @progbits, 1 #"))) = ("SPS30: error when reading version information"); &__c[0];})))))); }
    v.major = 1; // is reasonable minimal version
    v.minor = 0; // is not reasonable
  }
  if (dbglevel >0 ) {
    Serial.print(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "SPS30.ino" "." "73" "." "626" "\", \"aSM\", @progbits, 1 #"))) = ("SPS30: firmware level: "); &__c[0];})))))); Serial.print(v.major); Serial.print(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "SPS30.ino" "." "73" "." "627" "\", \"aSM\", @progbits, 1 #"))) = ("."); &__c[0];})))))); Serial.println(v.minor);
    Serial.print(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "SPS30.ino" "." "74" "." "628" "\", \"aSM\", @progbits, 1 #"))) = ("SPS30: hardware level: "); &__c[0];})))))); Serial.println(v.HW_version);
    Serial.print(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "SPS30.ino" "." "75" "." "629" "\", \"aSM\", @progbits, 1 #"))) = ("SPS30: SHDLC protocol: "); &__c[0];})))))); Serial.print(v.SHDLC_major); Serial.print(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "SPS30.ino" "." "75" "." "630" "\", \"aSM\", @progbits, 1 #"))) = ("."); &__c[0];})))))); Serial.println(v.SHDLC_minor);
    Serial.print(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "SPS30.ino" "." "76" "." "631" "\", \"aSM\", @progbits, 1 #"))) = ("SPS30: library level : "); &__c[0];})))))); Serial.print(v.DRV_major); Serial.print(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "SPS30.ino" "." "76" "." "632" "\", \"aSM\", @progbits, 1 #"))) = ("."); &__c[0];})))))); Serial.println(v.DRV_minor);
  }

  ret = sps30.GetAutoCleanInt(&autoCleanIntervalSPS30);
  if (ret == 0x00) {
    if (dbglevel > 0) { Serial.print(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "SPS30.ino" "." "81" "." "633" "\", \"aSM\", @progbits, 1 #"))) = ("SPS30: current Auto Clean interval: "); &__c[0];})))))); Serial.print(autoCleanIntervalSPS30); Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "SPS30.ino" "." "81" "." "634" "\", \"aSM\", @progbits, 1 #"))) = ("s"); &__c[0];})))))); }
  } else {
    if (dbglevel > 0) {Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "SPS30.ino" "." "83" "." "635" "\", \"aSM\", @progbits, 1 #"))) = ("SPS30: coulnd not obtain autoclean information"); &__c[0];})))))); }
  }
  if (sps30.start() == true) {
    if (dbglevel > 0) { Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "SPS30.ino" "." "86" "." "636" "\", \"aSM\", @progbits, 1 #"))) = ("SPS30: measurement started"); &__c[0];})))))); }
    stateSPS30 = IS_BUSY;
  } else {
    if (dbglevel > 0) { Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "SPS30.ino" "." "89" "." "637" "\", \"aSM\", @progbits, 1 #"))) = ("SPS30: could NOT start measurement"); &__c[0];})))))); }
    stateSPS30 = HAS_ERROR;
    sps30_avail = false;
    success = false;
  }

  // addresse issue with limited RAM on some microcontrollers
  if (dbglevel > 0) {
    if (sps30.I2C_expect() == 4) { Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "SPS30.ino" "." "97" "." "638" "\", \"aSM\", @progbits, 1 #"))) = ("SPS30: !!! Due to I2C buffersize only the SPS30 MASS concentration is available !!!"); &__c[0];})))))); }
  }
  if (dbglevel > 0) { Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "SPS30.ino" "." "99" "." "639" "\", \"aSM\", @progbits, 1 #"))) = ("SPS: initialized"); &__c[0];})))))); }
  delay(50);
  return success;
}


/******************************************************************************************************/
// Update SPS30
/******************************************************************************************************/
bool updateSPS30() {
  // Operation Mode:
  //   Init, send start command
  //   Read status
  //   Read data
  //   Determine time to stable
  //   Wait until stable
  //   Read data
  // Slow Mode:
  //   Got to idle
  //   Got to sleep
  //   Wait until sleep exceeded
  //   Wakeup
  //   Start measurement
  //   Goto wait until stable
  // Fast Mode:
  //   Wait until measurement interval exceeded
  //   Goto read data
  //
  bool success = true;

  switch(stateSPS30) {

    case IS_BUSY: { //---------------------
      if (dbglevel == 5) { Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "SPS30.ino" "." "132" "." "640" "\", \"aSM\", @progbits, 1 #"))) = ("SPS30: is busy"); &__c[0];})))))); }
      sps30_port->begin(sps30_i2c[0], sps30_i2c[1]);
      if (((v.major==2) && (v.minor>=2)) || (v.major>2)) {
        ret = sps30.GetStatusReg(&st); // takes 20ms
        if (ret == 0x00) {
          if (dbglevel == 5) { Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "SPS30.ino" "." "137" "." "641" "\", \"aSM\", @progbits, 1 #"))) = ("SPS30: reading status completed"); &__c[0];})))))); }
          if (st == STATUS_OK) {
            if (dbglevel == 5) { Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "SPS30.ino" "." "139" "." "642" "\", \"aSM\", @progbits, 1 #"))) = ("SPS30: ok"); &__c[0];})))))); }
          } else {
            if (dbglevel > 0) {
              if (st & STATUS_SPEED_ERROR) { Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "SPS30.ino" "." "142" "." "643" "\", \"aSM\", @progbits, 1 #"))) = ("SPS30: warning fan is turning too fast or too slow"); &__c[0];})))))); }
              if (st & STATUS_LASER_ERROR) { Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "SPS30.ino" "." "143" "." "644" "\", \"aSM\", @progbits, 1 #"))) = ("SPS30: error laser failure"); &__c[0];})))))); }
              if (st & STATUS_FAN_ERROR) { Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "SPS30.ino" "." "144" "." "645" "\", \"aSM\", @progbits, 1 #"))) = ("SPS30: error fan failure, fan is mechanically blocked or broken"); &__c[0];})))))); }
            }
            stateSPS30 = HAS_ERROR;
          } // status ok
        } else {
          if (dbglevel > 0) { Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "SPS30.ino" "." "149" "." "646" "\", \"aSM\", @progbits, 1 #"))) = ("SPS30: could not read status"); &__c[0];})))))); }
          stateSPS30 = HAS_ERROR;
        }// read status
      } // Firmware >= 2.2
      tmpTime = millis();
      ret = sps30.GetValues(&valSPS30);
      if (dbglevel == 5) { Serial.print(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "SPS30.ino" "." "155" "." "647" "\", \"aSM\", @progbits, 1 #"))) = ("SPS30: values read in "); &__c[0];})))))); Serial.print(millis() - tmpTime); Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "SPS30.ino" "." "155" "." "648" "\", \"aSM\", @progbits, 1 #"))) = ("ms"); &__c[0];})))))); }
      if (ret == 0X01) {
        if (dbglevel > 0) { Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "SPS30.ino" "." "157" "." "649" "\", \"aSM\", @progbits, 1 #"))) = ("SPS30: error data length during reading values"); &__c[0];})))))); }
        stateSPS30 = HAS_ERROR;
      } else if (ret != 0x00) {
        if (dbglevel > 0) { Serial.print(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "SPS30.ino" "." "160" "." "650" "\", \"aSM\", @progbits, 1 #"))) = ("SPS30: error reading values: "); &__c[0];})))))); Serial.println(ret); }
        stateSPS30 = HAS_ERROR;
      }
      lastSPS30 = currentTime;
      // adjust time to get stable readings, it takes longer  with lower concentration to get a precise reading
      totalParticles = (valSPS30.NumPM0 + valSPS30.NumPM1 + valSPS30.NumPM2 + valSPS30.NumPM4 + valSPS30.NumPM10);
      if (totalParticles < 100) { timeToStableSPS30 = 30000; } // hard coded from data sheet
      else if (totalParticles < 200) { timeToStableSPS30 = 16000; } // hard coded from data sheet
      else { timeToStableSPS30 = 8000; } // hard coded from data sheet
      timeSPS30Stable = currentTime + timeToStableSPS30;
      if (dbglevel == 5) { Serial.print(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "SPS30.ino" "." "170" "." "651" "\", \"aSM\", @progbits, 1 #"))) = ("SPS30: total particles - "); &__c[0];})))))); Serial.println(totalParticles); }
      if (dbglevel == 5) {
        Serial.print(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "SPS30.ino" "." "172" "." "652" "\", \"aSM\", @progbits, 1 #"))) = ("SPS30: current time: "); &__c[0];})))))); Serial.println(currentTime);
        Serial.print(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "SPS30.ino" "." "173" "." "653" "\", \"aSM\", @progbits, 1 #"))) = ("SPS30: time when stable: "); &__c[0];})))))); Serial.println(timeSPS30Stable);
      }
      stateSPS30 = WAIT_STABLE;
      break;
    } // is BUSY

    case WAIT_STABLE: { //---------------------
      if (dbglevel == 5) {
        Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "SPS30.ino" "." "181" "." "654" "\", \"aSM\", @progbits, 1 #"))) = ("SPS30: wait until stable"); &__c[0];}))))));
        Serial.print(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "SPS30.ino" "." "182" "." "655" "\", \"aSM\", @progbits, 1 #"))) = ("SPS30: current time: "); &__c[0];})))))); Serial.println(currentTime);
        Serial.print(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "SPS30.ino" "." "183" "." "656" "\", \"aSM\", @progbits, 1 #"))) = ("SPS30: time when stable: "); &__c[0];})))))); Serial.println(timeSPS30Stable);
      }
      if (currentTime >= timeSPS30Stable) {
        sps30_port->begin(sps30_i2c[0], sps30_i2c[1]);
        if (((v.major==2) && (v.minor>=2)) || (v.major>2)) {
          ret = sps30.GetStatusReg(&st); // takes 20ms
          if (ret == 0x00) {
            if (dbglevel == 5) { Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "SPS30.ino" "." "190" "." "657" "\", \"aSM\", @progbits, 1 #"))) = ("SPS30: reading status completed"); &__c[0];})))))); }
            if (st == STATUS_OK) {
              if (dbglevel == 5) { Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "SPS30.ino" "." "192" "." "658" "\", \"aSM\", @progbits, 1 #"))) = ("SPS30: ok"); &__c[0];}))))));}
            } else {
              if (dbglevel > 0) {
                if (st & STATUS_SPEED_ERROR) { Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "SPS30.ino" "." "195" "." "659" "\", \"aSM\", @progbits, 1 #"))) = ("(SPS30: warning, fan is turning too fast or too slow"); &__c[0];})))))); }
                if (st & STATUS_LASER_ERROR) { Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "SPS30.ino" "." "196" "." "660" "\", \"aSM\", @progbits, 1 #"))) = ("(SPS30: error laser failure"); &__c[0];})))))); }
                if (st & STATUS_FAN_ERROR) { Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "SPS30.ino" "." "197" "." "661" "\", \"aSM\", @progbits, 1 #"))) = ("(SPS30: Eerror fan failure, fan is mechanically blocked or broken"); &__c[0];})))))); }
              }
              stateSPS30 = HAS_ERROR;
            } // status ok/notok
          } else {
            if (dbglevel > 0) { Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "SPS30.ino" "." "202" "." "662" "\", \"aSM\", @progbits, 1 #"))) = ("SPS30: could not read status"); &__c[0];})))))); }
            stateSPS30 = HAS_ERROR;
          } // read status
        } // Firmware >= 2.2
        tmpTime = millis();
        ret = sps30.GetValues(&valSPS30);
        if (dbglevel == 5) {
          Serial.print(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "SPS30.ino" "." "209" "." "663" "\", \"aSM\", @progbits, 1 #"))) = ("SPS30: values read in "); &__c[0];}))))));
          Serial.print(millis() - tmpTime);
          Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "SPS30.ino" "." "211" "." "664" "\", \"aSM\", @progbits, 1 #"))) = ("ms"); &__c[0];}))))));
        }
        if (ret == 0X01) {
          if (dbglevel > 0) { Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "SPS30.ino" "." "214" "." "665" "\", \"aSM\", @progbits, 1 #"))) = ("SPS30: error data length during reading values"); &__c[0];})))))); }
          stateSPS30 = HAS_ERROR;
        } else if (ret != 0x00) {
          if (dbglevel > 0) { Serial.print(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "SPS30.ino" "." "217" "." "666" "\", \"aSM\", @progbits, 1 #"))) = ("SPS30: error reading values: "); &__c[0];})))))); Serial.println(ret); }
          stateSPS30 = HAS_ERROR;
        }
        sps30NewData = true;
        sps30NewDataWS = true;
        if (dbglevel == 5) { Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "SPS30.ino" "." "222" "." "667" "\", \"aSM\", @progbits, 1 #"))) = ("SPS30: going to idle"); &__c[0];})))))); }
        lastSPS30 = currentTime;
        stateSPS30 = IS_IDLE;
      } //if time stable
      break;
    } // wait stable

    case IS_IDLE : { //---------------------
      if (dbglevel == 5) { Serial.println("SPS30: is idle"); }
      if ((v.major<2) || (fastMode)) {
        timeSPS30Stable = (unsigned long) (lastSPS30 + intervalSPS30);
        if (dbglevel == 5) {
          Serial.print(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "SPS30.ino" "." "234" "." "668" "\", \"aSM\", @progbits, 1 #"))) = ("SPS30: lastSPS30: "); &__c[0];})))))); Serial.println(lastSPS30);
          Serial.print(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "SPS30.ino" "." "235" "." "669" "\", \"aSM\", @progbits, 1 #"))) = ("SPS30: current time: "); &__c[0];})))))); Serial.println(currentTime);
          Serial.print(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "SPS30.ino" "." "236" "." "670" "\", \"aSM\", @progbits, 1 #"))) = ("SPS30: time when stable: "); &__c[0];})))))); Serial.println(timeSPS30Stable);
          Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "SPS30.ino" "." "237" "." "671" "\", \"aSM\", @progbits, 1 #"))) = ("SPS30: going to wait until stable"); &__c[0];}))))));
        }
        stateSPS30 = WAIT_STABLE;
      } else {
        wakeTimeSPS30 = (unsigned long) (lastSPS30 + intervalSPS30 - 50 - timeToStableSPS30);
        sps30_port->begin(sps30_i2c[0], sps30_i2c[1]);
        ret = sps30.sleep(); // 5ms execution time
        if (ret != 0x00) {
          if (dbglevel > 0) { Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "SPS30.ino" "." "245" "." "672" "\", \"aSM\", @progbits, 1 #"))) = ("SPS30: error, could not go to sleep"); &__c[0];})))))); }
          stateSPS30 = HAS_ERROR;
        } else {
          if (dbglevel == 5) { Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "SPS30.ino" "." "248" "." "673" "\", \"aSM\", @progbits, 1 #"))) = ("SPS30: going to sleep"); &__c[0];})))))); }
          stateSPS30 = IS_SLEEPING;
        }
      }
      break;
    }

    case IS_SLEEPING : { //---------------------
      if (dbglevel == 5) { Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "SPS30.ino" "." "256" "." "674" "\", \"aSM\", @progbits, 1 #"))) = ("SPS30: is sleepig"); &__c[0];})))))); }
      if (currentTime >= wakeTimeSPS30) { // Wake up if sleep time exceeded
        sps30_port->begin(sps30_i2c[0], sps30_i2c[1]);
        if (dbglevel == 5) { Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "SPS30.ino" "." "259" "." "675" "\", \"aSM\", @progbits, 1 #"))) = ("SPS30: waking up"); &__c[0];})))))); }
        ret = sps30.wakeup();
        if (ret != 0x00) {
          if (dbglevel > 0) { Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "SPS30.ino" "." "262" "." "676" "\", \"aSM\", @progbits, 1 #"))) = ("SPS30: error could not wakeup"); &__c[0];})))))); }
          stateSPS30 = HAS_ERROR;
        } else {
          wakeSPS30 = currentTime;
          stateSPS30 = IS_WAKINGUP;
        }
      } // time interval
      break;
    } // case is sleeping

    case IS_WAKINGUP : { // this is only used in energy saving / slow mode
      if (dbglevel == 5) { Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "SPS30.ino" "." "273" "." "677" "\", \"aSM\", @progbits, 1 #"))) = ("SPS30: is waking up"); &__c[0];})))))); }
      if ((currentTime - wakeSPS30) >= 50) { // Give some time (50ms)to wake up 
        sps30_port->begin(sps30_i2c[0], sps30_i2c[1]);
        ret = sps30.start();
        if (ret != 0x00) {
          if (dbglevel > 0) { Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "SPS30.ino" "." "278" "." "678" "\", \"aSM\", @progbits, 1 #"))) = ("SPS30: error, could not start SPS30 measurements"); &__c[0];})))))); }
          stateSPS30 = HAS_ERROR;
        } else {
          if (dbglevel == 5) { Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "SPS30.ino" "." "281" "." "679" "\", \"aSM\", @progbits, 1 #"))) = ("SPS30: started"); &__c[0];})))))); }
          stateSPS30 = IS_BUSY;
        }
      }
      break;
    }

    case HAS_ERROR : { //---------------------
      // What are we going to do about that? Reset
      if (dbglevel > 0) { Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "SPS30.ino" "." "290" "." "680" "\", \"aSM\", @progbits, 1 #"))) = ("SPS30: error, going to reset"); &__c[0];})))))); }
      sps30_port->begin(sps30_i2c[0], sps30_i2c[1]);
      ret = sps30.reset();
      if (ret != 0x00) {
        if (dbglevel > 0) { Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "SPS30.ino" "." "294" "." "681" "\", \"aSM\", @progbits, 1 #"))) = ("SPS30: error, Could not reset"); &__c[0];})))))); }
        stateSPS30 = HAS_ERROR;
        sps30_avail = false;
        success = false;
        break;
      }
      if (dbglevel == 5) { Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "SPS30.ino" "." "300" "." "682" "\", \"aSM\", @progbits, 1 #"))) = ("SPS30: reset successful"); &__c[0];})))))); }
      if (sps30.start() == true) {
        if (dbglevel == 5) { Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "SPS30.ino" "." "302" "." "683" "\", \"aSM\", @progbits, 1 #"))) = ("SPS30: measurement started"); &__c[0];})))))); }
        stateSPS30 = IS_BUSY;
      } else {
        if (dbglevel > 0) { Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "SPS30.ino" "." "305" "." "684" "\", \"aSM\", @progbits, 1 #"))) = ("SPS30: could not start measurement"); &__c[0];})))))); }
        stateSPS30 = HAS_ERROR;
        sps30_avail = false;
        success = false;
      }
      break;
    }

  } // end cases

  return success;
}

void sps30JSON(char *payload) {
  char qualityMessage1[16];
  char qualityMessage2[16];
  checkPM2(valSPS30.MassPM2, qualityMessage1, 15);
  checkPM10(valSPS30.MassPM10, qualityMessage2, 15);
  sprintf(payload, "{\"sps30\":{\"avail\":%s,\"PM1\":%4.1f,\"PM2\":%4.1f,\"PM4\":%4.1f,\"PM10\":%4.1f,\"nPM0\":%4.1f,\"nPM1\":%4.1f,\"nPM2\":%4.1f,\"nPM4\":%4.1f,\"nPM10\":%4.1f,\"PartSize\":%4.1f,\"PM2_airquality\":\"%s\",\"PM10_airquality\":\"%s\"}}",
                       sps30_avail ? "true" : "false",
                       valSPS30.MassPM1,
                       valSPS30.MassPM2,
                       valSPS30.MassPM4,
                       valSPS30.MassPM10,
                       valSPS30.NumPM0,
                       valSPS30.NumPM1,
                       valSPS30.NumPM2,
                       valSPS30.NumPM4,
                       valSPS30.NumPM10,
                       valSPS30.PartSize,
                       qualityMessage1,
                       qualityMessage2);
}
# 1 "d:\\GitHub\\ESP Playground\\AirQuality\\software\\Unified Software\\Sensi\\Weather.ino"
/*



// Name address for Open Weather Map API

const char* server = "api.openweathermap.org";



//



// Replace with your unique URL resource

const char* resource = "REPLACE_WITH_YOUR_URL_RESOURCE";



//http://api.openweathermap.org/data/2.5/weather?q=Tucson,US&APPID=e05a9231d55d12a90f7e9d7903218b3c

const char* resource = "/data/2.5/weather?q=Porto,pt&appid=bd939aa3d23ff33d3c8f5dd1";



const unsigned long HTTP_TIMEOUT = 10000;  // max respone time from server

const size_t MAX_CONTENT_SIZE = 512;       // max size of the HTTP response



byte mac[] = {0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED};





// The type of data that we want to extract from the page

struct clientData {

  char temp[8];

  char humidity[8];

};





  if(connect(server)) {

    if(sendRequest(server, resource) && skipResponseHeaders()) {

      clientData clientData;

      if(readReponseContent(&clientData)) {

        printclientData(&clientData);

      }

    }

  }



  bool connect(const char* hostName) {

  Serial.print("Connect to ");

  Serial.println(hostName);



  bool ok = client.connect(hostName, 80);



  Serial.println(ok ? "Connected" : "Connection Failed!");

  return ok;

}



// Send the HTTP GET request to the server

bool sendRequest(const char* host, const char* resource) {

  Serial.print("GET ");

  Serial.println(resource);



  client.print("GET ");

  client.print(resource);

  client.println(" HTTP/1.1");

  client.print("Host: ");

  client.println(host);

  client.println("Connection: close");

  client.println();



  return true;

}



// Skip HTTP headers so that we are at the beginning of the response's body

bool skipResponseHeaders() {

  // HTTP headers end with an empty line

  char endOfHeaders[] = "\r\n\r\n";



  client.setTimeout(HTTP_TIMEOUT);

  bool ok = client.find(endOfHeaders);



  if (!ok) {

    Serial.println("No response or invalid response!");

  }

  return ok;

}

*/
# 77 "d:\\GitHub\\ESP Playground\\AirQuality\\software\\Unified Software\\Sensi\\Weather.ino"
// Parse the JSON from the input string and extract the interesting values
// Here is the JSON we need to parse
/*{

  "coord": {

    "lon":-110.9265,

    "lat":32.2217},

  "weather":[{

    "id":800,

    "main":"Clear",

    "description":"clear sky",

    "icon":"01d"}],

  "base":"stations",

  "main":{

    "temp":301.12,

    "feels_like":299.79,

    "temp_min":298.47,

    "temp_max":303.62,

    "pressure":1013,

    "humidity":16

  },

  "visibility":10000,

  "wind":{

    "speed":0.45,

    "deg":134,

    "gust":1.79

  },

  "clouds":{

    "all":1

  },

  "dt":1623422840,

  "sys":{

    "type":2,

    "id":2007774,

    "country":"US",

    "sunrise":1623413799,

    "sunset":1623465025},

    "timezone":-25200,

    "id":5318313,

    "name":"Tucson",

    "cod":200

  }

*/
# 120 "d:\\GitHub\\ESP Playground\\AirQuality\\software\\Unified Software\\Sensi\\Weather.ino"
/*

bool readReponseContent(struct clientData* clientData) {

  // Compute optimal size of the JSON buffer according to what we need to parse.

  // See https://bblanchon.github.io/ArduinoJson/assistant/

  // https://arduinojson.org/v5/assistant/



  const size_t bufferSize =

      JSON_ARRAY_SIZE(1) + JSON_OBJECT_SIZE(1) + JSON_OBJECT_SIZE(2) + 

      JSON_OBJECT_SIZE(3) + JSON_OBJECT_SIZE(4) + JSON_OBJECT_SIZE(5) + 

      JSON_OBJECT_SIZE(6) + JSON_OBJECT_SIZE(13)  + 391



  DynamicJsonBuffer jsonBuffer(bufferSize);



  JsonObject& root = jsonBuffer.parseObject(client);



  if (!root.success()) {

    Serial.println("JSON parsing failed!");

    return false;

  }



  // Here were copy the strings we're interested in using to your struct data

  strcpy(clientData->temp, root["main"]["temp"]);

  strcpy(clientData->humidity, root["main"]["humidity"]);

  // It's not mandatory to make a copy, you could just use the pointers

  // Since, they are pointing inside the "content" buffer, so you need to make

  // sure it's still in memory when you read the string



  return true;

}



// Print the data extracted from the JSON

void printclientData(const struct clientData* clientData) {

  Serial.print("Temp = ");

  Serial.println(clientData->temp);

  Serial.print("Humidity = ");

  Serial.println(clientData->humidity);

}



*/
# 1 "d:\\GitHub\\ESP Playground\\AirQuality\\software\\Unified Software\\Sensi\\WebSocket.ino"
/******************************************************************************************************/
// Web Socket Server 
/******************************************************************************************************/
// https://www.hackster.io/s-wilson/nodemcu-home-weather-station-with-websocket-7c77a3
// https://www.mischianti.org/2020/12/21/websocket-on-arduino-esp8266-and-esp32-temperature-and-humidity-realtime-update-3/
// http://www.martyncurrey.com/esp8266-and-the-arduino-ide-part-10a-iot-website-temperature-and-humidity-monitor/

void updateWebSocket() {
  // Operation:

  switch(stateWebSocket) {

    case IS_WAITING : { //---------------------
      // just wait...
      if ((currentTime - lastWebSocket) >= 1000 /* We check if WiFi is connected every 1 sec, if disconnected we attempted reconnection every 1 sec*/) {
        lastWebSocket = currentTime;
        if (dbglevel == 3) { Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "WebSocket.ino" "." "17" "." "685" "\", \"aSM\", @progbits, 1 #"))) = ("WebSocket: is waiting for network to come up"); &__c[0];})))))); }
      }
       break;
    }

    case START_UP : { //---------------------
      if ((currentTime - lastWebSocket) >= 1000 /* We check if WiFi is connected every 1 sec, if disconnected we attempted reconnection every 1 sec*/) {
        lastWebSocket = currentTime;
        if (false) { Serial.println("DBG:STARTUP: WS"); }
        webSocket.begin(); // Start server
        webSocket.onEvent(webSocketEvent);
        // webSocket.setReconnectInterval(5000);;
        // webSocket.setAuthorization("user", "Password");        
        if (dbglevel > 0) { Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "WebSocket.ino" "." "30" "." "686" "\", \"aSM\", @progbits, 1 #"))) = ("WebSocket: server started"); &__c[0];})))))); }
        stateWebSocket = CHECK_CONNECTION;
        AllmaxUpdateWS = 0;
      }
      break;
    }

    case CHECK_CONNECTION : { //---------------------
      //if ((currentTime - lastWebSocket) >= intervalWiFi) {
      //  lastWebSocket = currentTime;
        webSocket.loop();
        if (webSocket.connectedClients(false) == 0) { ws_connected = false; }
      //}
      break;
    }

  } // end switch state
} // http

//--------------------------------------------------------Websocket Event----------------------------------------------------
void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t lenght) {

  switch (type) {
    case WStype_DISCONNECTED:
      if (dbglevel == 3) { Serial.printf("Websocket [%u] disconnected!\r\n", num); }
      webSocket.disconnect(num);
      if (webSocket.connectedClients(false) == 0) { ws_connected = false; }
      break;
    case WStype_CONNECTED:
      {
        IPAddress ip = webSocket.remoteIP(num);
        if (dbglevel == 3) { Serial.printf("Websocket [%u] connection from %d.%d.%d.%d url: %s\r\n", num, ip[0], ip[1], ip[2], ip[3], payload); }
        ws_connected = true;
        //ForceSendValues = 1;
        webSocket.sendTXT(num, "Connected");
      }
      break;
    case WStype_TEXT:
      if (dbglevel == 3) { Serial.printf("Websocket [%u] got text data: %s\r\n", num, payload); }
      break;
    case WStype_BIN:
      if (dbglevel == 3) {
        Serial.printf("Websocket [%u] got binary data length: %u\r\n", num, lenght);
        hexdump(payload, lenght, 16);
      }
      break;
    case WStype_ERROR:
      break;
    case WStype_FRAGMENT_TEXT_START:
      break;
    case WStype_FRAGMENT_BIN_START:
      break;
    case WStype_FRAGMENT:
      break;
    case WStype_FRAGMENT_FIN:
      break;
    case WStype_PING:
      if (dbglevel == 3) { Serial.println("Websocket got ping"); }
      break;
    case WStype_PONG:
      break;
    default:
      if (dbglevel == 3) { Serial.println("Websocket got pong"); }
      break;
  }
}

void hexdump(const void *mem, uint32_t len, uint8_t cols) {
    const uint8_t* src = (const uint8_t*) mem;
    Serial.printf("\r\n[HEXDUMP] Address: 0x%08X len: 0x%X (%d)", (ptrdiff_t)src, len, len);
    for(uint32_t i = 0; i < len; i++) {
        if(i % cols == 0) {
            Serial.printf("\r\n[0x%08X] 0x%08X: ", (ptrdiff_t)src, i);
        }
        Serial.printf("%02X ", *src);
        src++;
    }
    Serial.printf("\r\n");
}

void updateWebSocketMessage() {
    char payLoad[512];

    if (bme280NewDataWS) {
      bme680JSON(payLoad);
      webSocket.broadcastTXT(payLoad);
      bme280NewDataWS = false;
      if (dbglevel == 3) { Serial.printf("BME280 WebSocket data boradcasted, len: %u\r\n", strlen(payLoad));}
    }

    if (scd30NewDataWS) {
      scd30JSON(payLoad);
      webSocket.broadcastTXT(payLoad);
      scd30NewDataWS = false;
      if (dbglevel == 3) { Serial.printf("SCD30 WebSocket data broadcasted, len: %u\r\n", strlen(payLoad));}
    }

    if (sgp30NewDataWS) {
      sgp30JSON(payLoad);
      webSocket.broadcastTXT(payLoad);
      sgp30NewDataWS = false;
      if (dbglevel == 3) { Serial.printf("SGP30 WebSocket data broadcasted, len: %u\r\n", strlen(payLoad));}
    }

    if (sps30NewDataWS) {
      sps30JSON(payLoad);
      webSocket.broadcastTXT(payLoad);
      sps30NewDataWS = false;
      if (dbglevel == 3) { Serial.printf("SPS30 WebSocket data broadcasted, len: %u\r\n", strlen(payLoad));}
    }

    if (ccs811NewDataWS) {
      ccs811JSON(payLoad);
      webSocket.broadcastTXT(payLoad);
      ccs811NewDataWS = false;
      if (dbglevel == 3) { Serial.printf("CCS811 WebSocket data broadcasted, len: %u\r\n", strlen(payLoad));}
    }

    if (bme680NewDataWS) {
      bme680JSON(payLoad);
      webSocket.broadcastTXT(payLoad);
      bme680NewDataWS = false;
      if (dbglevel == 3) { Serial.printf("BME680 WebSocket data broadcasted, len: %u\r\n", strlen(payLoad));}
    }

    if (mlxNewDataWS) {
      mlxJSON(payLoad);
      webSocket.broadcastTXT(payLoad);
      mlxNewDataWS = false;
      if (dbglevel == 3) { Serial.printf("MLX WebSocket data broadcasted, len: %u\r\n", strlen(payLoad));}
    }

    if (timeNewDataWS) {
      timeJSON(payLoad);
      webSocket.broadcastTXT(payLoad);
      timeNewDataWS = false;
      if (dbglevel == 3) { Serial.printf("Time WebSocket data broadcasted, len: %u\r\n", strlen(payLoad));}
    }

    if (dateNewDataWS) {
      dateJSON(payLoad);
      webSocket.broadcastTXT(payLoad);
      dateNewDataWS = false;
      if (dbglevel == 3) { Serial.printf("Date WebSocket data broadcasted, len: %u\r\n", strlen(payLoad));}
    }
}
# 1 "d:\\GitHub\\ESP Playground\\AirQuality\\software\\Unified Software\\Sensi\\WiFi.ino"
/******************************************************************************************************/
// Initialize WiFi
/******************************************************************************************************/
void initializeWiFi() {
  if (WiFi.status() == WL_NO_SHIELD) {
    wifi_avail = false;
    stateWiFi = IS_WAITING;
    stateMQTT = IS_WAITING;
    stateMDNS = IS_WAITING;
    stateOTA = IS_WAITING;
    stateNTP = IS_WAITING;
    if (dbglevel > 0) { Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "WiFi.ino" "." "12" "." "687" "\", \"aSM\", @progbits, 1 #"))) = ("WiFi: is not available"); &__c[0];})))))); }
  } else {
    wifi_avail = true;
    if (dbglevel > 0) { Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "WiFi.ino" "." "15" "." "688" "\", \"aSM\", @progbits, 1 #"))) = ("WiFi: is available"); &__c[0];})))))); }
    if (dbglevel > 0) { Serial.print("Wifi: MAC: "); Serial.println(WiFi.macAddress()); }
    if (WiFi.status() == WL_CONNECTED) { WiFi.disconnect(); } // make sure we are not connected
    if (mySettings.useWiFi == true) {
      sprintf(hostName, "esp8266-%06x", ESP.getChipId());
      WiFi.hostname(hostName);
      WiFi.mode(WIFI_STA);
      stateWiFi = START_UP;
      stateMQTT = IS_WAITING;
      stateMDNS = IS_WAITING;
      stateOTA = IS_WAITING;
      stateNTP = IS_WAITING;
      if (dbglevel > 0) { Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "WiFi.ino" "." "27" "." "689" "\", \"aSM\", @progbits, 1 #"))) = ("WiFi: mode set to client"); &__c[0];})))))); }
    } else {
      wifi_avail = false;
      stateWiFi = IS_WAITING;
      WiFi.mode(WIFI_OFF);
      if (dbglevel > 0) { Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "WiFi.ino" "." "32" "." "690" "\", \"aSM\", @progbits, 1 #"))) = ("WiFi: turned off"); &__c[0];})))))); }
    }
    stateMQTT = IS_WAITING;
    stateMDNS = IS_WAITING;
    stateOTA = IS_WAITING;
    stateNTP = IS_WAITING;
    delay(50);
  }
} // init wifi

/******************************************************************************************************/
// Update WiFi
/******************************************************************************************************/
void updateWiFi() {

  switch(stateWiFi) {

    case IS_WAITING : { //---------------------
      break;
    }

    case START_UP : { //---------------------
      if (dbglevel == 3) { Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "WiFi.ino" "." "54" "." "691" "\", \"aSM\", @progbits, 1 #"))) = ("WiFi: setting up known APs"); &__c[0];})))))); }
      if (strlen(mySettings.pw1) > 0) { wifiMulti.addAP(mySettings.ssid1, mySettings.pw1); } else { wifiMulti.addAP(mySettings.ssid1); }
      if (strlen(mySettings.pw2) > 0) { wifiMulti.addAP(mySettings.ssid2, mySettings.pw2); } else { wifiMulti.addAP(mySettings.ssid2); }
      if (strlen(mySettings.pw3) > 0) { wifiMulti.addAP(mySettings.ssid3, mySettings.pw3); } else { wifiMulti.addAP(mySettings.ssid3); }
      stateWiFi = IS_CONNECTING;
      stateMQTT = IS_WAITING;
      stateMDNS = IS_WAITING;
      stateOTA = IS_WAITING;
      stateNTP = IS_WAITING;
      stateHTTP = IS_WAITING;
      stateHTTPUpdater = IS_WAITING;
      lastWiFi = currentTime;
      break;
    }

    case IS_CONNECTING : { //---------------------
      if ((currentTime - lastWiFi) >= 1000 /* We check if WiFi is connected every 1 sec, if disconnected we attempted reconnection every 1 sec*/) {
        if (wifiMulti.run() == WL_CONNECTED) {
          if (dbglevel == 3) { Serial.print(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "WiFi.ino" "." "72" "." "692" "\", \"aSM\", @progbits, 1 #"))) = ("WiFi: connected to: "); &__c[0];})))))); Serial.print(WiFi.SSID()); }
          if (dbglevel == 3) { Serial.print(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "WiFi.ino" "." "73" "." "693" "\", \"aSM\", @progbits, 1 #"))) = (" at IP address: "); &__c[0];})))))); Serial.println(WiFi.localIP()); }
          wifi_connected = true;
          stateWiFi = CHECK_CONNECTION;
          stateMQTT = START_UP;
          stateOTA = START_UP;
          stateNTP = START_UP;
          stateHTTP = START_UP;
          stateHTTPUpdater = START_UP;
          stateMDNS = START_UP;
          stateWebSocket = START_UP;
          randomSeed(micros()); // init random generator (time until connect is random)
          AllmaxUpdateWifi = 0;
        } else {
          if (dbglevel == 3) { Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "WiFi.ino" "." "86" "." "694" "\", \"aSM\", @progbits, 1 #"))) = ("WiFi: is searching for known AP"); &__c[0];})))))); }
          wifi_connected = false;
        }
        lastWiFi = currentTime;
      }

      break;
    } // end is connecting

    case CHECK_CONNECTION : { //---------------------
      if ((currentTime - lastWiFi) >= 1000 /* We check if WiFi is connected every 1 sec, if disconnected we attempted reconnection every 1 sec*/) {
        if (wifiMulti.run(5000 /* WiFi Connection attempt time out*/) != WL_CONNECTED) { // if connected returns status, does not seem to need frequent calls
          if (dbglevel > 0) { Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "WiFi.ino" "." "98" "." "695" "\", \"aSM\", @progbits, 1 #"))) = ("WiFi: is searching for known AP"); &__c[0];})))))); }
          wifi_connected = false;
        } else {
          if (dbglevel == 3) { Serial.print(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "WiFi.ino" "." "101" "." "696" "\", \"aSM\", @progbits, 1 #"))) = ("WiFi: remains connected to: "); &__c[0];})))))); Serial.print(WiFi.SSID()); }
          if (dbglevel == 3) { Serial.print(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "WiFi.ino" "." "102" "." "697" "\", \"aSM\", @progbits, 1 #"))) = (" at IP address: "); &__c[0];})))))); Serial.println(WiFi.localIP()); }
          wifi_connected = true;
        }
        lastWiFi = currentTime;
      }
      break;
    }

  } // end switch case
} // update wifi
# 1 "d:\\GitHub\\ESP Playground\\AirQuality\\software\\Unified Software\\Sensi\\mDNS.ino"
/******************************************************************************************************/
// MDNS
/******************************************************************************************************/
void updateMDNS() {

  switch(stateMDNS) {

    case IS_WAITING : { //---------------------
      // just wait...
      if ((currentTime - lastMDNS) >= 1000 /* We check if WiFi is connected every 1 sec, if disconnected we attempted reconnection every 1 sec*/) {
        if ((dbglevel == 3) && mySettings.usemDNS) { Serial.println(((reinterpret_cast<const __FlashStringHelper *>((__extension__({static const char __c[] __attribute__((__aligned__(4))) __attribute__((section( "\".irom0.pstr." "mDNS.ino" "." "11" "." "698" "\", \"aSM\", @progbits, 1 #"))) = ("MDNS: waiting for network to come up"); &__c[0];})))))); }
        lastMDNS = currentTime;
      }
      break;
    }

    case START_UP : { //---------------------
      if ((currentTime - lastMDNS) >= 1000 /* We check if WiFi is connected every 1 sec, if disconnected we attempted reconnection every 1 sec*/) {
        lastMDNS = currentTime;
        if (false) { Serial.println("DBG:STARTUP: MDNS"); }

        // mDNS responder setup
        // by default OTA service is starting mDNS, if OTA is enabled we need to skip begin and update
        bool mDNSsuccess = true;
        if (!mySettings.useOTA) {
          if (false) { Serial.println("DBG:STARTUP: MDNS HOSTNAME"); }
          mDNSsuccess = MDNS.begin(hostName);
          if (mDNSsuccess == false) {if (dbglevel > 0) { Serial.println("MDNS: error setting up MDNS responder"); }}
        } else {
          if (false) { Serial.println("DBG:STARTUP: MDNS WAIT IS RUNNING"); }
          if (MDNS.isRunning()) { // need to wait until OTA startedup mDNS otherwise addService crashes
            mDNSsuccess = true;
          } else {
            mDNSsuccess = false;
          }
        }

        if (mDNSsuccess) {
          stateMDNS = CHECK_CONNECTION;
          AllmaxUpdatemDNS = 0;

          // mDNS announce service for website
          if (bool(mySettings.useHTTP)) {
            if (false) { Serial.println("DBG:STARTUP: MDNS HTTP&WS ANNOUNCE"); }
            if (!MDNS.addService("http", "tcp", 80)) {
              if (dbglevel > 0) { Serial.println("MDNS: could not add service for tcp 80"); }
            }
            if (!MDNS.addService("ws", "tcp", 81)) {
              if (dbglevel > 0) { Serial.println("MDNS: could not add service for ws 81"); }
            }
          }

          if (dbglevel > 0) { Serial.println("MDNS: initialized"); }

          // query mDNS on network
          /*

          int n = MDNS.queryService("esp", "tcp"); // Send out query for esp tcp services

          if (dbglevel > 0) { Serial.println(F("MDNS: query completed")); }

          if (n == 0) {

            if (dbglevel > 0) { Serial.println(F("MDNS: no services found")); }

          } else {

            if (dbglevel > 0) {

              Serial.println(F("MDNS: service(s) found"));

              for (int i = 0; i < n; ++i) {

                // Print details for each service found

                Serial.print(F("MDNS: ")); Serial.print(i + 1); Serial.print(F(": ")); Serial.print(MDNS.hostname(i)); Serial.print(F(" (")); Serial.print(MDNS.IP(i)); Serial.print(F(":"));  Serial.print(MDNS.port(i)); Serial.println(F(")"));

              } // for all mDNS services

            } 

          } // end found mDNS announcements

          */
# 71 "d:\\GitHub\\ESP Playground\\AirQuality\\software\\Unified Software\\Sensi\\mDNS.ino"
        } // end mDNS
        if (false) { Serial.println("DBG:STARTUP: MDNS END"); }
      } // end time interval
      break;
    } // end startup

    case CHECK_CONNECTION : { //---------------------
      //if ((currentTime - lastMDNS) >= intervalWiFi) {
        if (mySettings.useOTA == false) { MDNS.update(); } // Update MDNS but only if OTA is not enabled, OTA uses mDNS by default
      //  lastMDNS = currentTime;
      //}
      break;
    }

  } // switch
} // update mDNS
