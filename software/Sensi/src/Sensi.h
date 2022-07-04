/******************************************************************************************************/
// Hardware Pin Configuration
/******************************************************************************************************/
#define SCD30_RDY D8                                       // normal low, active high pin indicating data ready from SCD30 sensor (raising)
#define CCS811_INT D7                                      // normal high, active low interrupt, high to low (falling) at end of measurement
#define CCS811_WAKE D0                                     // normal high, active low to wake up sensor, might need to pull low and high w resistor

/******************************************************************************************************/
// Sensor updates
/******************************************************************************************************/
bool fastMode = true;                                      // Frequent or in frequent sensor updates

/******************************************************************************************************/
// Out of Range and Error Handling
/******************************************************************************************************/
bool allGood = true;                                       // a sensor is outside recommended limits
bool lastLCDInten = true;                                  // dark or bright?
bool blinkLCD = false;                                     // blink lcd background ?
unsigned long lastBlink;                                   // last time we switched LCD intensity
unsigned long intervalBlink = 1000;                        // auto populated during execution
unsigned long lastWarning;                                 // last time we check the sensors to create a warning message
#define intervalWarning 60000                              // Check sensors every minute wether to create warning message/blinking
#define rebootHour 3                                       // what hour will the esp reboot if sensors have error and ntp is available

/******************************************************************************************************/
// Sensor
/******************************************************************************************************/
enum SensorStates{IS_IDLE = 0, IS_MEASURING, IS_BUSY, DATA_AVAILABLE, GET_BASELINE, IS_SLEEPING, IS_WAKINGUP, WAIT_STABLE, HAS_ERROR};
                                                           // IS_IDLE        the sensor is powered up
                                                           // IS_MEASURING   the sensor is creating data autonomously
                                                           // IS_BUSY        the sensor is producing data and will not respond to commands
                                                           // DATA_AVAILABLE new data is available in sensor registers
                                                           // IS_SLEEPING    the sensor or parts of the sensot are in sleep mode
                                                           // IS_WAKINGUP    the sensor is getting out of sleep mode
                                                           // WAIT_STABLE    readings are not stable yet    
                                                           // HAS_ERROR      the communication with the sensor failed


/// Baselines of certain sensors
unsigned long lastBaseline;                                // last time baseline was onbtained
unsigned long intervalBaseline;                            // how often do we obtain the baselines?
#define intervalBaselineSlow 3600000                       // 1 hour
#define intervalBaselineFast  600000                       // 10 minutes

/******************************************************************************************************/
// Time Keeping
/******************************************************************************************************/
//
unsigned long intervalSYS;                                 // rate at which system information (CPU usage, Heap size) is displayed on serial port
unsigned long currentTime;                                 // Populated at beginning of main loop
unsigned long lastcurrentTime;                             // to update system unix type time
unsigned long lastTime;                                    // last time we updated runtime
unsigned long intervalLoop;                                // determines desired frequency to update device states
unsigned long intervalRuntime;                             // how often do we update the uptime counter of the system
unsigned long lastSYS;                                     // determines desired frequency to update system information on serial port
unsigned long tmpTime;                                     // temporary variable to measures execution times
long          myDelay;                                     // main loop delay, automatically computed
long          myDelayMin = 1000;                           // minium delay over the last couple seconds, if less than 0, ESP8266 can not keep up with
float         myDelayAvg = 0;                              // average delay over the last couplle seconds
unsigned long myLoop;                                      // main loop execution time, automatically computed
unsigned long myLoopMin;                                   // main loop recent shortest execution time, automatically computed
unsigned long myLoopMax;                                   // main loop recent maximumal execution time, automatically computed
unsigned long myLoopMaxAllTime=0;                          // main loop all time maximum execution time, automatically computed
float         myLoopAvg = 0;                               // average delay over the last couple seconds
float         myLoopMaxAvg = 0;                            // average max loop delay over the last couple seconds
unsigned long startYield = 0;                              // keep record of time for Yield interval
unsigned long lastYield = 0;                               // keep record of time for Yield interval
unsigned long yieldTime = 0;
unsigned long yieldTimeMin = 1000;
unsigned long yieldTimeMax = 0;
unsigned long yieldTimeMaxAllTime=0;

