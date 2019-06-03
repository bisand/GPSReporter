#include <GPRSLib.h>

GPRSLib::GPRSLib(char *buffer, uint16_t bufferSize)
{
	_serial1 = new AltSoftSerial(8, 9);
	_debugger = NULL;
	_buffer = buffer;
	_bufferSize = bufferSize;
}

GPRSLib::~GPRSLib()
{
	//delete _serial1;
}

void GPRSLib::setup(uint32_t baud, Stream &debugger, bool debug)
{
	_debugger = &debugger;
	pinMode(RESET_PIN, OUTPUT);
	digitalWrite(RESET_PIN, HIGH);
	delay(500);
	digitalWrite(RESET_PIN, LOW);
	delay(500);
	digitalWrite(RESET_PIN, HIGH);

	_baud = baud;
	_debug = debug;

	_serial1->begin(_baud);

	if (LED_FLAG)
	{
		pinMode(LED_PIN, OUTPUT);
	}
}

bool GPRSLib::gprsInit()
{
	// See code from here to get some examples:
	// https://github.com/Seeed-Studio/Seeeduino_GPRS
	_writeSerial(F("AT\r\n"));
	if (_readSerialUntilOkOrError(_buffer, _bufferSize) != FOUND_EITHER_TEXT)
		return false;
	_writeSerial(F("AT+CFUN=1\r\n"));
	if (_readSerialUntilOkOrError(_buffer, _bufferSize) != FOUND_EITHER_TEXT)
		return false;
	if(gprsSimStatus() != 0)
		return false;
	return true;
}

uint8_t GPRSLib::gprsSimStatus()
{
	uint8_t result = -1;
	uint8_t count = 0;
	while (count < 3)
	{
		_writeSerial(F("AT+CPIN?\r\n"));
		if (_readSerialUntilOkOrError(_buffer, _bufferSize) == FOUND_EITHER_TEXT)
		{
			_getResponseParams(_buffer, "+CPIN:", 1, _tmpBuf, sizeof(_tmpBuf));
			if (strstr(_tmpBuf, "READY") != NULL)
			{
				result = 0;
				break;
			}
		}
		count++;
		delay(300);
	}
	return result;
}

void GPRSLib::gprsDebug()
{
	if (_serial1->available())
	{
		_debugger->write(_serial1->read());
	}
	if (_debugger->available())
	{
		_serial1->write(_debugger->read());
	}
}

void GPRSLib::setSmsCallback(void (*smsCallback)(const char *tel, char *msg))
{
	_smsCallback = smsCallback;
}

bool GPRSLib::smsInit()
{
	//Set SMS mode to ASCII
	_writeSerial(F("AT+CMGF=1\r\n"));
	if (_readSerialUntilOkOrError(_buffer, _bufferSize) != FOUND_EITHER_TEXT)
		return false;
	delay(50);
	//Start listening to New SMS Message Indications
	_writeSerial(F("AT+CNMI=0,0,0,0,0\r\n"));
	if (_readSerialUntilOkOrError(_buffer, _bufferSize) != FOUND_EITHER_TEXT)
		return false;

	// for (size_t i = 0; i < 5; i++)
	// {
	// 	_writeSerial(F("AT+CMGD="));
	// 	char num[8];
	// 	_clearBuffer(num, sizeof(num));
	// 	itoa(i, num, 10);
	// 	_writeSerial(num);
	// 	_writeSerial(F("\r\n"));
	// 	char tmp[32];
	// 	_readSerialUntilOkOrError(tmp, sizeof(tmp));
	// }

	return true;
}

