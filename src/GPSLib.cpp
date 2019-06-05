#include "GPSLib.h"
#include <NMEAGPS.h>
#include <GPSport.h>

GPSLib::GPSLib()
{
}

GPSLib::~GPSLib()
{
}

void GPSLib::setup(uint32_t baud, bool debug)
{
    _debug = debug;

    gpsPort.begin(baud);
}

void GPSLib::loop()
{
    if (_gps.available(gpsPort))
    {
        gps_fix f = _gps.read();
        if (f.valid.location && f.latitude() != 0 && f.longitude() != 0)
            fix = f;

        if (!_debug)
            return;

        DBG_PRN(F("Location: "));
        if (fix.valid.location)
        {
            DBG_PRNFL(fix.latitude(), 6);
            DBG_PRN(',');
            DBG_PRNFL(fix.longitude(), 6);
        }

        DBG_PRN(F(", Altitude: "));
        if (fix.valid.altitude)
        {
            DBG_PRN(fix.altitude());
        }
        DBG_PRNLN();
    }
}