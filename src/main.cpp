#include "DHT.h"
#include "usbGps.h"
#include "GPRSLib.h"

#define RX 8
#define TX 9
#define RESET 2
#define DHTPIN 2      // Digital pin connected to the DHT sensor
#define DHTTYPE DHT22 // DHT 22  (AM2302), AM2321
#define BAUD 19200

unsigned long lastMillis = 0;
unsigned long interval = 30000;
bool usbReady = true;

//GSMSim gsm(RX, TX);
GPRSLib gprs;
DHT dht(DHTPIN, DHTTYPE);

void sendData(const char *data)
{
  char ipAddress[64];
  char httpResult[64];

  Serial.print("Connect Bearer: ");
  Serial.println(gprs.connectBearer("telenor"));
  // gprs.gprsGetIP(ipAddress);
  // Serial.println(ipAddress);
  // gprs.httpPost("https://bogenhuset.no/nodered/test", data, "application/json", false, httpResult, 32);
  // Serial.println(httpResult);
  delay(1000);
  Serial.print("Closing connection: ");
  Serial.println(gprs.gprsCloseConn());
}

void setup()
{
  Serial.begin(BAUD);
  //  gsm.start(BAUD);
  gprs.setup(BAUD, false);
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
    int qos = 0;
    gprs.signalQuality();
    unsigned long uptime = millis();
    char latitude[] = "5915.7709N";
    char longitude[] = "01028.7816E";
    float speed = 0.0;
    float heading = 0.0;

    byte jsonSize = 128;
    char *json = new char[jsonSize]();
    char *tmpBuf = new char[16]();
    memset(json, 0, jsonSize);

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
    strcat(json, "}");

    Serial.println(json);
    sendData(json);
    lastMillis = millis();

    delete [] json;
    delete [] tmpBuf;
  }
}