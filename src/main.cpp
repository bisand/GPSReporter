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

#define CFG_OK_LEN 2
#define CFG_OWNER_LEN 16
#define CFG_MMSI_LEN 16
#define CFG_SHIPNAME_LEN 20
#define CFG_CALLSIGN_LEN 10

struct Config
{
  char ok[CFG_OK_LEN];
  char owner[CFG_OWNER_LEN];
  char mmsi[CFG_MMSI_LEN] = "258117280";
  char shipname[CFG_SHIPNAME_LEN] = "Black Pearl";
  char callsign[CFG_CALLSIGN_LEN] = "LI5239";
};
Config config;

const char postUrl[] = "https://bogenhuset.no/nodered/ais/blackpearl\0";
const char postContentType[] = "application/json\0";

//GSMSim gsm(RX, TX);
GPRSLib gprs;
GPSLib gpsLib;
DHT dht(DHTPIN, DHTTYPE);

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

/*****************************************************
 * Clear config
 *****************************************************/
void clearConfig()
{
  for (uint16_t i = 0; i < EEPROM.length(); i++)
    EEPROM.update(i, 0);
}

/*****************************************************
 * Init config
 *****************************************************/
void initConfig()
{
  EEPROM.begin();
  Config *cfg = new Config();
  EEPROM.get(0, cfg);
  if (strcmp(cfg->ok, "OK") != 0)
  {
    EEPROM.put(0, config);
  }
  else
  {
    EEPROM.get(0, config);
  }
  delete cfg;
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

  EEPROM.get(0, config);
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
    gprs.getValue(msg, "mmsi", 1, config.mmsi, CFG_MMSI_LEN);
    Serial.print(F("MMSI: "));
    Serial.println(config.mmsi);
  }
  else if (strcasestr_P(msg, PSTR("callsign")) != NULL)
  {
    gprs.getValue(msg, "callsign", 1, config.callsign, CFG_CALLSIGN_LEN);
    Serial.print(F("Callsign: "));
    Serial.println(config.callsign);
  }
  else if (strcasestr_P(msg, PSTR("shipname")) != NULL)
  {
    gprs.getValue(msg, "shipname", 1, config.shipname, CFG_SHIPNAME_LEN);
    Serial.print(F("Ship name: "));
    Serial.println(config.shipname);
  }
  else
  {
    Serial.print(F("Unknown SMS: "));
    Serial.println(msg);
  }
  EEPROM.put(0, config);
}

/*****************************************************
 * Send data
 *****************************************************/
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

  initConfig();

  if (GSM_DEBUG)
  {
    gprs.setup(BAUD, Serial, DEBUG);
    return;
  }

  gprs.setup(BAUD, Serial, DEBUG);
  delay(5000);

  // Init SMS.
  gprs.setSmsCallback(smsReceived);
  gprs.smsInit();

  // Init GPRS.
  gprs.gprsInit();

  delay(1000);

  while (!gprs.gprsIsConnected())
  {
    Serial.print(F("."));
    gprs.connectBearer("telenor");
    delay(1000);
  }
  Serial.println(F(""));
  Serial.println(F("Connected!"));

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
const uint8_t js = 255;
char json[js];
char tmpBuf[16];

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

    for (size_t i = 0; i < js; i++)
    {
      json[i] = '\0';
    }

    // Create Json string.
    strncat_P(json, PSTR("{"), js - strlen(json) - 1);
    strncat_P(json, PSTR("\"mmsi\":\""), js - strlen(json) - 1);
    strncat(json, config.mmsi, js - strlen(json) - 1); // MMSI
    strncat_P(json, PSTR("\",\"cs\":\""), js - strlen(json) - 1);
    strncat(json, config.callsign, js - strlen(json) - 1); // Call sign
    strncat_P(json, PSTR("\",\"sn\":\""), js - strlen(json) - 1);
    strncat(json, config.shipname, js - strlen(json) - 1); // Temperature
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