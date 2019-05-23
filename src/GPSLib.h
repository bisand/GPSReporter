#include <NeoSWSerial.h>
#include <NeoGPS_cfg.h>

class GPSLib
{
private:
    NeoSWSerial *_serial1;
    bool _debug;
    char _buffer[255];
    uint32_t _index;
    bool _newline;
    uint32_t _oldlines;

    static void _handleRxChar( uint8_t c );
    void _clearBuffer();

public:
    GPSLib(/* args */);
    ~GPSLib();

    void setup(uint32_t baud, bool debug = false);
    void loop();
};
