/******************************************************************************************************/
// MQTT
/******************************************************************************************************/
#include <PubSubClient.h>                                  // MQTT publishing

#define MQTT_PORT         1883                             // default mqtt port
#define intervalMQTTFast   500                             // check for MQTT conenction and incoming data every 500ms
#define intervalMQTTSlow  5000                             //                                             every 5 secs
bool mqtt_connected = false;                               // is mqtt server connected?
bool mqtt_sent = false;                                    // did we publish data?
bool sendMQTTonce = true;                                  // send system constants to broker at beginning
unsigned long intervalMQTT = intervalMQTTFast;             // automatically set during setup
unsigned long lastMQTTPublish;                             // last tie we published mqtt data
unsigned long lastMQTT;                                    // last time we checked if mqtt is connected
volatile WiFiStates stateMQTT       = IS_WAITING;          // keeping track of MQTT state
void updateMQTT(void);
void updateMQTTMessage(void);
void updateMQTTpayload(char *payload);
void mqttCallback(char* topic, byte* payload, unsigned int len);
