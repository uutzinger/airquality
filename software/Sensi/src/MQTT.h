/******************************************************************************************************/
// MQTT
/******************************************************************************************************/
#include <PubSubClient.h>                                  // MQTT publishing

#define MQTT_PORT         1883                             // default mqtt port
#define intervalMQTTFast  1000                             // MQTT pulbish and conenction checking every  1.0 secs
#define intervalMQTTSlow 60000                             //                                      every 60.0 secs
bool mqtt_connected = false;                               // is mqtt server connected?
bool mqtt_sent = false;                                    // did we publish data?
bool sendMQTTonce = true;                                  // send system constants to broker at beginning
unsigned long intervalMQTT = intervalMQTTFast;             // automatically set during setup
unsigned long lastMQTTPublish;                             // last time we published mqtt data
unsigned long lastMQTTStatusPublish;                       // last time we published mqtt sensor availability data
unsigned long lastMQTT;                                    // last time we checked if mqtt is connected
volatile WiFiStates stateMQTT = IS_WAITING;                // keeping track of MQTT state
void updateMQTT(void);
void updateMQTTMessage(void);
void updateMQTTpayload(char *payload, size_t len);
void mqttCallback(char* topic, uint8_t* payload, unsigned int len);
