#include <GPRSLib.h>

GPRSLib::GPRSLib(/* args */)
{
	_serial = new AltSoftSerial(8, 9);
}

GPRSLib::~GPRSLib()
{
	delete _serial;
}

void GPRSLib::setup(unsigned int baud, bool debug = false)
{
	pinMode(RESET_PIN, OUTPUT);
	digitalWrite(RESET_PIN, HIGH);

	_baud = baud;
	_debug = debug;

	_serial->begin(_baud);

	if (LED_FLAG)
	{
		pinMode(LED_PIN, OUTPUT);
	}
}

bool GPRSLib::gprsIsConnected()
{
	_writeSerial("AT+SAPBR=2,1\r");
	int idx = _readSerial(_buffer, BUFFER_RESERVE_MEMORY);
	_readSerial(_buffer, idx);

	char par[8];
	//if (strstr(_buffer, "\"0.0.0.0\"") == NULL || strstr(_buffer, "ERR") == NULL)
	_getResponseParams(_buffer, "+SAPBR:", 2, par);
	_trimChar(par, ' ');
	if (par == "1")
		return true;

	return false;
}
// GET IP Address
void GPRSLib::gprsGetIP(char ipAddress[32])
{
	_writeSerial("AT+SAPBR=2,1\r\n");
	int idx = _readSerial(_buffer, BUFFER_RESERVE_MEMORY);
	_readSerial(_buffer, BUFFER_RESERVE_MEMORY, idx);

	//if (strstr(_buffer, "\"0.0.0.0\"") == NULL || strstr(_buffer, "ERR") == NULL)
	char par[8];
	_getResponseParams(_buffer, "+SAPBR:", 2, par);
	_trimChar(par, ' ');
	if (par != "1")
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
	_writeSerial("AT+SAPBR=0,1\r");
	int idx = _readSerial(_buffer, BUFFER_RESERVE_MEMORY);
	_readSerial(_buffer, BUFFER_RESERVE_MEMORY, idx);

	if (strstr(_buffer, "OK") != NULL)
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
	_writeSerial((const char *)"AT+SAPBR=3,1,\"CONTYPE\",\"GPRS\"\r");
	int res = _readSerial(_buffer, BUFFER_RESERVE_MEMORY);
	_readSerial(_buffer, BUFFER_RESERVE_MEMORY, res);
	if (strstr(_buffer, "OK") == NULL)
	{
		return false;
	}

	_writeSerial("AT+SAPBR=3,1,\"APN\",\"");
	_writeSerial(apn);
	_writeSerial("\"\r");
	res = _readSerial(_buffer, BUFFER_RESERVE_MEMORY);
	_readSerial(_buffer, BUFFER_RESERVE_MEMORY, res);
	if (strstr(_buffer, "OK") == NULL)
	{
		return false;
	}

	_writeSerial("AT+SAPBR=3,1,\"USER\",\"");
	_writeSerial(username);
	_writeSerial("\"\r");
	res = _readSerial(_buffer, BUFFER_RESERVE_MEMORY);
	_readSerial(_buffer, BUFFER_RESERVE_MEMORY, res);
	if (strstr(_buffer, "OK") == NULL)
	{
		return false;
	};

	_writeSerial("AT+SAPBR=3,1,\"PWD\",\"");
	_writeSerial(password);
	_writeSerial("\"\r");
	res = _readSerial(_buffer, BUFFER_RESERVE_MEMORY);
	_readSerial(_buffer, BUFFER_RESERVE_MEMORY, res);
	if (strstr(_buffer, "OK") == NULL)
	{
		return false;
	}

	// Open bearer
	_writeSerial("AT+SAPBR=1,1\r");
	res = _readSerial(_buffer, BUFFER_RESERVE_MEMORY);
	_readSerial(_buffer, BUFFER_RESERVE_MEMORY, res);
	if (strstr(_buffer, "OK") == NULL)
	{
		return false;
	}

	// Query bearer
	_writeSerial("AT+SAPBR=2,1\r");
	res = _readSerial(_buffer, BUFFER_RESERVE_MEMORY);
	res = _readSerial(_buffer, BUFFER_RESERVE_MEMORY, res);
	res = _readSerial(_buffer, BUFFER_RESERVE_MEMORY, res);
	char out[8];
	_getResponseParams(_buffer, "+SAPBR:", 2, out);
	if (strstr(out, "1") == NULL)
	{
		return false;
	}
	res = _readSerial(_buffer, BUFFER_RESERVE_MEMORY, res);
	if (strstr(_buffer, "OK") == NULL)
	{
		return false;
	}
	return true;
}

