#include "DHT.h"
#include "GPSLib.h"
#include "GPRSLib.h"
#include "EEPROM.h"
#include "ArduinoUniqueID.h"

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
unsigned long smsInterval = 10000;
unsigned long gpsLastMillis = 0;
unsigned long gpsInterval = 50;
bool usbReady = true;

#define CFG_OK_START 0
#define CFG_OK_LEN 2
#define CFG_OWNER_START CFG_OK_START + CFG_OK_LEN
#define CFG_OWNER_LEN 16
#define CFG_MMSI_START CFG_OWNER_START + CFG_OWNER_LEN
#define CFG_MMSI_LEN 16
#define CFG_SHIPNAME_START CFG_MMSI_START + CFG_MMSI_LEN
#define CFG_SHIPNAME_LEN 20
#define CFG_CALLSIGN_START CFG_SHIPNAME_START + CFG_SHIPNAME_LEN
#define CFG_CALLSIGN_LEN 10

char owner[CFG_OWNER_LEN];
static const char mmsi[] = "258117280";
static const char shipname[] = "Black Pearl";
static const char callsign[] = "LI5239";
static const char postUrl[] = "https://bogenhuset.no/nodered/ais/blackpearl";
static const char postContentType[] = "application/json";

//GSMSim gsm(RX, TX);
GPRSLib gprs;
GPSLib gpsLib;
DHT dht(DHTPIN, DHTTYPE);

void sendData(const char *data)
{
  char httpResult[32];

  gprs.connectBearer("telenor");
  delay(50);
  Serial.print(F("Posting data: "));
  Serial.println(gprs.httpPost(postUrl, data, postContentType, false, httpResult, sizeof(httpResult)));
  delay(50);
  gprs.gprsCloseConn();
}

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

void readConfig(uint16_t start, uint16_t length, char *data)
{
  for (size_t i = 0; i < length; i++)
  {
    data[i] = EEPROM.read(start + i);
  }
}

void writeConfig(uint16_t start, uint16_t length, const char *data)
{
  EEPROM.put(start, data);
  return;
  uint8_t slen = strlen(data);
  for (size_t i = 0; i < length; i++)
  {
    if (i < slen)
      EEPROM.write(start + i, data[i]);
    else
      EEPROM.write(start + i, '\0');
  }
}

void clearConfig()
{
  for (uint16_t i = 0; i < EEPROM.length(); i++)
    EEPROM.write(i, 0);
}

void initConfig()
{
  EEPROM.begin();
  char cfgOk[CFG_OK_LEN];
  readConfig(CFG_OK_START, CFG_OK_LEN, cfgOk);
  if (strcmp(cfgOk, "OK") != 0)
  {
    clearConfig();
    writeConfig(CFG_OK_START, CFG_OK_LEN, "OK");
    writeConfig(CFG_MMSI_START, CFG_MMSI_LEN, "000000000");
    writeConfig(CFG_SHIPNAME_START, CFG_SHIPNAME_LEN, "");
    writeConfig(CFG_CALLSIGN_START, CFG_CALLSIGN_LEN, "");
  }
}

void smsReceived(const char *tel, char *msg)
{
  // Serial.print(F("Receiving SMS from \""));
  // Serial.print(tel);
  // Serial.println(F("\""));
  // Serial.print(F("With message: \""));
  // Serial.print(msg);
  // Serial.println(F("\""));

  // char owner[CFG_OWNER_LEN];
  // readConfig(CFG_OWNER_START, CFG_OWNER_LEN, owner);
  // if (strlen(owner) < 1)
  // {
  //   strcpy(owner, tel);
  //   writeConfig(CFG_OWNER_START, CFG_OWNER_LEN, owner);
  // }
  // if (strcmp(owner, tel) != 0)
  // {
  //   Serial.print(F("User \""));
  //   Serial.print(tel);
  //   Serial.println(F("\" is not authenticated."));
  //   Serial.print(F("Expected: \""));
  //   Serial.print(owner);
  //   Serial.print(F("\""));
  //   return;
  // }
  // if (strcasestr(msg, "resetgsm") != NULL)
  // {
  //   Serial.println(F("Reset GSM"));
  //   // _resetGsm();
  // }
  // else if (strcasestr(msg, "reset") != NULL)
  // {
  //   Serial.println(F("Reset ALL"));
  //   // _reset();
  // }
  // else if (strcasestr(msg, "mmsi") != NULL)
  // {
  //   gprs.getValue(msg, "mmsi", 1, mmsi, sizeof(mmsi));
  //   writeConfig(CFG_MMSI_START, CFG_MMSI_LEN, mmsi);
  //   Serial.print(F("MMSI: "));
  //   Serial.println(mmsi);
  // }
  // else if (strcasestr(msg, "callsign") != NULL)
  // {
  //   gprs.getValue(msg, "callsign", 1, callsign, sizeof(callsign));
  //   writeConfig(CFG_CALLSIGN_START, CFG_CALLSIGN_LEN, callsign);
  //   Serial.print(F("Callsign: "));
  //   Serial.println(callsign);
  // }
  // else if (strcasestr(msg, "shipname") != NULL)
  // {
  //   gprs.getValue(msg, "shipname", 1, shipname, sizeof(shipname));
  //   writeConfig(CFG_SHIPNAME_START, CFG_SHIPNAME_LEN, shipname);
  //   Serial.print(F("Ship name: "));
  //   Serial.println(shipname);
  // }
  // else
  // {
  //   Serial.print(F("Unknown SMS: "));
  //   Serial.println(msg);
  // }
}

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

