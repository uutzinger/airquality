//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Weather                                                                                                            //
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClient.h>

/************************************************************************************************************************************/
// Build Configuration
/************************************************************************************************************************************/
#define VER "1.0.0"

#define FASTMODE true

// 1000 api calls per day free, a day has 86400 secs
#define intervalWeatherFast          100000                // 100 sec  
#define intervalWeatherSlow         3600000                // 1 hr

// yieldOS()
#define MAXUNTILYIELD 50            // time in ms until yield should be called
#define yieldFunction() delay(1)    // options are delay(0) delay(1) yield() and empty for no yield)

char tmpStr[256];  // universal buffer for formatting texT

bool fastMode = FASTMODE;                                      // Frequent or in frequent sensor updates

unsigned long lastYield = 0;                               // keep record of time of last Yield event
unsigned long yieldTime = 0;
unsigned long yieldTimeMin = 1000;
unsigned long yieldTimeMax = 0;
unsigned long yieldTimeMaxAllTime=0;

unsigned long startUpdate = 0;
unsigned long deltaUpdate = 0;
unsigned long maxUpdateTime = 0;
unsigned long maxUpdateWeather = 0;
unsigned long AllmaxUpdateWeather = 0;

const char    waitmsg[] PROGMEM         = {"Waiting 10 seconds, skip by hitting enter"};      // Allows user to open serial terminal to observe the debug output before the loop starts

// The type of data that we want to extract from the page
struct clientData {
  float temp;
  float tempMin;
  float tempMax;
  int pressure;
  int humidity;
  float windSpeed;
  int windDirection;
  int visibility;
  char description[64];
};


/************************************************************************************************************************************/
// Global Variables
/************************************************************************************************************************************/

bool          weather_avail;
clientData    weatherData;

unsigned long intervalWeather;
unsigned long lastWeather;

bool getWeatherData() {                                
  //client function to send/receive GET request data.
  WiFiClient weatherClient;
  HTTPClient http;

  // http://api.openweathermap.org/data/2.5/weather?q=Tucson,US&units=metric&APPID=e05a9231d55d12a90f7e9d7903218b3c
  // {"coord":{"lon":-110.9265,"lat":32.2217},
  //  "weather":[{"id":800,"main":"Clear","description":"clear sky","icon":"01d"}],
  //  "base":"stations",
  //  "main":{"temp":25.73,"feels_like":26.21,"temp_min":24.03,"temp_max":27.46,"pressure":1014,"humidity":71},
  //  "visibility":10000,
  //  "wind":{"speed":1.54,"deg":160},
  //  "clouds":{"all":0},
  //  "dt":1662824394,
  //  "sys":{"type":2,"id":2007774,"country":"US","sunrise":1662815078,"sunset":1662860221},
  //  "timezone":-25200,
  //  "id":5318313,
  //  "name":"Tucson",
  //  "cod":200
  // }

  http.useHTTP10(true);
  snprintf_P(tmpStr, sizeof(tmpStr), PSTR("http://api.openweathermap.org/data/2.5/weather?q=%s,%s&units=metric&APPID=%s"),
             mySettings.weatherCity,mySettings.weatherCountryCode,mySettings.weatherApiKey);
  http.begin(weatherClient, tmpStr);

  if (http.GET()>0) {
    String payload = http.getString();
  } else {
    if (mySettings.debuglevel > 0) { printSerialTelnetLogln(F("Weather: can not obtain data")); }
    return false;
  }
  
  DynamicJsonDocument doc(JSONSIZE);
  DeserializationError error = deserializeJson(doc, http.getStream());
  if (error) { 
    printSerialTelnetLogln(F("Failed to parse weather JSON"));
    return false;
  }

  // Free resources
  http.end();
  
  JsonObject weather = doc["weather"][0];
  strlcpy(weatherData.description, weather["description"] | "N.A.", sizeof(weatherData.description));

  JsonObject main = doc["main"];
  weatherData.temp     = main["temp"];     // 25.73
  weatherData.tempMin  = main["temp_min"]; // 27.46
  weatherData.tempMax  = main["temp_max"]; // 27.46
  weatherData.pressure = main["pressure"]; // 1014
  weatherData.humidity = main["humidity"]; // 71  }
  
  weatherData.windSpeed     = doc["wind"]["speed"]; // 1.54
  weatherData.windDirection = doc["wind"]["deg"]; // 160
  
  return true;
}

