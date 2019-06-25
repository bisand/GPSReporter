#define SERIAL_PORT_HARDWARE_OPEN Serial1

#include "debug.h"
#include "DHT.h"
#include "GPSLib.h"
#include "GPRSLib.h"
#include "EEPROM.h"
#include "ArduinoJson.h"
#include <Wire.h>
#include "HMC5583L.h"

#if SERIAL_TX_BUFFER_SIZE > 16
#warning To increase available memory, you should set Hardware Serial buffers to 16. (framework-arduinoavr\cores\arduino\HardwareSerial.h)
#endif
#ifdef DBG
#warning Debugging is enabled. Remember to disable before production.
#endif

#define RX 8
#define TX 9
#define RESET 12

#define DHTPIN 33     // Digital pin connected to the DHT sensor
#define DHTTYPE DHT22 // DHT 22  (AM2302), AM2321
#define BAUD_SERIAL 115200
#define BAUD_GPRS 115200
#define BAUD_GPS 9600
#define FULL_DEBUG false
#define GSM_DEBUG false

/*****************************************************
 * Global declarations
 *****************************************************/
static const char postUrl[] PROGMEM = "https://bogenhuset.no/nodered/ais/blackpearl";
static const char postContentType[] PROGMEM = "application/json";

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

StaticJsonDocument<512> jsonDoc;
char gprsBuffer[255];
char tmpBuffer[8];
char imei[16];
char ccid[21];
char dateTime[20];
GPRSLib gprs(gprsBuffer, sizeof(gprsBuffer), RESET, Serial2);
GPSLib gpsLib(Serial1);
DHT dht(DHTPIN, DHTTYPE);
HMC5583L compass = HMC5583L(HMC5583L_DEFAULT_ADDRESS);
Config config;

/*****************************************************
 * Retrieves a PROGMEM constant and return a pointer
 * to its allocated memory location.
 * Remember to release the allocated result with free()
 *****************************************************/
char *pgm(const char *s)
{
  const __FlashStringHelper *str = (__FlashStringHelper *)FPSTR(s);
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
#if defined(ESP8266)
    EEPROM.write(i, t);
#else
#endif
  }
#if defined(ESP8266)
  EEPROM.commit();
#endif
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
    saveConfig();
  }
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
    if (strcmp(ccid, val) != 0)
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
  if (!gprs.gprsIsBearerOpen())
    gprs.gprsConnectBearer("telenor");

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

  return res == SUCCESS;
}

/*****************************************************
 * Get datetime from GPS data.
 *****************************************************/
char *getDate(char *buffer)
{
  sprintf_P(buffer, PSTR("%02d-%02d-%02dT%02d:%02d:%02d"), gpsLib.gps.date.year(), gpsLib.gps.date.month(), gpsLib.gps.date.day(), gpsLib.gps.time.hour(), gpsLib.gps.time.minute(), gpsLib.gps.time.second());
  return buffer;
}

/*****************************************************
 * Connect
 *****************************************************/
void connect()
{
  while (true)
  {
    if (gprs.gprsConnect("telenor"))
    {
      DBG_PRNLN(F("Connected!"));
      break;
    }
    else
    {
      DBG_PRNLN(F("Unable to connect! Retrying in 5 seconds..."));
      gprs.resetGsm();
      delay(6000);
    }
  }
}

/*****************************************************
 * Reconnect
 *****************************************************/
void reconnect()
{
  DBG_PRNLN(F("Disconnecting..."));
  if (!gprs.gprsDisconnect())
  {
    gprs.resetGsm();
    delay(6000);
  }
  DBG_PRNLN(F("Connecting..."));
  connect();
}

float getHeading()
{
  // I2C scanner. Scanning ...
  // Found address: 13 (0xD)
  // Found address: 119 (0x77)
  // Done.
  // Found 2 device(s).

  float currentAngle = compass.getAngle();
  return currentAngle;
}

/*****************************************************
 * 
 * More global variables.
 * 
 *****************************************************/
uint8_t qos = 99;
uint16_t pwr = 0;
float temp = 0.0;
float humi = 0.0;
float hidx = 0.0;
float heading = 0;

uint32_t publishCount = 0;
uint32_t errorCount = 0;

unsigned long gpsMillis = 0;
unsigned long sensMillis = 0;
unsigned long smsMillis = 0;
unsigned long pubMillis = 0;
unsigned long errResMillis = 0;
/*****************************************************
 * 
 * SETUP
 * 
 *****************************************************/
