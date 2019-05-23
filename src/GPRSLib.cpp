#include <GPRSLib.h>

GPRSLib::GPRSLib(/* args */)
{
	_serial1 = new AltSoftSerial(8, 9);
}

GPRSLib::~GPRSLib()
{
	//delete _serial1;
}

void GPRSLib::setup(uint32_t baud, bool debug)
{
	pinMode(RESET_PIN, OUTPUT);
	digitalWrite(RESET_PIN, HIGH);

	_baud = baud;
	_debug = debug;

	_serial1->begin(_baud);

	if (LED_FLAG)
	{
		pinMode(LED_PIN, OUTPUT);
	}
}

bool GPRSLib::gprsIsConnected()
{
	bool result = false;
	_writeSerial(F("AT+SAPBR=2,1\r"));
	ReadSerialResult res = _readSerialUntilOkOrError(_buffer, sizeof(_buffer));

	char par[8];
	_getResponseParams(_buffer, "+SAPBR:", 2, par, sizeof(par));
	if (res == FOUND_EITHER_TEXT && atoi(par) == 1)
		result = true;

	return result;
}

// GET IP Address
Result GPRSLib::gprsGetIP(char *ipAddress, uint8_t bufferSize)
{
	_writeSerial(F("AT+SAPBR=2,1\r\n"));
	if (_readSerialUntilOkOrError(_buffer, sizeof(_buffer)) != 1)
		return ERROR_OPEN_GPRS_CONTEXT;

	char par[8];
	_getResponseParams(_buffer, "+SAPBR:", 2, par, sizeof(par));
	if (atoi(par) != 1)
		return ERROR_OPEN_GPRS_CONTEXT;

	if (strstr(_buffer, "+SAPBR:") == NULL)
		return ERROR_QUERY_GPRS_CONTEXT;

	_getResponseParams(_buffer, "+SAPBR:", 3, ipAddress, bufferSize);
	_trimChar(ipAddress, '\"');
	return SUCCESS;
}

Result GPRSLib::gprsCloseConn()
{
	_writeSerial(F("AT+SAPBR=0,1\r"));
	if (_readSerialUntilOkOrError(_buffer, sizeof(_buffer)) == 1)
		return SUCCESS;

	return ERROR_CLOSE_GPRS_CONTEXT;
}

Result GPRSLib::connectBearer()
{
	return connectBearer("internet", "", "");
}

Result GPRSLib::connectBearer(const char *apn)
{
	return connectBearer(apn, "", "");
}

Result GPRSLib::connectBearer(const char *apn, const char *username, const char *password)
{
	_writeSerial(F("AT+SAPBR=3,1,\"CONTYPE\",\"GPRS\"\r"));
	if (_readSerialUntilOkOrError(_buffer, sizeof(_buffer)) != FOUND_EITHER_TEXT)
		return ERROR_BEARER_PROFILE_GPRS;

	_writeSerial(F("AT+SAPBR=3,1,\"APN\",\""));
	_writeSerial(apn);
	_writeSerial(F("\"\r"));
	if (_readSerialUntilOkOrError(_buffer, sizeof(_buffer)) != FOUND_EITHER_TEXT)
		return ERROR_BEARER_PROFILE_APN;

	_writeSerial(F("AT+SAPBR=3,1,\"USER\",\""));
	_writeSerial(username);
	_writeSerial(F("\"\r"));
	if (_readSerialUntilOkOrError(_buffer, sizeof(_buffer)) != FOUND_EITHER_TEXT)
		return ERROR_BEARER_PROFILE_APN;

	_writeSerial(F("AT+SAPBR=3,1,\"PWD\",\""));
	_writeSerial(password);
	_writeSerial(F("\"\r"));
	if (_readSerialUntilOkOrError(_buffer, sizeof(_buffer)) != FOUND_EITHER_TEXT)
		return ERROR_BEARER_PROFILE_APN;

	// Open bearer
	_writeSerial(F("AT+SAPBR=1,1\r"));
	if (_readSerialUntilOkOrError(_buffer, sizeof(_buffer)) != FOUND_EITHER_TEXT)
		return ERROR_OPEN_GPRS_CONTEXT;

	// Query bearer
	_writeSerial(F("AT+SAPBR=2,1\r"));
	int res = _readSerialUntilOkOrError(_buffer, sizeof(_buffer));
	char out[8];
	_getResponseParams(_buffer, "+SAPBR:", 2, out, sizeof(out));
	if (res != FOUND_EITHER_TEXT || atoi(out) != 1)
		return ERROR_OPEN_GPRS_CONTEXT;

	return SUCCESS;
}

