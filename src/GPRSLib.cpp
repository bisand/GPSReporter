#include <GPRSLib.h>

GPRSLib::GPRSLib(/* args */)
{
	_serial = new AltSoftSerial(8, 9);
}

GPRSLib::~GPRSLib()
{
	//delete _serial;
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
	int idx = _readSerialUntilCrLf(_buffer, BUFFER_RESERVE_MEMORY);
	_readSerialUntilCrLf(_buffer, idx);

	char par[8];
	//if (strstr(_buffer, "\"0.0.0.0\"") == NULL || strstr(_buffer, "ERR") == NULL)
	_getResponseParams(_buffer, "+SAPBR:", 2, par, 8);
	_trimChar(par, ' ');
	if (par[0] == '1')
		return true;

	return false;
}

// GET IP Address
bool GPRSLib::gprsGetIP(char *ipAddress, uint8_t bufferSize)
{
	memset(ipAddress, '\0', bufferSize);
	_writeSerial("AT+SAPBR=2,1\r\n");
	if (_readSerialUntilOkOrError(_buffer, BUFFER_RESERVE_MEMORY) != 1)
	{
		return false;
	}

	char par[8];
	_getResponseParams(_buffer, "+SAPBR:", 2, par, 8);
	Serial.print("par: ");
	Serial.println(par);
	//_trimChar(par, ' ');
	if (strstr(par, "1") == NULL)
	{
		strcpy(ipAddress, "ERROR: NOT_CONNECTED");
		return false;
	}

	if (strstr(_buffer, "+SAPBR:") != NULL)
	{
		_getResponseParams(_buffer, "+SAPBR:", 3, ipAddress, bufferSize);
		//_trimChar(ipAddress, '\"');
		return true;
	}

	strcpy(ipAddress, "ERROR: NO_IP_FETCH");
	return false;
}

bool GPRSLib::gprsCloseConn()
{
	_writeSerial("AT+SAPBR=0,1\r");
	if (_readSerialUntilOkOrError(_buffer, BUFFER_RESERVE_MEMORY) == 1)
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
	if (_readSerialUntilOkOrError(_buffer, BUFFER_RESERVE_MEMORY) != FOUND_EITHER_TEXT)
	{
		return false;
	}

	_writeSerial("AT+SAPBR=3,1,\"APN\",\"");
	_writeSerial(apn);
	_writeSerial("\"\r");
	if (_readSerialUntilOkOrError(_buffer, BUFFER_RESERVE_MEMORY) != FOUND_EITHER_TEXT)
	{
		return false;
	}

	_writeSerial("AT+SAPBR=3,1,\"USER\",\"");
	_writeSerial(username);
	_writeSerial("\"\r");
	if (_readSerialUntilOkOrError(_buffer, BUFFER_RESERVE_MEMORY) != FOUND_EITHER_TEXT)
	{
		return false;
	};

	_writeSerial("AT+SAPBR=3,1,\"PWD\",\"");
	_writeSerial(password);
	_writeSerial("\"\r");
	if (_readSerialUntilOkOrError(_buffer, BUFFER_RESERVE_MEMORY) != FOUND_EITHER_TEXT)
	{
		return false;
	}

	// Open bearer
	_writeSerial("AT+SAPBR=1,1\r");
	if (_readSerialUntilOkOrError(_buffer, BUFFER_RESERVE_MEMORY) != FOUND_EITHER_TEXT)
	{
		return false;
	}

	// Query bearer
	_writeSerial("AT+SAPBR=2,1\r");
	int res = _readSerialUntilOkOrError(_buffer, BUFFER_RESERVE_MEMORY);
	char out[8];
	_getResponseParams(_buffer, "+SAPBR:", 2, out, 8);
	if (res != FOUND_EITHER_TEXT || atoi(out) != 1)
	{
		return false;
	}
	return true;
}