void setup()
{
  Serial.begin(BAUD);

  Serial.println(F(""));
  Serial.print(F("Starting..."));

  // initConfig();
  // writeConfig(CFG_MMSI_START, CFG_MMSI_LEN, "258117280");
  // writeConfig(CFG_SHIPNAME_START, CFG_SHIPNAME_LEN, "Black Pearl");
  // writeConfig(CFG_CALLSIGN_START, CFG_CALLSIGN_LEN, "LI5239");

  // readConfig(CFG_MMSI_START, CFG_MMSI_LEN, mmsi);
  // readConfig(CFG_SHIPNAME_START, CFG_SHIPNAME_LEN, shipname);
  // readConfig(CFG_CALLSIGN_START, CFG_CALLSIGN_LEN, callsign);

  if (GSM_DEBUG)
  {
    gprs.setup(BAUD, Serial, DEBUG);
    return;
  }

  gprs.setup(BAUD, Serial, DEBUG);
  delay(5000);

  // Init SMS.
  // gprs.setSmsCallback(smsReceived);
  // gprs.smsInit();

  // Init GPRS.
  gprs.gprsInit();

  delay(1000);

  while (!gprs.gprsIsConnected())
  {
    Serial.print(F("."));
    gprs.connectBearer(PSTR("telenor"));
    delay(5000);
  }
  Serial.println(F(""));
  Serial.println(F("Connected!"));

  // Init GPS.
  gpsLib.setup(9600, Serial, DEBUG);

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

  if (millis() > gpsLastMillis + gpsInterval)
  {
    gpsLib.loop();
    gpsLastMillis = millis();
  }
  else if (millis() > smsLastMillis + smsInterval)
  {
    // Serial.println(F("Checking SMS"));
    // gprs.smsRead();
    smsLastMillis = millis();
  }
  else if (millis() > lastMillis + interval)
  {
    Serial.println(F("Publishing result..."));
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

    uint8_t js = 255;
    char json[js];
    char tmpBuf[16];

    for (size_t i = 0; i < js; i++)
    {
      json[i] = '\0';
    }

    // Create Json string.
    strncat_P(json, PSTR("{"), js - strlen(json) - 1);
    strncat_P(json, PSTR("\"mmsi\":\""), js - strlen(json) - 1);
    strncat(json, mmsi, js - strlen(json) - 1); // MMSI
    strncat_P(json, PSTR("\",\"cs\":\""), js - strlen(json) - 1);
    strncat(json, callsign, js - strlen(json) - 1); // Call sign
    strncat_P(json, PSTR("\",\"sn\":\""), js - strlen(json) - 1);
    strncat(json, shipname, js - strlen(json) - 1); // Temperature
    strncat_P(json, PSTR("\",\"tmp\":"), js - strlen(json) - 1);
    strncat(json, dtostrf(temperature, 7, 2, tmpBuf), js - strlen(json) - 1); // Temperature
    strncat_P(json, PSTR(",\"hum\":"), js - strlen(json) - 1);
    strncat(json, dtostrf(humidity, 7, 2, tmpBuf), js - strlen(json) - 1); // Humidity
    strncat_P(json, PSTR(",\"hix\":"), js - strlen(json) - 1);
    strncat(json, dtostrf(heatIndex, 7, 2, tmpBuf), js - strlen(json) - 1); // Heat index
    strncat_P(json, PSTR(",\"lat\":"), js - strlen(json) - 1);
    strncat(json, dtostrf(latitude, 10, 6, tmpBuf), js - strlen(json) - 1); // Latitude
    strncat_P(json, PSTR(",\"lon\":"), js - strlen(json) - 1);
    strncat(json, dtostrf(longitude, 10, 6, tmpBuf), js - strlen(json) - 1); // Longitude
    strncat_P(json, PSTR(",\"hdg\":"), js - strlen(json) - 1);
    strncat(json, dtostrf(heading, 7, 2, tmpBuf), js - strlen(json) - 1); // Heading
    strncat_P(json, PSTR(",\"sog\":"), js - strlen(json) - 1);
    strncat(json, dtostrf(speed, 7, 2, tmpBuf), js - strlen(json) - 1); // Speed over ground
    strncat_P(json, PSTR(",\"qos\":"), js - strlen(json) - 1);
    strncat(json, dtostrf(qos, 7, 2, tmpBuf), js - strlen(json) - 1); // GPRS signal quality
    strncat_P(json, PSTR(",\"mem\":"), js - strlen(json) - 1);
    strncat(json, utoa(freeMemory(), tmpBuf, 10), js - strlen(json) - 1); // Free memory
    strncat_P(json, PSTR(",\"upt\":"), js - strlen(json) - 1);
    strncat(json, ultoa(uptime, tmpBuf, 10), js - strlen(json) - 1); // Uptime
    strncat_P(json, PSTR("}"), js - strlen(json) - 1);

    Serial.println(json);
    sendData(json);

    lastMillis = millis();
    Serial.println(F("Done!"));
  }
}