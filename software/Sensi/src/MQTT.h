/******************************************************************************************************/
// MQTT
/******************************************************************************************************/
#ifndef MQTT_H_
#define MQTT_H_

#include <PubSubClient.h>                                  // MQTT publishing
#include <WiFiClient.h>

#define MQTT_PORT            1883                          // default mqtt port
#define intervalMQTTFast     1000                          // MQTT pulbish and conenction checking every  1 sec
#define intervalMQTTSlow    60000                          //                                      every 60 secs
#define intervalMQTTconnect 15000                          // time in between mqtt server connection attempts
#define mqttClientID       "Sensi" 

void initializeMQTT(void);
void updateMQTT(void);
void updateMQTTMessage(void);
void updateMQTTpayload(char *payload, size_t len);
void mqttCallback(char* topic, uint8_t* payload, unsigned int len);

#endif