bool          scheduleReboot = false;                      // is it time to reboot the ESP?
bool          rebootNow = false;                           // immediate reboot was requested
unsigned long startUpdate = 0;
unsigned long deltaUpdate = 0;
unsigned long maxUpdateTime = 0;
unsigned long maxUpdateWifi = 0;
unsigned long maxUpdateOTA = 0;
unsigned long maxUpdateNTP = 0;
unsigned long maxUpdateTelnet = 0;
unsigned long maxUpdateMQTT = 0;
unsigned long maxUpdateWS = 0;
unsigned long maxUpdateHTTP = 0;
unsigned long maxUpdateHTTPUPDATER = 0;
unsigned long maxUpdatemDNS = 0;
unsigned long maxUpdateSCD30 = 0;
unsigned long maxUpdateSGP30 = 0;
unsigned long maxUpdateCCS811 = 0;
unsigned long maxUpdateSPS30 = 0;
unsigned long maxUpdateBME280 = 0; 
unsigned long maxUpdateBME680 = 0; 
unsigned long maxUpdateMLX = 0; 
unsigned long maxUpdateMAX = 0;
unsigned long maxUpdateMQTTMESSAGE = 0; 
unsigned long maxUpdateWSMESSAGE = 0; 
unsigned long maxUpdateLCD = 0;
unsigned long maxUpdateINPUT = 0; 
unsigned long maxUpdateRT = 0;
unsigned long maxUpdateEEPROM = 0; 
//unsigned long maxUpdateJS = 0;
unsigned long maxUpdateBASE = 0;
unsigned long maxUpdateBLINK = 0;
unsigned long maxUpdateREBOOT = 0;
unsigned long maxUpdateALLGOOD = 0;
unsigned long maxUpdateSYS = 0;

unsigned long AllmaxUpdateTime = 0;
unsigned long AllmaxUpdateWifi = 0;
unsigned long AllmaxUpdateOTA = 0;
unsigned long AllmaxUpdateNTP = 0;
unsigned long AllmaxUpdateTelnet = 0;
unsigned long AllmaxUpdateMQTT = 0;
unsigned long AllmaxUpdateWS = 0;
unsigned long AllmaxUpdateHTTP = 0;
unsigned long AllmaxUpdateHTTPUPDATER = 0;
unsigned long AllmaxUpdatemDNS = 0;
unsigned long AllmaxUpdateSCD30 = 0;
unsigned long AllmaxUpdateSGP30 = 0;
unsigned long AllmaxUpdateCCS811 = 0;
unsigned long AllmaxUpdateSPS30 = 0;
unsigned long AllmaxUpdateBME280 = 0; 
unsigned long AllmaxUpdateBME680 = 0; 
unsigned long AllmaxUpdateMLX = 0; 
unsigned long AllmaxUpdateMAX = 0;
unsigned long AllmaxUpdateMQTTMESSAGE = 0; 
unsigned long AllmaxUpdateWSMESSAGE = 0; 
unsigned long AllmaxUpdateLCD = 0;
unsigned long AllmaxUpdateINPUT = 0; 
unsigned long AllmaxUpdateRT = 0;
unsigned long AllmaxUpdateEEPROM = 0; 
//unsigned long AllmaxUpdateJS = 0;
unsigned long AllmaxUpdateBASE = 0;
unsigned long AllmaxUpdateBLINK = 0;
unsigned long AllmaxUpdateREBOOT = 0;
unsigned long AllmaxUpdateALLGOOD = 0;
unsigned long AllmaxUpdateSYS = 0;

