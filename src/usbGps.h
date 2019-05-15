#include <Arduino.h>
#include <usbhub.h>
#include <cdcacm.h>
#include <cdcprolific.h>
#include <SPI.h>

#define READ_DELAY 100
#define DATA_BUFFER_SIZE 64

class PLAsyncOper : public CDCAsyncOper
{
public:
        uint8_t OnInit(ACM *pacm);
};

class UsbGps
{
private:
        bool _usbReady;
        char _data[DATA_BUFFER_SIZE*2];
        byte _dataIdx;

public:
        char rmcData[DATA_BUFFER_SIZE];
        byte rcmDataIdx;
        void setup();
        void loop();
};

USB Usb;
USBHub Hub(&Usb);
PLAsyncOper AsyncOper;
PL2303 Pl(&Usb, &AsyncOper);
uint32_t read_delay;
UsbGps usbGps;

uint8_t PLAsyncOper::OnInit(ACM *pacm)
{
        uint8_t rcode;

        // Set DTR = 1
        rcode = pacm->SetControlLineState(1);

        if (rcode)
        {
                ErrorMessage<uint8_t>(PSTR("SetControlLineState"), rcode);
                return rcode;
        }

        LINE_CODING lc;
        lc.dwDTERate = 9600; //default serial speed of GPS unit
        lc.bCharFormat = 0;
        lc.bParityType = 0;
        lc.bDataBits = 8;

        rcode = pacm->SetLineCoding(&lc);

        if (rcode)
                ErrorMessage<uint8_t>(PSTR("SetLineCoding"), rcode);

        return rcode;
}

void UsbGps::setup()
{
        _usbReady = true;
        if (Usb.Init() == -1)
        {
                _usbReady = false;
                Serial.println(F("Usb shield did not start."));
                return;
        }
        Serial.println(F("Usb shield successfully started."));
}

void UsbGps::loop()
{
        if(!_usbReady)
                return;

        uint8_t rcode;
        uint16_t rcvd = DATA_BUFFER_SIZE;
        uint8_t buf[DATA_BUFFER_SIZE]; //serial buffer equals Max.packet size of bulk-IN endpoint
        char ch;

        Usb.Task();

        if (Pl.isReady())
        {
                /* reading the GPS */
                if ((int32_t)((uint32_t)millis() - read_delay) >= 0L)
                {
                        read_delay += READ_DELAY;
                        rcode = Pl.RcvData(&rcvd, buf);
                        if (rcode && rcode != hrNAK)
                                ErrorMessage<uint8_t>(PSTR("Ret"), rcode);
                        if (rcvd)
                        { //more than zero bytes received
                                for (uint16_t i = 0; i < rcvd; i++)
                                {
                                        ch = (char)buf[i];
                                        if (ch == '$')
                                        {
                                                _dataIdx = 0;
                                                _data[_dataIdx] = ch;
                                        }
                                        else
                                        {
                                                _data[++_dataIdx]= ch;
                                        }
                                        if (ch == '\n')
                                        {
                                                _dataIdx = 0;
                                                if (strstr(_data, "$GPRMC") != NULL || strstr(_data, "$GNRMC") != NULL || strstr(_data, "$GLRMC") != NULL)
                                                {
                                                        strcpy(rmcData, _data);
                                                        Serial.println(rmcData);
                                                        _data[_dataIdx] = 0;                                                        
                                                }
                                                else
                                                {
                                                        _data[_dataIdx] = 0;
                                                }
                                        }
                                }
                        }
                }
        }
}
