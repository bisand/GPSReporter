#include <GSMSim.h>
#include "DHT.h"
#include <ArduinoJson.h>
#include <stdio.h>


#define RX 10
#define TX 11
#define RESET 2
#define DHTPIN 2        // Digital pin connected to the DHT sensor
#define DHTTYPE DHT22   // DHT 22  (AM2302), AM2321
#define BAUD 19200

unsigned long lastMillis = 0;
unsigned long interval = 15000;

GSMSim gsm(RX, TX);
DHT dht(DHTPIN, DHTTYPE);

struct gprsData
{
  float temperature = 0.0;
  float humidity = 0.0;
  float heatIndex = 0.0;
  float latitude = 0.0;
  float longitude = 0.0;
  float heading = 0.0;
  float SOG = 0.0;
  int signalQuality = 0;
  unsigned long uptime = 0;
};

void setup() {
  Serial.begin(BAUD);
  gsm.start(BAUD);
  dht.begin();
}

String urlencode(String str)
{
    int c;
    const char *hex = str.c_str();

    while( (c = getchar()) != EOF ){
      if( ('a' <= c && c <= 'z')
      || ('A' <= c && c <= 'Z')
      || ('0' <= c && c <= '9') ){
        putchar(c);
      } else {
        putchar('%');
        putchar(hex[c >> 4]);
        putchar(hex[c & 15]);
      }
    }

    String encodedString(hex);
    return encodedString;
    
}

// void sendMessage2(const String message)
// {
  // String url = "\"URL\",\"bogenhuset.no/nodered/iottest?message="+message+"\"";

  // SIM.ipBearer(SET, "3,1,\"Contype\",\"GPRS\"");    // Configure profile 1 connection as "GPRS"
  // SIM.ipBearer(SET, "3,1,\"APN\",\"telenor\"");    // Set profile 1 access point name to "internet"
  // SIM.ipBearer(SET, "1,1");                         // Open GPRS connection on profile 1
  // SIM.ipBearer(SET, "2,1");                         // Display IP address of profile 1
  // SIM.httpInit(EXE);                                // Initialize HTTP functionality
  // SIM.httpSsl(SET, "1");
  // SIM.httpParams(SET, "\"CID\",1");                 // Choose profile 1 as HTTP channel
  // SIM.httpParams(SET, url.c_str());   // Set URL to www.sim.com
  // SIM.httpAction(SET, "0");                         // Get the webpage
  // while(!SIM.available()) {;}                       // Wait until the webpage has arrived
  // SIM.httpRead(EXE);                                // Send the received webpage to Arduino
  // SIM.httpEnd(EXE);                                 // Terminate HTTP functionality
  // SIM.ipBearer(SET, "0,1");                         // Close GPRS connection on profile 1

  // Serial.println("Message sent.");
// }

void sendMessage(JsonObject data)
{
  String message = "";
  serializeJson(data, message);

  String url = "https://bogenhuset.no/nodered/test";
  Serial.println(url);
  Serial.println(gsm.gprsConnectBearer());
  Serial.println(gsm.gprsGetIP()); // String ip address.
  Serial.println(gsm.gprsHTTPPost(url.c_str(), message));
  Serial.println(gsm.gprsCloseConn()); 
}

void loop() {

  if(millis() > lastMillis + interval)
  {
    // allocate the memory for the document
    const size_t CAPACITY = JSON_OBJECT_SIZE(10);
    StaticJsonDocument<CAPACITY> doc;

    // create an object
    JsonObject object = doc.to<JsonObject>();
    object["t"] = dht.readTemperature();
    object["h"] = dht.readHumidity();
    object["hi"] = dht.computeHeatIndex(object["t"], object["h"], false);
    object["lat"] = 0.0;
    object["lon"] = 0.0;
    object["hd"] = 0.0;
    object["sog"] = 0.0;
    object["qos"] = gsm.signalQuality();
    object["upt"] = millis();

    sendMessage(object);
    lastMillis = millis();
  }

}