time_t actualTime;                                         // https://www.cplusplus.com/reference/ctime/time_t/
tm* localTime;                                             // https://www.cplusplus.com/reference/ctime/tm/
// Member	  Type	Meaning	                    Range
// tm_sec	  int	seconds after the minute	0-61*
// tm_min	  int	minutes after the hour	    0-59
// tm_hour	  int	hours since midnight	    0-23
// tm_mday	  int	day of the month	        1-31
// tm_mon	  int	months since January	    0-11
// tm_year	  int	years since 1900	
// tm_wday	  int	days since Sunday	        0-6
// tm_yday	  int	days since January 1	    0-365
// tm_isdst	  int	Daylight Saving Time flag	
const char* weekDays[7] PROGMEM = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
const char* months[12]  PROGMEM = {"January", "February", "March", "April", "May", "June", "July", "August", "September", "October", "November", "December"};
bool timeNewData   = false;                                // do we have new data mqtt
bool timeNewDataWS = false;                                // do we have new data for websocket
bool dateNewData   = false;                                // do we have new data mqtt
bool dateNewDataWS = false;                                // do we have new data websocket
bool ntpFirstTime  = true;
bool time_avail    = false;                                // do not display time until time is established

// i2c speed
#define I2C_FAST    100000                                 // Fast   400 000
#define I2C_REGULAR 100000                                 // Normal 100 000
#define I2C_SLOW     50000                                 // Slow    50 000

// Serial & Telnet
#define BAUDRATE 115200                                    // serial communicaiton speed, terminal settings need to match
char serialInputBuff[64];
bool serialReceived = false;                               // keep track of serial input
unsigned long lastSerialInput = 0;                         // keep track of serial flood
unsigned long lastTmp = 0;
#define SERIALMAXRATE 500                                  // milli secs between serial command inputs

//
char tmpStr[128];                                          // Buffer for formatting text that will be sent to USB serial or telnet

//https://arduino-esp8266.readthedocs.io/en/latest/PROGMEM.html
const char singleSeparator[] PROGMEM      = {"--------------------------------------------------------------------------------"};
const char doubleSeparator[] PROGMEM      = {"================================================================================"};
const char mON[] PROGMEM                  = {"on"};
const char mOFF[] PROGMEM                 = {"off"};
const char clearHome[] PROGMEM            = {"\e[2J\e[H\r\n"};
const char waitmsg[] PROGMEM              = {"Waiting 10 seconds, skip by hitting enter"};      // Allows user to open serial terminal to observe the debug output before the loop starts

/******************************************************************************************************/
// LittleFS
/******************************************************************************************************/
#include <LittleFS.h>
LittleFSConfig fileSystemConfig = LittleFSConfig();
bool fsOK = false;
#define LOGFILESIZE 100000
#define INTERVALLOGFILE 5000
File logFile;
unsigned long lastLogFile; // last time we checked logfile size or closes the logfile

/******************************************************************************************************/
// Support Functions
/******************************************************************************************************/
#include <Wire.h>
bool checkI2C(uint8_t address, TwoWire *myWire);                   // is device attached at this address?
void switchI2C(TwoWire *myPort, int sdaPin, int sclPin, uint32_t i2cSpeed);
void serialTrigger(const char* mess, int timeout);              // delays until user input
bool inputHandle(void);                                         // hanldes user input
void helpMenu(void);                                            // lists user functions
void printSettings(void);                                       // lists program settings
void defaultSettings(void);                                     // converts settings back to default. need to save to EEPROM to retain
void printSensors(void);                                        // lists current sensor values
void printState(void);                                          // lists states of system devices and sensors
void timeJSON(char *payload);                                   // provide time
void dateJSON(char *payload);                                   // provide date
void systemJSON(char *payload);                                 // provide system stats
void ipJSON(char *payload);                                     // provide ip
void hostnameJSON(char *payload);                               // provide hostname
void max30JSON(char *payload);                                  // provide max data, temporarily
unsigned long maxyieldOS(void);                                             // execute yield or delay
