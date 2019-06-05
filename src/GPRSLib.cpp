#include "GPRSLib.h"

GPRSLib::GPRSLib(char *buffer, uint16_t bufferSize)
{
	_buffer = buffer;
	_bufferSize = bufferSize;
}

GPRSLib::~GPRSLib()
{
}

void GPRSLib::setup(uint32_t baud, bool debug)
{
	pinMode(RESET_PIN, OUTPUT);
	digitalWrite(RESET_PIN, HIGH);
	delay(500);
	digitalWrite(RESET_PIN, LOW);
	delay(500);
	digitalWrite(RESET_PIN, HIGH);

	_baud = baud;
	_debug = debug;

	_serial1.begin(_baud);

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
	if (gprsSimStatus() != 0)
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
	if (_serial1.available())
	{
		Serial.write(_serial1.read());
	}
	if (Serial.available())
	{
		_serial1.write(Serial.read());
	}
}

void GPRSLib::setSmsCallback(void (*smsCallback)(const char *tel, char *cmd, char *val))
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
			_removeChar(_buffer, '\n');
			_removeChar(_buffer, '\r');
			if (_smsCallback != NULL && strlen(_buffer) > 1)
			{
				getSmsCmd(_buffer, _smsCmd, sizeof(_smsCmd));
				getSmsVal(_buffer, _smsVal, sizeof(_smsVal));
				_smsCallback(_tmpBuf, _smsCmd, _smsVal);
			}
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
			DBG_PRN(F("Error deleting SMS: "));
			DBG_PRNLN(msgIdx);
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

bool GPRSLib::gprsGetImei(char *buffer, uint8_t bufferSize)
{
	bool result = false;
	_writeSerial(F("AT+GSN\r\n"));
	delay(100);
	ReadSerialResult res = _readSerialUntilOkOrError(_buffer, _bufferSize);

	// strncpy(buffer, _buffer, bufferSize);
	_getResponseParams(_buffer, "AT+GSN", 1, buffer, bufferSize);
	if (res == FOUND_EITHER_TEXT)
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

Result GPRSLib::httpPostJson(const char *url, JsonDocument *data, const char *contentType, bool read, char *output, uint16_t outputSize)
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
	serializeJson((*data), _serial1);
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
	// 	DBG_PRNLN(_buffer);
	// 	delay(50);
	// 	DBG_PRN(F("."));
	// } while (strstr(_buffer, "+HTTPACTION:") == NULL);
	// DBG_PRNLN(F(""));
	// DBG_PRNLN(F("POST buffer: "));
	// DBG_PRNLN(_buffer);

	_getResponseParams(_buffer, "+HTTPACTION:", 1, _tmpBuf, sizeof(_tmpBuf));
	if (atoi(_tmpBuf) != 1)
	{
		DBG_PRN(F("ERROR: +HTTPACTION: ")); //return ERROR_HTTP_POST;
		DBG_PRN(_tmpBuf);
	}
	_getResponseParams(_buffer, "+HTTPACTION:", 2, _tmpBuf, sizeof(_tmpBuf));
	int httpResult = atoi(_tmpBuf);
	if (httpResult < 200 || httpResult >= 300)
	{
		DBG_PRN(F("ERROR: +HTTPACTION: ")); //return ERROR_HTTP_POST;
		DBG_PRN(_tmpBuf);
	}
	if (read)
	{
		_writeSerial(F("AT+HTTPREAD\r\n"));
		if (_readSerialUntilOkOrError(_buffer, _bufferSize, 10000) == FOUND_EITHER_TEXT)
		{
			_getResponseParams(_buffer, "+HTTPREAD:", 2, output, outputSize);
		}
	}

	_writeSerial(F("AT+HTTPTERM\r\n"));
	_readSerialUntilOkOrError(_buffer, _bufferSize);

	return SUCCESS;
}

