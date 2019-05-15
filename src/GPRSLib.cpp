#include <GPRSLib.h>

GPRSLib::GPRSLib(/* args */)
{
}

GPRSLib::~GPRSLib()
{
}

void GPRSLib::setup(unsigned int baud)
{
	pinMode(RESET_PIN, OUTPUT);
	digitalWrite(RESET_PIN, HIGH);

	_baud = baud;

	this->begin(_baud);

	if (LED_FLAG)
	{
		pinMode(LED_PIN, OUTPUT);
	}
}

bool GPRSLib::gprsIsConnected()
{
	this->print(("AT+SAPBR=2,1\r"));
	int idx = _readSerial(_buffer);
	delay(50);
	_readSerial(_buffer, idx);

	if (strstr(_buffer, "\"0.0.0.0\"") == NULL || strstr(_buffer, "ERR") == NULL)
		return false;

	return true;
}
// GET IP Address
void GPRSLib::gprsGetIP(char* ipAddress)
{
	this->print(("AT+SAPBR=2,1\r\n"));
	int idx = _readSerial(_buffer);
	delay(50);
	_readSerial(_buffer, idx);

	if (strstr(_buffer, "\"0.0.0.0\"") == NULL || strstr(_buffer, "ERR") == NULL)
		ipAddress = (char*)"ERROR:NO_IP";

	if (strstr(_buffer, "+SAPBR:") == NULL)
	{
		_extractTextBetween(_buffer, '\"', ipAddress);
	}

	ipAddress = (char*)"ERROR:NO_IP_FETCH";
}

void GPRSLib::_extractTextBetween(const char *buffer, const int chr, char *output)
{
	char *pch;
	byte len = strlen(buffer);
	pch = strchr(buffer, chr);
	if (pch == NULL)
		output = (char*)"";
	byte start = (byte)(pch - buffer + 1);
	byte end = (byte)len;
	while (pch != NULL)
	{
		pch = strchr(pch + 1, chr);
		if (pch != NULL)
			end = (byte)(pch - buffer + 1);
	}

	memset(output, '\0', len);
	strncpy(output, &buffer[start], end - start);
}

bool GPRSLib::gprsCloseConn()
{
	this->print(F("AT+SAPBR=0,1\r"));
	int idx = _readSerial(_buffer);
	delay(50);
	_readSerial(_buffer, idx);

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
	this->print(F("AT+SAPBR=3,1,\"CONTYPE\",\"GPRS\"\r"));
	_readSerial(_buffer);

	if (strstr(_buffer, "OK") == NULL)
		return false;

	delay(100);
	this->print(F("AT+SAPBR=3,1,\"APN\",\""));
	this->print(apn);
	this->print(F("\"\r"));
	_readSerial(_buffer);

	if (strstr(_buffer, "OK") == NULL)
		return false;

	delay(100);

	this->print(F("AT+SAPBR=3,1,\"USER\",\""));
	this->print(username);
	this->print(F("\"\r"));
	_readSerial(_buffer);

	if (strstr(_buffer, "OK") == NULL)
		return false;

	delay(100);

	this->print(F("AT+SAPBR=3,1,\"PWD\",\""));
	this->print(password);
	this->print(F("\"\r"));
	_readSerial(_buffer);

	if (strstr(_buffer, "OK") == NULL)
		return false;

	delay(100);
	this->print("AT+SAPBR=1,1\r");
	int res = _readSerial(_buffer);
	delay(50);
	_readSerial(_buffer, res);
	if (strstr(_buffer, "OK") == NULL)
		return false;

	this->print("AT+SAPBR=2,1\r");
	_readSerial(_buffer);

	if (strstr(_buffer, "\"0.0.0.0\"") == NULL || strstr(_buffer, "ERR") == NULL)
		return false;

	return true;
}

//////////////////////////////////////
//			PRIVATE METHODS			//
//////////////////////////////////////

// READ FROM SERIAL
int GPRSLib::_readSerial(char *buffer, int startIndex = 0, unsigned int timeout = TIME_OUT_READ_SERIAL)
{
	if (startIndex > BUFFER_RESERVE_MEMORY)
		return 0;

	int index = startIndex;
	uint64_t timeOld = millis();

	while (!this->available() && !(millis() > timeOld + timeout))
		delay(13);

	while (this->available())
		if (this->available() && index < BUFFER_RESERVE_MEMORY)
			buffer[index++] = (char)this->read();

	return index;
}
