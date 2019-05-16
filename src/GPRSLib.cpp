#include <GPRSLib.h>

GPRSLib::GPRSLib(/* args */)
{
	_serial = new AltSoftSerial();
}

GPRSLib::~GPRSLib()
{
	delete _serial;
}

void GPRSLib::setup(unsigned int baud)
{
	pinMode(RESET_PIN, OUTPUT);
	digitalWrite(RESET_PIN, HIGH);

	_baud = baud;

	_serial->begin(_baud);

	if (LED_FLAG)
	{
		pinMode(LED_PIN, OUTPUT);
	}
}

bool GPRSLib::gprsIsConnected()
{
	_serial->print(("AT+SAPBR=2,1\r"));
	int idx = _readSerial(_buffer, BUFFER_RESERVE_MEMORY);
	delay(100);
	_readSerial(_buffer, idx);
	
	char par[8];
	//if (strstr(_buffer, "\"0.0.0.0\"") == NULL || strstr(_buffer, "ERR") == NULL)
	_getResponseParams(_buffer, "+SAPBR:", 2, par);
	_trimChar(par, ' ');
	if(par == "1")
		return true;

	return false;
}
// GET IP Address
void GPRSLib::gprsGetIP(char ipAddress[32])
{
	_serial->print(("AT+SAPBR=2,1\r\n"));
	int idx = _readSerial(_buffer, BUFFER_RESERVE_MEMORY);
	delay(50);
	_readSerial(_buffer, BUFFER_RESERVE_MEMORY, idx);

	//if (strstr(_buffer, "\"0.0.0.0\"") == NULL || strstr(_buffer, "ERR") == NULL)
	char par[8];
	_getResponseParams(_buffer, "+SAPBR:", 2, par);
	_trimChar(par, ' ');
	if(par != "1")
	{
		ipAddress = (char *)"ERROR:NOT_CONNECTED";
		return;
	}

	if (strstr(_buffer, "+SAPBR:") == NULL)
	{
		_getResponseParams(_buffer, "+SAPBR:", 3, ipAddress);
		_trimChar(ipAddress, '\"');
		return;
	}

	ipAddress = (char *)"ERROR:NO_IP_FETCH";
}

bool GPRSLib::gprsCloseConn()
{
	_serial->print(("AT+SAPBR=0,1\r"));
	int idx = _readSerial(_buffer, BUFFER_RESERVE_MEMORY);
	delay(50);
	_readSerial(_buffer, BUFFER_RESERVE_MEMORY, idx);

	if (strstr(_buffer, "OK") == NULL)
		return true;

	return false;
}

bool GPRSLib::connectBearer()
{
	return connectBearer("internet", "", "");
}

bool GPRSLib::connectBearer(const char *apn)
{
	return connectBearer(apn, "", "");
}

bool GPRSLib::connectBearer(const char *apn, const char *username, const char *password)
{
	_serial->print(("AT+SAPBR=3,1,\"CONTYPE\",\"GPRS\"\r"));
	_readSerial(_buffer, BUFFER_RESERVE_MEMORY);
	if (strstr(_buffer, "OK") == NULL)
	{
		Serial.print("ERROR: ");
		Serial.println(_buffer);
		return false;
	}

	delay(100);
	_serial->print(("AT+SAPBR=3,1,\"APN\",\""));
	_serial->print(apn);
	_serial->print(("\"\r"));
	_readSerial(_buffer, BUFFER_RESERVE_MEMORY);
	if (strstr(_buffer, "OK") == NULL)
	{
		Serial.print("ERROR: ");
		Serial.println(_buffer);
		return false;
	}

	delay(100);

	_serial->print(("AT+SAPBR=3,1,\"USER\",\""));
	_serial->print(username);
	_serial->print(("\"\r"));
	_readSerial(_buffer, BUFFER_RESERVE_MEMORY);
	if (strstr(_buffer, "OK") == NULL)
	{
		Serial.print("ERROR: ");
		Serial.println(_buffer);
		return false;
	}

	delay(100);

	_serial->print(("AT+SAPBR=3,1,\"PWD\",\""));
	_serial->print(password);
	_serial->print(("\"\r"));
	_readSerial(_buffer, BUFFER_RESERVE_MEMORY);
	if (strstr(_buffer, "OK") == NULL)
	{
		Serial.print("ERROR: ");
		Serial.println(_buffer);
		return false;
	}

	delay(100);

	// Open bearer
	_serial->print("AT+SAPBR=1,1\r");
	int res = _readSerial(_buffer, BUFFER_RESERVE_MEMORY);
	delay(100);
	_readSerial(_buffer, BUFFER_RESERVE_MEMORY, res);
	if (strstr(_buffer, "OK") == NULL)
	{
		Serial.print("ERROR opening bearer: ");
		Serial.println(_buffer);
		return false;
	}

	// Query bearer
	_serial->print("AT+SAPBR=2,1\r");
	_readSerial(_buffer, BUFFER_RESERVE_MEMORY);
	if (strstr(_buffer, "\"0.0.0.0\"") == NULL || strstr(_buffer, "ERR") == NULL)
	{
		Serial.print("ERROR querying bearer: ");
		Serial.println(_buffer);
		return false;
	}
	return true;
}

