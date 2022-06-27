/******************************************************************************************************/
// ESPTelnet
/******************************************************************************************************/

#include "ESPTelnet.h"  

String telnetInput;

char telnetInputBuff[64];
bool telnetConnected = false; 
bool telnetReceived = false;

unsigned long lastTelnet;                                     // last time we checked telnet input
unsigned long lastTelnetInput = 0;                            // keep track of telnet flood

volatile WiFiStates stateTelnet = IS_WAITING;                 // keeping track of network time state

void updateTelnet(void);
