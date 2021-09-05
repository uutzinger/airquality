/*

// Name address for Open Weather Map API
const char* server = "api.openweathermap.org";

//

// Replace with your unique URL resource
const char* resource = "REPLACE_WITH_YOUR_URL_RESOURCE";

//http://api.openweathermap.org/data/2.5/weather?q=Tucson,US&APPID=e05a9231d55d12a90f7e9d7903218b3c
const char* resource = "/data/2.5/weather?q=Porto,pt&appid=bd939aa3d23ff33d3c8f5dd1";

const unsigned long HTTP_TIMEOUT = 10000;  // max respone time from server
const size_t MAX_CONTENT_SIZE = 512;       // max size of the HTTP response

byte mac[] = {0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED};


// The type of data that we want to extract from the page
struct clientData {
  char temp[8];
  char humidity[8];
};


  if(connect(server)) {
    if(sendRequest(server, resource) && skipResponseHeaders()) {
      clientData clientData;
      if(readReponseContent(&clientData)) {
        printclientData(&clientData);
      }
    }
  }

  bool connect(const char* hostName) {
  Serial.print("Connect to ");
  Serial.println(hostName);

  bool ok = client.connect(hostName, 80);

  Serial.println(ok ? "Connected" : "Connection Failed!");
  return ok;
}

// Send the HTTP GET request to the server
bool sendRequest(const char* host, const char* resource) {
  Serial.print("GET ");
  Serial.println(resource);

  client.print("GET ");
  client.print(resource);
  client.println(" HTTP/1.1");
  client.print("Host: ");
  client.println(host);
  client.println("Connection: close");
  client.println();

  return true;
}

// Skip HTTP headers so that we are at the beginning of the response's body
bool skipResponseHeaders() {
  // HTTP headers end with an empty line
  char endOfHeaders[] = "\r\n\r\n";

  client.setTimeout(HTTP_TIMEOUT);
  bool ok = client.find(endOfHeaders);

  if (!ok) {
    Serial.println("No response or invalid response!");
  }
  return ok;
}
*/

// Parse the JSON from the input string and extract the interesting values
// Here is the JSON we need to parse
/*{
  "coord": {
    "lon":-110.9265,
    "lat":32.2217},
  "weather":[{
    "id":800,
    "main":"Clear",
    "description":"clear sky",
    "icon":"01d"}],
  "base":"stations",
  "main":{
    "temp":301.12,
    "feels_like":299.79,
    "temp_min":298.47,
    "temp_max":303.62,
    "pressure":1013,
    "humidity":16
  },
  "visibility":10000,
  "wind":{
    "speed":0.45,
    "deg":134,
    "gust":1.79
  },
  "clouds":{
    "all":1
  },
  "dt":1623422840,
  "sys":{
    "type":2,
    "id":2007774,
    "country":"US",
    "sunrise":1623413799,
    "sunset":1623465025},
    "timezone":-25200,
    "id":5318313,
    "name":"Tucson",
    "cod":200
  }
*/

/*
bool readReponseContent(struct clientData* clientData) {
  // Compute optimal size of the JSON buffer according to what we need to parse.
  // See https://bblanchon.github.io/ArduinoJson/assistant/
  // https://arduinojson.org/v5/assistant/

  const size_t bufferSize =
      JSON_ARRAY_SIZE(1) + JSON_OBJECT_SIZE(1) + JSON_OBJECT_SIZE(2) + 
      JSON_OBJECT_SIZE(3) + JSON_OBJECT_SIZE(4) + JSON_OBJECT_SIZE(5) + 
      JSON_OBJECT_SIZE(6) + JSON_OBJECT_SIZE(13)  + 391

  DynamicJsonBuffer jsonBuffer(bufferSize);

  JsonObject& root = jsonBuffer.parseObject(client);

  if (!root.success()) {
    Serial.println("JSON parsing failed!");
    return false;
  }

  // Here were copy the strings we're interested in using to your struct data
  strcpy(clientData->temp, root["main"]["temp"]);
  strcpy(clientData->humidity, root["main"]["humidity"]);
  // It's not mandatory to make a copy, you could just use the pointers
  // Since, they are pointing inside the "content" buffer, so you need to make
  // sure it's still in memory when you read the string

  return true;
}

// Print the data extracted from the JSON
void printclientData(const struct clientData* clientData) {
  Serial.print("Temp = ");
  Serial.println(clientData->temp);
  Serial.print("Humidity = ");
  Serial.println(clientData->humidity);
}

*/