void GPRSLib::smsRead()
{
	// Example:
	// AT+CMGL="ALL"
	// +CMGL: 1,"REC UNREAD","+4798802600","","19/05/28,21:15:44+08"
	// Test 1
	// Test 2
	//
	// +CMGL: 2,"REC UNREAD","+4798802600","","19/05/28,21:15:48+08"
	// Test 3
	//
	// OK

	bool newMsg = false;
	uint64_t timerStart, timerEnd;
	uint32_t timeout = 5000;
	uint8_t msgIdx = 0;
	uint8_t msgIds[10];
	uint8_t msgCount = 0;
	timerStart = millis();
	_writeSerial(F("AT+CMGL=\"ALL\"\r\n"));
	do
	{
		_readSerialUntilCrLf(_buffer, _bufferSize);
		if (!newMsg && strstr(_buffer, "+CMGL:") != NULL)
		{
			_getResponseParams(_buffer, "+CMGL:", 1, _tmpBuf, sizeof(_tmpBuf));
			msgIdx = atoi(_tmpBuf);
			_getResponseParams(_buffer, "+CMGL:", 3, _tmpBuf, sizeof(_tmpBuf));
			_trimChar(_tmpBuf, '\"');
			msgIds[msgCount++] = msgIdx;
			newMsg = true;
		}
		else if (newMsg && strstr(_buffer, "OK\r\n") == NULL)
		{
			if (_smsCallback != NULL && strlen(_buffer) > 1)
				_smsCallback(_tmpBuf, _buffer);
		}
		else if (newMsg && strstr(_buffer, "OK\r\n") != NULL)
		{
			newMsg = false;
			msgIdx = 0;
		}
		timerEnd = millis();
		if (timerEnd - timerStart > timeout)
		{
			break;
		}

	} while (strstr(_buffer, "OK\r\n") == NULL);

	for (size_t i = 0; i < msgCount; i++)
	{
		char id[2];
		itoa(msgIds[i], id, 10);
		_writeSerial(F("AT+CMGD="));
		_writeSerial(id);
		_writeSerial(F("\r\n"));
		ReadSerialResult r = _readSerialUntilOkOrError(_tmpBuf, sizeof(_tmpBuf));
		if (r != FOUND_EITHER_TEXT)
		{
			_debugger->print(F("Error deleting SMS: "));
			_debugger->println(msgIdx);
		}
	}
}

bool GPRSLib::gprsIsRegistered()
{
	bool result = false;
	_writeSerial(F("AT+SAPBR=2,1\r\n"));
	ReadSerialResult res = _readSerialUntilOkOrError(_buffer, _bufferSize);

	_getResponseParams(_buffer, "+SAPBR:", 2, _tmpBuf, sizeof(_tmpBuf));
	if (res == FOUND_EITHER_TEXT && atoi(_tmpBuf) == 1)
		result = true;

	return result;
}

bool GPRSLib::gprsIsConnected()
{
	bool result = false;
	_writeSerial(F("AT+SAPBR=2,1\r\n"));
	ReadSerialResult res = _readSerialUntilOkOrError(_buffer, _bufferSize);

	_getResponseParams(_buffer, "+SAPBR:", 2, _tmpBuf, sizeof(_tmpBuf));
	if (res == FOUND_EITHER_TEXT && atoi(_tmpBuf) == 1)
		result = true;

	return result;
}

// GET IP Address
Result GPRSLib::gprsGetIP(char *ipAddress, uint16_t bufferSize)
{
	_writeSerial(F("AT+SAPBR=2,1\r\n"));
	if (_readSerialUntilOkOrError(_buffer, _bufferSize) != 1)
		return ERROR_OPEN_GPRS_CONTEXT;

	_getResponseParams(_buffer, "+SAPBR:", 2, _tmpBuf, sizeof(_tmpBuf));
	if (atoi(_tmpBuf) != 1)
		return ERROR_OPEN_GPRS_CONTEXT;

	if (strstr(_buffer, "+SAPBR:") == NULL)
		return ERROR_QUERY_GPRS_CONTEXT;

	_getResponseParams(_buffer, "+SAPBR:", 3, ipAddress, bufferSize);
	_trimChar(ipAddress, '\"');
	return SUCCESS;
}

