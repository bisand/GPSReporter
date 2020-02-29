#include "debug.h"
#include <Arduino.h>
#include <TinyGPS++.h>

class GPSLib
{
private:
    bool _debug;
    Stream &_serial;
    
public:
    GPSLib(Stream &serial);
    ~GPSLib();

    TinyGPSPlus gps;

    void setup(bool debug = false);
    void loop();
};
