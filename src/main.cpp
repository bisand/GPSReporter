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
#define DEBUG false
#define GSM_DEBUG false

unsigned long lastMillis = 0;
unsigned long interval = 15000;
unsigned long smsLastMillis = 0;
unsigned long smsInterval = 30000;
unsigned long gpsLastMillis = 0;
unsigned long gpsInterval = 50;
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
#ifdef DEBUG
    Serial.print("Saved config invalid - using defaults ");
    Serial.print(config.checksum);
    Serial.print(" <> ");
    Serial.println(sum);
#endif
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
  Serial.print(F("Receiving SMS from \""));
  Serial.print(tel);
  Serial.println(F("\""));
  Serial.print(F("With message: \""));
  Serial.print(msg);
  Serial.println(F("\""));

  loadConfig();
  if (strlen(config.owner) < 1)
  {
    strcpy(config.owner, tel);
  }

  if (strcasestr(msg, "reset") != NULL)
  {
    char tmp[16];
    gprs.getValue(msg, "reset", 1, tmp, 16);
    if (strcmp(imei, tmp) != 0)
    {
      Serial.print(F("IMEI \""));
      Serial.print(tmp);
      Serial.println(F("\" is not authenticated."));
      Serial.print(F("Expected: \""));
      Serial.print(imei);
      Serial.println(F("\""));
      return;
    }
    Serial.println(F("Reset ALL"));
    defaultConfig();
    strcpy(config.owner, tel);
    saveConfig();
    delay(1000);
    resetAll = true;
  }

  if (strcmp(config.owner, tel) != 0)
  {
    Serial.print(F("User \""));
    Serial.print(tel);
    Serial.println(F("\" is not authenticated."));
    Serial.print(F("Expected: \""));
    Serial.print(config.owner);
    Serial.println(F("\""));
    return;
  }
  if (strcasestr(msg, "resetgsm") != NULL)
  {
    Serial.println(F("Reset GSM"));
    delay(1000);
    gprs.resetGsm();
  }
  else if (strcasestr(msg, "mmsi") != NULL)
  {
    gprs.getValue(msg, "mmsi", 1, config.mmsi, 16);
    Serial.print(F("MMSI: "));
    Serial.println(config.mmsi);
  }
  else if (strcasestr(msg, "callsign") != NULL)
  {
    gprs.getValue(msg, "callsign", 1, config.callsign, 10);
    Serial.print(F("Callsign: "));
    Serial.println(config.callsign);
  }
  else if (strcasestr(msg, "shipname") != NULL)
  {
    gprs.getValue(msg, "shipname", 1, config.shipname, 20);
    Serial.print(F("Ship name: "));
    Serial.println(config.shipname);
  }
  else
  {
    Serial.print(F("Unknown SMS: "));
    Serial.println(msg);
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
  Serial.print(F("Posting data: "));
  Serial.println(gprs.httpPostJson(postUrl, data, postContentType, true, tmpBuffer, sizeof(tmpBuffer)));
  Serial.println(tmpBuffer);
  delay(50);
  gprs.gprsCloseConn();
}

/*****************************************************
 * 
 * SETUP
 * 
 *****************************************************/
void setup()
{
  Serial.begin(BAUD);

  Serial.println(F(""));
  Serial.print(F("Starting..."));

  if (GSM_DEBUG)
  {
    gprs.setup(BAUD, Serial, DEBUG);
    return;
  }

  gprs.setup(BAUD, Serial, DEBUG);
  gprs.setSmsCallback(smsReceived);
  delay(5000);

  // Init GPRS.
  gprs.gprsInit();
  Serial.print(F("."));

  delay(500);

  // Init SMS.
  gprs.smsInit();
  Serial.print(F("."));

  delay(500);

  while (!gprs.gprsIsConnected())
  {
    Serial.print(F("."));
    gprs.connectBearer("telenor");
    delay(1000);
  }
  Serial.println(F("."));
  Serial.println(F("Connected!"));

  if (gprs.gprsGetImei(imei, sizeof(imei)))
  {
    Serial.print(F("IMEI: "));
    Serial.println(imei);
  }

  // Init GPS.
  gpsLib.setup(9600, Serial, DEBUG);

  // Init Temperature sensor.
  dht.begin();
}

/*****************************************************
 * 
 * LOOP
 * 
 *****************************************************/
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
  else if (millis() > smsLastMillis + smsInterval)
  {
    Serial.println(F("Checking SMS"));
    gprs.smsRead();
    smsLastMillis = millis();
  }
  else if (millis() > lastMillis + interval)
  {
    loadConfig();
    Serial.print(F("MMSI: "));
    Serial.println(config.mmsi);
    Serial.print(F("Callsign: "));
    Serial.println(config.callsign);
    Serial.print(F("Ship name: "));
    Serial.println(config.shipname);
    Serial.flush();

    // Generate JSON document.
    JsonObject obj = jsonDoc.to<JsonObject>();
    obj["mmsi"].set(config.mmsi);
    obj["cs"].set(config.callsign);
    obj["sn"].set(config.shipname);
    obj["tmp"].set(dht.readTemperature());
    obj["hum"].set(dht.readHumidity());
    obj["hix"].set(dht.computeHeatIndex(dht.readTemperature(), dht.readHumidity(), false));
    obj["lat"].set(gpsLib.fix.latitude());
    obj["lon"].set(gpsLib.fix.longitude());
    obj["hdg"].set(gpsLib.fix.heading());
    obj["sog"].set(gpsLib.fix.speed());
    obj["qos"].set(gprs.signalQuality());
    obj["upt"].set(millis());

    sendJsonData(&jsonDoc);

    lastMillis = millis();
    Serial.println(F("Done!"));
  }
}