uint8_t GPRSLib::signalQuality()
{
	_writeSerial("AT+CSQ\r");
	if (_readSerialUntilOkOrError(_buffer, BUFFER_RESERVE_MEMORY) != FOUND_EITHER_TEXT)
	{
		char out[8];
		_getResponseParams(_buffer, "+CSQ:", 1, out, 8);
		return atoi(out);
	}
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
	_readSerialUntilCrLf(_buffer, BUFFER_RESERVE_MEMORY);
	_writeSerial("AT+HTTPINIT\r");
	_readSerialUntilCrLf(_buffer, BUFFER_RESERVE_MEMORY);
	if (strstr(_buffer, "OK") == NULL)
	{
		output = (char *)"HTTP_INIT_ERROR";
		return 503;
	}
	if (https)
	{
		// Set SSL if https
		_writeSerial("AT+HTTPSSL=1\r");
		_readSerialUntilCrLf(_buffer, BUFFER_RESERVE_MEMORY);
		if (strstr(_buffer, "OK") == NULL)
		{
			output = (char *)"HTTPSSL_ERROR";
			return 400;
		}
	}

	// Set bearer profile id
	_writeSerial("AT+HTTPPARA=\"CID\",1\r");
	_readSerialUntilCrLf(_buffer, BUFFER_RESERVE_MEMORY);
	if (strstr(_buffer, "OK") == NULL)
	{
		output = (char *)"HTTP_PARAMETER_CID_ERROR";
		return 400;
	}

	// Set url
	_writeSerial("AT+HTTPPARA=\"URL\",\"");
	_writeSerial(url);
	_writeSerial("\"\r");
	_readSerialUntilCrLf(_buffer, BUFFER_RESERVE_MEMORY);
	if (strstr(_buffer, "OK") == NULL)
	{
		output = (char *)"HTTP_PARAMETER_URL_ERROR";
		return 400;
	}

	// Set content type
	_writeSerial("AT+HTTPPARA=\"CONTENT\",\"");
	_writeSerial(contentType);
	_writeSerial("\"\r");
	_readSerialUntilCrLf(_buffer, BUFFER_RESERVE_MEMORY);
	if (strstr(_buffer, "OK") == NULL)
	{
		output = (char *)"HTTP_PARAMETER_CONTENT_ERROR";
		return 400;
	}

	// Indicate that data will be transfered within 30 secods.
	_writeSerial("AT+HTTPDATA=");
	_writeSerial((char *)strlen(data));
	_writeSerial(",30000\r");
	_readSerialUntilCrLf(_buffer, BUFFER_RESERVE_MEMORY);
	if (strstr(_buffer, "OK") == NULL)
	{
		output = (char *)"HTTP_DATA_ERROR";
		return 400;
	}

	// Send data.
	_writeSerial(data);
	_writeSerial("\r");
	_readSerialUntilCrLf(_buffer, BUFFER_RESERVE_MEMORY);
	if (strstr(_buffer, "OK") == NULL)
	{
		output = (char *)"HTTP_DATA_DOWNLOAD_ERROR";
		return 400;
	}

	// Set action and perform request 1=POST
	_writeSerial("AT+HTTPACTION=1\r");
	_readSerialUntilCrLf(_buffer, BUFFER_RESERVE_MEMORY);
	if (strstr(_buffer, "OK") == NULL)
	{
		output = (char *)"HTTP_ACTION_ERROR";
		return 400;
	}

	// Get the response.
	delay(100);
	_readSerialUntilCrLf(_buffer, BUFFER_RESERVE_MEMORY, 0, 10000);
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
	_readSerialUntilCrLf(_buffer, BUFFER_RESERVE_MEMORY, 0, 10000);

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
	_readSerialUntilCrLf(_buffer, BUFFER_RESERVE_MEMORY);

	return 200;
}

//////////////////////////////////////
//			PRIVATE METHODS			//
//////////////////////////////////////

// Read serial until the text "OK" or "ERROR" are found.
// Results:
// -2 = Timeout.
// -1 = Index exceeded buffer size before anything was found.
// 1 = OK is found.
// 2 = ERROR is found.
ReadSerialResult GPRSLib::_readSerialUntilOkOrError(char *buffer, uint32_t bufferSize, uint32_t timeout = TIME_OUT_READ_SERIAL)
{
	return _readSerialUntilEitherOr(buffer, bufferSize, "\r\nOK\r\n\0", "\r\nERROR\r\n\0", timeout);
}

