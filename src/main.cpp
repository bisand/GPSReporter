#include "DHT.h"
#include "GPSLib.h"
#include "GPRSLib.h"
#include "EEPROM.h"

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

#define CONFIG_TEST_START 0
#define CONFIG_TEST_LENGTH 2
#define CONFIG_MMSI_START CONFIG_TEST_START + CONFIG_TEST_LENGTH
#define CONFIG_MMSI_LENGTH 16
#define CONFIG_NAME_START CONFIG_MMSI_START + CONFIG_MMSI_LENGTH
#define CONFIG_NAME_LENGTH 20
#define CONFIG_CALLSIGN_START CONFIG_NAME_START + CONFIG_NAME_LENGTH
#define CONFIG_CALLSIGN_LENGTH 10

char mmsi[CONFIG_MMSI_LENGTH];
char name[CONFIG_NAME_LENGTH];
char callsign[CONFIG_CALLSIGN_LENGTH];

void readConfig(uint16_t start, uint16_t length, char *data)
{
  for (size_t i = 0; i < length; i++)
  {
    data[i] = (char)EEPROM.read(start + i);
  }
}

void writeConfig(uint16_t start, uint16_t length, const char *data)
{
  for (size_t i = 0; i < length; i++)
  {
    if (i < strlen(data))
      EEPROM.update(start + i, (uint8_t)data[i]);
    else
      EEPROM.update(start + i, '\0');
  }
}

void initConfig()
{
  char cfgTest[CONFIG_TEST_LENGTH];
  readConfig(CONFIG_TEST_START, CONFIG_TEST_LENGTH, cfgTest);
  if (!strcmp(cfgTest, "OK"))
  {
    for (int i = 0; i < EEPROM.length(); i++)
    {
      EEPROM.write(i, 0);
    }
    writeConfig(CONFIG_TEST_START, CONFIG_TEST_LENGTH, "OK");
    writeConfig(CONFIG_MMSI_START, CONFIG_MMSI_LENGTH, "000000000");
    writeConfig(CONFIG_NAME_START, CONFIG_NAME_LENGTH, "");
    writeConfig(CONFIG_CALLSIGN_START, CONFIG_CALLSIGN_LENGTH, "");
  }
}

void smsReceived(const char *tel, const char *msg)
{
  if (strcasestr(msg, "resetgsm") != NULL)
  {
    Serial.println(F("Reset GSM"));
    // _resetGsm();
  }
  else if (strcasestr(msg, "reset") != NULL)
  {
    Serial.println(F("Reset ALL"));
    // _reset();
  }
  else if (strcasestr(msg, "mmsi") != NULL)
  {
    char tmp[CONFIG_MMSI_LENGTH];
    gprs.getValue(msg, "mmsi", 1, tmp, sizeof(tmp));
    writeConfig(CONFIG_MMSI_START, CONFIG_MMSI_LENGTH, tmp);
    strcpy(mmsi, tmp);
    Serial.print(F("MMSI: "));
    Serial.println(tmp);
  }
  else if (strcasestr(msg, "callsign") != NULL)
  {
    char tmp[CONFIG_CALLSIGN_LENGTH];
    gprs.getValue(msg, "callsign", 1, tmp, sizeof(tmp));
    writeConfig(CONFIG_CALLSIGN_START, CONFIG_CALLSIGN_LENGTH, tmp);
    strcpy(callsign, tmp);
    Serial.print(F("Callsign: "));
    Serial.println(tmp);
  }
  else if (strcasestr(msg, "shipname") != NULL)
  {
    char tmp[CONFIG_NAME_LENGTH];
    gprs.getValue(msg, "shipname", 1, tmp, sizeof(tmp));
    writeConfig(CONFIG_NAME_START, CONFIG_NAME_LENGTH, tmp);
    strcpy(name, tmp);
    Serial.print(F("Ship name: "));
    Serial.println(tmp);
  }
  else
  {
    Serial.println(msg);
  }
}

void setup()
{
  Serial.begin(BAUD);

  Serial.print(F("Starting..."));

  initConfig();
  readConfig(CONFIG_MMSI_START, CONFIG_MMSI_LENGTH, mmsi);
  readConfig(CONFIG_NAME_START, CONFIG_NAME_LENGTH, name);
  readConfig(CONFIG_CALLSIGN_START, CONFIG_CALLSIGN_LENGTH, callsign);

  if (GSM_DEBUG)
  {
    gprs.setup(BAUD, false);
    return;
  }

  gprs.setup(BAUD, false);
  delay(10000);
  // Init SMS.
  gprs.setSmsCallback(smsReceived);
  gprs.smsInit();
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
    strcat(json, "\"mmsi\":");
    strcat(json, mmsi); // Temperature
    strcat(json, "\"callsign\":");
    strcat(json, callsign); // Temperature
    strcat(json, "\"name\":");
    strcat(json, name); // Temperature
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