#include <Arduino.h>
#include <usbhub.h>
#include <cdcacm.h>
#include <cdcprolific.h>
#include <SPI.h>

#define READ_DELAY 100

class PLAsyncOper : public CDCAsyncOper
{
public:
        uint8_t OnInit(ACM *pacm);
};

class UsbGps
{
public:
        String readGpsData();
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

String UsbGps::readGpsData()
{
        uint8_t rcode;
        uint8_t buf[64]; //serial buffer equals Max.packet size of bulk-IN endpoint
        uint16_t rcvd = 64;
        String result = "";

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
                                        result += (char)buf[i];
                                }
                        }
                }
        }
        return result;
}