// Read serial until one of the two texts are found.
// Return:
// -2 = Timeout.
// -1 = Index exceeded buffer size before anything was found.
// 1 = eitherText is found.
// 2 = orText is found.
ReadSerialResult GPRSLib::_readSerialUntilEitherOr(char *buffer, uint32_t bufferSize, const char *eitherText, const char *orText, uint32_t timeout = TIME_OUT_READ_SERIAL)
{
	ReadSerialResult result = NOTHING_FOUND;
	memset(buffer, 0, bufferSize);

	uint64_t index = 0;
	uint64_t timerStart, timerEnd;
	timerStart = millis();
	bool cei = false, cor = false;
	char c;

	while (1)
	{
		while (_serial->available())
		{
			c = _serial->read();
			buffer[index++] = c;
			if (strstr(buffer, eitherText) != NULL)
			{
				result = FOUND_EITHER_TEXT;
				cei = true;
				break;
			}
			if (strstr(buffer, orText) != NULL)
			{
				result = FOUND_OR_TEXT;
				cor = true;
				break;
			}
			if (index >= bufferSize)
			{
				result = INDEX_EXCEEDED_BUFFER_SIZE;
				break;
			}
		}
		if (cei || cor || index >= bufferSize)
			break;
		timerEnd = millis();
		if (timerEnd - timerStart > timeout)
		{
			result = TIMEOUT;
			break;
		}
	}
	buffer[index] = '\0';

	if (_debug)
	{
		Serial.println("[DEBUG] [_readSerialUntilEitherOr]");
		if (cei)
		{
			Serial.print("Either text:");
			Serial.println(eitherText);
		}
		if (cor)
		{
			Serial.print("Or text:");
			Serial.println(orText);
		}
		Serial.println("Buffer: ");
		Serial.println(buffer);
	}

	return result;
}

// Read serial until <CR> and <LF> is found.
// Return:
// Length of the read string.
int GPRSLib::_readSerialUntilCrLf(char *buffer, uint32_t bufferSize, uint32_t startIndex = 0, uint32_t timeout = TIME_OUT_READ_SERIAL)
{
	if (startIndex >= bufferSize)
		return 0;
	if (startIndex == 0)
		memset(buffer, '\0', bufferSize);

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
		if ((cr && lf) || index >= bufferSize)
			break;
		timerEnd = millis();
		if (timerEnd - timerStart > timeout)
			break;
	}
	buffer[index] = '\0';

	if (_debug)
	{
		Serial.print("[DEBUG] [_readSerialUntilCrLf] - ");
		Serial.println(buffer);
	}

	return index;
}

int GPRSLib::_writeSerial(const char *buffer)
{
	if (_debug)
	{
		Serial.print("[DEBUG] [_writeSerial] - ");
		Serial.println(buffer);
	}
	_serial->print(buffer);
	_serial->flush();
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
	uint32_t start = 0;
	uint32_t len, l;
	l = len = strlen(buffer);
	bool sf, ef = false;
	char *tmpBuf = strdup(buffer);
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
	free(tmpBuf);
	return;
}

bool GPRSLib::_getResponseParams(char *buffer, const char *cmd, uint8_t paramNum, char *output, uint16_t outputLength)
{
	bool result = false;
	uint8_t idx = 0;
	uint8_t cmdLen = strlen(cmd);
	char *foundCmd = strstr(buffer, cmd);
	if (!foundCmd)
	{
		return result;
	}

	memset(output, '\0', outputLength);

	char *tok, *r, *end;
	r = end = strdup(&foundCmd[cmdLen]);

	while ((tok = strsep(&end, ",")) != NULL)
		if (++idx == paramNum)
		{
			strcpy(output, tok);
			result = true;
			break;
		}
	free(r);
	return result;
}
