#include "GPSLib.h"
#include <NeoSWSerial.h>

GPSLib::GPSLib(/* args */)
{
    _serial1 = new NeoSWSerial(4, 3);
}

GPSLib::~GPSLib()
{
}

void GPSLib::_handleRxChar(uint8_t c)
{
    // if (_index < sizeof(_buffer) - 1)
    // {
    //     _buffer[_index] = c;
    //     _buffer[_index + 1] = '\0';
    // }

    // if (c == '\n')
    //     _newlines++;
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
    _serial1->attachInterrupt(GPSLib::_handleRxChar);
    _serial1->begin(baud);
}

void GPSLib::loop()
{
    while (_serial1->available())
    {
        char c = _serial1->read();
        _buffer[_index++] = c;
        _buffer[_index] = '\0';
        if (c == '\n' || _index >= sizeof(_buffer) - 1)
        {
            _newline = true;
            break;
        }
    }
    if (_newline)
    {
        char tmp[255];
        _index = 0;
        strcpy(tmp, (char *)_buffer);

        if (_debug)
        {
            if (strlen(tmp) > 0)
            {
                Serial.println(tmp);
            }
        }
        _newline = false;

        _clearBuffer();
    }
}