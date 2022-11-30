/******************************************************************************************************/
// MQTT
/******************************************************************************************************/
#include "src/MQTT.h"
#include "src/WiFi.h"
#include "src/Sensi.h"
#include "src/Config.h"
#include "src/BME280.h"
#include "src/BME68x.h"
#include "src/CCS811.h"
#include "src/MLX.h"
#include "src/SCD30.h"
#include "src/SGP30.h"
#include "src/SPS30.h"
#include "src/Weather.h"
#include "src/Print.h"

bool          mqtt_connected = false;                      // is mqtt server connected?
bool          mqtt_sent = false;                           // did we publish data?
bool          mqtt_useregular = true;                      // want to be on regular or fallback
bool          mqtt_onfallback = true;                      // did fallback connect?
bool          mqtt_onregular = true;                       // did regular connect?
bool          availNewData = true;                         // 
bool          availNewDataHandeled = false;                //
bool          intervalNewData = true;                      // 
bool          intervalNewDataHandeled = false;
bool          systemNewDataHandeled = false;               // update system status on mqtt
unsigned long intervalMQTTreconnect = intervalMQTTconnect; //
unsigned long intervalMQTT = intervalMQTTSlow;             // automatically set during setup
unsigned long lastMQTTPublish;                             // last time we published mqtt data
unsigned long lastMQTTStatusPublish;                       // last time we published mqtt sensor availability data
unsigned long lastMQTT;                                    // last time we checked if mqtt is connected

volatile WiFiStates stateMQTT = IS_WAITING;                // keeping track of MQTT state
WiFiClient    mqttWiFiClient;                              // The WiFi interface
PubSubClient  mqttClient(mqttWiFiClient);                  // The MQTT interface

// Extern variables
extern unsigned long yieldTime;        // Sensi
extern unsigned long lastYield;        // Sensi
extern unsigned long AllmaxUpdateMQTT; // Sensi
extern Settings      mySettings;       // Config
extern unsigned long currentTime;      // Sensi
extern char          tmpStr[256];           // Sensi

extern bool          bme280NewData;
extern bool          bme280NewDataHandeled;
extern bool          bme280_avail;        // bme68x
extern float         bme280_pressure;
extern float         bme280_pressure24hrs;
extern unsigned long intervalBME280;
extern float         bme280_hum;
extern float         bme280_ah;
extern float         bme280_temp;

extern bool          bme68x_avail;        // bm680
extern bme68xData    bme68x; 
extern bool          bme68xNewData;
extern bool          bme68xNewDataHandeled;
extern Bme68x        bme68xSensor; 
extern float         bme68x_pressure24hrs;//
extern unsigned long intervalBME68x;
extern float         bme68x_ah;

extern bool          scd30_avail;         // scd30
extern bool          scd30NewData;
extern bool          scd30NewDataHandeled;
extern bool          scd30NewDataHandeled;
extern unsigned long intervalSCD30;
extern uint16_t      scd30_ppm;
extern float         scd30_hum;
extern float         scd30_temp;

extern bool          ccs811_avail;        // ccs811
extern bool          ccs811NewData;
extern bool          ccs811NewDataHandeled;
extern CCS811        ccs811;      
extern uint8_t       ccs811Mode;

extern bool          sgp30_avail;         // sgp30
extern bool          sgp30NewData;
extern bool          sgp30NewDataHandeled;
extern unsigned long intervalSGP30; 
extern SGP30         sgp30;

extern bool          sps30_avail;         // sps30
extern bool          sps30NewData;
extern bool          sps30NewDataHandeled;
extern unsigned long intervalSPS30;
extern sps30_measurement valSPS30;

extern bool          therm_avail;         // MLX
extern bool          mlxNewData;
extern bool          mlxNewDataHandeled;
extern unsigned long intervalMLX;
extern IRTherm       therm;
extern float         mlxOffset;

extern bool          weather_avail;       // weather
extern bool          weatherNewData;
extern bool          weatherNewDataHandeled;                       // have we handeled the new data?

extern clientData    weatherData;
extern volatile WiFiStates stateWeather;

extern bool          timeNewData;         // Sensi
extern bool          dateNewData;
extern bool          max30NewData;
extern bool          time_avail;
extern tm           *localTime; 
extern bool          ntp_avail;

extern unsigned long intervalLCD;
extern bool          lcd_avail;

/******************************************************************************************************/
// Initialize MQTT
/******************************************************************************************************/

