/******************************************************************************************************/
// WebSocketsServer
/******************************************************************************************************/
#ifndef WEBSOCKET_H_
#define WEBSOCKET_H_

#include <WebSocketsServer.h>                              // Low latancy data exchange over TCP

#define intervalWebSocket  500                             // update interval to update websocket data

void initializeWebSocket(void);
void updateWebSocket(void);

void hexdump(const void *mem, uint32_t len, uint8_t cols);
void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t lenght);
void updateWebSocketMessage(void);

#endif
