#include "DHT.h"
#include "GPSLib.h"
#include "GPRSLib.h"
#include "EEPROM.h"
#include "ArduinoJson.h"

#define RX 8
#define TX 9
#define RESET 2

#define DHTPIN 2      // Digital pin connected to the DHT sensor
#define DHTTYPE DHT22 // DHT 22  (AM2302), AM2321
#define BAUD 19200
#define FULL_DEBUG false
#define GSM_DEBUG false

uint64_t lastMillis = 0;
uint64_t interval = 15000;
uint64_t sensLastMillis = 0;
uint64_t sensInterval = 5000;
uint64_t smsLastMillis = 0;
uint64_t smsInterval = 30000;
uint64_t gpsLastMillis = 0;
uint64_t gpsInterval = 50;
bool usbReady = true;
bool resetAll = false;

/*****************************************************
 * Global declarations
 *****************************************************/
struct Config
{
  char owner[16];
  char mmsi[16];
  char shipname[20];
  char callsign[10];
  uint32_t checksum;
};

const char postUrl[] = "https://bogenhuset.no/nodered/ais/blackpearl\0";
const char postContentType[] = "application/json\0";

char gprsBuffer[128];
char tmpBuffer[32];
char imei[16];
StaticJsonDocument<256> jsonDoc;
GPRSLib gprs(gprsBuffer, sizeof(gprsBuffer));
GPSLib gpsLib;
DHT dht(DHTPIN, DHTTYPE);
Config config;

/* defaultConfig() is used when the config read from the EEPROM fails */
/* the integrity check (which usually happens after a change to the */
/* struct, or an EEPROM read failure) */
void defaultConfig()
{
  // config.mmsi[CFG_MMSI_LEN] = "258117280";
  // config.shipname[CFG_SHIPNAME_LEN] = "Black Pearl";
  // config.callsign[CFG_CALLSIGN_LEN] = "LI5239";
  memset(config.callsign, '\0', 10);
  memset(config.mmsi, '\0', 16);
  memset(config.owner, '\0', 16);
  memset(config.shipname, '\0', 20);
}

/* loadConfig() populates the settings variable, so call this before */
/* attempting to use settings. If the EEPROM read fails, then */
/* the defaultConfig() function will be called, so settings are always */
/* populated, one way or another. */
void loadConfig()
{
  config.checksum = 0;
  unsigned int sum = 0;
  unsigned char t;
  for (unsigned int i = 0; i < sizeof(config); i++)
  {
    t = (unsigned char)EEPROM.read(i);
    *((char *)&config + i) = t;
    if (i < sizeof(config) - sizeof(config.checksum))
    {
      /* Don't checksum the checksum! */
      sum = sum + t;
    }
  }
  /* Now check the data we just read */
  if (config.checksum != sum)
  {
    DEBUG_PRINT("Saved config invalid - using defaults ");
    DEBUG_PRINT(config.checksum);
    DEBUG_PRINT(" <> ");
    DEBUG_PRINTLN(sum);
    defaultConfig();
  }
}

/* saveConfig() writes to an EEPROM (or flash on an ESP board). */
/* Call this after making any changes to the setting variable */
/* The checksum will be calculated as the data is written */
void saveConfig()
{
  unsigned int sum = 0;
  unsigned char t;
  for (unsigned int i = 0; i < sizeof(Config); i++)
  {
    if (i == sizeof(config) - sizeof(config.checksum))
    {
      config.checksum = sum;
    }
    t = *((unsigned char *)&config + i);
    if (i < sizeof(config) - sizeof(config.checksum))
    {
      /* Don't checksum the checksum! */
      sum = sum + t;
    }
    EEPROM.update(i, t);
  }
#if defined(ESP8266)
  EEPROM.commit();
#endif
}

/*****************************************************
 * SMS received callback
 *****************************************************/
void smsReceived(const char *tel, char *msg)
{
  DEBUG_PRINT(F("Receiving SMS from \""));
  DEBUG_PRINT(tel);
  DEBUG_PRINTLN(F("\""));
  DEBUG_PRINT(F("With message: \""));
  DEBUG_PRINT(msg);
  DEBUG_PRINTLN(F("\""));

  loadConfig();
  if (strlen(config.owner) < 1)
  {
    strcpy(config.owner, tel);
  }

  if (strcasestr(msg, "resetall") != NULL)
  {
    char tmp[16];
    gprs.getValue(msg, "resetall", 1, tmp, 16);
    if (strcmp(imei, tmp) != 0)
    {
      DEBUG_PRINT(F("IMEI \""));
      DEBUG_PRINT(tmp);
      DEBUG_PRINTLN(F("\" is not authenticated."));
      DEBUG_PRINT(F("Expected: \""));
      DEBUG_PRINT(imei);
      DEBUG_PRINTLN(F("\""));
      return;
    }
    DEBUG_PRINTLN(F("Reset ALL"));
    defaultConfig();
    strcpy(config.owner, tel);
    saveConfig();
    delay(1000);
    resetAll = true;
  }

  if (strcmp(config.owner, tel) != 0)
  {
    DEBUG_PRINT(F("User \""));
    DEBUG_PRINT(tel);
    DEBUG_PRINTLN(F("\" is not authenticated."));
    DEBUG_PRINT(F("Expected: \""));
    DEBUG_PRINT(config.owner);
    DEBUG_PRINTLN(F("\""));
    return;
  }
  if (strcasestr(msg, "resetgsm") != NULL)
  {
    DEBUG_PRINTLN(F("Reset GSM"));
    delay(1000);
    gprs.resetGsm();
  }
  else if (strcasestr(msg, "reset") != NULL)
  {
    DEBUG_PRINTLN(F("Reset board"));
    delay(1000);
    gprs.resetAll();
  }
  else if (strcasestr(msg, "mmsi") != NULL)
  {
    gprs.getValue(msg, "mmsi", 1, config.mmsi, 16);
    DEBUG_PRINT(F("MMSI: "));
    DEBUG_PRINTLN(config.mmsi);
  }
  else if (strcasestr(msg, "callsign") != NULL)
  {
    gprs.getValue(msg, "callsign", 1, config.callsign, 10);
    DEBUG_PRINT(F("Callsign: "));
    DEBUG_PRINTLN(config.callsign);
  }
  else if (strcasestr(msg, "shipname") != NULL)
  {
    gprs.getValue(msg, "shipname", 1, config.shipname, 20);
    DEBUG_PRINT(F("Ship name: "));
    DEBUG_PRINTLN(config.shipname);
  }
  else
  {
    DEBUG_PRINT(F("Unknown SMS: "));
    DEBUG_PRINTLN(msg);
  }
  saveConfig();
}

