#include "debug.h"
#include "DHT.h"
#include "GPSLib.h"
#include "GPRSLib.h"
#include "EEPROM.h"
#include "ArduinoJson.h"

#if SERIAL_TX_BUFFER_SIZE > 16
#warning To increase available memory, you should set Hardware Serial buffers to 16. (framework-arduinoavr\cores\arduino\HardwareSerial.h)
#endif
#ifdef DBG
#warning Debugging is enabled. Remember to disable before production.
#endif

#define RX 8
#define TX 9
#define RESET 2

#define DHTPIN 2      // Digital pin connected to the DHT sensor
#define DHTTYPE DHT22 // DHT 22  (AM2302), AM2321
#define BAUD 19200
#define FULL_DEBUG false
#define GSM_DEBUG false

/*****************************************************
 * Global declarations
 *****************************************************/
static const char postUrl[] PROGMEM = "https://bogenhuset.no/nodered/ais/blackpearl";
static const char postContentType[] PROGMEM = "application/json";

uint64_t lastMillis = 0;
uint64_t interval = 15000;
uint64_t sensLastMillis = 0;
uint64_t sensInterval = 5000;
uint64_t smsLastMillis = 0;
uint64_t smsInterval = 7500;
uint64_t gpsLastMillis = 0;
uint64_t gpsInterval = 50;
bool resetAll = false;

struct Config
{
  bool active;
  char owner[16];
  char mmsi[16];
  char shipname[20];
  char callsign[10];
  uint32_t checksum;
};

StaticJsonDocument<256> jsonDoc;
char gprsBuffer[128];
char tmpBuffer[8];
char imei[16];
GPRSLib gprs(gprsBuffer, sizeof(gprsBuffer));
GPSLib gpsLib;
DHT dht(DHTPIN, DHTTYPE);
Config config;

/*****************************************************
 * Retrieves a PROGMEM constant and return a pointer
 * to its allocated memory location.
 * Remember to release the allocated result with free()
 *****************************************************/
