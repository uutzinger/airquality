/******************************************************************************************************/
// MQTT
/******************************************************************************************************/
#include "VSC.h"
#ifdef EDITVSC
#include "src/MQTT.h"
#include "src/WiFi.h"
#include "src/Sensi.h"
#include "src/Config.h"
#include "src/BME280.h"
#include "src/BME680.h"
#include "src/CCS811.h"
#include "src/MLX.h"
#include "src/SCD30.h"
#include "src/SGP30.h"
#include "src/SPS30.h"
#endif

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
      if ((currentTime - lastMQTT) >= intervalWiFi) {
        D_printSerialTelnet(F("D:U:MQTT:S.."));
        lastMQTT = currentTime;
        mqttClient.setCallback(mqttCallback);                             // start listener
        mqttClient.setServer(mySettings.mqtt_server, MQTT_PORT);          // start connection to server
        if (mySettings.debuglevel > 0) { sprintf_P(tmpStr, PSTR("Connecting to MQTT: %s"), mySettings.mqtt_server); R_printSerialTelnetLogln(tmpStr); }
        mqtt_connected = false;
        // connect to server with or without username password
        if (strlen(mySettings.mqtt_username) > 0)   { mqtt_connected = mqttClient.connect("SensiEPS8266Client", mySettings.mqtt_username, mySettings.mqtt_password); }
        else                                        { mqtt_connected = mqttClient.connect("SensiEPS8266Client"); }
        if (mqtt_connected == false) { // try fall back server
          mqttClient.setServer(mySettings.mqtt_fallback, MQTT_PORT);
          if (mySettings.debuglevel > 0) { sprintf_P(tmpStr, "Connecting to MQTT: %s", mySettings.mqtt_fallback); R_printSerialTelnetLogln(tmpStr); }
          if (strlen(mySettings.mqtt_username) > 0) { mqtt_connected = mqttClient.connect("SensiEPS8266Client", mySettings.mqtt_username, mySettings.mqtt_password); }
          else                                      { mqtt_connected = mqttClient.connect("SensiEPS8266Client"); }
        }
        // publish connection status
        if (mqtt_connected == true) {
          char MQTTtopicStr[64];                                     // String allocated for MQTT topic 
          sprintf_P(MQTTtopicStr,PSTR("%s/status"),mySettings.mqtt_mainTopic);
          mqttClient.publish(MQTTtopicStr, "Sensi up");
          if (mySettings.debuglevel > 0) { R_printSerialTelnetLogln(F("MQTT: connected successfully")); }
          stateMQTT = CHECK_CONNECTION;  // advance to connection monitoring and publishing
          AllmaxUpdateMQTT = 0;
        } else {  // connection failed
          if (mySettings.debuglevel > 0) { R_printSerialTelnetLogln(F("MQTT: conenction failed")); }
        } // failed connectig, repeat starting up
      } //interval time
      break;
    }
    
    case CHECK_CONNECTION : { //---------------------
      if ((currentTime - lastMQTT) >= intervalMQTT) {
        D_printSerialTelnet(F("D:U:MQTT:CC.."));
        lastMQTT = currentTime;
        if (mqttClient.loop() != true) {
          mqtt_connected = false;
          stateMQTT = START_UP;
          if (mySettings.debuglevel == 3) { R_printSerialTelnetLogln(F("MQTT: is disconnected. Reconnecting to server")); }
        }
      }
      break;          
    }

    default: {if (mySettings.debuglevel > 0) { R_printSerialTelnetLogln(F("MQTT Error: invalid switch statement")); break;}}
          
  } // end switch state
}

