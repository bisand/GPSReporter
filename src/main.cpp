#include "debug.h"
#include "DHT.h"
#include "GPSLib.h"
#include "GPRSLib.h"
#include "EEPROM.h"
#include "ArduinoJson.h"

#if SERIAL_TX_BUFFER_SIZE > 16
#warning To increase available memory, you should set Hardware Serial buffers to 16. (framework-arduinoavr\cores\arduino\HardwareSerial.h)
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
uint64_t lastMillis = 0;
uint64_t interval = 15000;
uint64_t sensLastMillis = 0;
uint64_t sensInterval = 5000;
uint64_t smsLastMillis = 0;
uint64_t smsInterval = 30000;
uint64_t gpsLastMillis = 0;
uint64_t gpsInterval = 50;
bool resetAll = false;

struct Config
{
  char owner[16];
  char mmsi[16];
  char shipname[20];
  char callsign[10];
  uint32_t checksum;
};

const char postUrl[] PROGMEM = "https://bogenhuset.no/nodered/ais/blackpearl";
const char postContentType[] PROGMEM = "application/json";

StaticJsonDocument<256> jsonDoc;
char gprsBuffer[128];
char tmpBuffer[8];
char imei[16];
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
  loadConfig();
  if (strlen(config.owner) < 1)
  {
    strcpy(config.owner, tel);
  }

  if (strcmp_P(cmd, PSTR("resetall")) == 0)
  {
    if (strcmp(imei, val) != 0)
      return;

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
    gprs.resetGsm();
  }
  else if (strcmp_P(cmd, PSTR("reset")) == 0)
  {
    gprs.resetAll();
  }
  else if (strcmp_P(cmd, PSTR("mmsi")) == 0)
  {
    strncpy(config.mmsi, val, sizeof(config.mmsi));
  }
  else if (strcmp_P(cmd, PSTR("callsign")) == 0)
  {
    strncpy(config.callsign, val, sizeof(config.callsign));
  }
  else if (strcmp_P(cmd, PSTR("shipname")) == 0)
  {
    strncpy(config.shipname, val, sizeof(config.shipname));
  }
  else
  {
  }
  saveConfig();
}

char p_buffer[50];
#define P(str) (strcpy_P(p_buffer, PSTR(str)), p_buffer)

/*****************************************************
 * Send data
 *****************************************************/
void sendJsonData(JsonDocument *data)
{
  gprs.connectBearer("telenor");
  delay(50);
  char tmpUrl[strlen_P(postUrl)];
  strcpy_P(tmpUrl, (char *)pgm_read_ptr(&postUrl));
  char tmpType[strlen_P(postContentType)];
  strcpy_P(tmpType, (char *)pgm_read_ptr(&postContentType));
  Result res = gprs.httpPostJson(tmpUrl, data, tmpType, true, tmpBuffer, sizeof(tmpBuffer));
  if (res != SUCCESS)
    DBG_PRNLN(F("HTTP POST failed!"));
  delay(50);
  gprs.gprsCloseConn();
  delay(50);
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

  if (millis() > gpsLastMillis + gpsInterval)
  {
    gpsLib.loop();
    gpsLastMillis = millis();
  }
  else if (millis() > sensLastMillis + sensInterval)
  {
    qos = gprs.signalQuality();
    delay(100);
    temp = dht.readTemperature();
    humi = dht.readHumidity();
    hidx = dht.computeHeatIndex(temp, humi, false);
    sensLastMillis = millis();
  }
  else if (millis() > smsLastMillis + smsInterval)
  {
    gprs.smsRead();
    smsLastMillis = millis();
  }
  else if (millis() > lastMillis + interval)
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
    jsonDoc["hdg"].set(gpsLib.fix.heading());
    jsonDoc["sog"].set(gpsLib.fix.speed());
    jsonDoc["qos"].set(qos);
    jsonDoc["upt"].set(millis());
    delay(50);
    sendJsonData(&jsonDoc);
    delay(200);
    jsonDoc.clear();
    delay(200);

    lastMillis = millis();
  }
}