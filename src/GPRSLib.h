#include <Arduino.h>
#include <AltSoftSerial.h>

#define DEFAULT_BAUD_RATE 9600
#define BUFFER_RESERVE_MEMORY 255
#define TIME_OUT_READ_SERIAL 5000

class GPRSLib
{
private:
    uint32_t _baud;
    uint8_t _timeout;
    char _buffer[BUFFER_RESERVE_MEMORY];
    AltSoftSerial *_serial;

    int _readSerial(char *buffer, unsigned int bufferSize, int startIndex = 0, unsigned int timeout = TIME_OUT_READ_SERIAL);
    void _extractTextBetween(const char *buffer, const int chr, char *output, unsigned int outputSize);

public:
    uint8_t RX_PIN;
    uint8_t TX_PIN;
    uint8_t RESET_PIN;
    uint8_t LED_PIN;
    bool LED_FLAG;
    uint32_t BAUDRATE;

    GPRSLib(/* args */);
    ~GPRSLib();

    void setup(unsigned int baud);
    void gprsGetIP(char *ipAddress);
    bool gprsCloseConn();
    bool gprsIsConnected();
    bool connectBearer();
    bool connectBearer(const char *apn);
    bool connectBearer(const char *apn, const char *username, const char *password);
    uint8_t signalQuality();
    int httpPost(const char *url, const char *data, const char *contentType, bool read, char *output, unsigned int outputSize);
};