/******************************************************************************************************/
// Update MQTT Message
/******************************************************************************************************/
void updateMQTTMessage() {
  char MQTTtopicStr[64];                                     // String allocated for MQTT topic 
  mqtt_sent = false;
  
  // --------------------- this sends new data to mqtt server as soon as its available ----------------
  if (mySettings.sendMQTTimmediate) {
    char MQTTpayloadStr[512];                                 // String allocated for MQTT message
    if (scd30NewData)  {
      sprintf_P(MQTTtopicStr,PSTR("%s/data"),mySettings.mqtt_mainTopic);
      scd30JSON(MQTTpayloadStr);
      mqttClient.publish(MQTTtopicStr, MQTTpayloadStr);
      scd30NewData = false;
      if (mySettings.debuglevel == 3) { R_printSerialTelnetLogln(F("SCD30 MQTT updated")); }
      mqtt_sent = true;
      delay(1);
    }
    
    if (sgp30NewData)  {
      sprintf_P(MQTTtopicStr,PSTR("%s/data"),mySettings.mqtt_mainTopic);
      sgp30JSON(MQTTpayloadStr);
      mqttClient.publish(MQTTtopicStr, MQTTpayloadStr);
      sgp30NewData = false;
      if (mySettings.debuglevel == 3) { R_printSerialTelnetLogln(F("SGP30 MQTT updated")); }      
      mqtt_sent = true;
      delay(1);
    }
    
    if (sps30NewData)  {
      sprintf_P(MQTTtopicStr,PSTR("%s/data"),mySettings.mqtt_mainTopic);
      sps30JSON(MQTTpayloadStr);
      mqttClient.publish(MQTTtopicStr, MQTTpayloadStr);
      sps30NewData = false;
      if (mySettings.debuglevel == 3) { R_printSerialTelnetLogln(F("SPS30 MQTT updated")); }      
      mqtt_sent = true;
      delay(1);
    }
    
    if (ccs811NewData) {
      sprintf_P(MQTTtopicStr,PSTR("%s/data"),mySettings.mqtt_mainTopic);
      ccs811JSON(MQTTpayloadStr);
      mqttClient.publish(MQTTtopicStr, MQTTpayloadStr);
      ccs811NewData = false;
      if (mySettings.debuglevel == 3) { R_printSerialTelnetLogln(F("CCS811 MQTT updated")); }      
      mqtt_sent = true;
      delay(1);
    }
    
    if (bme680NewData) {
      sprintf_P(MQTTtopicStr,PSTR("%s/data"),mySettings.mqtt_mainTopic);
      bme680JSON(MQTTpayloadStr);
      mqttClient.publish(MQTTtopicStr, MQTTpayloadStr);
      bme680NewData = false;
      if (mySettings.debuglevel == 3) { R_printSerialTelnetLogln(F("BME680 MQTT updated")); }
      mqtt_sent = true;
      delay(1);
    }
    
    if (bme280NewData) {
      sprintf_P(MQTTtopicStr,PSTR("%s/data"),mySettings.mqtt_mainTopic);
      bme280JSON(MQTTpayloadStr);
      mqttClient.publish(MQTTtopicStr, MQTTpayloadStr);
      bme280NewData = false;
      if (mySettings.debuglevel == 3) { R_printSerialTelnetLogln(F("BME280 MQTT updated")); }
      mqtt_sent = true;
      delay(1);
    }

    
    if (mlxNewData) {
      sprintf_P(MQTTtopicStr,PSTR("%s/data"),mySettings.mqtt_mainTopic);
      mlxJSON(MQTTpayloadStr);
      mqttClient.publish(MQTTtopicStr, MQTTpayloadStr);
      mlxNewData = false;
      if (mySettings.debuglevel == 3) { R_printSerialTelnetLogln(F("MLX MQTT updated")); }
      mqtt_sent = true;
      delay(1);
    }

    if (timeNewData) {
      sprintf_P(MQTTtopicStr,PSTR("%s/status"),mySettings.mqtt_mainTopic);
      timeJSON(MQTTpayloadStr);
      mqttClient.publish(MQTTtopicStr, MQTTpayloadStr);
      mlxNewData = false;
      if (mySettings.debuglevel == 3) { R_printSerialTelnetLogln(F("Time MQTT updated")); }
      mqtt_sent = true;
      delay(1);
    }

    if (dateNewData) {
      sprintf_P(MQTTtopicStr,PSTR("%s/status"),mySettings.mqtt_mainTopic);
      dateJSON(MQTTpayloadStr);
      mqttClient.publish(MQTTtopicStr, MQTTpayloadStr);
      mlxNewData = false;
      if (mySettings.debuglevel == 3) { R_printSerialTelnetLogln(F("Data MQTT updated")); }
      mqtt_sent = true;
      delay(1);
    }
    
    if (sendMQTTonce) { // send sensor polling once at boot up
      sprintf_P(MQTTtopicStr,PSTR("%s/status"),mySettings.mqtt_mainTopic);
      sprintf_P(MQTTpayloadStr, PSTR("{\"mlx_interval\":%u,\"lcd_interval\":%u,\"ccs811_mode\":%u,\"sgp30_interval\":%u,\"scd30_interval\":%u,\"sps30_interval\":%u,\"bme680_interval\":%u,\"bme280_interval\":%u}"),
                                  intervalMLX,        intervalLCD,        ccs811Mode,        intervalSGP30,        intervalSCD30,        intervalSPS30,        intervalBME680,        intervalBME280);
      mqttClient.publish(MQTTtopicStr, MQTTpayloadStr);
      delay(1);      
      sendMQTTonce = false;  // do not update interval information again
    }
    
    if (currentTime - lastMQTTPublish > intervalMQTTSlow) { // update sensor status every mninute
      sprintf_P(MQTTtopicStr,PSTR("%s/status"),mySettings.mqtt_mainTopic);
      sprintf_P(MQTTpayloadStr, PSTR("{\"mlx_avail\":%u,\"lcd_avail\":%u,\"ccs811_avail\":%u,\"sgp30_avail\":%u,\"scd30_avail\":%u,\"sps30_avail\":%u,\"bme680_avail\":%u,\"bme280_avail\":%u,\"ntp_avail\":%u}"),
                                  therm_avail,     lcd_avail,       ccs811_avail,       sgp30_avail,       scd30_avail,       sps30_avail,       bme680_avail,       bme280_avail,       ntp_avail);
      mqttClient.publish(MQTTtopicStr, MQTTpayloadStr);
      delay(1);      

      lastMQTTPublish = currentTime;
      mqtt_sent = true;
    }
  } 
      
  // --------------- this creates single message ---------------------------------------------------------
  else {
    if (currentTime - lastMQTTPublish > intervalMQTT) { // wait for interval time
      char MQTTpayloadStr[1024];                            // String allocated for MQTT message
      updateMQTTpayload(MQTTpayloadStr);                    // creating payload String
      sprintf_P(MQTTtopicStr, PSTR("%s/data/all"),mySettings.mqtt_mainTopic);
      if (mySettings.debuglevel == 3) { R_printSerialTelnetLogln(MQTTpayloadStr);}      
      mqttClient.publish(MQTTtopicStr, MQTTpayloadStr);
      lastMQTTPublish = currentTime;
      mqtt_sent = true;
    } // end interval
  } // end send immidiate
  
  // --------------- dubg status ---------------------------------------------------------
  if (mqtt_sent == true) {
    if (mySettings.debuglevel == 3) { R_printSerialTelnetLogln(F("MQTT: published data")); }
  }
}

