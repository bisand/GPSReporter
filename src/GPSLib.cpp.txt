
#include "GPSLib.h"

GPSLib::GPSLib(Stream &serial) : _serial(serial)
{
}

GPSLib::~GPSLib()
{
}

void GPSLib::setup(bool debug)
{
    _debug = debug;
}

void GPSLib::loop()
{
    while (_serial.available() > 0)
    {
        if (gps.encode(_serial.read()))
        {
        }
    }
}