char *pgm(const char *s)
{
  const __FlashStringHelper *str = FPSTR(s);
  if (!str)
    return NULL; // return if the pointer is void
  size_t len = strlen_P((PGM_P)str) + 1;
  char *m = (char *)malloc(len);
  if (m == NULL)
    return NULL;
  if (len == 1)
  {
    m[0] = '\0';
    return m;
  }
  return (char *)memcpy_P(m, (PGM_P)str, len);
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
    DBG_PRN(F("Saved config invalid - using defaults "));
    DBG_PRN(config.checksum);
    DBG_PRN(F(" <> "));
    DBG_PRNLN(sum);
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
void smsReceived(const char *tel, char *cmd, char *val)
{
  DBG_PRNLN(F("New SMS received"));
  DBG_PRN(F("Command: "));
  DBG_PRNLN(cmd);
  DBG_PRN(F("Value: "));
  DBG_PRNLN(val);
  loadConfig();
  if (strlen(config.owner) < 1)
  {
    strcpy(config.owner, tel);
  }

  if (strcmp_P(cmd, PSTR("resetall")) == 0)
  {
    if (strcmp(imei, val) != 0)
      return;
    char *sms = pgm(PSTR("Restoring default values..."));
    gprs.smsSend(tel, sms);
    free(sms);

    defaultConfig();
    strcpy(config.owner, tel);
    saveConfig();
    delay(1000);
    resetAll = true;
  }

  if (strcmp(config.owner, tel) != 0)
    return;

  if (strcmp_P(cmd, PSTR("resetgsm")) == 0)
  {
    char *sms = pgm(PSTR("Restarting GSM module..."));
    gprs.smsSend(tel, sms);
    free(sms);
    gprs.resetGsm();
  }
  else if (strcmp_P(cmd, PSTR("reset")) == 0)
  {
    char *sms = pgm(PSTR("Restarting..."));
    gprs.smsSend(tel, sms);
    free(sms);
    delay(1000);
    resetAll = true;
  }
  else if (strcmp_P(cmd, PSTR("mmsi")) == 0)
  {
    strncpy(config.mmsi, val, sizeof(config.mmsi));
    saveConfig();
    char *sms = pgm(PSTR("MMSI successfully stored."));
    gprs.smsSend(tel, sms);
    free(sms);
  }
  else if (strcmp_P(cmd, PSTR("callsign")) == 0)
  {
    strncpy(config.callsign, val, sizeof(config.callsign));
    saveConfig();
    char *sms = pgm(PSTR("Callsign successfully stored."));
    gprs.smsSend(tel, sms);
    free(sms);
  }
  else if (strcmp_P(cmd, PSTR("shipname")) == 0)
  {
    strncpy(config.shipname, val, sizeof(config.shipname));
    saveConfig();
    char *sms = pgm(PSTR("MMSI successfully stored."));
    gprs.smsSend(tel, sms);
    free(sms);
  }
  else
  {
  }
}

/*****************************************************
 * Send data
 *****************************************************/
bool sendJsonData(JsonDocument *data)
{
  gprs.connectBearer("telenor");

  // Retrieve text from flash memory.
  char *tmpUrl = pgm(postUrl);
  char *tmpType = pgm(postContentType);
  if (tmpUrl == NULL || tmpType == NULL)
  {
    DBG_PRNLN(F("Failed to allocate memory for PostUrl or ContentType. Restarting..."));
    delay(1000);
    gprs.resetAll();
  }

  Result res = gprs.httpPostJson(tmpUrl, data, tmpType, true, tmpBuffer, sizeof(tmpBuffer));

  free(tmpUrl);
  free(tmpType);

  gprs.gprsCloseConn();

  return res == SUCCESS;
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
    gprs.setup(BAUD, FULL_DEBUG);
    return;
  }

  gprs.setup(BAUD, FULL_DEBUG);
  gprs.setSmsCallback(smsReceived);
  delay(5000);

  // Init GPRS.
  gprs.gprsInit();
  DBG_PRN(F("."));

  delay(500);

  // Init SMS.
  gprs.smsInit();
  DBG_PRN(F("."));

  delay(500);

  while (!gprs.gprsIsConnected())
  {
    DBG_PRN(F("."));
    gprs.connectBearer("telenor");
    delay(1000);
  }
  DBG_PRNLN(F("."));
  DBG_PRNLN(F("Connected!"));

  if (gprs.gprsGetImei(imei, sizeof(imei)))
  {
    DBG_PRN(F("IMEI: "));
    DBG_PRNLN(imei);
  }

  // Init GPS.
  gpsLib.setup(9600, FULL_DEBUG);

  // Init Temperature sensor.
  dht.begin();
  Serial.println(F("Started!"));
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

  if (millis() > lastMillis + interval)
  {
    loadConfig();
    // Generate JSON document.
    jsonDoc["mmsi"].set(config.mmsi);
    jsonDoc["cs"].set(config.callsign);
    jsonDoc["sn"].set(config.shipname);
    jsonDoc["tmp"].set(temp);
    jsonDoc["hum"].set(humi);
    jsonDoc["hix"].set(hidx);
    jsonDoc["lat"].set(gpsLib.fix.latitude());
    jsonDoc["lon"].set(gpsLib.fix.longitude());
    jsonDoc["hdg"].set(gpsLib.fix.heading()); // Should be switched out with compass data.
    jsonDoc["cog"].set(gpsLib.fix.heading());
    jsonDoc["sog"].set(gpsLib.fix.speed());
    jsonDoc["qos"].set(qos);
    jsonDoc["upt"].set(millis());
    bool res = sendJsonData(&jsonDoc);
    jsonDoc.clear();

    DBG_PRN(F("Publish data "));
    if (res)
      DBG_PRNLN(F("succeeded!"));
    else
      DBG_PRNLN(F("failed!"));

    lastMillis = millis();
  }
  if (millis() > smsLastMillis + smsInterval)
  {
    int8_t r = gprs.smsRead();
    if (r == 0)
      DBG_PRNLN(F("Checked SMS!"));
    else if (r > 0)
    {
      DBG_PRN(F("Found "));
      DBG_PRN(r);
      DBG_PRNLN(F(" SMS!"));
    }
    else if (r == -1)
      DBG_PRNLN(F("SMS check timed out!"));

    smsLastMillis = millis();
  }
  if (millis() > sensLastMillis + sensInterval)
  {
    qos = gprs.signalQuality();
    temp = dht.readTemperature();
    humi = dht.readHumidity();
    hidx = dht.computeHeatIndex(temp, humi, false);

    DBG_PRNLN(F("Read sensors!"));

    sensLastMillis = millis();
  }
  if (millis() > gpsLastMillis + gpsInterval)
  {
    gpsLib.loop();
    gpsLastMillis = millis();
  }
}