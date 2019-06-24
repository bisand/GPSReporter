#include <Arduino.h>
#include <ArduinoJson.h>
#include "debug.h"

#ifndef FPSTR
#define FPSTR(pstr_pointer) (reinterpret_cast<const __FlashStringHelper *>(pstr_pointer))
#endif
#define TIME_OUT_READ_SERIAL 10000

// #define BEARER_PROFILE_GPRS "AT+SAPBR=3,1,\"CONTYPE\",\"GPRS\"\r\n"
// #define BEARER_PROFILE_APN "AT+SAPBR=3,1,\"APN\",\"%s\"\r\n"
// #define QUERY_BEARER "AT+SAPBR=2,1\r\n"
// #define OPEN_GPRS_CONTEXT "AT+SAPBR=1,1\r\n"
// #define CLOSE_GPRS_CONTEXT "AT+SAPBR=0,1\r\n"
// #define HTTP_INIT "AT+HTTPINIT\r\n"
// #define HTTP_CID "AT+HTTPPARA=\"CID\",1\r\n"
// #define HTTP_PARA "AT+HTTPPARA=\"URL\",\"%s\"\r\n"
// #define HTTP_GET "AT+HTTPACTION=0\r\n"
// #define HTTP_POST "AT+HTTPACTION=1\n"
// #define HTTP_DATA "AT+HTTPDATA=%d,%d\r\n"
// #define HTTP_READ "AT+HTTPREAD\r\n"
// #define HTTP_CLOSE "AT+HTTPTERM\r\n"
// #define HTTP_CONTENT "AT+HTTPPARA=\"CONTENT\",\"application/json\"\r\n"
// #define HTTPS_ENABLE "AT+HTTPSSL=1\r\n"
// #define HTTPS_DISABLE "AT+HTTPSSL=0\r\n"
// #define NORMAL_MODE "AT+CFUN=1,1\r\n"
// #define REGISTRATION_STATUS "AT+CREG?\r\n"
// #define SIGNAL_QUALITY "AT+CSQ\r\n"
// #define READ_VOLTAGE "AT+CBC\r\n"
// #define SLEEP_MODE_2 "AT+CSCLK=2\r\n"
// #define SLEEP_MODE_1 "AT+CSCLK=1\r\n"
// #define SLEEP_MODE_0 "AT+CSCLK=0\r\n"
// #define READ_GPS "AT+CIPGSMLOC=1,1\r\n"
// #define OK "OK\r\n"
// #define DOWNLOAD "DOWNLOAD"
// #define HTTP_2XX ",2XX,"
// #define HTTPS_PREFIX "https://"
// #define CONNECTED "+CREG: 0,1"
// #define ROAMING "+CREG: 0,5"
// #define BEARER_OPEN "+SAPBR: 1,1"

enum Result : uint8_t {
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

enum ReadSerialResult : int8_t
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
    uint16_t _baud;
    uint16_t _timeout = TIME_OUT_READ_SERIAL;
    Stream &_serial1;
    bool _debug;
    char *_buffer;
    char _tmpBuf[32];
    char _smsCmd[16];
    char _smsVal[32];
    uint16_t _bufferSize;

    void _clearBuffer(char *buffer, uint8_t size);
    uint8_t _readSerialUntil(char *buffer, uint8_t bufferSize, char *terminator, uint8_t startIndex, unsigned long timeout);
    uint8_t _readSerialUntilCrLf(char *buffer, uint8_t bufferSize);
    uint8_t _readSerialUntilCrLf(char *buffer, uint8_t bufferSize, uint8_t startIndex);
    uint8_t _readSerialUntilCrLf(char *buffer, uint8_t bufferSize, uint8_t startIndex, unsigned long timeout);
    ReadSerialResult _readSerialUntilOkOrError(char *buffer, uint8_t bufferSize);
    ReadSerialResult _readSerialUntilOkOrError(char *buffer, uint8_t bufferSize, unsigned long timeout);
    ReadSerialResult _readSerialUntilEitherOr(char *buffer, uint8_t bufferSize, const char *eitherText, const char *orText);
    ReadSerialResult _readSerialUntilEitherOr(char *buffer, uint8_t bufferSize, const char *eitherText, const char *orText, unsigned long timeout);
    uint8_t _writeSerial(const char *buffer);
    uint8_t _writeSerial(const __FlashStringHelper *buffer);
    void _extractTextBetween(const char *buffer, const int chr, char *output, unsigned int outputSize);
    bool _getResponseParams(char *buffer, const char *cmd, uint8_t paramNum, char *output, uint16_t outputLength);
    void _trimChar(char *buffer, char chr);
    void _removeChar(char *buffer, char chr);
    void _lowerCmd(char *s);
    void _lower(char *s);
    void (*_smsCallback)(const char* tel, char *cmd, char *val);
public:
    // uint8_t RX_PIN;
    // uint8_t TX_PIN;
    uint8_t RESET_PIN;
    uint8_t LED_PIN;
    bool LED_FLAG;
    uint32_t BAUDRATE;

    GPRSLib(char *buffer, uint16_t bufferSize, uint8_t resetPin, Stream &serial);
    ~GPRSLib();

    void flush();
    void resetAll();
    void resetGsm();
    void setSmsCallback(void (*smsCallback)(const char* tel, char *cmd, char *val));
    void setup(bool debug = false);
    Result gprsGetIP(char *ipAddress, uint8_t bufferSize);
    Result gprsCloseBearer();
    bool gprsGetImei(char *buffer, uint8_t bufferSize);
    bool gprsGetCcid(char *buffer, uint8_t bufferSize);
    bool gprsRegister();
    bool gprsIsRegistered();
    bool gprsDetach();
    bool gprsAttach();
    bool gprsIsAttached();
    bool gprsIsBearerOpen();
    bool gprsInit();
    bool gprsSimReady();
    void gprsDebug();
    bool smsInit();
    int8_t smsRead();
    bool smsSend(const char *tel, const char *msg);
    Result gprsConnectBearer();
    Result gprsConnectBearer(const char *apn);
    Result gprsConnectBearer(const char *apn, const char *username, const char *password);
    bool gprsConnect();
    bool gprsConnect(const char *apn);
    bool gprsConnect(const char *apn, const char *username, const char *password);
    bool gprsConnect(const char *apn, const char *username, const char *password, uint8_t retryCount);
    bool gprsDisconnect();
    uint8_t signalQuality();
    uint16_t batteryVoltage();
    uint8_t batteryPercent();
    Result httpPostJson(const char *url, JsonDocument *data, const char *contentType, bool read, char *output, uint16_t outputSize);
    bool getValue(char *buffer, char *cmd, char *output, uint16_t outputLength);
    bool getSmsCmd(char *buffer, char *output, uint16_t outputLength);
    bool getSmsVal(char *buffer, char *output, uint16_t outputLength);
};