uint8_t GPRSLib::signalQuality()
{
	_writeSerial(F("AT+CSQ\r"));
	if (_readSerialUntilOkOrError(_buffer, sizeof(_buffer)) == FOUND_EITHER_TEXT)
	{
		char out[8];
		_getResponseParams(_buffer, "+CSQ:", 1, out, sizeof(out));
		return atoi(out);
	}
	return 99;
}

Result GPRSLib::httpPost(const char *url, const char *data, const char *contentType, bool read, char *output, unsigned int outputSize)
{
	_clearBuffer(output, outputSize);
	bool https = false;
	if (strstr(url, "https://") != NULL)
		https = true;

	// if (!gprsIsConnected())
	// 	return ERROR_OPEN_GPRS_CONTEXT;

	// Terminate http connection, if it opened before!
	_writeSerial(F("AT+HTTPTERM\r"));
	_readSerialUntilOkOrError(_buffer, sizeof(_buffer));
	_writeSerial(F("AT+HTTPINIT\r"));
	if (_readSerialUntilOkOrError(_buffer, sizeof(_buffer)) != FOUND_EITHER_TEXT)
		return ERROR_HTTP_INIT;

	if (https)
	{
		// Set SSL if https
		_writeSerial(F("AT+HTTPSSL=1\r"));
		if (_readSerialUntilOkOrError(_buffer, sizeof(_buffer)) != FOUND_EITHER_TEXT)
			return ERROR_HTTPS_ENABLE;
	}

	// Set bearer profile id
	_writeSerial(F("AT+HTTPPARA=\"CID\",1\r"));
	if (_readSerialUntilOkOrError(_buffer, sizeof(_buffer)) != FOUND_EITHER_TEXT)
		return ERROR_HTTP_PARA;

	// Set url
	_writeSerial(F("AT+HTTPPARA=\"URL\",\""));
	_writeSerial(url);
	_writeSerial(F("\"\r"));
	if (_readSerialUntilOkOrError(_buffer, sizeof(_buffer)) != FOUND_EITHER_TEXT)
		return ERROR_HTTP_PARA;

	// Set content type
	_writeSerial(F("AT+HTTPPARA=\"CONTENT\",\""));
	_writeSerial(contentType);
	_writeSerial(F("\"\r"));
	if (_readSerialUntilOkOrError(_buffer, sizeof(_buffer)) != FOUND_EITHER_TEXT)
		return ERROR_HTTP_PARA;

	// Indicate that buffer will be transfered within 30 secods.
	char dLen[8];
	itoa(strlen(data), dLen, 10);
	_writeSerial(F("AT+HTTPDATA="));
	_writeSerial(dLen);
	_writeSerial(F(",30000\r"));
	if (_readSerialUntilEitherOr(_buffer, sizeof(_buffer), "DOWNLOAD\r\n", "ERROR\r\n", 30000) != FOUND_EITHER_TEXT)
		return ERROR_HTTP_DATA;

	// Send data.
	_writeSerial(data);
	_writeSerial(F("\r"));
	if (_readSerialUntilOkOrError(_buffer, sizeof(_buffer)) != FOUND_EITHER_TEXT)
		return ERROR_HTTP_DATA;

	// Set action and perform request 1=POST
	_writeSerial(F("AT+HTTPACTION=1\r"));
	delay(500);
	ReadSerialResult res = _readSerialUntilOkOrError(_buffer, sizeof(_buffer));
	int idx = _readSerialUntilCrLf(_buffer, sizeof(_buffer));
	idx = _readSerialUntilCrLf(_buffer, sizeof(_buffer), idx);
	char out[8];
	_getResponseParams(_buffer, "+HTTPACTION:", 1, out, sizeof(out));
	if (res != FOUND_EITHER_TEXT || atoi(out) != 1)
		return ERROR_HTTP_POST;

	_getResponseParams(_buffer, "+HTTPACTION:", 2, out, sizeof(out));
	int httpResult = atoi(out);
	if (httpResult < 200 || httpResult >= 300)
		return ERROR_HTTP_POST;

	if (read)
	{
		_writeSerial(F("AT+HTTPREAD\r"));
		if (_readSerialUntilOkOrError(_buffer, sizeof(_buffer), 10000) == FOUND_EITHER_TEXT)
		{
			_getResponseParams(_buffer, "+HTTPREAD:", 1, out, sizeof(out));
		}
	}

	_writeSerial(F("AT+HTTPTERM\r"));
	_readSerialUntilOkOrError(_buffer, sizeof(_buffer));

	return SUCCESS;
}

//////////////////////////////////////
//			PRIVATE METHODS			//
//////////////////////////////////////

void GPRSLib::_clearBuffer(char *buffer, uint32_t size)
{
	for (size_t i = 0; i < size; i++)
	{
		buffer[i] = '\0';
	}
}

