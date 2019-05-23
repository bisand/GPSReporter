#include "GPSLib.h"
#include <NMEAGPS.h>
#include <GPSport.h>

GPSLib::GPSLib(/* args */)
{
    _gps = new NMEAGPS(); // This parses the GPS characters
}

GPSLib::~GPSLib()
{
}

void GPSLib::_clearBuffer()
{
    for (size_t i = 0; i < sizeof(_buffer); i++)
    {
        _buffer[i] = '\0';
    }
}

void GPSLib::setup(uint32_t baud, bool debug = false)
{
    _debug = debug;
    // DEBUG_PORT.begin(baud);
    // while (!Serial)
    //     ;
    DEBUG_PORT.print(F("NMEAsimple.INO: started\n"));

    gpsPort.begin(baud);
}

void GPSLib::loop()
{
    while (_gps->available(gpsPort))
    {
        fix = _gps->read();

        if (!_debug)
            continue;

        DEBUG_PORT.print(F("Location: "));
        if (fix.valid.location)
        {
            DEBUG_PORT.print(fix.latitude(), 6);
            DEBUG_PORT.print(',');
            DEBUG_PORT.print(fix.longitude(), 6);
        }

        DEBUG_PORT.print(F(", Altitude: "));
        if (fix.valid.altitude)
            DEBUG_PORT.print(fix.altitude());

        DEBUG_PORT.println();
    }
}