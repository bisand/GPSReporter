#include "GPSLib.h"
#include <NMEAGPS.h>
#include <GPSport.h>

GPSLib::GPSLib()
{
    _gps = new NMEAGPS(); // This parses the GPS characters
}

GPSLib::~GPSLib()
{
    delete _gps;
}

void GPSLib::setup(uint32_t baud, bool debug)
{
    _debug = debug;

    gpsPort.begin(baud);
}

void GPSLib::loop()
{
    if (_gps->available(gpsPort))
    {
        gps_fix f = _gps->read();
        if (f.valid.location && f.latitude() != 0 && f.longitude() != 0)
            fix = f;

        if (!_debug)
            return;

        DEBUG_PRINT(F("Location: "));
        if (fix.valid.location)
        {
            DEBUG_PRINTFLOAT(fix.latitude(), 6);
            DEBUG_PRINT(',');
            DEBUG_PRINTFLOAT(fix.longitude(), 6);
        }

        DEBUG_PRINT(F(", Altitude: "));
        if (fix.valid.altitude)
        {
            DEBUG_PRINT(fix.altitude());
        }
        DEBUG_PRINTLN();
    }
}