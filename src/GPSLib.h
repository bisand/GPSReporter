#include <NeoSWSerial.h>
#include <NMEAGPS.h>

class GPSLib
{
private:
    NeoSWSerial *_serial1;
    bool _debug;
    Stream *_debugger;
    uint32_t _index;
    bool _newline;
    uint32_t _oldlines;

    NMEAGPS *_gps; // This parses the GPS characters

    void _clearBuffer();

public:
    GPSLib();
    ~GPSLib();

    gps_fix fix; // This holds on to the latest values

    void setup(uint32_t baud, Stream &debugger, bool debug = false);
    void loop();
};
