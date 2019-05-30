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
  Serial.println(gprs.httpPost("https://bogenhuset.no/nodered/ais/blackpearl", data, "application/json", false, httpResult, sizeof(httpResult)));
  delay(50);
  gprs.gprsCloseConn();
}

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
char mmsi[CFG_MMSI_LEN];
char shipname[CFG_SHIPNAME_LEN];
char callsign[CFG_CALLSIGN_LEN];

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
  char cfgOk[CFG_OK_LEN];
  readConfig(CFG_OK_START, CFG_OK_LEN, cfgOk);
  if (strcmp(cfgOk, "OK") != 0)
  {
    for (uint16_t i = 0; i < EEPROM.length(); i++)
    {
      EEPROM.write(i, 0);
    }
    writeConfig(CFG_OK_START, CFG_OK_LEN, "OK");
    writeConfig(CFG_MMSI_START, CFG_MMSI_LEN, "000000000");
    writeConfig(CFG_SHIPNAME_START, CFG_SHIPNAME_LEN, "");
    writeConfig(CFG_CALLSIGN_START, CFG_CALLSIGN_LEN, "");
  }
}

void smsReceived(const char *tel, char *msg)
{
  Serial.print(F("Receiving SMS from \""));
  Serial.print(tel);
  Serial.println(F("\""));
  Serial.print(F("With message: \""));
  Serial.print(msg);
  Serial.println(F("\""));

  char owner[CFG_OWNER_LEN];
  readConfig(CFG_OWNER_START, CFG_OWNER_LEN, owner);
  if (strlen(owner) < 1)
  {
    strcpy(owner, tel);
    writeConfig(CFG_OWNER_START, CFG_OWNER_LEN, owner);
  }
  if (strcmp(owner, tel) != 0)
  {
    Serial.print(F("User \""));
    Serial.print(tel);
    Serial.println(F("\" is not authenticated."));
    Serial.print(F("Expected: \""));
    Serial.print(owner);
    Serial.print(F("\""));
    return;
  }
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
    char tmp[CFG_MMSI_LEN];
    gprs.getValue(msg, "mmsi", 1, tmp, sizeof(tmp));
    // writeConfig(CFG_MMSI_START, CFG_MMSI_LEN, tmp);
    // strcpy(mmsi, tmp);
    Serial.print(F("MMSI: "));
    Serial.println(tmp);
  }
  else if (strcasestr(msg, "callsign") != NULL)
  {
    char tmp[CFG_CALLSIGN_LEN];
    gprs.getValue(msg, "callsign", 1, tmp, sizeof(tmp));
    // writeConfig(CFG_CALLSIGN_START, CFG_CALLSIGN_LEN, tmp);
    // strcpy(callsign, tmp);
    Serial.print(F("Callsign: "));
    Serial.println(tmp);
  }
  else if (strcasestr(msg, "shipname") != NULL)
  {
    char tmp[CFG_SHIPNAME_LEN];
    gprs.getValue(msg, "shipname", 1, tmp, sizeof(tmp));
    // writeConfig(CFG_SHIPNAME_START, CFG_SHIPNAME_LEN, tmp);
    // strcpy(shipname, tmp);
    Serial.print(F("Ship name: "));
    Serial.println(tmp);
  }
  else
  {
    Serial.print(F("Unknown SMS: "));
    Serial.println(msg);
  }
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

  initConfig();
  readConfig(CFG_MMSI_START, CFG_MMSI_LEN, mmsi);
  readConfig(CFG_SHIPNAME_START, CFG_SHIPNAME_LEN, shipname);
  readConfig(CFG_CALLSIGN_START, CFG_CALLSIGN_LEN, callsign);

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
    Serial.println(F("Checking SMS"));
    gprs.smsRead();
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
    strcat(json, "\"mmsi\":\"");
    strcat(json, mmsi); // Temperature
    strcat(json, "\",\"callsign\":\"");
    strcat(json, callsign); // Temperature
    strcat(json, "\",\"shipname\":\"");
    strcat(json, shipname); // Temperature
    strcat(json, "\",\"tmp\":");
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
    strcat(json, ",\"mem\":");
    strcat(json, utoa(freeMemory(), tmpBuf, 10)); // Uptime
    strcat(json, ",\"upt\":");
    strcat(json, ultoa(uptime, tmpBuf, 10)); // Uptime
    strcat(json, "}");

    // Serial.println(json);
    sendData(json);

    lastMillis = millis();
    Serial.println(F("Done!"));

  }
}