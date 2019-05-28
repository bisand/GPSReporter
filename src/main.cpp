#include "DHT.h"
#include "GPSLib.h"
#include "GPRSLib.h"

#define RX 8
#define TX 9
#define RESET 2
#define DHTPIN 2      // Digital pin connected to the DHT sensor
#define DHTTYPE DHT22 // DHT 22  (AM2302), AM2321
#define BAUD 19200
#define GSM_DEBUG false

unsigned long lastMillis = 0;
unsigned long interval = 15000;
unsigned long smsLastMillis = 0;
unsigned long smsInterval = 5000;
unsigned long gpsLastMillis = 0;
unsigned long gpsInterval = 100;
bool usbReady = true;

//GSMSim gsm(RX, TX);
GPRSLib gprs;
GPSLib gpsLib;
DHT dht(DHTPIN, DHTTYPE);

void sendData(const char *data)
{
  char httpResult[32];

  gprs.connectBearer("telenor");
  delay(500);
  gprs.httpPost("https://bogenhuset.no/nodered/ais/blackpearl", data, "application/json", false, httpResult, sizeof(httpResult));
  delay(500);
  gprs.gprsCloseConn();
}

void setup()
{
  Serial.begin(BAUD);

  gprs.setup(BAUD, false);
  Serial.print(F("Starting..."));
  delay(10000);

  // Init GPRS.
  gprs.gprsInit();
  delay(1000);
  while (!gprs.gprsIsConnected())
  {
    Serial.print(".");
    gprs.connectBearer("telenor");
    delay(5000);
  }
  Serial.println("Connected!");
  gprs.smsInit();

  // Init GPS.
  gpsLib.setup(9600, BAUD, false);

  // Init Temperature sensor.
  dht.begin();
}

void loop()
{
  if (GSM_DEBUG)
  {
    gprs.gprsDebug();
    return;
  }

  gpsLib.loop();

  if (millis() > smsLastMillis + smsInterval)
  {
    gprs.smsRead();
    smsLastMillis = millis();
  }
  else if (millis() > lastMillis + interval)
  {
    // Obtain values
    float temperature = dht.readTemperature();
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

    for (size_t i = 0; i < jsonSize; i++)
    {
      json[i] = '\0';
    }

    // // Create Json string.
    strcat(json, "{");
    strcat(json, "\"tmp\":");
    strcat(json, dtostrf(temperature, 7, 2, tmpBuf)); // Temperature
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

    sendData(json);
    lastMillis = millis();
  }
}