void weatherJSON(char *payload, size_t len) {
  snprintf_P(payload, len, PSTR("{ \"weather\": { \"avail\": %s, \"description\":\"%s\", \"T\": %5.2f, \"Tmin\": %5.2f, \"Tmax\": %5.2f, \"p\": %5.1f, \"rH\": %d, \"ws\": %4.1f, \"wd\": %d, \"v\": %d}}"), 
            weather_success ? "true" : "false", 
            weather_success ? weatherData.description : "N.A.",
            weather_success ? weatherData.temp  : -1.,
            weather_success ? weatherData.tempMin  : -1.,
            weather_success ? weatherData.tempMax  : -1.,
            weather_success ? weatherData.pressure  : -1.,
            weather_success ? weatherData.humidity : -1.,
            weather_success ? weatherData.windSpeed  : -1.,
            weather_success ? weatherData.windDirection  : -1.,
            weather_success ? weatherData.visibility  : -1.);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// SETUP
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void setup() { 
  fastMode = FASTMODE;

  Serial.begin(BAUDRATE);               // open serial port at specified baud rate
  Serial.setTimeout(1000);              // default is 1000 
  
  // give user time to obeserve boot information
  serialTrigger(waitmsg, 10000);
  
  /************************************************************************************************************************************/
  // Time Intervals for Loop, Display and MQTT publishing, keeping track of runtime
  /************************************************************************************************************************************/
  // Software is designed to run either with frequent updates or in slow mode with updates in the minutes

  // Updaterate of devices is defined in their respective .h files
  if (fastMode == true) {                                     // fast mode -----------------------------------
    intervalLoop     =                  100;                  // 0.1 sec, main loop runs 10 times per second
    intervalWeather  = intervalWeatherFast;                   // 1.5 minutes
  } else {                                                    // slow mode -----------------------------------
    intervalLoop     =                 1000;                  // 1 sec, main loop runs once a second, ESP needs main loop to run faster than 100ms otherwise yield is needed
    intervalWeather  = intervalWeatherSlow;                   // 1 hr
  }

  initializeWeather();

  R_printSerialTelnetLogln(F("System initialized")); }
  
  /************************************************************************************************************************************/
  // Initialize Timing System
  /************************************************************************************************************************************/
  currentTime           = millis();
  lastcurrentTime       = currentTime;
  lastTime              = currentTime;
  lastWeather           = currentTime;
    
} // end setup

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Main Loop
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void loop() {
  currentTime = lastYield = millis();                   // keep track of loop time
  
    
    /**********************************************************************************************************************************/
    // Update the State Machines for all Devices and System Processes
    /**********************************************************************************************************************************/
    // Update Wireless Services 
    if (wifi_avail               && mySettings.useWiFi) { 
      startUpdate = millis();
      updateWiFi(); //<<<<<<<<<<<<<<<<<<<<--------------- WiFi update, this will reconnect if disconnected
      deltaUpdate = millis() - startUpdate;
      if (maxUpdateWifi    < deltaUpdate) { maxUpdateWifi = deltaUpdate; }
      if (AllmaxUpdateWifi < deltaUpdate) { AllmaxUpdateWifi = deltaUpdate; }
      yieldTime += yieldOS(); 
    } 
    if (wifi_avail                && mySettings.useWiFi && mySettings.useWeather)    { 
      startUpdate = millis();
      updateWeather();  //<<<<<<<<<<<<<<<<<<<<------------ Update Telnet -----------------------------------
      deltaUpdate = millis() - startUpdate;
      if (maxUpdateWeather    < deltaUpdate) { maxUpdateWeather = deltaUpdate; }
      if (AllmaxUpdateWeather < deltaUpdate) { AllmaxUpdateWeather = deltaUpdate; }
      yieldTime += yieldOS(); 
    }
    
} // end loop

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Support functions
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// yield when necessary, there should be a yield/delay every 100ms
unsigned long yieldOS() {
  unsigned long beginYieldOS = millis(); // measure how long it takes to complete yield
  if ( (beginYieldOS - lastYield) > MAXUNTILYIELD ) { // only yield if sufficient time expired since previous yield
    yieldFunction(); // replaced with define statement at beginning of the this document
    lastYield = millis();
    return (lastYield - beginYieldOS);
  }
  return (millis()-beginYieldOS);
}

// Boot helper
// Prints message and waits until timeout or user send character on serial terminal
void serialTrigger(const char* mess, int timeout) {
  unsigned long startTime = millis();
  bool clearSerial = true;
  Serial.println(); Serial.println(mess);
  while ( !Serial.available() && (millis() - startTime < timeout) ) {
    delay(1000);
  }
  while (Serial.available()) { Serial.read(); }
}

