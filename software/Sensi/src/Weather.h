/******************************************************************************************************/
// Open Weather
/******************************************************************************************************/
#ifndef WEATHER_H_
#define WEATHER_H_

#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClient.h>

// 1000 api calls per day free, a day has 86400 secs
// 1000000 calls per month 31*24*60*60 = 2678400 secs = 2.7 secs
#define intervalWeatherFast          100000                // 100 sec  
#define intervalWeatherSlow         3600000                // 1 hr

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

void initializeWeather(void);
void updateWeather(void);
bool getWeatherData(void);                                

void weatherJSON(char *payload, size_t len);             // convert readings to serialzied JSON
void weatherJSONMQTT(char *payload, size_t len);        // convert readings to serialzied JSON

#endif