Result GPRSLib::gprsCloseConn()
{
	_writeSerial(F("AT+SAPBR=0,1\r\n"));
	if (_readSerialUntilOkOrError(_buffer, _bufferSize) == 1)
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
	_writeSerial(F("AT+SAPBR=3,1,\"CONTYPE\",\"GPRS\"\r\n"));
	if (_readSerialUntilOkOrError(_buffer, _bufferSize) != FOUND_EITHER_TEXT)
		return ERROR_BEARER_PROFILE_GPRS;

	_writeSerial(F("AT+SAPBR=3,1,\"APN\",\""));
	_writeSerial(apn);
	_writeSerial(F("\"\r\n"));
	if (_readSerialUntilOkOrError(_buffer, _bufferSize) != FOUND_EITHER_TEXT)
		return ERROR_BEARER_PROFILE_APN;

	_writeSerial(F("AT+SAPBR=3,1,\"USER\",\""));
	_writeSerial(username);
	_writeSerial(F("\"\r\n"));
	if (_readSerialUntilOkOrError(_buffer, _bufferSize) != FOUND_EITHER_TEXT)
		return ERROR_BEARER_PROFILE_APN;

	_writeSerial(F("AT+SAPBR=3,1,\"PWD\",\""));
	_writeSerial(password);
	_writeSerial(F("\"\r\n"));
	if (_readSerialUntilOkOrError(_buffer, _bufferSize) != FOUND_EITHER_TEXT)
		return ERROR_BEARER_PROFILE_APN;

	// Open bearer
	_writeSerial(F("AT+SAPBR=1,1\r\n"));
	if (_readSerialUntilOkOrError(_buffer, _bufferSize) != FOUND_EITHER_TEXT)
		return ERROR_OPEN_GPRS_CONTEXT;

	// Query bearer
	_writeSerial(F("AT+SAPBR=2,1\r\n"));
	int res = _readSerialUntilOkOrError(_buffer, _bufferSize);
	_getResponseParams(_buffer, "+SAPBR:", 2, _tmpBuf, sizeof(_tmpBuf));
	if (res != FOUND_EITHER_TEXT || atoi(_tmpBuf) != 1)
		return ERROR_OPEN_GPRS_CONTEXT;

	return SUCCESS;
}

uint8_t GPRSLib::signalQuality()
{
	_writeSerial(F("AT+CSQ\r\n"));
	if (_readSerialUntilOkOrError(_buffer, _bufferSize) == FOUND_EITHER_TEXT)
	{
		_getResponseParams(_buffer, "+CSQ:", 1, _tmpBuf, sizeof(_tmpBuf));
		return atoi(_tmpBuf);
	}
	return 99;
}