uint8_t GPRSLib::signalQuality()
{
	// _serial->print(("AT+CSQ\r"));
	// _readSerial(_buffer, BUFFER_RESERVE_MEMORY);

	// if ((strstr(_buffer, "+CSQ:") != NULL)
	// 	return _buffer.substring(_buffer.indexOf("+CSQ: ") + 6, _buffer.indexOf(",")).toInt();

	return 99;
}

int GPRSLib::httpPost(const char *url, const char *data, const char *contentType, bool read, char *output, unsigned int outputSize)
{
	bool https = false;
	if (strstr(url, "https://") != NULL)
	{
		https = true;
	}

	if (!gprsIsConnected())
	{
		output = (char *)"GPRS_NOT_CONNECTED";
		return 503;
	}
	// Terminate http connection, if it opened before!
	_serial->print(("AT+HTTPTERM\r"));
	_readSerial(_buffer, BUFFER_RESERVE_MEMORY);
	_serial->print(("AT+HTTPINIT\r"));
	_readSerial(_buffer, BUFFER_RESERVE_MEMORY);
	if (strstr(_buffer, "OK") == NULL)
	{
		output = (char *)"HTTP_INIT_ERROR";
		return 503;
	}
	if (https)
	{
		// Set SSL if https
		_serial->print("AT+HTTPSSL=1\r");
		_readSerial(_buffer, BUFFER_RESERVE_MEMORY);
		if (strstr(_buffer, "OK") == NULL)
		{
			output = (char *)"HTTPSSL_ERROR";
			return 400;
		}
	}

	// Set bearer profile id
	_serial->print(("AT+HTTPPARA=\"CID\",1\r"));
	_readSerial(_buffer, BUFFER_RESERVE_MEMORY);
	if (strstr(_buffer, "OK") == NULL)
	{
		output = (char *)"HTTP_PARAMETER_CID_ERROR";
		return 400;
	}

	// Set url
	_serial->print(("AT+HTTPPARA=\"URL\",\""));
	_serial->print(url);
	_serial->print("\"\r");
	_readSerial(_buffer, BUFFER_RESERVE_MEMORY);
	if (strstr(_buffer, "OK") == NULL)
	{
		output = (char *)"HTTP_PARAMETER_URL_ERROR";
		return 400;
	}

	// Set content type
	_serial->print(("AT+HTTPPARA=\"CONTENT\",\""));
	_serial->print(contentType);
	_serial->print("\"\r");
	_readSerial(_buffer, BUFFER_RESERVE_MEMORY);
	if (strstr(_buffer, "OK") == NULL)
	{
		output = (char *)"HTTP_PARAMETER_CONTENT_ERROR";
		return 400;
	}

	// Indicate that data will be transfered within 30 secods.
	_serial->print(("AT+HTTPDATA="));
	_serial->print(strlen(data));
	_serial->print((",30000\r"));
	_readSerial(_buffer, BUFFER_RESERVE_MEMORY);
	if (strstr(_buffer, "OK") == NULL)
	{
		output = (char *)"HTTP_DATA_ERROR";
		return 400;
	}

	// Send data.
	_serial->print(data);
	_serial->print(("\r"));
	_readSerial(_buffer, BUFFER_RESERVE_MEMORY);
	if (strstr(_buffer, "OK") == NULL)
	{
		output = (char *)"HTTP_DATA_DOWNLOAD_ERROR";
		return 400;
	}

	// Set action and perform request 1=POST
	_serial->print(("AT+HTTPACTION=1\r"));
	_readSerial(_buffer, BUFFER_RESERVE_MEMORY);
	if (strstr(_buffer, "OK") == NULL)
	{
		output = (char *)"HTTP_ACTION_ERROR";
		return 400;
	}

	// Get the response.
	delay(100);
	_readSerial(_buffer, BUFFER_RESERVE_MEMORY, 0, 10000);
	if (strstr(_buffer, "+HTTPACTION: 1,") == NULL)
	{
		output = (char *)"HTTP_ACTION_READ_ERROR";
		return 400;
	}

	char code[16];
	memset(code, '\0', 16);
	_extractTextBetween(_buffer, ',', output, outputSize);

	// String code = _buffer.substring(_buffer.indexO(",") + 1, _buffer.lastIndexO(","));
	// String length = _buffer.substring(_buffer.lastIndexO(",") + 1);
	// code.trim();
	// length.trim();

	_serial->print(("AT+HTTPREAD\r"));
	_readSerial(_buffer, BUFFER_RESERVE_MEMORY, 0, 10000);

	String reading = "";

	// if (_buffer.indexO("+HTTPREAD:") == -1)
	// 	return "ERROR:HTTP_READ_ERROR";

	// String kriter = "+HTTPREAD: " + length;
	// String dataString = _buffer.substring(_buffer.indexO(kriter) + kriter.length(), _buffer.lastIndexO("OK"));
	// reading = dataString;

	// String result = "METHOD:POST|HTTPCODE:";
	// result += code;
	// result += "|LENGTH:";
	// result += length;
	// result += "|DATA:";
	// reading.trim();
	// result += reading;

	_serial->print(("AT+HTTPTERM\r"));
	_readSerial(_buffer, BUFFER_RESERVE_MEMORY);

	return 200;
}

