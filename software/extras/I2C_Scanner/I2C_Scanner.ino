/* 
* i2c_port_address_scanner
* Scans ports D0 to D7 on an ESP8266 and searches for I2C device. based on the original code
* available on Arduino.cc and later improved by user Krodal and Nick Gammon (www.gammon.com.au/forum/?id=10896)
* D8 throws exceptions thus it has been left out
*
*/

/*
 * For Sensi
I2C device found at address 0x20  ! LCD
I2C device found at address 0x27  ! LCD
I2C device found at address 0x57  ! MAX30105
I2C device found at address 0x58  ! SGP30
I2C device found at address 0x5A  ! MLX IR sensor
I2C device found at address 0x5B  ! CCS811
I2C device found at address 0x61  ! SCD30
I2C device found at address 0x69  ! SPS30
I2C device found at address 0x76  ! BME280
I2C device found at address 0x77  ! BME680
**********************************
*/
 
#include <Wire.h>

//uint8_t portArray[] = {16, 5, 4, 0, 2, 14, 12, 13};
//String portMap[] = {"D0", "D1", "D2", "D3", "D4", "D5", "D6", "D7"}; //for Wemos
uint8_t portArray[] = {5, 4, 0, 2, 14, 12};
String portMap[] = {"D1", "D2", "D3", "D4", "D5", "D6"}; //for Wemos


void scanPorts() { 
  for (uint8_t i = 0; i < sizeof(portArray); i++) {
    for (uint8_t j = 0; j < sizeof(portArray); j++) {
      if (i != j){
        Serial.print("Scanning (SDA : SCL) - " + portMap[i] + " : " + portMap[j] + " - ");
        Wire.begin(portArray[i], portArray[j]);
        check_if_exist_I2C();
      }
    }
  }
}

void check_if_exist_I2C() {
  byte error, address;
  int nDevices;
  nDevices = 0;
  for (address = 1; address < 127; address++ )  {
    // The i2c_scanner uses the return value of
    // the Write.endTransmisstion to see if
    // a device did acknowledge to the address.
    Wire.beginTransmission(address);
    error = Wire.endTransmission();

    if (error == 0){
      if (nDevices == 0) {
        Serial.print("\n**********************************");
      }
      Serial.print("\nI2C device found at address 0x");
      if (address < 16)
        Serial.print("0");
      Serial.print(address, HEX);
      Serial.print(" !");

      nDevices++;
    } else if (error == 4) {
      Serial.print("\nUnknow error at address 0x");
      if (address < 16)
        Serial.print("0");
      Serial.println(address, HEX);
    }
  } //for loop
  if (nDevices == 0)
    Serial.println("No I2C devices found");
  else
    Serial.print("\n**********************************\n");
  //delay(1000);           // wait 1 seconds for next scan, did not find it necessary
}

void setup() {
  pinMode(D6, OUTPUT);
  digitalWrite(D6, LOW);   // set CCS811 to wake up
  delay(1); // wakeuptim 50 microseconds
  Serial.begin(115200);
  while (!Serial);             // Leonardo: wait for serial monitor
  Serial.println("\n\nI2C Scanner to scan for devices on each port pair D0 to D7");
  scanPorts();
}

void loop() {
}