Result GPRSLib::httpPostJson(const char *url, JsonDocument *data, const char *contentType, bool read, char *output, unsigned int outputSize)
{
	_clearBuffer(output, outputSize);
	bool https = false;
	if (strstr(url, "https://") != NULL)
		https = true;

	// if (!gprsIsConnected())
	// 	return ERROR_OPEN_GPRS_CONTEXT;

	// Terminate http connection, if it opened before!
	_writeSerial(F("AT+HTTPTERM\r\n"));
	_readSerialUntilOkOrError(_buffer, _bufferSize);
	_writeSerial(F("AT+HTTPINIT\r\n"));
	if (_readSerialUntilOkOrError(_buffer, _bufferSize) != FOUND_EITHER_TEXT)
		return ERROR_HTTP_INIT;

	if (https)
	{
		// Set SSL if https
		_writeSerial(F("AT+HTTPSSL=1\r\n"));
		if (_readSerialUntilOkOrError(_buffer, _bufferSize) != FOUND_EITHER_TEXT)
			return ERROR_HTTPS_ENABLE;
	}

	// Set bearer profile id
	_writeSerial(F("AT+HTTPPARA=\"CID\",1\r\n"));
	if (_readSerialUntilOkOrError(_buffer, _bufferSize) != FOUND_EITHER_TEXT)
		return ERROR_HTTP_PARA;

	// Set url
	_writeSerial(F("AT+HTTPPARA=\"URL\",\""));
	_writeSerial(url);
	_writeSerial(F("\"\r\n"));
	if (_readSerialUntilOkOrError(_buffer, _bufferSize) != FOUND_EITHER_TEXT)
	{
		Serial.print(url);
		return ERROR_HTTP_PARA;
	}
	// Set content type
	_writeSerial(F("AT+HTTPPARA=\"CONTENT\",\""));
	_writeSerial(contentType);
	_writeSerial(F("\"\r\n"));
	if (_readSerialUntilOkOrError(_buffer, _bufferSize) != FOUND_EITHER_TEXT)
		return ERROR_HTTP_PARA;

	// Indicate that buffer will be transfered within 30 secods.
	itoa(measureJson((*data)), _tmpBuf, 10);
	_writeSerial(F("AT+HTTPDATA="));
	_writeSerial(_tmpBuf);
	_writeSerial(F(",10000\r\n"));
	if (_readSerialUntilEitherOr(_buffer, _bufferSize, "DOWNLOAD\r\n", "ERROR\r\n", 10000) != FOUND_EITHER_TEXT)
		return ERROR_HTTP_DATA;

	// Send data.
	// _writeSerial(data);
	serializeJson((*data), (*_serial1));
	_writeSerial(F("\r\n"));
	if (_readSerialUntilOkOrError(_buffer, _bufferSize) != FOUND_EITHER_TEXT)
		return ERROR_HTTP_DATA;

	// Set action and perform request 1=POST
	_writeSerial(F("AT+HTTPACTION=1\r\n"));
	ReadSerialResult res = _readSerialUntilOkOrError(_buffer, _bufferSize);
	if (res != FOUND_EITHER_TEXT)
		return ERROR_HTTP_POST;

	delay(1000);
	int idx = _readSerialUntilCrLf(_buffer, _bufferSize);
	idx = _readSerialUntilCrLf(_buffer, _bufferSize, idx);

	// uint16_t idx = 0;
	// do
	// {
	// 	idx = _readSerialUntilCrLf(_buffer, _bufferSize, idx);
	// 	_debugger->println(_buffer);
	// 	delay(50);
	// 	_debugger->print(F("."));
	// } while (strstr(_buffer, "+HTTPACTION:") == NULL);
	// _debugger->println(F(""));
	// _debugger->println(F("POST buffer: "));
	// _debugger->println(_buffer);

	_getResponseParams(_buffer, "+HTTPACTION:", 1, _tmpBuf, sizeof(_tmpBuf));
	if (atoi(_tmpBuf) != 1)
		_debugger->print(F("ERROR: +HTTPACTION:")); //return ERROR_HTTP_POST;

	_getResponseParams(_buffer, "+HTTPACTION:", 2, _tmpBuf, sizeof(_tmpBuf));
	int httpResult = atoi(_tmpBuf);
	if (httpResult < 200 || httpResult >= 300)
		_debugger->print(F("ERROR: +HTTPACTION:")); //return ERROR_HTTP_POST;

	if (read)
	{
		_writeSerial(F("AT+HTTPREAD\r\n"));
		if (_readSerialUntilOkOrError(_buffer, _bufferSize, 10000) == FOUND_EITHER_TEXT)
		{
			_getResponseParams(_buffer, "+HTTPREAD:", 1, _tmpBuf, sizeof(_tmpBuf));
		}
	}

	_writeSerial(F("AT+HTTPTERM\r\n"));
	_readSerialUntilOkOrError(_buffer, _bufferSize);

	return SUCCESS;
}

bool GPRSLib::getValue(char *buffer, const char *cmd, uint8_t paramNum, char *output, uint16_t outputLength)
{
	bool result = false;
	uint8_t idx = 0;
	char *buf = strstr(buffer, cmd);
	if (buf == NULL)
	{
		return result;
	}
	uint8_t cmdLen = strlen(cmd);
	char *cmdBuf = strdup(&buf[cmdLen]);

	_clearBuffer(output, outputLength);

	char *pch;
	pch = strtok(cmdBuf, " ");
	while (pch != NULL)
	{
		if (++idx == paramNum)
		{
			strcpy(output, pch);
			result = true;
			break;
		}
		pch = strtok(NULL, " ");
	}
	free(cmdBuf);
	return result;
}

void (*resetFunc)(void) = 0; //declare reset function @ address 0

void GPRSLib::resetAll()
{
	resetFunc();
}