void initializeMQTT() {    
  D_printSerialTelnet(F("D:U:MQTT:IN.."));
  mqttClient.setCallback(mqttCallback);                             // start listener
  delay(50); lastYield = millis();
} // init MQTT

/******************************************************************************************************/
// Update MQTT
/******************************************************************************************************/

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
      if ((currentTime - lastMQTT) >= intervalWiFi) {
        D_printSerialTelnet(F("D:U:MQTT:IW.."));
        if (mySettings.debuglevel == 3) { R_printSerialTelnetLogln(F("MQTT: is waiting for network to come up")); }          
        lastMQTT = currentTime;
      }
      break;
    }
    
    case START_UP : { //---------------------
      if ((currentTime - lastMQTT) >= intervalMQTTreconnect) {
        D_printSerialTelnet(F("D:U:MQTT:S.."));
        lastMQTT = currentTime;
        
        if ( mqtt_useregular ) {
          mqttClient.setServer(mySettings.mqtt_server, MQTT_PORT);          // start connection to server
          if (mySettings.debuglevel > 0) { 
            snprintf_P(tmpStr, sizeof(tmpStr), PSTR("Connecting to MQTT: %s"), mySettings.mqtt_server); 
            R_printSerialTelnetLogln(tmpStr); 
          }
          mqtt_connected = false;
          // connect to server with or without username password
          if (strlen(mySettings.mqtt_username) > 0)   { mqtt_connected = mqttClient.connect(mqttClientID, mySettings.mqtt_username, mySettings.mqtt_password); } // crashes here
          else                                        { mqtt_connected = mqttClient.connect(mqttClientID); }
          if ( mqtt_connected ) { mqtt_onregular = true; } else { mqtt_onregular = false; }
        } else {
          mqttClient.setServer(mySettings.mqtt_fallback, MQTT_PORT);
          if (mySettings.debuglevel > 0) { 
            snprintf_P(tmpStr, sizeof(tmpStr), "Connecting to MQTT: %s", mySettings.mqtt_fallback); 
            R_printSerialTelnetLogln(tmpStr);
          }
          if (strlen(mySettings.mqtt_username) > 0) { mqtt_connected = mqttClient.connect(mqttClientID, mySettings.mqtt_username, mySettings.mqtt_password); }
          else                                      { mqtt_connected = mqttClient.connect(mqttClientID); }          
          if ( mqtt_connected ) { mqtt_onfallback = true; } else { mqtt_onfallback = false; }
 
        }

        // go slower if both server can not be reached
        if ( (mqtt_onregular == false) && (mqtt_onfallback == false) ) {
          intervalMQTTreconnect = intervalMQTTconnect * 4;
        } else {
          intervalMQTTreconnect = intervalMQTTconnect;
        } 

        if (mqtt_connected == true) {
          // publish connection status
          char MQTTtopicStr[64];                                     // String allocated for MQTT topic 
          snprintf_P(MQTTtopicStr,sizeof(MQTTtopicStr),PSTR("%s/status"),mySettings.mqtt_mainTopic);
          mqttClient.publish(MQTTtopicStr, "up");
          if (mySettings.debuglevel > 0) { R_printSerialTelnetLogln(F("MQTT: connected successfully")); }
          stateMQTT = CHECK_CONNECTION;  // advance to connection monitoring and publishing
          AllmaxUpdateMQTT = 0;
        } else {  
          // connection failed, switch server
          mqtt_useregular = !mqtt_useregular;
          if (mySettings.debuglevel > 0) { R_printSerialTelnetLogln(F("MQTT: conenction attempt failed, switching to fallback")); }
        } // failed connectig, repeat starting up
      } //interval time
      break;
    }
    
    case CHECK_CONNECTION : { //---------------------
      //if ((currentTime - lastMQTT) >= intervalMQTT) {
      //  D_printSerialTelnet(F("D:U:MQTT:CC.."));
        lastMQTT = currentTime;
        if (mqttClient.loop() == false) {
          mqtt_connected = false;
          stateMQTT = START_UP;
          if (mySettings.debuglevel == 3) { R_printSerialTelnetLogln(F("MQTT: is disconnected. Reconnecting to server")); }
        }
      //}
      break;          
    }

    default: {if (mySettings.debuglevel > 0) { R_printSerialTelnetLogln(F("MQTT Error: invalid switch statement")); break;}}
          
  } // end switch state
}

