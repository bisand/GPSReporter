#include "GPSLib.h"
#include <NMEAGPS.h>
#include <GPSport.h>

GPSLib::GPSLib(/* args */)
{
    _gps = new NMEAGPS(); // This parses the GPS characters
}

GPSLib::~GPSLib()
{
    delete _gps;
}

void GPSLib::_clearBuffer()
{
    for (size_t i = 0; i < sizeof(_buffer); i++)
    {
        _buffer[i] = '\0';
    }
}

void GPSLib::setup(uint32_t baud, Stream &debugger, bool debug = false)
{
    _debug = debug;
    _debugger = debugger;
    _debugger.println(F("GPSLib started"));

    gpsPort.begin(baud);
}

void GPSLib::loop()
{
    while (_gps->available(gpsPort))
    {
        gps_fix f = _gps->read();
        if (f.valid.location && f.latitude() != 0 && f.longitude() != 0)
            fix = f;

        if (!_debug)
            continue;

        _debugger.print(F("Location: "));
        if (fix.valid.location)
        {
            _debugger.print(fix.latitude(), 6);
            _debugger.print(',');
            _debugger.print(fix.longitude(), 6);
        }

        _debugger.print(F(", Altitude: "));
        if (fix.valid.altitude)
        {
            _debugger.print(fix.altitude());
        }
        _debugger.println();
    }
}