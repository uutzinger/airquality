/******************************************************************************************************/
// Open Weather
/******************************************************************************************************/
// https://randomnerdtutorials.com/esp32-http-get-open-weather-map-thingspeak-arduino/
// https://www.instructables.com/Online-Weather-Display-Using-OpenWeatherMap/

#include "src/Config.h"
#include "src/Sensi.h"
#include "src/Weather.h"
#include "src/WiFi.h"
#include "src/Print.h"

unsigned long intervalWeather;
unsigned long lastWeather;
unsigned long weather_lastError;

bool weather_success = false;
bool weather_avail = true;                                 // do we have this sensor?
bool weatherNewData = false;                               // do we have new data
bool weatherNewDataWS = false;                             // do we have new data for websocket

volatile WiFiStates stateWeather = IS_WAITING;             // keeping track of MQTT state
clientData weatherData;

// External Variables
extern Settings      mySettings;   // Config
extern bool          fastMode;     // Sensi
extern unsigned long lastYield;    // Sensi
extern unsigned long currentTime;  // Sensi
extern char          tmpStr[256];  // Sensi

bool getWeatherData() {                                
  //client function to send/receive GET request data.
  WiFiClient weatherClient;
  HTTPClient http;

  // http://api.openweathermap.org/data/2.5/weather?q=Tucson,US&units=metric&APPID=replacewithyourapikey
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
  // { "weather": { "avail": true, "description":"clear sky", "T": 20.48, "Tmin": 18.21, "Tmax": 23.49, "p": 1017.0, "rH": 0, "ws":  4.6, "wd": 0, "v": 1080131584}} len: 159


  if (mySettings.debuglevel == 3) { R_printSerialTelnetLogln(F("Weather: start HTTP")); }
  http.useHTTP10(true);
  snprintf_P(tmpStr, sizeof(tmpStr), PSTR("http://api.openweathermap.org/data/2.5/weather?q=%s,%s&units=metric&APPID=%s"),
             mySettings.weatherCity,mySettings.weatherCountryCode,mySettings.weatherApiKey);
  http.begin(weatherClient, tmpStr);

  //printSerialTelnetLogln(F("Weather: get response"));
  //if (http.GET()>0) {
  //  String payload = http.getString();
  //} else {
  //  if (mySettings.debuglevel > 0) { printSerialTelnetLogln(F("Weather: can not obtain data")); }
  //  return false;
  //}


  if (mySettings.debuglevel == 3) { R_printSerialTelnetLogln(F("Weather: get response")); }
  if (http.GET()>0) { // server is responding
    if (mySettings.debuglevel == 3) { R_printSerialTelnetLogln(F("Weather: alloc JSON"));} 
    DynamicJsonDocument doc(JSONSIZE);
    if (mySettings.debuglevel == 3) { R_printSerialTelnetLogln(F("Weather: deserialize JSON")); }
    DeserializationError error = deserializeJson(doc, http.getStream());
    if (error) { 
      if (mySettings.debuglevel > 0) { R_printSerialTelnetLogln(F("Failed to parse weather JSON")); }
      http.end(); 
      return false;
    } else {
      if (mySettings.debuglevel == 3) { R_printSerialTelnetLogln(F("Weather: extract data")); }
      
      JsonObject weather = doc["weather"][0];
      strlcpy(weatherData.description, weather["description"] | "N.A.", sizeof(weatherData.description));
    
      JsonObject main = doc["main"];
      weatherData.temp     = float(main["temp"]);     // 25.73
      weatherData.tempMin  = float(main["temp_min"]); // 27.46
      weatherData.tempMax  = float(main["temp_max"]); // 27.46
      weatherData.pressure = int(main["pressure"]);   // 1014
      weatherData.humidity = int(main["humidity"]);   // 71

      JsonObject wind = doc["wind"];
      weatherData.windSpeed     = float(wind["speed"]); // 1.54
      weatherData.windDirection = int(wind["deg"]);     // 160    

      weatherData.visibility = int(doc["visibility"]);
    }
  } else {
    if (mySettings.debuglevel == 3) { R_printSerialTelnetLogln(F("Weather: free HTTP")); }
    http.end(); 
    return false;
  }
  if (mySettings.debuglevel == 3) { R_printSerialTelnetLogln(F("Weather: free HTTP")); }
  // Free resources
  http.end(); 
  return true;
}

/******************************************************************************************************/
// Initialize Weather
/******************************************************************************************************/

void initializeWeather(){
  // no initialization
  D_printSerialTelnet(F("D:U:Weather:IN.."));
  if (mySettings.debuglevel  > 0) { R_printSerialTelnetLogln(F("Weather: client initialized")); }
  delay(50); lastYield = millis();  
}

/******************************************************************************************************/
// Update Weather
/******************************************************************************************************/

void updateWeather() {
  switch(stateWeather) {
    
    case IS_WAITING : { //---------------------
      // just wait...
      if ((currentTime - lastWeather) >= intervalWiFi) {
        D_printSerialTelnet(F("D:U:Tel:IW.."));
        if ((mySettings.debuglevel == 3) && mySettings.useWeather) { R_printSerialTelnetLogln(F("Weather: waiting for network to come up")); }          
        lastWeather = currentTime;
      }
      break;
    }

    case START_UP : { //---------------------
      if ((currentTime - lastWeather) >= intervalWiFi) {
        D_printSerialTelnet(F("D:U:Weather:S.."));
        lastWeather = currentTime;
        stateWeather = CHECK_CONNECTION;        
      } // end time interval
      break;
    } // end startup

    case CHECK_CONNECTION : { //---------------------
      if ((currentTime - lastWeather) >= intervalWeather) {
        D_printSerialTelnet(F("D:U:Weather:CC.."));
        if ( getWeatherData() ) {
          weatherNewData   = true;
          weatherNewDataWS = true;
          weather_success = true;
        } else {
          if ((mySettings.debuglevel > 0) && mySettings.useWeather) { R_printSerialTelnetLogln(F("Weather: could not obtain data")); }
          weather_success = false;
          weather_lastError = currentTime;
        }
        lastWeather = currentTime; 
      }
      break;
    }

    default: {if (mySettings.debuglevel > 0) { R_printSerialTelnetLogln(F("Weather Error: invalid switch statement")); break;}}
  }
}

/******************************************************************************************************/
// JSON Weather
/******************************************************************************************************/

void weatherJSON(char *payLoad, size_t len){
  const char * str = "{ \"weather\": ";
  size_t l = strlen(str);
  strlcpy(payLoad, str, l+1);
  weatherJSONMQTT(payLoad+l, len-l-1);
  strlcat(payLoad, "}", len);
}

void weatherJSONMQTT(char *payload, size_t len) {
  snprintf_P(payload, len, PSTR("{ \"avail\": %s, \"description\":\"%s\", \"T\": %5.2f, \"Tmin\": %5.2f, \"Tmax\": %5.2f, \"p\": %d, \"rH\": %d, \"ws\": %4.1f, \"wd\": %d, \"v\": %d}"), 
            weather_success ? "true"                    : "false", 
            weather_success ? weatherData.description   : "N.A.",
            weather_success ? weatherData.temp          : -1.,
            weather_success ? weatherData.tempMin       : -1.,
            weather_success ? weatherData.tempMax       : -1.,
            weather_success ? weatherData.pressure      : -1,
            weather_success ? weatherData.humidity      : -1,
            weather_success ? weatherData.windSpeed     : -1.,
            weather_success ? weatherData.windDirection : -1,
            weather_success ? weatherData.visibility    : -1);
}