void setup()
{
  Wire.begin();
  compass.initialize();
  compass.setStartingAngle();

  Serial1.begin(BAUD_GPS, SERIAL_8N1, 25, 26);
  Serial2.begin(BAUD_GPRS, SERIAL_8N1, 14, 27);
  Serial.begin(BAUD_SERIAL);

  Serial.println(F(""));
  Serial.print(F("Starting..."));

  if (GSM_DEBUG)
  {
    gprs.setup(FULL_DEBUG);
    return;
  }

  gprs.setup(FULL_DEBUG);
  gprs.setSmsCallback(smsReceived);
  delay(5000);

  gprs.gprsInit();
  while (!gprs.gprsSimReady())
  {
    gprs.resetGsm();
    delay(6000);
  }

  DBG_PRNLN(F("..."));
  connect();

  if (gprs.gprsGetImei(imei, sizeof(imei)))
  {
    DBG_PRN(F("IMEI: "));
    DBG_PRNLN(imei);
  }

  if (gprs.gprsGetCcid(ccid, sizeof(ccid)))
  {
    DBG_PRN(F("CCID: "));
    DBG_PRNLN(ccid);
  }

  // Init SMS.
  gprs.smsInit();

  // Init GPS.
  gpsLib.setup(FULL_DEBUG);

  // Init Temperature sensor.
  dht.begin();
  Serial.println(F("Started!"));
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

  if (publishCount >= UINT32_MAX)
    publishCount = 0;
  if (errorCount >= UINT32_MAX)
    errorCount = 0;

  // Grab current "time".
  unsigned long currentMillis = millis();

  // Most of the time.
  if (currentMillis - gpsMillis >= 10UL)
  {
    gpsLib.loop();
    gpsMillis = currentMillis;
  }
  // Every 5 seconds
  if (currentMillis - sensMillis >= 5000UL)
  {
    DBG_PRN(sensMillis);
    DBG_PRN(F(" - Sensors "));
    qos = gprs.signalQuality();
    pwr = gprs.batteryVoltage();
    temp = dht.readTemperature();
    humi = dht.readHumidity();
    hidx = dht.computeHeatIndex(temp, humi, false);
    // heading = getHeading();
    DBG_PRNLN(F("read"));
    // DBG_PRN(F("Heading: "));
    // DBG_PRNLN(heading);

    sensMillis = currentMillis;
  }
  // Every 10 seconds.
  if (currentMillis - smsMillis >= 10000UL)
  {
    DBG_PRN(smsMillis);
    DBG_PRN(F(" - Checking SMS: "));
    int8_t r = gprs.smsRead();
    if (r == 0)
    {
      DBG_PRNLN(F("Done"));
    }
    else if (r > 0)
    {
      DBG_PRN(F("Found "));
      DBG_PRN(r);
      DBG_PRNLN(F(" SMS"));
    }
    else if (r == -1)
    {
      DBG_PRNLN(F("Timed out"));
      gprs.smsInit();
    }

    smsMillis = currentMillis;
  }
  // Every 5 minutes.
  if (currentMillis - errResMillis >= 300000UL)
  {
    DBG_PRN(errResMillis);
    DBG_PRNLN(F(" - Resetting error count"));
    errorCount = 0;
    errResMillis = currentMillis;
  }
  // Every 15 seconds.
  if (currentMillis - pubMillis >= 15000UL)
  {
    DBG_PRN(pubMillis);
    DBG_PRN(F(" - Publish data "));
    loadConfig();
    // Generate JSON document.
    jsonDoc["mmsi"].set(config.mmsi);
    jsonDoc["cs"].set(config.callsign);
    jsonDoc["sn"].set(config.shipname);
    jsonDoc["tmp"].set(temp);
    jsonDoc["hum"].set(humi);
    jsonDoc["hix"].set(hidx);
    jsonDoc["lat"].set(gpsLib.gps.location.lat());
    jsonDoc["lon"].set(gpsLib.gps.location.lng());
    jsonDoc["hdg"].set(heading); // Should be switched out with compass data.
    jsonDoc["cog"].set(gpsLib.gps.course.deg());
    jsonDoc["sog"].set(gpsLib.gps.speed.knots());
    jsonDoc["utc"].set(getDate(dateTime));
    jsonDoc["qos"].set(qos);
    jsonDoc["pwr"].set(pwr);
    jsonDoc["pub"].set(publishCount);
    jsonDoc["err"].set(errorCount);
    jsonDoc["ups"].set(currentMillis / 1000);
    bool res = sendJsonData(&jsonDoc);
    jsonDoc.clear();

    if (res)
    {
      publishCount = publishCount + 1;
      DBG_PRNLN(F("succeeded"));
    }
    else
    {
      errorCount = errorCount + 1;
      errResMillis = currentMillis;
      DBG_PRNLN(F("failed"));
      DBG_PRN(F("Error count: "));
      DBG_PRNLN(errorCount);
      // Try to reconnect if there are more that x amount of errors.
      if (errorCount > 10)
      {
        DBG_PRN(F("Error count is "));
        DBG_PRN(errorCount);
        DBG_PRNLN(F(". Reconnecting..."));
        reconnect();
        errorCount = 0;
      }
    }

    pubMillis = currentMillis;
  }
}
