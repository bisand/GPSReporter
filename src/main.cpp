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
unsigned long interval = 30000;

GSMSim gsm(RX, TX);
DHT dht(DHTPIN, DHTTYPE);

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

void sendMessage(JsonObject data)
{
  String message;
  serializeJson(data, message);

  String url = "https://bogenhuset.no/nodered/test";
  Serial.println(url);
  Serial.println(gsm.gprsConnectBearer());
  Serial.println(gsm.gprsGetIP()); // String ip address.
  Serial.println(gsm.gprsHTTPPost(url, message, "application/json"));
  Serial.println(gsm.gprsCloseConn()); 
}

void setup() {
  Serial.begin(BAUD);
  gsm.start(BAUD);
  dht.begin();

  // Serial.println(gsm.timeSetServer(2));
  // Serial.println(gsm.timeSyncFromServer());
  // Serial.println(gsm.timeGetRaw());
}

void loop() {

  if(millis() > lastMillis + interval)
  {
    // allocate the memory for the document
    const size_t CAPACITY = JSON_OBJECT_SIZE(11);
    StaticJsonDocument<CAPACITY> doc;

    // create an object
    JsonObject object = doc.to<JsonObject>();
    object["tmp"] = dht.readTemperature();        // Temperature
    object["wtp"] = 0.0;                          // Water temperature
    object["hum"] = dht.readHumidity();           // Humidity
    object["hix"] = dht.computeHeatIndex(object["tmp"], object["hum"], false); // Heat index
    object["lat"] = 0.0;                          // Latitude
    object["lon"] = 0.0;                          // Longitude
    object["hdg"] = 0.0;                          // Heading
    object["sog"] = 0.0;                          // Speed over ground
    object["qos"] = gsm.signalQuality();          // GPRS signal quality
    object["upt"] = millis();                     // Uptime

    sendMessage(object);
    lastMillis = millis();
  }

}