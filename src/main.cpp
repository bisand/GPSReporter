#include "DHT.h"
#include "GPSLib.h"
#include "GPRSLib.h"

#define RX 8
#define TX 9
#define RESET 2
#define DHTPIN 2      // Digital pin connected to the DHT sensor
#define DHTTYPE DHT22 // DHT 22  (AM2302), AM2321
#define BAUD 19200

unsigned long lastMillis = 0;
unsigned long interval = 1000;
bool usbReady = true;

//GSMSim gsm(RX, TX);
GPRSLib gprs;
GPSLib gpsLib;
DHT dht(DHTPIN, DHTTYPE);

void sendData(const char *data)
{
  char ipAddress[32];
  char httpResult[32];

  Serial.print(F("Connect Bearer: \0"));
  Serial.println(gprs.connectBearer("telenor"));
  // Serial.print(F("IP Address: "));
  // Serial.print(gprs.gprsGetIP(ipAddress, 32));
  // Serial.print(F(" - "));
  // Serial.println(ipAddress);
  Serial.print(F("HTTP POST: "));
  Serial.print(gprs.httpPost("https://bogenhuset.no/nodered/test", data, "application/json", false, httpResult, sizeof(httpResult)));
  Serial.println(httpResult);
  delay(1000);
  Serial.print(F("Closing connection: \0"));
  Serial.println(gprs.gprsCloseConn());
}

void setup()
{
  Serial.begin(BAUD);
  gprs.setup(BAUD, false);
  gpsLib.setup(9600, false);
  dht.begin();
  //usbGps.setup();
}

void loop()
{
  gpsLib.loop();

  if (millis() > lastMillis + interval)
  {
    interval = 15000;
    //Serial.print(usbGps.rmcData);

    // Obtain values
    float temperature = dht.readTemperature();
    float waterTemperature = 0.0;
    float humidity = dht.readHumidity();
    float heatIndex = dht.computeHeatIndex(temperature, humidity, false);
    int qos = gprs.signalQuality();
    unsigned long uptime = millis();
    float latitude = gpsLib.fix.latitude();
    float longitude = gpsLib.fix.longitude();
    float speed = gpsLib.fix.speed();
    float heading = gpsLib.fix.heading();

    uint8_t jsonSize = 255;
    char json[jsonSize];
    char tmpBuf[16];
    //memset(json, 0, jsonSize);

    // // Create Json string.
    strcat(json, "{");
    strcat(json, "\"tmp\":");
    strcat(json, dtostrf(temperature, 7, 2, tmpBuf)); // Temperature
    strcat(json, ",\"wtp\":");
    strcat(json, dtostrf(waterTemperature, 7, 2, tmpBuf)); // Water temperature
    strcat(json, ",\"hum\":");
    strcat(json, dtostrf(humidity, 7, 2, tmpBuf)); // Humidity
    strcat(json, ",\"hix\":");
    strcat(json, dtostrf(heatIndex, 7, 2, tmpBuf)); // Heat index
    strcat(json, ",\"lat\":");
    strcat(json, dtostrf(latitude, 10, 6, tmpBuf)); // Latitude
    strcat(json, ",\"lon\":");
    strcat(json, dtostrf(longitude, 10, 6, tmpBuf)); // Longitude
    strcat(json, ",\"hdg\":");
    strcat(json, dtostrf(heading, 7, 2, tmpBuf)); // Heading
    strcat(json, ",\"sog\":");
    strcat(json, dtostrf(speed, 7, 2, tmpBuf)); // Speed over ground
    strcat(json, ",\"qos\":");
    strcat(json, dtostrf(qos, 7, 2, tmpBuf)); // GPRS signal quality
    strcat(json, ",\"upt\":");
    strcat(json, ultoa(uptime, tmpBuf, 10)); // Uptime
    strcat(json, "}\0");

    Serial.println(json);
    sendData(json);
    lastMillis = millis();
  }
}