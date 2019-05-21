#include <Arduino.h>
#include <AltSoftSerial.h>

#define BUFFER_RESERVE_MEMORY 255
#define TIME_OUT_READ_SERIAL 10000

class GPRSLib
{
private:
    uint32_t _baud;
    uint8_t _timeout;
    char _buffer[BUFFER_RESERVE_MEMORY];
    AltSoftSerial *_serial;
    bool _debug;

    int _readSerialUntilCrLf(char *buffer, uint32_t bufferSize, uint32_t startIndex = 0, uint32_t timeout = TIME_OUT_READ_SERIAL);
    int _readSerialUntilOkOrError(char *buffer, uint32_t bufferSize, uint32_t timeout = TIME_OUT_READ_SERIAL);
    int _readSerialUntilEitherOr(char *buffer, uint32_t bufferSize, const char *eitherText, const char *orText, uint32_t timeout = TIME_OUT_READ_SERIAL);
    int _writeSerial(const char *buffer);
    void _extractTextBetween(const char *buffer, const int chr, char *output, unsigned int outputSize);
    bool _getResponseParams(char *buffer, const char *cmd, uint8_t paramNum, char *output);
    void _trimChar(char *buffer, char chr);
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