/******************************************************************************************************/
// Update MQTT Message
/******************************************************************************************************/
void updateMQTTMessage() {
  char MQTTtopicStr[64];
  mqtt_sent = false;

  if ( (currentTime - lastMQTTPublish) > intervalMQTT ) { // wait for interval time, sending lots of MQTT messages breaks the software
    lastMQTTPublish = currentTime;

    // --------------------- this sends new data to mqtt server as soon as its available ----------------
    if (mySettings.sendMQTTimmediate) {
      
      char MQTTpayloadStr[512]; 

      // might need to limit system status update also
      if (!systemNewDataHandeled)  {
        snprintf_P(MQTTtopicStr, sizeof(MQTTtopicStr),PSTR("%s/status/system"),mySettings.mqtt_mainTopic);
        systemJSONMQTT(MQTTpayloadStr, sizeof(MQTTpayloadStr));
        mqttClient.publish(MQTTtopicStr, MQTTpayloadStr);
        mqtt_sent = true;
        systemNewDataHandeled = true;
        yieldTime += yieldOS(); 
          
      } else if (scd30NewData && !scd30NewDataHandeled )  {
        snprintf_P(MQTTtopicStr, sizeof(MQTTtopicStr),PSTR("%s/data/scd30"),mySettings.mqtt_mainTopic);
        scd30JSONMQTT(MQTTpayloadStr, sizeof(MQTTpayloadStr));
        mqttClient.publish(MQTTtopicStr, MQTTpayloadStr);
        scd30NewData = false;
        if (mySettings.debuglevel == 3) { R_printSerialTelnetLogln(F("SCD30 MQTT updated")); }
        mqtt_sent = true;
        scd30NewDataHandeled = true;
        yieldTime += yieldOS(); 
    
      } else if (sgp30NewData && !sgp30NewDataHandeled) {
        snprintf_P(MQTTtopicStr, sizeof(MQTTtopicStr),PSTR("%s/data/sgp30"),mySettings.mqtt_mainTopic);
        sgp30JSONMQTT(MQTTpayloadStr, sizeof(MQTTpayloadStr));
        mqttClient.publish(MQTTtopicStr, MQTTpayloadStr);
        sgp30NewData = false;
        if (mySettings.debuglevel == 3) { R_printSerialTelnetLogln(F("SGP30 MQTT updated")); }      
        mqtt_sent = true;
        sgp30NewDataHandeled = true;
        yieldTime += yieldOS(); 

      } else if (sps30NewData && !sps30NewDataHandeled) {
        snprintf_P(MQTTtopicStr, sizeof(MQTTtopicStr),PSTR("%s/data/sps30"),mySettings.mqtt_mainTopic);
        sps30JSONMQTT(MQTTpayloadStr, sizeof(MQTTpayloadStr));
        mqttClient.publish(MQTTtopicStr, MQTTpayloadStr);
        sps30NewData = false;
        if (mySettings.debuglevel == 3) { R_printSerialTelnetLogln(F("SPS30 MQTT updated")); }      
        mqtt_sent = true;
        sps30NewDataHandeled = true;
        yieldTime += yieldOS(); 
      
      } else if (ccs811NewData && !ccs811NewDataHandeled) {
        snprintf_P(MQTTtopicStr, sizeof(MQTTtopicStr),PSTR("%s/data/ccs811"),mySettings.mqtt_mainTopic);
        ccs811JSONMQTT(MQTTpayloadStr, sizeof(MQTTpayloadStr));
        mqttClient.publish(MQTTtopicStr, MQTTpayloadStr);
        ccs811NewData = false;
        if (mySettings.debuglevel == 3) { R_printSerialTelnetLogln(F("CCS811 MQTT updated")); }      
        mqtt_sent = true;
        ccs811NewDataHandeled = true;
        yieldTime += yieldOS(); 

      } else if (bme68xNewData && !bme68xNewDataHandeled) {
        snprintf_P(MQTTtopicStr, sizeof(MQTTtopicStr),PSTR("%s/data/bme68x"),mySettings.mqtt_mainTopic);
        bme68xJSONMQTT(MQTTpayloadStr, sizeof(MQTTpayloadStr));
        mqttClient.publish(MQTTtopicStr, MQTTpayloadStr);
        bme68xNewData = false;
        if (mySettings.debuglevel == 3) { R_printSerialTelnetLogln(F("BME68x MQTT updated")); }
        mqtt_sent = true;
        bme68xNewDataHandeled = true;
        yieldTime += yieldOS(); 
      
      } else if (bme280NewData && !bme280NewDataHandeled) {
        snprintf_P(MQTTtopicStr, sizeof(MQTTtopicStr),PSTR("%s/data/bme280"),mySettings.mqtt_mainTopic);
        bme280JSONMQTT(MQTTpayloadStr, sizeof(MQTTpayloadStr));
        mqttClient.publish(MQTTtopicStr, MQTTpayloadStr);
        bme280NewData = false;
        if (mySettings.debuglevel == 3) { R_printSerialTelnetLogln(F("BME280 MQTT updated")); }
        mqtt_sent = true;
        bme280NewDataHandeled = true;
        yieldTime += yieldOS(); 
      
      } else if (mlxNewData && !mlxNewDataHandeled) {
        snprintf_P(MQTTtopicStr, sizeof(MQTTtopicStr),PSTR("%s/data/mlx"),mySettings.mqtt_mainTopic);
        mlxJSONMQTT(MQTTpayloadStr, sizeof(MQTTpayloadStr));
        mqttClient.publish(MQTTtopicStr, MQTTpayloadStr);
        mlxNewData = false;
        if (mySettings.debuglevel == 3) { R_printSerialTelnetLogln(F("MLX MQTT updated")); }
        mqtt_sent = true;
        mlxNewDataHandeled = true;
        yieldTime += yieldOS();

      } else if (weatherNewData && !weatherNewDataHandeled) {
        snprintf_P(MQTTtopicStr, sizeof(MQTTtopicStr),PSTR("%s/data/weather"),mySettings.mqtt_mainTopic);
        weatherJSONMQTT(MQTTpayloadStr, sizeof(MQTTpayloadStr));
        mqttClient.publish(MQTTtopicStr, MQTTpayloadStr);
        weatherNewData = false;
        if (mySettings.debuglevel == 3) { R_printSerialTelnetLogln(F("Weather MQTT updated")); }
        mqtt_sent = true;
        weatherNewDataHandeled = true;
        yieldTime += yieldOS(); 
      
      } else if (availNewData && !availNewDataHandeled) { // update sensor status every mninute
        snprintf_P(MQTTtopicStr,sizeof(MQTTtopicStr),PSTR("%s/status/sensors"),mySettings.mqtt_mainTopic);
        snprintf_P(MQTTpayloadStr,sizeof(MQTTpayloadStr), PSTR("{\"mlx_avail\":%d,\"lcd_avail\":%d,\"ccs811_avail\":%d,\"sgp30_avail\":%d,\"scd30_avail\":%d,\"sps30_avail\":%d,\"bme68x_avail\":%d,\"bme280_avail\":%d,\"ntp_avail\":%d}"),
                  therm_avail, lcd_avail, ccs811_avail, sgp30_avail, scd30_avail, sps30_avail, bme68x_avail, bme280_avail, ntp_avail);
        mqttClient.publish(MQTTtopicStr, MQTTpayloadStr);
        availNewData = false;
        mqtt_sent = true;
        availNewDataHandeled = true;
        yieldTime += yieldOS(); 

      } else if (intervalNewData && !intervalNewDataHandeled) { // send sensor polling once at boot up
        snprintf_P(MQTTtopicStr, sizeof(MQTTtopicStr),PSTR("%s/status/intervals"),mySettings.mqtt_mainTopic);
        snprintf_P(MQTTpayloadStr,sizeof(MQTTpayloadStr), PSTR("{\"mlx_interval\":%lu,\"lcd_interval\":%lu,\"ccs811_mode\":%hhu,\"sgp30_interval\":%lu,\"scd30_interval\":%lu,\"sps30_interval\":%lu,\"bme68x_interval\":%lu,\"bme280_interval\":%lu}"),
                  intervalMLX, intervalLCD, ccs811Mode, intervalSGP30, intervalSCD30, intervalSPS30, intervalBME68x, intervalBME280);
        mqttClient.publish(MQTTtopicStr, MQTTpayloadStr);
        intervalNewData = false;
        intervalNewDataHandeled = true;
        yieldTime += yieldOS(); 

      } else {
        // we have handeled all outstanding new data requests
        scd30NewDataHandeled    = false;
        sgp30NewDataHandeled    = false;
        sps30NewDataHandeled    = false;
        ccs811NewDataHandeled   = false;
        bme68xNewDataHandeled   = false;
        bme280NewDataHandeled   = false;
        mlxNewDataHandeled      = false;
        weatherNewDataHandeled  = false;
        availNewDataHandeled    = false;
        intervalNewDataHandeled = false;
        systemNewDataHandeled   = false;
      }

      // when minute changed, dont queue and send right away
      if (timeNewData) {
        snprintf_P(MQTTtopicStr, sizeof(MQTTtopicStr),PSTR("%s/status/time"),mySettings.mqtt_mainTopic);
        timeJSONMQTT(MQTTpayloadStr, sizeof(MQTTpayloadStr));
        mqttClient.publish(MQTTtopicStr, MQTTpayloadStr);
        timeNewData = false;
        if (mySettings.debuglevel == 3) { R_printSerialTelnetLogln(F("Time MQTT updated")); }
        mqtt_sent = true;
        yieldTime += yieldOS(); 
      }

      // when day changed, dont queue and send right away
      if (dateNewData) {
        snprintf_P(MQTTtopicStr, sizeof(MQTTtopicStr),PSTR("%s/status/date"),mySettings.mqtt_mainTopic);
        dateJSONMQTT(MQTTpayloadStr, sizeof(MQTTpayloadStr));
        mqttClient.publish(MQTTtopicStr, MQTTpayloadStr);
        dateNewData = false;
        if (mySettings.debuglevel == 3) { R_printSerialTelnetLogln(F("Date MQTT updated")); }
        mqtt_sent = true;
        yieldTime += yieldOS(); 
      }
      
    } // end send immidiate

    // --------------- this creates single message ---------------------------------------------------------
    else {
      char MQTTpayloadStr[1024];                            // String allocated for MQTT message
      updateMQTTpayload(MQTTpayloadStr, sizeof(MQTTpayloadStr));                    // creating payload String
      snprintf_P(MQTTtopicStr, sizeof(MQTTtopicStr), PSTR("%s/data/all"),mySettings.mqtt_mainTopic);
      if (mySettings.debuglevel == 3) { R_printSerialTelnetLogln(MQTTpayloadStr);}      
      yieldTime += yieldOS(); 
      mqttClient.publish(MQTTtopicStr, MQTTpayloadStr);
      lastMQTTPublish = currentTime;
      mqtt_sent = true;
      yieldTime += yieldOS(); 
    } // end send single message
  }
  
  // --------------- debug status ---------------------------------------------------------
  if (mqtt_sent == true) {
    if (mySettings.debuglevel == 3) { R_printSerialTelnetLogln(F("MQTT: published data")); }
  }
}

