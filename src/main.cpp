#include "DHT.h"
#include "GPSLib.h"
#include "GPRSLib.h"
#include "EEPROM.h"
#include "ArduinoUniqueID.h"
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
StaticJsonDocument<256> jsonDoc;
GPRSLib gprs(gprsBuffer, sizeof(gprsBuffer));
GPSLib gpsLib;
DHT dht(DHTPIN, DHTTYPE);
Config config;

/*****************************************************
 * Free memory function
 *****************************************************/
#ifdef __arm__
// should use uinstd.h to define sbrk but Due causes a conflict
extern "C" char *sbrk(int incr);
#else  // __ARM__
extern char *__brkval;
#endif // __arm__

int freeMemory()
{
  char top;
#ifdef __arm__
  return &top - reinterpret_cast<char *>(sbrk(0));
#elif defined(CORE_TEENSY) || (ARDUINO > 103 && ARDUINO != 151)
  return &top - __brkval;
#else  // __arm__
  return __brkval ? &top - __brkval : &top - __malloc_heap_start;
#endif // __arm__
}

/*****************************************************
 * Get unique device ID
 *****************************************************/
void getUniqueId(char *id, uint8_t size)
{
  memset(id, '\0', size);
  for (size_t i = 0; i < 8; i++)
  {
    char num[2];
    itoa(UniqueID8[i], num, 16);
    strcat(id, num);
  }
}

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

  // char uniqueId[8];
  // getUniqueId(uniqueId, 8);
  // Config *config = new Config();
  // if (strstr(msg, uniqueId) != NULL)
  //   defaultConfig();
  // else
  loadConfig();
  if (strlen(config.owner) < 1)
  {
    strcpy(config.owner, tel);
  }
  if (strcmp(config.owner, tel) != 0)
  {
    Serial.print(F("User \""));
    Serial.print(tel);
    Serial.println(F("\" is not authenticated."));
    Serial.print(F("Expected: \""));
    Serial.print(config.owner);
    Serial.print(F("\""));
    return;
  }
  if (strcasestr_P(msg, PSTR("resetgsm")) != NULL)
  {
    Serial.println(F("Reset GSM"));
    delay(1000);
    gprs.resetGsm();
  }
  else if (strcasestr_P(msg, PSTR("reset")) != NULL)
  {
    Serial.println(F("Reset ALL"));
    delay(1000);
    gprs.resetGsm();
  }
  else if (strcasestr_P(msg, PSTR("mmsi")) != NULL)
  {
    gprs.getValue(msg, "mmsi", 1, config.mmsi, 16);
    Serial.print(F("MMSI: "));
    Serial.println(config.mmsi);
  }
  else if (strcasestr_P(msg, PSTR("callsign")) != NULL)
  {
    gprs.getValue(msg, "callsign", 1, config.callsign, 10);
    Serial.print(F("Callsign: "));
    Serial.println(config.callsign);
  }
  else if (strcasestr_P(msg, PSTR("shipname")) != NULL)
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
  Serial.println(gprs.httpPostJson(postUrl, data, postContentType, false, tmpBuffer, sizeof(tmpBuffer)));
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
  // char uniqueId[16];
  // getUniqueId(uniqueId, 16);
  // Serial.print(F("Unique ID: "));
  // Serial.println(uniqueId);


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
    Serial.println(config.mmsi);
    Serial.println(F("Creating json string..."));
    jsonDoc[F("mmsi")] = config.mmsi;
    jsonDoc[F("cs")] = config.callsign;
    jsonDoc[F("sn")] = config.shipname;
    jsonDoc[F("tmp")] = dht.readTemperature();
    jsonDoc[F("hum")] = dht.readHumidity();
    jsonDoc[F("hix")] = dht.computeHeatIndex(dht.readTemperature(), dht.readHumidity(), false);
    jsonDoc[F("lat")] = gpsLib.fix.latitude();
    jsonDoc[F("lon")] = gpsLib.fix.longitude();
    jsonDoc[F("hdg")] = gpsLib.fix.heading();
    jsonDoc[F("sog")] = gpsLib.fix.speed();
    jsonDoc[F("qos")] = gprs.signalQuality();
    jsonDoc[F("mem")] = freeMemory();
    jsonDoc[F("upt")] = millis();
    Serial.println(F("json string created."));

    // serializeJson(jsonDoc, Serial);
    // Serial.println(F(""));
    sendJsonData(&jsonDoc);

    lastMillis = millis();
    Serial.println(F("Done!"));
  }
}