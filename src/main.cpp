#include "DHT.h"
#include "usbGps.h"
#include "GPRSLib.h"

#define RX 7
#define TX 8
#define RESET 2
#define DHTPIN 2      // Digital pin connected to the DHT sensor
#define DHTTYPE DHT22 // DHT 22  (AM2302), AM2321
#define BAUD 19200

unsigned long lastMillis = 0;
unsigned long interval = 15000;
bool usbReady = true;
char json[128];

//GSMSim gsm(RX, TX);
GPRSLib gprs;
DHT dht(DHTPIN, DHTTYPE);

void sendData(const char *data)
{
  char ipAddress[16];
  char httpResult[32];

  gprs.connectBearer("telenor");
  gprs.gprsGetIP(ipAddress);
  Serial.println(ipAddress);
  gprs.httpPost("https://bogenhuset.no/nodered/test", data, "application/json", false, httpResult, 32);
  Serial.println(httpResult);
  gprs.gprsCloseConn();
}

void setup()
{
  Serial.begin(BAUD);
//  gsm.start(BAUD);
  gprs.setup(BAUD);
  dht.begin();
  //usbGps.setup();
}

void loop()
{
  //usbGps.loop();

  if (millis() > lastMillis + interval)
  {
    //Serial.print(usbGps.rmcData);

    // Obtain values
    float temperature = dht.readTemperature();
    float waterTemperature = 0.0;
    float humidity = dht.readHumidity();
    float heatIndex = dht.computeHeatIndex(temperature, humidity, false);
    int qos = 0; gprs.signalQuality();
    unsigned long uptime = millis();
    float latitude = 0.0;
    float longitude = 0.0;
    float speed = 0.0;
    float heading = 0.0;

    // // Create Json string.
    strcpy(json, "{}");
    // json += "\"tmp\":" + String(temperature);       // Temperature
    // json += ",\"wtp\":" + String(waterTemperature); // Water temperature
    // json += ",\"hum\":" + String(humidity);         // Humidity
    // json += ",\"hix\":" + String(heatIndex);        // Heat index
    // json += ",\"lat\":" + String(latitude);         // Latitude
    // json += ",\"lon\":" + String(longitude);        // Longitude
    // json += ",\"hdg\":" + String(heading);          // Heading
    // json += ",\"sog\":" + String(speed);            // Speed over ground
    // json += ",\"qos\":" + String(qos);              // GPRS signal quality
    // json += ",\"upt\":" + String(uptime);           // Uptime
    //json += "}";

    Serial.println(json);
    sendData(json);
    lastMillis = millis();
  }
}