/**************************************************************************************/
// Create single MQTT payload message
/**************************************************************************************/

  // NEED TO FIX THIS TO MATCH MQTT ENTRIES OF SINGLE SENSOR UPDATES

void updateMQTTpayload(char *payload, size_t len) {
  char tmpbuf[24];
  strcpy(payload, "{");
  if (scd30_avail && mySettings.useSCD30)   { // ==CO2===========================================================
    strncat(payload,"\"scd30_CO2\":",      len-strlen(payload)); snprintf_P(tmpbuf, sizeof(tmpbuf), PSTR("%4dppm"),  int(scd30_ppm));                strncat(payload,tmpbuf, len-strlen(payload)); strncat(payload,", ", len-strlen(payload)); 
    strncat(payload,"\"scd30_rH\":",       len-strlen(payload)); snprintf_P(tmpbuf, sizeof(tmpbuf), PSTR("%4.1f%%"),     scd30_hum);                 strncat(payload,tmpbuf, len-strlen(payload)); strncat(payload,", ", len-strlen(payload));
    strncat(payload,"\"scd30_T\":",        len-strlen(payload)); snprintf_P(tmpbuf, sizeof(tmpbuf), PSTR("%+5.1fC"),     scd30_temp);                strncat(payload,tmpbuf, len-strlen(payload)); strncat(payload,", ", len-strlen(payload));
  }  // end if avail scd30
  if (bme68x_avail && mySettings.useBME68x) { // ===rH,T,aQ=================================================
    strncat(payload,"\"bme68x_p\":",       len-strlen(payload)); snprintf_P(tmpbuf, sizeof(tmpbuf), PSTR("%4dbar"),(int)(bme68x.pressure/100.0));    strncat(payload,tmpbuf, len-strlen(payload)); strncat(payload,", ", len-strlen(payload)); 
    strncat(payload,"\"bme68x_rH\":",      len-strlen(payload)); snprintf_P(tmpbuf, sizeof(tmpbuf), PSTR("%4.1f%%"),     bme68x.humidity);           strncat(payload,tmpbuf, len-strlen(payload)); strncat(payload,", ", len-strlen(payload));
    strncat(payload,"\"bme68x_aH\":",      len-strlen(payload)); snprintf_P(tmpbuf, sizeof(tmpbuf), PSTR("%4.1fg"),      bme68x_ah);                 strncat(payload,tmpbuf, len-strlen(payload)); strncat(payload,", ", len-strlen(payload));
    strncat(payload,"\"bme68x_T\":",       len-strlen(payload)); snprintf_P(tmpbuf, sizeof(tmpbuf), PSTR("%+5.1fC"),     bme68x.temperature);        strncat(payload,tmpbuf, len-strlen(payload)); strncat(payload,", ", len-strlen(payload));
    strncat(payload,"\"bme68x_aq\":",      len-strlen(payload)); snprintf_P(tmpbuf, sizeof(tmpbuf), PSTR("%dOhm"),       bme68x.gas_resistance);     strncat(payload,tmpbuf, len-strlen(payload)); strncat(payload,", ", len-strlen(payload));
  } // end if avail bme68x
  if (bme280_avail && mySettings.useBME280) { // ===rH,T,aQ=================================================
    strncat(payload,"\"bme280_p\":",       len-strlen(payload)); snprintf_P(tmpbuf, sizeof(tmpbuf), PSTR("%4dbar"),(int)(bme280_pressure/100.0));    strncat(payload,tmpbuf, len-strlen(payload)); strncat(payload,", ", len-strlen(payload)); 
    strncat(payload,"\"bme280_rH\":",      len-strlen(payload)); snprintf_P(tmpbuf, sizeof(tmpbuf), PSTR("%4.1f%%"),     bme280_hum);                strncat(payload,tmpbuf, len-strlen(payload)); strncat(payload,", ", len-strlen(payload));
    strncat(payload,"\"bme280_aH\":",      len-strlen(payload)); snprintf_P(tmpbuf, sizeof(tmpbuf), PSTR("%4.1fg"),      bme280_ah);                 strncat(payload,tmpbuf, len-strlen(payload)); strncat(payload,", ", len-strlen(payload));
    strncat(payload,"\"bme280_T\":",       len-strlen(payload)); snprintf_P(tmpbuf, sizeof(tmpbuf), PSTR("%+5.1fC"),     bme280_temp);               strncat(payload,tmpbuf, len-strlen(payload)); strncat(payload,", ", len-strlen(payload));
  } // end if avail bme68x
  if (sgp30_avail && mySettings.useSGP30)   { // ===CO2,tVOC=================================================
    strncat(payload,"\"sgp30_CO2\":",      len-strlen(payload)); snprintf_P(tmpbuf, sizeof(tmpbuf), PSTR("%4dppm"),      sgp30.CO2);                 strncat(payload,tmpbuf, len-strlen(payload)); strncat(payload,", ", len-strlen(payload));
    strncat(payload,"\"sgp30_tVOC\":",     len-strlen(payload)); snprintf_P(tmpbuf, sizeof(tmpbuf), PSTR("%4dppb"),      sgp30.TVOC);                strncat(payload,tmpbuf, len-strlen(payload)); strncat(payload,", ", len-strlen(payload));
  } // end if avail sgp30
  if (ccs811_avail && mySettings.useCCS811) { // ===CO2,tVOC=================================================
    strncat(payload,"\"ccs811_CO2\":",     len-strlen(payload)); snprintf_P(tmpbuf, sizeof(tmpbuf), PSTR("%4dppm"),      ccs811.getCO2());           strncat(payload,tmpbuf, len-strlen(payload)); strncat(payload,", ", len-strlen(payload));
    strncat(payload,"\"ccs811_tVOC\":",    len-strlen(payload)); snprintf_P(tmpbuf, sizeof(tmpbuf), PSTR("%4dppb"),      ccs811.getTVOC());          strncat(payload,tmpbuf, len-strlen(payload)); strncat(payload,", ", len-strlen(payload));
  } // end if avail ccs811
  if (sps30_avail && mySettings.useSPS30)   { // ===Particle=================================================
    strncat(payload,"\"sps30_PM1\":",      len-strlen(payload)); snprintf_P(tmpbuf, sizeof(tmpbuf), PSTR("%3.0fµg/m3"),  valSPS30.mc_1p0);           strncat(payload,tmpbuf, len-strlen(payload)); strncat(payload,", ", len-strlen(payload));
    strncat(payload,"\"sps30_PM2\":",      len-strlen(payload)); snprintf_P(tmpbuf, sizeof(tmpbuf), PSTR("%3.0fµg/m3"),  valSPS30.mc_2p5);           strncat(payload,tmpbuf, len-strlen(payload)); strncat(payload,", ", len-strlen(payload));
    strncat(payload,"\"sps30_PM4\":",      len-strlen(payload)); snprintf_P(tmpbuf, sizeof(tmpbuf), PSTR("%3.0fµg/m3"),  valSPS30.mc_4p0);           strncat(payload,tmpbuf, len-strlen(payload)); strncat(payload,", ", len-strlen(payload));
    strncat(payload,"\"sps30_nPM10\":",    len-strlen(payload)); snprintf_P(tmpbuf, sizeof(tmpbuf), PSTR("%3.0fµg/m3"),  valSPS30.mc_10p0);          strncat(payload,tmpbuf, len-strlen(payload)); strncat(payload,", ", len-strlen(payload));
    strncat(payload,"\"sps30_nPM0\":",     len-strlen(payload)); snprintf_P(tmpbuf, sizeof(tmpbuf), PSTR("%3.0f#/m3"),   valSPS30.nc_0p5);           strncat(payload,tmpbuf, len-strlen(payload)); strncat(payload,", ", len-strlen(payload));
    strncat(payload,"\"sps30_nPM2\":",     len-strlen(payload)); snprintf_P(tmpbuf, sizeof(tmpbuf), PSTR("%3.0f#/m3"),   valSPS30.nc_2p5);           strncat(payload,tmpbuf, len-strlen(payload)); strncat(payload,", ", len-strlen(payload));
    strncat(payload,"\"sps30_nPM4\":",     len-strlen(payload)); snprintf_P(tmpbuf, sizeof(tmpbuf), PSTR("%3.0f#/m3"),   valSPS30.nc_4p0);           strncat(payload,tmpbuf, len-strlen(payload)); strncat(payload,", ", len-strlen(payload));
    strncat(payload,"\"sps30_nPM10\":",    len-strlen(payload)); snprintf_P(tmpbuf, sizeof(tmpbuf), PSTR("%3.0f#/m3"),   valSPS30.nc_10p0);          strncat(payload,tmpbuf, len-strlen(payload)); strncat(payload,", ", len-strlen(payload));
    strncat(payload,"\"sps30_PartSize\":", len-strlen(payload)); snprintf_P(tmpbuf, sizeof(tmpbuf), PSTR("%3.0fµm"),     valSPS30.typical_particle_size); strncat(payload,tmpbuf, len-strlen(payload)); strncat(payload,", ", len-strlen(payload));
  }// end if avail SPS30
  if (therm_avail && mySettings.useMLX)     { // ====To,Ta================================================
    strncat(payload,"\"MLX_To\":",         len-strlen(payload)); snprintf_P(tmpbuf, sizeof(tmpbuf), PSTR("%+5.1fC"),    (therm.object()+mlxOffset)); strncat(payload,tmpbuf, len-strlen(payload)); strncat(payload,", ", len-strlen(payload));
    strncat(payload,"\"MLX_Ta\":",         len-strlen(payload)); snprintf_P(tmpbuf, sizeof(tmpbuf), PSTR("%+5.1fC"),     therm.ambient());           strncat(payload,tmpbuf, len-strlen(payload)); strncat(payload,", ", len-strlen(payload));
  }// end if avail  MLX
  strncat(payload, "}",                    len-strlen(payload));
} // update MQTT

/******************************************************************************************************/
// MQTT Call Back in case we receive MQTT message from network
/******************************************************************************************************/

void mqttCallback(char* topic, uint8_t* payload, unsigned int len) {
  char *p = (char *)malloc(len);
  if (mySettings.debuglevel > 0) {
    snprintf_P(tmpStr, sizeof(tmpStr), PSTR("MQTT: Message arrived [%s] "), topic); 
    R_printSerialTelnetLogln(tmpStr);
    memcpy(p,payload,len);
    printSerialTelnetLogln(p);
    yieldTime += yieldOS();
 }
 free(p);
}
