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
GPSLib gps;
DHT dht(DHTPIN, DHTTYPE);

void sendData(const char *data)
{
  char ipAddress[32];
  char httpResult[32];

  Serial.print(F("Connect Bearer: "));
  Serial.println(gprs.connectBearer("telenor"));
  Serial.print(F("IP Address: "));
  Serial.print(gprs.gprsGetIP(ipAddress, 32));
  Serial.print(F(" - "));
  Serial.println(ipAddress);
  Serial.print(F("HTTP POST: "));
  Serial.print(gprs.httpPost("https://bogenhuset.no/nodered/test", data, "application/json", false, httpResult, sizeof(httpResult)));
  Serial.println(httpResult);
  delay(1000);
  Serial.print(F("Closing connection: "));
  Serial.println(gprs.gprsCloseConn());
}

void setup()
{
  Serial.begin(BAUD);
  gprs.setup(BAUD, true);
  gps.setup(BAUD, true);
  dht.begin();
  //usbGps.setup();
}

void loop()
{
  //usbGps.loop();

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
    char latitude[] = "5915.7709N";
    char longitude[] = "01028.7816E";
    float speed = 0.0;
    float heading = 0.0;

    uint8_t jsonSize = 255;
    char json[jsonSize];
    char tmpBuf[16];
    //memset(json, 0, jsonSize);

    // // Create Json string.
    strcat(json, "{");
    strcat(json, "\"tmp\":");
    strcat(json, dtostrf(temperature, 5, 2, tmpBuf)); // Temperature
    strcat(json, ",\"wtp\":");
    strcat(json, dtostrf(waterTemperature, 5, 2, tmpBuf)); // Water temperature
    strcat(json, ",\"hum\":");
    strcat(json, dtostrf(humidity, 5, 2, tmpBuf)); // Humidity
    strcat(json, ",\"hix\":");
    strcat(json, dtostrf(heatIndex, 5, 2, tmpBuf)); // Heat index
    strcat(json, ",\"lat\":");
    strcat(json, latitude); // Latitude
    strcat(json, ",\"lon\":");
    strcat(json, longitude); // Longitude
    strcat(json, ",\"hdg\":");
    strcat(json, dtostrf(heading, 5, 2, tmpBuf)); // Heading
    strcat(json, ",\"sog\":");
    strcat(json, dtostrf(speed, 5, 2, tmpBuf)); // Speed over ground
    strcat(json, ",\"qos\":");
    strcat(json, dtostrf(qos, 5, 2, tmpBuf)); // GPRS signal quality
    strcat(json, ",\"upt\":");
    strcat(json, dtostrf(uptime, 5, 2, tmpBuf)); // Uptime
    strcat(json, "}\0");

    Serial.println(json);
    sendData(json);
    lastMillis = millis();
  }
}