void GPRSLib::lowerCmd(char *s)
{
	char *p;

	for (p = s; *p != ' ' && *p != '\0'; p++)
		*p = (char)tolower(*p);
}

void GPRSLib::lower(char *s)
{
	char *p;

	for (p = s; *p != '\0'; p++)
		*p = (char)tolower(*p);
}

bool GPRSLib::getSmsCmd(char *buffer, char *output, uint16_t outputLength)
{
	if (outputLength < 1)
		return false;
	_clearBuffer(output, outputLength);
	uint16_t count = 0;
	for (; *buffer != ' ' && *buffer != '\0'; buffer++)
	{
		if (count++ >= outputLength - 1)
		{
			*output++ = '\0';
			break;
		}
		*output++ = (char)tolower(*buffer);
	}

	return strlen(output) > 0;
}

bool GPRSLib::getSmsVal(char *buffer, char *output, uint16_t outputLength)
{
	if (outputLength < 1)
		return false;
	_clearBuffer(output, outputLength);

	char *buf = strstr(buffer, " ");
	if (buf != NULL && outputLength > 1)
	{
		strncpy(output, &buf[1], outputLength - 1);
		output[outputLength - 1] = '\0';
	}
	return strlen(output) > 0;
}

bool GPRSLib::getValue(char *buffer, char *cmd, char *output, uint16_t outputLength)
{
	bool result = false;
	_clearBuffer(output, outputLength);
	char *cmdTest = strdup(cmd);
	lower(cmdTest);
	uint16_t len = strlen(cmdTest);
	char cmdBuf[len + 1];
	_clearBuffer(cmdBuf, len + 1);
	getSmsCmd(buffer, cmdBuf, len + 1);
	lower(cmdBuf);
	if (strncmp(cmdTest, cmdBuf, len) == 0)
	{
		if (getSmsVal(buffer, output, outputLength))
			result = true;
	}
	free(cmdTest);
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

void GPRSLib::flush()
{
	while (_serial1.available() > 0)
	{
		_serial1.read();
	}
}

//////////////////////////////////////
//			PRIVATE METHODS			//
//////////////////////////////////////
void GPRSLib::_clearBuffer(char *buffer, uint8_t size)
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
ReadSerialResult GPRSLib::_readSerialUntilOkOrError(char *buffer, uint8_t bufferSize)
{
	return _readSerialUntilEitherOr(buffer, bufferSize, "OK\r\n", "ERROR\r\n", TIME_OUT_READ_SERIAL);
}

ReadSerialResult GPRSLib::_readSerialUntilOkOrError(char *buffer, uint8_t bufferSize, uint16_t timeout)
{
	return _readSerialUntilEitherOr(buffer, bufferSize, "OK\r\n", "ERROR\r\n", timeout);
}

// Read serial until one of the two texts are found.
// Return:
// -2 = Timeout.
// -1 = Index exceeded buffer size before anything was found.
// 1 = eitherText is found.
// 2 = orText is found.
ReadSerialResult GPRSLib::_readSerialUntilEitherOr(char *buffer, uint8_t bufferSize, const char *eitherText, const char *orText)
{
	return _readSerialUntilEitherOr(buffer, bufferSize, eitherText, orText, TIME_OUT_READ_SERIAL);
}

ReadSerialResult GPRSLib::_readSerialUntilEitherOr(char *buffer, uint8_t bufferSize, const char *eitherText, const char *orText, uint16_t timeout)
{
	ReadSerialResult result = NOTHING_FOUND;
	uint8_t index = 0;
	uint64_t timerStart, timerEnd;
	timerStart = millis();
	uint8_t sumEither, sumOr, lenEither, lenOr;
	sumEither = sumOr = 0;
	lenEither = strlen(eitherText);
	lenOr = strlen(orText);
	char c;

	buffer[0] = '\0';

	while (1)
	{
		if (_serial1.available() > 0)
		{
			c = _serial1.read();
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
		DBG_PRNLN(F("[DEBUG] [_readSerialUntilEitherOr]\0"));
		if (result == FOUND_EITHER_TEXT)
		{
			DBG_PRN(F("Either text: "));
			if (strcmp(eitherText, "\r\n") == 0)
				DBG_PRNLN(F("<CR><LF>"));
			else
				DBG_PRNLN(eitherText);
		}
		if (result == FOUND_OR_TEXT)
		{
			DBG_PRN(F("Or text: "));
			if (strcmp(orText, "\r\n") == 0)
				DBG_PRNLN(F("<CR><LF>"));
			else
				DBG_PRNLN(orText);
		}
		DBG_PRNLN(F("Buffer: "));
		DBG_PRNLN(buffer);
	}

	return result;
}

// Read serial until <CR> and <LF> is found.
// Return:
// Length of the read string.
int GPRSLib::_readSerialUntilCrLf(char *buffer, uint8_t bufferSize)
{
	return _readSerialUntilCrLf(buffer, bufferSize, 0, TIME_OUT_READ_SERIAL);
}
int GPRSLib::_readSerialUntilCrLf(char *buffer, uint8_t bufferSize, uint8_t startIndex)
{
	return _readSerialUntilCrLf(buffer, bufferSize, startIndex, TIME_OUT_READ_SERIAL);
}
int GPRSLib::_readSerialUntilCrLf(char *buffer, uint8_t bufferSize, uint8_t startIndex, uint16_t timeout)
{
	char term[] = "\r\n";
	return _readSerialUntil(buffer, bufferSize, term, startIndex, TIME_OUT_READ_SERIAL);
}
int GPRSLib::_readSerialUntil(char *buffer, uint8_t bufferSize, char *terminator, uint8_t startIndex, uint16_t timeout)
{
	if (startIndex >= bufferSize)
		return 0;

	uint8_t index = startIndex;
	uint8_t count = 0;
	uint8_t sum = 0;
	uint8_t len = strlen(terminator);
	uint64_t timerStart, timerEnd;
	timerStart = millis();
	char c;

	while (1)
	{
		if (_serial1.available() > 0)
		{
			c = _serial1.read();
			buffer[index++] = c;
			buffer[index] = '\0';
			sum = (c == terminator[sum]) ? sum + 1 : 0;
			count++;
			if (sum == len)
				break;
			if (index >= bufferSize - 1)
				break;
		}
		timerEnd = millis();
		if (timerEnd - timerStart > timeout)
			break;
	}

	if (_debug)
	{
		DBG_PRNLN(F("[DEBUG] [_readSerialUntil]\0"));
		if (strcmp(terminator, "\r\n") == 0)
			DBG_PRNLN(F("<CR><LF>"));
		else
			DBG_PRNLN(terminator);
		DBG_PRNLN(F("Buffer: "));
		DBG_PRNLN(buffer);
	}

	return count;
}

int GPRSLib::_writeSerial(const __FlashStringHelper *buffer)
{
	_serial1.print(buffer);
	if (_debug)
	{
		DBG_PRN(F("[DEBUG] [_writeSerial] -> \0"));
		DBG_PRNLN(buffer);
	}
	return 0;
}

int GPRSLib::_writeSerial(const char *buffer)
{
	_serial1.print(buffer);
	if (_debug)
	{
		DBG_PRN(F("[DEBUG] [_writeSerial] -> \0"));
		DBG_PRNLN(buffer);
	}
	return 0;
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

void GPRSLib::_removeChar(char *buffer, char chr)
{
	uint32_t len = strlen(buffer);
	uint32_t pos = 0;
	char *tmpBuf = strdup(buffer);
	_clearBuffer(buffer, len);

	for (size_t i = 0; i < len; i++)
	{
		if (tmpBuf[i] != chr)
		{
			buffer[pos++] = tmpBuf[i];
		}
	}

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
