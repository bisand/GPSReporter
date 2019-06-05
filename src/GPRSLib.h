#include <Arduino.h>
#include <AltSoftSerial.h>
#include <ArduinoJson.h>
#include "debug.h"

#define BUFFER_RESERVE_MEMORY 128
#define TIME_OUT_READ_SERIAL 5000

#define BEARER_PROFILE_GPRS "AT+SAPBR=3,1,\"CONTYPE\",\"GPRS\"\r\n"
#define BEARER_PROFILE_APN "AT+SAPBR=3,1,\"APN\",\"%s\"\r\n"
#define QUERY_BEARER "AT+SAPBR=2,1\r\n"
#define OPEN_GPRS_CONTEXT "AT+SAPBR=1,1\r\n"
#define CLOSE_GPRS_CONTEXT "AT+SAPBR=0,1\r\n"
#define HTTP_INIT "AT+HTTPINIT\r\n"
#define HTTP_CID "AT+HTTPPARA=\"CID\",1\r\n"
#define HTTP_PARA "AT+HTTPPARA=\"URL\",\"%s\"\r\n"
#define HTTP_GET "AT+HTTPACTION=0\r\n"
#define HTTP_POST "AT+HTTPACTION=1\n"
#define HTTP_DATA "AT+HTTPDATA=%d,%d\r\n"
#define HTTP_READ "AT+HTTPREAD\r\n"
#define HTTP_CLOSE "AT+HTTPTERM\r\n"
#define HTTP_CONTENT "AT+HTTPPARA=\"CONTENT\",\"application/json\"\r\n"
#define HTTPS_ENABLE "AT+HTTPSSL=1\r\n"
#define HTTPS_DISABLE "AT+HTTPSSL=0\r\n"
#define NORMAL_MODE "AT+CFUN=1,1\r\n"
#define REGISTRATION_STATUS "AT+CREG?\r\n"
#define SIGNAL_QUALITY "AT+CSQ\r\n"
#define READ_VOLTAGE "AT+CBC\r\n"
#define SLEEP_MODE_2 "AT+CSCLK=2\r\n"
#define SLEEP_MODE_1 "AT+CSCLK=1\r\n"
#define SLEEP_MODE_0 "AT+CSCLK=0\r\n"
#define READ_GPS "AT+CIPGSMLOC=1,1\r\n"
#define OK "OK\r\n"
#define DOWNLOAD "DOWNLOAD"
#define HTTP_2XX ",2XX,"
#define HTTPS_PREFIX "https://"
#define CONNECTED "+CREG: 0,1"
#define ROAMING "+CREG: 0,5"
#define BEARER_OPEN "+SAPBR: 1,1"

enum Result {
  SUCCESS = 0,
  ERROR_INITIALIZATION = 1,
  ERROR_BEARER_PROFILE_GPRS = 2,
  ERROR_BEARER_PROFILE_APN = 3,
  ERROR_OPEN_GPRS_CONTEXT = 4,
  ERROR_QUERY_GPRS_CONTEXT = 5,
  ERROR_CLOSE_GPRS_CONTEXT = 6,
  ERROR_HTTP_INIT = 7,
  ERROR_HTTP_CID = 8,
  ERROR_HTTP_PARA = 9,
  ERROR_HTTP_GET = 10,
  ERROR_HTTP_READ = 11,
  ERROR_HTTP_CLOSE = 12,
  ERROR_HTTP_POST = 13,
  ERROR_HTTP_DATA = 14,
  ERROR_HTTP_CONTENT = 15,
  ERROR_NORMAL_MODE = 16,
  ERROR_LOW_CONSUMPTION_MODE = 17,
  ERROR_HTTPS_ENABLE = 18,
  ERROR_HTTPS_DISABLE = 19
};
enum ReadSerialResult
{
    CME_ERROR = -4,
    CMS_ERROR = -3,
    TIMEOUT = -2,
    INDEX_EXCEEDED_BUFFER_SIZE = -1,
    NOTHING_FOUND = 0,
    FOUND_EITHER_TEXT = 1,
    FOUND_OR_TEXT = 2
};

class GPRSLib
{
private:
    uint32_t _baud;
    uint32_t _timeout = TIME_OUT_READ_SERIAL;
    AltSoftSerial *_serial1;
    bool _debug;
    char *_buffer;
    char _tmpBuf[32];
    char _smsCmd[16];
    char _smsVal[32];
    uint16_t _bufferSize;

    void _clearBuffer(char *buffer, uint32_t size);
    int _readSerialUntil(char *buffer, uint8_t bufferSize, char *terminator, uint8_t startIndex, uint16_t timeout);
    int _readSerialUntilCrLf(char *buffer, uint8_t bufferSize);
    int _readSerialUntilCrLf(char *buffer, uint8_t bufferSize, uint8_t startIndex);
    int _readSerialUntilCrLf(char *buffer, uint8_t bufferSize, uint8_t startIndex, uint16_t timeout);
    ReadSerialResult _readSerialUntilOkOrError(char *buffer, uint8_t bufferSize);
    ReadSerialResult _readSerialUntilOkOrError(char *buffer, uint8_t bufferSize, uint16_t timeout);
    ReadSerialResult _readSerialUntilEitherOr(char *buffer, uint8_t bufferSize, const char *eitherText, const char *orText);
    ReadSerialResult _readSerialUntilEitherOr(char *buffer, uint8_t bufferSize, const char *eitherText, const char *orText, uint16_t timeout);
    int _writeSerial(const char *buffer);
    int _writeSerial(const __FlashStringHelper *buffer);
    void _extractTextBetween(const char *buffer, const int chr, char *output, unsigned int outputSize);
    bool _getResponseParams(char *buffer, const char *cmd, uint8_t paramNum, char *output, uint16_t outputLength);
    void _trimChar(char *buffer, char chr);
    void _removeChar(char *buffer, char chr);
    void (*_smsCallback)(const char* tel, char *cmd, char *val);
public:
    // uint8_t RX_PIN;
    // uint8_t TX_PIN;
    uint8_t RESET_PIN;
    uint8_t LED_PIN;
    bool LED_FLAG;
    uint32_t BAUDRATE;

    GPRSLib(char *buffer, uint16_t bufferSize);
    ~GPRSLib();

    void flush();
    void resetAll();
    void resetGsm();
    void setSmsCallback(void (*smsCallback)(const char* tel, char *cmd, char *val));
    void setup(uint32_t baud, bool debug = false);
    Result gprsGetIP(char *ipAddress, uint16_t bufferSize);
    Result gprsCloseConn();
    bool gprsGetImei(char *buffer, uint8_t bufferSize);
    bool gprsIsRegistered();
    bool gprsIsConnected();
    bool gprsInit();
    uint8_t gprsSimStatus();
    void gprsDebug();
    bool smsInit();
    void smsRead();
    Result connectBearer();
    Result connectBearer(const char *apn);
    Result connectBearer(const char *apn, const char *username, const char *password);
    uint8_t signalQuality();
    Result httpPostJson(const char *url, JsonDocument *data, const char *contentType, bool read, char *output, unsigned int outputSize);
    void lowerCmd(char *s);
    void lower(char *s);
    bool getValue(char *buffer, char *cmd, char *output, uint16_t outputLength);
    bool getSmsCmd(char *buffer, char *output, uint16_t outputLength);
    bool getSmsVal(char *buffer, char *output, uint16_t outputLength);
};
