/******************************************************************************************************/
// Signal Analysis
/******************************************************************************************************/
// functions to check wether sensor values are in reasonable range
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