// Read serial until the text "OK" or "ERROR" are found.
// Results:
// -2 = Timeout.
// -1 = Index exceeded buffer size before anything was found.
// 1 = OK is found.
// 2 = ERROR is found.
ReadSerialResult GPRSLib::_readSerialUntilOkOrError(char *buffer, uint32_t bufferSize)
{
	return _readSerialUntilEitherOr(buffer, bufferSize, "OK\r\n", "ERROR\r\n", TIME_OUT_READ_SERIAL);
}

ReadSerialResult GPRSLib::_readSerialUntilOkOrError(char *buffer, uint32_t bufferSize, uint32_t timeout)
{
	return _readSerialUntilEitherOr(buffer, bufferSize, "OK\r\n", "ERROR\r\n", timeout);
}

// Read serial until one of the two texts are found.
// Return:
// -2 = Timeout.
// -1 = Index exceeded buffer size before anything was found.
// 1 = eitherText is found.
// 2 = orText is found.
ReadSerialResult GPRSLib::_readSerialUntilEitherOr(char *buffer, uint32_t bufferSize, const char *eitherText, const char *orText)
{
	return _readSerialUntilEitherOr(buffer, bufferSize, eitherText, orText, TIME_OUT_READ_SERIAL);
}

ReadSerialResult GPRSLib::_readSerialUntilEitherOr(char *buffer, uint32_t bufferSize, const char *eitherText, const char *orText, uint32_t timeout)
{
	ReadSerialResult result = NOTHING_FOUND;
	uint32_t index = 0;
	uint32_t timerStart, timerEnd;
	timerStart = millis();
	bool cei = false, cor = false;
	char c;

	while (1)
	{
		while (_serial1->available())
		{
			c = _serial1->read();
			buffer[index++] = c;
			buffer[index] = '\0';
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

	if (_debug)
	{
		Serial.println("[DEBUG] [_readSerialUntilEitherOr]\0");
		if (cei)
		{
			Serial.print("Either text: ");
			Serial.println(eitherText);
		}
		if (cor)
		{
			Serial.print("Or text: ");
			Serial.println(orText);
		}
		Serial.println("Buffer: ");
		Serial.println(buffer);
	}

	while (_serial1->available())
		_serial1->read();

	return result;
}

// Read serial until <CR> and <LF> is found.
// Return:
// Length of the read string.
int GPRSLib::_readSerialUntilCrLf(char *buffer, uint32_t bufferSize)
{
	return _readSerialUntilCrLf(buffer, bufferSize, 0, TIME_OUT_READ_SERIAL);
}
int GPRSLib::_readSerialUntilCrLf(char *buffer, uint32_t bufferSize, uint32_t startIndex)
{
	return _readSerialUntilCrLf(buffer, bufferSize, startIndex, TIME_OUT_READ_SERIAL);
}
int GPRSLib::_readSerialUntilCrLf(char *buffer, uint32_t bufferSize, uint32_t startIndex, uint32_t timeout)
{
	if (startIndex >= bufferSize)
		return 0;

	uint64_t index = startIndex;
	uint64_t timerStart, timerEnd;
	timerStart = millis();
	bool cr = false, lf = false;
	char c;

	while (1)
	{
		while (_serial1->available())
		{
			c = _serial1->read();
			if (c == '\r')
				cr = true;
			if (c == '\n')
				lf = true;
			buffer[index++] = c;
			buffer[index] = '\0';
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

	if (_debug)
	{
		Serial.println(F("[DEBUG] [_readSerialUntilCrLf]\0"));
		Serial.println(buffer);
	}

	return index;
}

int GPRSLib::_writeSerial(const __FlashStringHelper *buffer)
{
	_serial1->print(buffer);
	_serial1->flushOutput();
	if (_debug)
	{
		Serial.print(F("[DEBUG] [_writeSerial] - \0"));
		Serial.println(buffer);
	}
	return strlen_P((const char *)buffer);
}

int GPRSLib::_writeSerial(const char *buffer)
{
	_serial1->print(buffer);
	_serial1->flushOutput();
	if (_debug)
	{
		Serial.print(F("[DEBUG] [_writeSerial] - \0"));
		Serial.println(buffer);
	}
	return strlen_P(buffer);
}

void GPRSLib::_extractTextBetween(const char *buffer, const int chr, char *output, unsigned int outputSize)
{
	// memset(output, '\0', outputSize);
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

	char *tok, *r, *end;
	r = end = strdup(&foundCmd[cmdLen]);

	while ((tok = strsep(&end, ",")) != NULL)
		if (++idx == paramNum)
		{
			if (strlen(tok) <= outputLength)
			{
				strcpy(output, tok);
				result = true;
			}
			break;
		}
	free(r);
	return result;
}
