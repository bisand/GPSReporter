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

void GPSLib::setup(uint32_t baud, uint32_t debugBaud, bool debug = false)
{
    _debug = debug;
    Serial.println(F("GPSLib started"));

    gpsPort.begin(baud);
}

void GPSLib::loop()
{
    while (_gps->available(gpsPort))
    {
        fix = _gps->read();

        if (!_debug)
            continue;

        Serial.print(F("Location: "));
        if (fix.valid.location)
        {
            Serial.print(fix.latitude(), 6);
            Serial.print(',');
            Serial.print(fix.longitude(), 6);
        }

        Serial.print(F(", Altitude: "));
        if (fix.valid.altitude){
            Serial.print(fix.altitude());
        }
        Serial.println();
    }
}