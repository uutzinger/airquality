/******************************************************************************************************/
// WebSocketsServer
/******************************************************************************************************/
#include <WebSocketsServer.h>                              // Low latancy data exchange over TCP

// #define intervalWebSocket  500                             // update interval to update websocket data
bool ws_connected = false;                                 // mqtt connection established?
unsigned long lastWebSocket;	    					   // last time we checked for websocket
volatile WiFiStates stateWebSocket  = IS_WAITING;          // keeping track of websocket
void updateWebSocket(void);
void hexdump(const void *mem, uint32_t len, uint8_t cols);
void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t lenght);
void updateWebSocketMessage(void);
