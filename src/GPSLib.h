#include "debug.h"

#define RX_PIN 4
#define TX_PIN 3

#include <NeoSWSerial.h>
#include <NMEAGPS.h>

class GPSLib
{
private:
    bool _debug;
    uint32_t _index;
    bool _newline;
    uint32_t _oldlines;

    NMEAGPS _gps; // This parses the GPS characters

    void _clearBuffer();

public:
    GPSLib();
    ~GPSLib();

    gps_fix fix; // This holds on to the latest values

    void setup(uint32_t baud, bool debug = false);
    void loop();
};
