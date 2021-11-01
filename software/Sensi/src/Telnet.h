/******************************************************************************************************/
// ESPTelnet
/******************************************************************************************************/

#include "ESPTelnet.h"  

String telnetInputBuff;

bool telnetConnected = false; 
bool telnetReceived = false;

unsigned long lastTelnet;                                     // last time we checked network time

volatile WiFiStates stateTelnet = IS_WAITING;                 // keeping track of network time state

void updateTelnet(void);
