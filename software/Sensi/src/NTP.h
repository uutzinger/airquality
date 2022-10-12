/******************************************************************************************************/
// Network Time Protocol
/******************************************************************************************************/
#ifndef NTP_H_
#define NTP_H_

#include <ESPNtpClient.h>

// uses ESPNtpClient library
// https://tttapa.github.io/ESP8266/Chap15%20-%20NTP.html

#define NTP_TIMEOUT                5000                    // default 5000 milliseconds
#define NTP_INTERVAL               1800                    // default 1800 seconds 64..1024s is NTPv4 specs
#define NTP_SHORTINTERVAL            15                    // default 15
#define NTP_MIN_SYNC_ACCURACY_US 100000                    // default 5000, offset of upto 25ms is not uncommon
#define TIME_SYNC_THRESHOLD        2500                    // default 2500
#define NTP_MAX_RESYNC_RETRY          3                    // default 3
#define NTP_NUM_OFFSET_AVE_ROUNDS     1                    // default 1

void onNTPeventReceived(NTPEvent_t event);
void processSyncEvent(NTPEvent_t ntpEvent);

void initializeNTP(void);
void updateNTP(void);

#endif
