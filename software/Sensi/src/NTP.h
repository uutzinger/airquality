/******************************************************************************************************/
// Network Time Protocol
/******************************************************************************************************/

#define NTP_TIMEOUT 5000                                   // default 5000 milliseconds
#define NTP_INTERVAL 1800                                  // default 1800 seconds 64..1024s is NTPv4 specs
#define NTP_SHORTINTERVAL 15                               // default 15
#define NTP_MIN_SYNC_ACCURACY_US 100000                    // default 5000, offset of upto 25ms is not uncommon
#define TIME_SYNC_THRESHOLD  2500                          // default 2500
#define NTP_MAX_RESYNC_RETRY 3                             // default 3
#define NTP_NUM_OFFSET_AVE_ROUNDS 1                        // default 1

#include <ESPNtpClient.h>
unsigned long lastNTP;                                     // last time we checked network time
volatile WiFiStates stateNTP = IS_WAITING;                 // keeping track of network time state

bool syncEventTriggered = false;                           // True if NTP client triggered a time event
NTPEvent_t ntpEvent;                                       // Last triggered event

bool ntp_avail = false;                                    // True of NTP client communicted with NTP server
bool timeSynced = false;                                   // True if successful sync occured
bool updateDate = true;                                    // Ready to broadcast date (websocket and mqtt)
bool updateTime = true;                                    // Ready to broadcast time (websocket and mqtt) 
bool ntpSetupError = false;

void updateNTP(void);

/*
//unsigned long intervalNTP = 1000;                        // main loop update interval
unsigned long intervalNTP = 64000;                         // Request NTP time every minute
//
IPAddress timeServerIP;                                    // e.g. time.nist.gov NTP server address
const int NTP_PACKET_SIZE = 48;                            // NTP time stamp is in the first 48 bytes of the message
byte NTPBuffer[NTP_PACKET_SIZE];                           // buffer to hold incoming and outgoing packets
unsigned long lastNTPResponse = 0;                         // last time in system clock we got NTP response
unsigned long lastNTPSent = 0;                             // keeps track when packets went out
uint32_t ntpTime = 0;                                      // the time received from NTP server
WiFiUDP ntpUDP;                                            // UDP structure for network time client
uint32_t getTime(void);
void sendNTPpacket(IPAddress& address);
*/
