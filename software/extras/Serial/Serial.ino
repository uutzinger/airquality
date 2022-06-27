// Serial & Telnet
#define BAUDRATE 115200                                    // serial communicaiton speed, terminal settings need to match
#define SERIALMAXRATE 500                                  // to aovide dumping long past buffer into input window
char serialInputBuff[64];
bool serialReceived = false;                               // keep track of serial input
unsigned long lastSerialInput = 0;                         // keep track of serial flood
unsigned long lastTmp = 0;
const char singleSeparator[] PROGMEM      = {"--------------------------------------------------------------------------------"};
const char doubleSeparator[] PROGMEM      = {"================================================================================"};
const char waitmsg[] PROGMEM              = {"Setup Completed.\n\rWaiting 10 seconds, skip by hitting enter"};      // Allows user to open serial terminal to observe the debug output before the loop starts

void setup() {
  // put your setup code here, to run once:
  Serial.begin(BAUDRATE);
  Serial.setTimeout(1000); // Serial read timeout in ms, 1000 is default
  serialTrigger(waitmsg, 10000);
  Serial.println("Ok, let's go.");
}

void loop() {
  // put your main code here, to run repeatedly:
  if (Serial.available()) {
      int bytesread = Serial.readBytesUntil('\n', serialInputBuff, sizeof(serialInputBuff));  // Read from serial until newline is read or timeout exceeded
      while (Serial.available() > 0) { Serial.read(); } // Clear remaining data in input buffer
      serialInputBuff[bytesread] = '\0';
      serialReceived = true;
    } else {
      serialReceived = false;
    }
    inputHandle();
}

void serialTrigger(const char* mess, int timeout) {
  unsigned long startTime = millis();
  bool clearSerial = true;
  Serial.println();
  Serial.println(mess);
  while (!Serial.available()) {
    delay(1000);
    if (millis() - startTime >= timeout) {
      clearSerial = false;
      break;
    }
  }
  if (clearSerial) { while (Serial.available()) {Serial.read();} }
}
  
void helpMenu() {
  Serial.println(FPSTR(doubleSeparator));
  Serial.println(F("| Serial Test, 2022, Urs Utzinger                                              |"));
  Serial.println(FPSTR(doubleSeparator));
  Serial.println(F("|..............................................................................|"));
  Serial.println(F("|.....................................................................no device|"));  
  Serial.println(FPSTR(singleSeparator));
  Serial.println(F("| a test unsigned int..........................................................|"));  
  Serial.println(F("| b test int...................................................................|"));  
  Serial.println(F("| c test float.................................................................|"));  
  Serial.println(F("| d test none..................................................................|"));  
  Serial.println(F("| e test int with txt..........................................................|"));  
  Serial.println(FPSTR(doubleSeparator));
}