/*****************************************************
 * Send data
 *****************************************************/
void sendJsonData(JsonDocument *data)
{
  gprs.connectBearer("telenor");
  delay(50);
  Result res = gprs.httpPostJson(postUrl, data, postContentType, true, tmpBuffer, sizeof(tmpBuffer));
  if(res != SUCCESS)
    DEBUG_PRINTLN(F("HTTP POST failed!"));
  delay(50);
  gprs.gprsCloseConn();
  delay(50);
  DEBUG_PRINTLN(res);
  DEBUG_PRINTLN(tmpBuffer);
}

/*****************************************************
 * 
 * SETUP
 * 
 *****************************************************/
void setup()
{
  Serial.begin(BAUD);

  DEBUG_PRINTLN(F(""));
  DEBUG_PRINT(F("Starting..."));

  if (GSM_DEBUG)
  {
    gprs.setup(BAUD, FULL_DEBUG);
    return;
  }

  gprs.setup(BAUD, FULL_DEBUG);
  gprs.setSmsCallback(smsReceived);
  delay(5000);

  // Init GPRS.
  gprs.gprsInit();
  DEBUG_PRINT(F("."));

  delay(500);

  // Init SMS.
  gprs.smsInit();
  DEBUG_PRINT(F("."));

  delay(500);

  while (!gprs.gprsIsConnected())
  {
    DEBUG_PRINT(F("."));
    gprs.connectBearer("telenor");
    delay(1000);
  }
  DEBUG_PRINTLN(F("."));
  DEBUG_PRINTLN(F("Connected!"));

  if (gprs.gprsGetImei(imei, sizeof(imei)))
  {
    DEBUG_PRINT(F("IMEI: "));
    DEBUG_PRINTLN(imei);
  }

  // Init GPS.
  gpsLib.setup(9600, FULL_DEBUG);

  // Init Temperature sensor.
  dht.begin();
  DEBUG_PRINTLN(F("Ready!"));
}

/*****************************************************
 * 
 * LOOP
 * 
 *****************************************************/
uint8_t qos = 99;
float temp, humi, hidx;

void loop()
{
  if (GSM_DEBUG)
  {
    gprs.gprsDebug();
    return;
  }

  if (resetAll)
    gprs.resetAll();

  if (millis() > gpsLastMillis + gpsInterval)
  {
    gpsLib.loop();
    gpsLastMillis = millis();
  }
  else if (millis() > sensLastMillis + sensInterval)
  {
    qos = gprs.signalQuality();
    delay(50);
    temp = dht.readTemperature();
    humi = dht.readHumidity();
    hidx = dht.computeHeatIndex(dht.readTemperature(), dht.readHumidity(), false);
    sensLastMillis = millis();
    DEBUG_PRINTLN(F("Sensors Done!"));
  }
  else if (millis() > smsLastMillis + smsInterval)
  {
    gprs.smsRead();
    smsLastMillis = millis();
    DEBUG_PRINTLN(F("SMS Done!"));
  }
  else if (millis() > lastMillis + interval)
  {
    loadConfig();
    DEBUG_PRINT(F("MMSI: "));
    DEBUG_PRINTLN(config.mmsi);
    DEBUG_PRINT(F("Callsign: "));
    DEBUG_PRINTLN(config.callsign);
    DEBUG_PRINT(F("Ship name: "));
    DEBUG_PRINTLN(config.shipname);

    // Generate JSON document.
    jsonDoc["mmsi"].set(config.mmsi);
    jsonDoc["cs"].set(config.callsign);
    jsonDoc["sn"].set(config.shipname);
    jsonDoc["tmp"].set(temp);
    jsonDoc["hum"].set(humi);
    jsonDoc["hix"].set(hidx);
    jsonDoc["lat"].set(gpsLib.fix.latitude());
    jsonDoc["lon"].set(gpsLib.fix.longitude());
    jsonDoc["hdg"].set(gpsLib.fix.heading());
    jsonDoc["sog"].set(gpsLib.fix.speed());
    jsonDoc["qos"].set(qos);
    jsonDoc["upt"].set(millis());

    sendJsonData(&jsonDoc);
    delay(100);
    jsonDoc.clear();
    delay(100);

    lastMillis = millis();
    DEBUG_PRINTLN(F("Http Done!"));
  }
}