/**************************************************************************************/
// Create single MQTT payload message
/**************************************************************************************/

  // NEED TO FIX THIS TO MATCH MQTT ENTRIES OF SINGLE SENSOR UPDATES

void updateMQTTpayload(char *payload) {
  char tmpbuf[24];
  strcpy(payload, "{");
  if (scd30_avail && mySettings.useSCD30) { // ==CO2===========================================================
    strcat(payload,"\"scd30_CO2\":");      sprintf_P(tmpbuf, PSTR("%4dppm"), int(scd30_ppm));              strcat(payload,tmpbuf); strcat(payload,", "); 
    strcat(payload,"\"scd30_rH\":");       sprintf_P(tmpbuf, PSTR("%4.1f%%"), scd30_hum);                  strcat(payload,tmpbuf); strcat(payload,", ");
    strcat(payload,"\"scd30_T\":");        sprintf_P(tmpbuf, PSTR("%+5.1fC"),scd30_temp);                  strcat(payload,tmpbuf); strcat(payload,", ");
  }  // end if avail scd30
  if (bme680_avail && mySettings.useBME680) { // ===rH,T,aQ=================================================
    strcat(payload,"\"bme680_p\":");       sprintf_P(tmpbuf, PSTR("%4dbar"),(int)(bme680.pressure/100.0)); strcat(payload,tmpbuf); strcat(payload,", "); 
    strcat(payload,"\"bme680_rH\":");      sprintf_P(tmpbuf, PSTR("%4.1f%%"),bme680.humidity);             strcat(payload,tmpbuf); strcat(payload,", ");
    strcat(payload,"\"bme680_aH\":");      sprintf_P(tmpbuf, PSTR("%4.1fg"),bme680_ah);                    strcat(payload,tmpbuf); strcat(payload,", ");
    strcat(payload,"\"bme680_T\":");       sprintf_P(tmpbuf, PSTR("%+5.1fC"),bme680.temperature);          strcat(payload,tmpbuf); strcat(payload,", ");
    strcat(payload,"\"bme680_aq\":");      sprintf_P(tmpbuf, PSTR("%dOhm"),bme680.gas_resistance);         strcat(payload,tmpbuf); strcat(payload,", ");
  } // end if avail bme680
  if (bme280_avail && mySettings.useBME280) { // ===rH,T,aQ=================================================
    strcat(payload,"\"bme280_p\":");       sprintf_P(tmpbuf, PSTR("%4dbar"),(int)(bme280_pressure/100.0)); strcat(payload,tmpbuf); strcat(payload,", "); 
    strcat(payload,"\"bme280_rH\":");      sprintf_P(tmpbuf, PSTR("%4.1f%%"),bme280_hum);                  strcat(payload,tmpbuf); strcat(payload,", ");
    strcat(payload,"\"bme280_aH\":");      sprintf_P(tmpbuf, PSTR("%4.1fg"),bme280_ah);                    strcat(payload,tmpbuf); strcat(payload,", ");
    strcat(payload,"\"bme280_T\":");       sprintf_P(tmpbuf, PSTR("%+5.1fC"),bme280_temp);                 strcat(payload,tmpbuf); strcat(payload,", ");
  } // end if avail bme680
  if (sgp30_avail && mySettings.useSGP30) { // ===CO2,tVOC=================================================
    strcat(payload,"\"sgp30_CO2\":");      sprintf_P(tmpbuf, PSTR("%4dppm"), sgp30.CO2);                   strcat(payload,tmpbuf); strcat(payload,", ");
    strcat(payload,"\"sgp30_tVOC\":");     sprintf_P(tmpbuf, PSTR("%4dppb"), sgp30.TVOC);                  strcat(payload,tmpbuf); strcat(payload,", ");
  } // end if avail sgp30
  if (ccs811_avail && mySettings.useCCS811) { // ===CO2,tVOC=================================================
    strcat(payload,"\"ccs811_CO2\":");     sprintf_P(tmpbuf, PSTR("%4dppm"), ccs811.getCO2());             strcat(payload,tmpbuf); strcat(payload,", ");
    strcat(payload,"\"ccs811_tVOC\":");    sprintf_P(tmpbuf, PSTR("%4dppb"), ccs811.getTVOC());            strcat(payload,tmpbuf); strcat(payload,", ");
  } // end if avail ccs811
  if (sps30_avail && mySettings.useSPS30) { // ===Particle=================================================
    strcat(payload,"\"sps30_PM1\":");      sprintf_P(tmpbuf, PSTR("%3.0fµg/m3"),valSPS30.MassPM1);         strcat(payload,tmpbuf); strcat(payload,", ");
    strcat(payload,"\"sps30_PM2\":");      sprintf_P(tmpbuf, PSTR("%3.0fµg/m3"),valSPS30.MassPM2);         strcat(payload,tmpbuf); strcat(payload,", ");
    strcat(payload,"\"sps30_PM4\":");      sprintf_P(tmpbuf, PSTR("%3.0fµg/m3"),valSPS30.MassPM4);         strcat(payload,tmpbuf); strcat(payload,", ");
    strcat(payload,"\"sps30_nPM10\":");    sprintf_P(tmpbuf, PSTR("%3.0fµg/m3"),valSPS30.MassPM10);        strcat(payload,tmpbuf); strcat(payload,", ");
    strcat(payload,"\"sps30_nPM0\":");     sprintf_P(tmpbuf, PSTR("%3.0f#/m3"), valSPS30.NumPM0);          strcat(payload,tmpbuf); strcat(payload,", ");
    strcat(payload,"\"sps30_nPM2\":");     sprintf_P(tmpbuf, PSTR("%3.0f#/m3"), valSPS30.NumPM2);          strcat(payload,tmpbuf); strcat(payload,", ");
    strcat(payload,"\"sps30_nPM4\":");     sprintf_P(tmpbuf, PSTR("%3.0f#/m3"), valSPS30.NumPM4);          strcat(payload,tmpbuf); strcat(payload,", ");
    strcat(payload,"\"sps30_nPM10\":");    sprintf_P(tmpbuf, PSTR("%3.0f#/m3"), valSPS30.NumPM10);         strcat(payload,tmpbuf); strcat(payload,", ");
    strcat(payload,"\"sps30_PartSize\":"); sprintf_P(tmpbuf, PSTR("%3.0fµm"),   valSPS30.PartSize);        strcat(payload,tmpbuf); strcat(payload,", ");
  }// end if avail SPS30
  if (therm_avail && mySettings.useMLX) { // ====To,Ta================================================
    strcat(payload,"\"MLX_To\":");         sprintf_P(tmpbuf, PSTR("%+5.1fC"),(therm.object()+mlxOffset));  strcat(payload,tmpbuf); strcat(payload,", ");
    strcat(payload,"\"MLX_Ta\":");         sprintf_P(tmpbuf, PSTR("%+5.1fC"),therm.ambient());             strcat(payload,tmpbuf); strcat(payload,", ");
  }// end if avail  MLX
  strcat(payload, "}");
} // update MQTT


/******************************************************************************************************/
// MQTT Call Back in case we receive MQTT message from network
/******************************************************************************************************/

void mqttCallback(char* topic, byte* payload, unsigned int len) {
  char payloadC[64];
  if (mySettings.debuglevel > 0) { 
    sprintf_P(tmpStr, PSTR("MQTT: Message received\r\nMQTT: topic: %s\r\nMQTT: payload: "), topic); R_printSerialTelnetLogln(tmpStr);
    memcpy( (void *)( payloadC ), (void *) payload, (len < 64) ? len : 63 );
    payloadC[len]='\0';
    printSerialTelnetLogln(payloadC);
  }
}
