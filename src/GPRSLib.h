#include <Arduino.h>
#include <AltSoftSerial.h>

#define BUFFER_RESERVE_MEMORY 128
#define TIME_OUT_READ_SERIAL 10000

#define BEARER_PROFILE_GPRS "AT+SAPBR=3,1,\"Contype\",\"GPRS\"\r\n"
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

enum ReadSerialResult
{
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
    uint8_t _timeout;
    char _buffer[BUFFER_RESERVE_MEMORY];
    AltSoftSerial *_serial;
    bool _debug;

    int _readSerialUntilCrLf(char *buffer, uint32_t bufferSize, uint32_t startIndex = 0, uint32_t timeout = TIME_OUT_READ_SERIAL);
    ReadSerialResult _readSerialUntilOkOrError(char *buffer, uint32_t bufferSize, uint32_t timeout = TIME_OUT_READ_SERIAL);
    ReadSerialResult _readSerialUntilEitherOr(char *buffer, uint32_t bufferSize, const char *eitherText, const char *orText, uint32_t timeout = TIME_OUT_READ_SERIAL);
    int _writeSerial(const char *buffer);
    void _extractTextBetween(const char *buffer, const int chr, char *output, unsigned int outputSize);
    bool _getResponseParams(char *buffer, const char *cmd, uint8_t paramNum, char *output, uint16_t outputLength);
    void _trimChar(char *buffer, char chr);
    char *_getBuffer(size_t size);
public:
    uint8_t RX_PIN;
    uint8_t TX_PIN;
    uint8_t RESET_PIN;
    uint8_t LED_PIN;
    bool LED_FLAG;
    uint32_t BAUDRATE;

    GPRSLib(/* args */);
    ~GPRSLib();

    void setup(unsigned int baud, bool debug = false);
    bool gprsGetIP(char *ipAddress, uint8_t bufferSize);
    bool gprsCloseConn();
    bool gprsIsConnected();
    bool connectBearer();
    bool connectBearer(const char *apn);
    bool connectBearer(const char *apn, const char *username, const char *password);
    uint8_t signalQuality();
    int httpPost(const char *url, const char *data, const char *contentType, bool read, char *output, unsigned int outputSize);
};