bool inputHandle() {
  bool          processSerial = false;
  char          command[2];
  char          value[64];
  char          text[64];
  char          rtext[64];
  char          tmpStr[64];
  char        * pEnd;  
  unsigned long tmpuI;
  long          tmpI;
  float         tmpF;
  
  if (serialReceived) {
    lastTmp = millis();
    if ((lastTmp - lastSerialInput) > SERIALMAXRATE) { // limit command input rate to protect from pasted text
      lastSerialInput = lastTmp;
      if ( strlen(serialInputBuff) > 0) { processSerial = true; }
      else                              { processSerial = false;}
    }
    Serial.print(F("Input Buffer: "));
    Serial.println(serialInputBuff);
    Serial.print(F("Length Input Buffer: "));
    Serial.println(strlen(serialInputBuff));
    serialReceived = false;
  }
  
  if (processSerial){
    if (strlen(serialInputBuff) > 0) { strncpy(command, serialInputBuff, 1); command[1]='\0'; }
    if (strlen(serialInputBuff) > 1) { strncpy(value, serialInputBuff+1, sizeof(value)); } // might also be &serialInputBuffer
    else                             { value[0] = '\0';}
    
    Serial.print(F("Length Command: "));
    Serial.println(strlen(command));
    Serial.print(F("Command: "));
    Serial.println(command);
    Serial.print(F("Length Value: "));
    Serial.println(strlen(value));
    Serial.print(F("Value: "));
    Serial.println(value);  

    if (command[0] == 'a') { 
      if (strlen(value) > 0) {
        tmpuI = strtoul(value, NULL, 10);
        if ((tmpuI >= 0) && (tmpuI <= 9)) {
          sprintf_P(tmpStr, PSTR("a received with %lu\r\n"), tmpuI);  
          Serial.print(tmpStr);
        } else {
          sprintf_P(tmpStr, PSTR("a received with %lu out of valid range 0..99999"), tmpuI); 
          Serial.println(tmpStr);
        }
      } else {
          sprintf_P(tmpStr, PSTR("a received without argument\r\n"));  
          Serial.print(tmpStr);        
      }
    }
    
    else if (command[0] == 'b') { 
      if (strlen(value) > 0) {
        tmpI = strtol(value, NULL, 10);
        if ((tmpI >= 0) && (tmpI <= 9)) {
          sprintf_P(tmpStr, PSTR("a received with %ld\r\n"), tmpI);  
          Serial.print(tmpStr);
        } else {
          sprintf_P(tmpStr, PSTR("a received with %ld out of valid range 0..99999"), tmpI); 
          Serial.println(tmpStr);
        }
      } else {
          sprintf_P(tmpStr, PSTR("b received without argument\r\n"));  
          Serial.print(tmpStr);        
      }
    }
    
    else if (command[0] == 'c') {  
      if (strlen(value) > 0) {
        tmpF = strtof(value,NULL);
        if ((tmpF >= 0.0) && (tmpF <= 10.0)) {
          sprintf_P(tmpStr, PSTR("c received: %f"), tmpF); 
          Serial.println(tmpStr);
        } else {
          sprintf_P(tmpStr, PSTR("c recevied %f out of valid Range 0..10"), tmpF); 
          Serial.println(tmpStr);
        }
      } else {
          sprintf_P(tmpStr, PSTR("c received without argument\r\n"));  
          Serial.print(tmpStr);        
      }
    }
    else if (command[0] == 'd') { 
      if (strlen(value) == 0) {
        sprintf_P(tmpStr, PSTR("d received with no value")); 
        Serial.println(tmpStr);
      } else {
        sprintf_P(tmpStr, PSTR("d expected with no value but recevied value %s"), value); 
        Serial.println(tmpStr);        
      }
    }
    
    else if (command[0] == 'e') {   
      if (strlen(value) > 0) { // we have a value
        tmpI = strtol(value,&pEnd,10);
        strncpy(text,pEnd,sizeof(text));
        Serial.print(F("Length text: "));
        Serial.println(strlen(text));
        Serial.print(F("text: "));
        Serial.println(text);
        if (strlen(text) > 0) {
          switch (tmpI) {
            case 1:
              strncpy(rtext, text, sizeof(rtext));
              sprintf_P(tmpStr, PSTR("text 1 is: %s"), rtext); 
              Serial.println(tmpStr);
              break;
            case 2:
              strncpy(rtext, text, sizeof(rtext));
              sprintf_P(tmpStr, PSTR("text 2 is: %s"), rtext); 
              Serial.println(tmpStr);
              break;
            default:
              sprintf_P(tmpStr, PSTR("choice %ld with text: %s"), tmpI, text); 
              Serial.println(tmpStr);
              break;
          }
        } else {
          sprintf_P(tmpStr, PSTR("received e %ld without text"), tmpI); 
          Serial.println(tmpStr);          
        }
      } else {
        sprintf_P(tmpStr, PSTR("received e without option"), tmpI); 
        Serial.println(tmpStr);                  
      }
    }
      
    else if (command[0] =='?') {
      helpMenu();
    }
    
    else {
      Serial.print(F("Command not available: "));
      Serial.println(command);
    } 
  }
}