uint8_t GPRSLib::signalQuality()
{
	// _writeSerial("AT+CSQ\r");
	// _readSerial(_buffer, BUFFER_RESERVE_MEMORY);

	// if ((strstr(_buffer, "+CSQ:") != NULL)
	// 	return _buffer.substring(_buffer.indexOf("+CSQ: ") + 6, _buffer.indexOf(",").toInt();

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
	_writeSerial("AT+HTTPTERM\r");
	_readSerial(_buffer, BUFFER_RESERVE_MEMORY);
	_writeSerial("AT+HTTPINIT\r");
	_readSerial(_buffer, BUFFER_RESERVE_MEMORY);
	if (strstr(_buffer, "OK") == NULL)
	{
		output = (char *)"HTTP_INIT_ERROR";
		return 503;
	}
	if (https)
	{
		// Set SSL if https
		_writeSerial("AT+HTTPSSL=1\r");
		_readSerial(_buffer, BUFFER_RESERVE_MEMORY);
		if (strstr(_buffer, "OK") == NULL)
		{
			output = (char *)"HTTPSSL_ERROR";
			return 400;
		}
	}

	// Set bearer profile id
	_writeSerial("AT+HTTPPARA=\"CID\",1\r");
	_readSerial(_buffer, BUFFER_RESERVE_MEMORY);
	if (strstr(_buffer, "OK") == NULL)
	{
		output = (char *)"HTTP_PARAMETER_CID_ERROR";
		return 400;
	}

	// Set url
	_writeSerial("AT+HTTPPARA=\"URL\",\"");
	_writeSerial(url);
	_writeSerial("\"\r");
	_readSerial(_buffer, BUFFER_RESERVE_MEMORY);
	if (strstr(_buffer, "OK") == NULL)
	{
		output = (char *)"HTTP_PARAMETER_URL_ERROR";
		return 400;
	}

	// Set content type
	_writeSerial("AT+HTTPPARA=\"CONTENT\",\"");
	_writeSerial(contentType);
	_writeSerial("\"\r");
	_readSerial(_buffer, BUFFER_RESERVE_MEMORY);
	if (strstr(_buffer, "OK") == NULL)
	{
		output = (char *)"HTTP_PARAMETER_CONTENT_ERROR";
		return 400;
	}

	// Indicate that data will be transfered within 30 secods.
	_writeSerial("AT+HTTPDATA=");
	_writeSerial((char*)strlen(data));
	_writeSerial(",30000\r");
	_readSerial(_buffer, BUFFER_RESERVE_MEMORY);
	if (strstr(_buffer, "OK") == NULL)
	{
		output = (char *)"HTTP_DATA_ERROR";
		return 400;
	}

	// Send data.
	_writeSerial(data);
	_writeSerial("\r");
	_readSerial(_buffer, BUFFER_RESERVE_MEMORY);
	if (strstr(_buffer, "OK") == NULL)
	{
		output = (char *)"HTTP_DATA_DOWNLOAD_ERROR";
		return 400;
	}

	// Set action and perform request 1=POST
	_writeSerial("AT+HTTPACTION=1\r");
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

	// String code = _buffer.substring(_buffer.indexO(",") + 1, _buffer.lastIndexO(",");
	// String length = _buffer.substring(_buffer.lastIndexO(",") + 1);
	// code.trim();
	// length.trim();

	_writeSerial("AT+HTTPREAD\r");
	_readSerial(_buffer, BUFFER_RESERVE_MEMORY, 0, 10000);

	String reading = "";

	// if (_buffer.indexO("+HTTPREAD:") == -1)
	// 	return "ERROR:HTTP_READ_ERROR";

	// String kriter = "+HTTPREAD: " + length;
	// String dataString = _buffer.substring(_buffer.indexO(kriter) + kriter.length(), _buffer.lastIndexO("OK");
	// reading = dataString;

	// String result = "METHOD:POST|HTTPCODE:";
	// result += code;
	// result += "|LENGTH:";
	// result += length;
	// result += "|DATA:";
	// reading.trim();
	// result += reading;

	_writeSerial("AT+HTTPTERM\r");
	_readSerial(_buffer, BUFFER_RESERVE_MEMORY);

	return 200;
}

//////////////////////////////////////
//			PRIVATE METHODS			//
//////////////////////////////////////

// READ FROM SERIAL
int GPRSLib::_readSerial(char *buffer, uint32_t bufferSize, uint32_t startIndex = 0, uint32_t timeout = TIME_OUT_READ_SERIAL)
{
	//delay(50);
	if (startIndex >= bufferSize)
		return 0;
	if (startIndex == 0)
		//memset(buffer, '\0', bufferSize);
		for (size_t i = 0; i < bufferSize; i++)
		{
			buffer[i] = 0;
		}

	uint64_t index = startIndex;
	uint64_t timerStart, timerEnd;
	timerStart = millis();
	bool cr = false, lf = false;
	char c;

	while (1)
	{
		while (_serial->available())
		{
			c = _serial->read();
			if (c == '\r' && index > 0)
				cr = true;
			if (c == '\n' && index > 1)
				lf = true;
			buffer[index++] = c;
			if (cr && lf)
				break;
			if (index >= bufferSize)
				break;
		}
		if (cr && lf)
			break;
		if (index >= bufferSize)
			break;
		timerEnd = millis();
		if (timerEnd - timerStart > timeout)
			break;
	}
	buffer[index] = '\0';

	if(_debug){
		Serial.print("[DEBUG] [_readSerial] - ");
		Serial.println(buffer);
	}

	return index;
}

int GPRSLib::_writeSerial(const char *buffer)
{
	if(_debug){
		Serial.print("[DEBUG] [_writeSerial] - ");
		Serial.println(buffer);
	}
	_serial->print(buffer);
	_serial->flush();
	//delay(50);
	return strlen(buffer);
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