//////////////////////////////////////
//			PRIVATE METHODS			//
//////////////////////////////////////

// READ FROM SERIAL
int GPRSLib::_readSerial(char *buffer, unsigned int bufferSize, int startIndex = 0, unsigned int timeout = TIME_OUT_READ_SERIAL)
{
	if (startIndex > bufferSize)
		return 0;

	int index = startIndex;
	uint64_t timeOld = millis();

	while (!_serial->available() > 0 && !(millis() > timeOld + timeout))
		delay(13);
	if (_serial->available() > 0 && index < bufferSize)
		while (_serial->available() > 0)
		{
			buffer[index++] = _serial->read();
			if (index >= bufferSize)
				break;
		}

	if (index >= bufferSize)
		buffer[index - 1] = '\0';
	else
		buffer[index] = '\0';

	return index;
}

void GPRSLib::_extractTextBetween(const char *buffer, const int chr, char *output, unsigned int outputSize)
{
    memset(output, '\0', outputSize);
	strncpy(output, buffer, outputSize);
	_trimChar(output, chr);
}

void GPRSLib::_trimChar(char *buffer, char chr)
{
    int start = 0;
    int len, l = strlen(buffer);
    bool sf, ef = false;
    char tmpBuf[l];
    strcpy(tmpBuf, buffer);
    memset(buffer, '\0', l);

    for (size_t i = 0; i < l; i++)
    {
        if (!sf && !ef && tmpBuf[i] == chr)
        {
            sf = true;
            start = i + 1;
            continue;
        }
        if (sf && !ef && tmpBuf[i] == chr)
        {
            ef = true;
            len = (i - start);
            continue;
        }
    }

    strncpy(buffer, &tmpBuf[start], len);
    return;
}

bool GPRSLib::_getResponseParams(char *buffer, const char *cmd, uint8_t paramNum, char *output)
{
    uint8_t idx = 0;
    uint8_t bufLen = strlen(buffer);
    uint8_t cmdLen = strlen(cmd);
    char *pch;
    char tmpChr[bufLen - cmdLen];
    memset(tmpChr, '\0', bufLen - cmdLen);
    memset(output, '\0', bufLen - cmdLen);
    strncpy(tmpChr, &buffer[cmdLen], (bufLen - cmdLen) - 1);

    pch = strtok(tmpChr, ",");
    while (pch != NULL)
    {
        if (++idx == paramNum)
        {
            strcpy(output, pch);
            return true;
        }
        pch = strtok(NULL, ",");
    }
    output[0] = '\0';
    return false;
}