void GPRSLib::resetGsm()
{
	digitalWrite(RESET_PIN, LOW);
	delay(500);
	digitalWrite(RESET_PIN, HIGH);
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
	uint8_t sumEither, sumOr, lenEither, lenOr;
	sumEither = sumOr = 0;
	lenEither = strlen(eitherText);
	lenOr = strlen(orText);
	char c;

	buffer[0] = '\0';

	while (1)
	{
		if (_serial1->available() > 0)
		{
			c = _serial1->read();
			buffer[index++] = c;
			buffer[index] = '\0';
			sumEither = (c == eitherText[sumEither]) ? sumEither + 1 : 0;
			if (sumEither == lenEither)
			{
				result = FOUND_EITHER_TEXT;
				break;
			}
			sumOr = (c == orText[sumOr]) ? sumOr + 1 : 0;
			if (sumOr == lenOr)
			{
				result = FOUND_EITHER_TEXT;
				break;
			}
			if (index >= bufferSize - 1)
			{
				result = INDEX_EXCEEDED_BUFFER_SIZE;
				break;
			}
		}
		timerEnd = millis();
		if (timerEnd - timerStart > timeout)
		{
			result = TIMEOUT;
			break;
		}
	}

	if (_debug)
	{
		_debugger->println(F("[DEBUG] [_readSerialUntilEitherOr]\0"));
		if (result == FOUND_EITHER_TEXT)
		{
			_debugger->print(F("Either text: "));
			_debugger->println(eitherText);
		}
		if (result == FOUND_OR_TEXT)
		{
			_debugger->print(F("Or text: "));
			_debugger->println(orText);
		}
		_debugger->println(F("Buffer: "));
		_debugger->println(buffer);
	}

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

	uint32_t index = startIndex;
	uint32_t count = 0;
	uint64_t timerStart, timerEnd;
	timerStart = millis();
	bool cr = false, lf = false;
	char c;

	// if (startIndex > 0)
	// 	_clearBuffer(buffer, bufferSize);

	while (1)
	{
		while (_serial1->available() > 0)
		{
			c = _serial1->read();
			if (c == '\r')
				cr = true;
			if (c == '\n')
				lf = true;
			buffer[index++] = c;
			buffer[index] = '\0';
			count++;
			if (cr && lf)
				break;
			if (index >= bufferSize - 1)
				break;
		}
		if ((cr && lf) || index >= bufferSize - 1)
			break;
		timerEnd = millis();
		if (timerEnd - timerStart > timeout)
			break;
	}

	if (_debug)
	{
		_debugger->println(F("[DEBUG] [_readSerialUntilCrLf]\0"));
		_debugger->println(F("Buffer: "));
		_debugger->println(buffer);
	}

	return count;
}

int GPRSLib::_writeSerial(const __FlashStringHelper *buffer)
{
	_serial1->print(buffer);
	//_serial1->flushOutput();
	if (_debug)
	{
		_debugger->print(F("[DEBUG] [_writeSerial] -> \0"));
		_debugger->println(buffer);
	}
	// delay(50);
	return strlen_P((const char *)buffer);
}

int GPRSLib::_writeSerial(const char *buffer)
{
	_serial1->print(buffer);
	//_serial1->flushOutput();
	if (_debug)
	{
		_debugger->print(F("[DEBUG] [_writeSerial] -> \0"));
		_debugger->println(buffer);
	}
	//delay(50);
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
	len = l = strlen(buffer);
	bool sf, ef;
	sf = ef = false;
	char *tmpBuf = strdup(buffer);
	_clearBuffer(buffer, l);

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
}

bool GPRSLib::_getResponseParams(char *buffer, const char *cmd, uint8_t paramNum, char *output, uint16_t outputLength)
{
	bool result = false;
	uint8_t idx = 0;
	char *buf = strstr(buffer, cmd);
	if (buf == NULL)
	{
		return result;
	}
	uint8_t cmdLen = strlen(cmd);
	char *cmdBuf = strdup(&buf[cmdLen]);

	_clearBuffer(output, outputLength);

	char *pch;
	pch = strtok(cmdBuf, ",\r\n");
	while (pch != NULL)
	{
		if (++idx == paramNum)
		{
			strcpy(output, pch);
			result = true;
			break;
		}
		pch = strtok(NULL, ",\r\n");
	}
	free(cmdBuf);
	return result;
}
