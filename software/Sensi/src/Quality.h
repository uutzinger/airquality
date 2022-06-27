/******************************************************************************************************/
// Signal Analysis
/******************************************************************************************************/
// functions to check whether sensor values are in reasonable range
//https://arduino-esp8266.readthedocs.io/en/latest/PROGMEM.html
const char PROGMEM normal1[]         = {"N"};
const char PROGMEM threshold1[]      = {"T"};
const char PROGMEM poor1[]           = {"P"};
const char PROGMEM excessive1[]      = {"!"};
const char PROGMEM high1[]           = {"H"};
const char PROGMEM low1[]            = {"L"};
const char PROGMEM hot1[]            = {"H"};
const char PROGMEM warm1[]           = {"h"};
const char PROGMEM coldish1[]        = {"c"};
const char PROGMEM cold1[]           = {"C"};
const char PROGMEM fever1[]          = {"F"};
const char PROGMEM excessiveFever1[] = {"!"};
const char PROGMEM thresholdLow1[]   = {"l"};
const char PROGMEM thresholdHigh1[]  = {"h"};

const char PROGMEM normal4[]         = {"nrm "};
const char PROGMEM threshold4[]      = {"thr "};
const char PROGMEM poor4[]           = {"poor"};
const char PROGMEM excessive4[]      = {"!   "};
const char PROGMEM high4[]           = {"high"};
const char PROGMEM low4[]            = {"low "};
const char PROGMEM hot4[]            = {"hot "};
const char PROGMEM warm4[]           = {"warm"};
const char PROGMEM cold4[]           = {"!col"};
const char PROGMEM coldish4[]        = {"cold"};
const char PROGMEM fever4[]          = {"fev "};
const char PROGMEM excessiveFever4[] = {"!fev"};
const char PROGMEM thresholdLow4[]   = {"tL  "};
const char PROGMEM thresholdHigh4[]  = {"tH  "};

const char PROGMEM normalF[]         = {"Normal"};
const char PROGMEM thresholdF[]      = {"Threshold"};
const char PROGMEM poorF[]           = {"Poor"};
const char PROGMEM excessiveF[]      = {"Excessive"};
const char PROGMEM highF[]           = {"High"};
const char PROGMEM lowF[]            = {"Low"};
const char PROGMEM hotF[]            = {"Hot"};
const char PROGMEM warmF[]           = {"Warm"};
const char PROGMEM coldF[]           = {"Very Cold"};
const char PROGMEM coldishF[]        = {"Cold"};
const char PROGMEM feverF[]          = {"Fever"};
const char PROGMEM excessiveFeverF[] = {"Excessive Fever"};
const char PROGMEM thresholdLowF[]   = {"Threshold Low"};
const char PROGMEM thresholdHighF[]  = {"Threshold High"};

bool checkCO2(float co2, char *message, int len);
bool checkHumidity(float rH, char *message, int len);
bool checkGasResistance(uint32_t res, char *message, int len);
bool checkTVOC(float tVOC, char *message, int len);
bool checkPM2(float PM2, char *message, int len);
bool checkPM10(float PM10, char *message, int len);
bool checkPM(float PM2, float PM10,  char *message, int len);
bool checkFever(float T, char *message, int len);
bool checkAmbientTemperature(float T, char *message, int len);
bool checkdP(float dP, char *message, int len);
bool sensorsWarning(void);
