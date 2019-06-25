#include "HMC5583L.h"

HMC5583L::HMC5583L(int address)
{
    //set starting angle and sensor i2c address
    sensorAddress = address;
    startingAngle = 0.0f;
}

void HMC5583L::initialize()
{
    //sensor intialization
    Wire.beginTransmission(sensorAddress);
    Wire.write(0x02);
    Wire.write(0x00);
    Wire.endTransmission();
}

float HMC5583L::getRotationAngle()
{
    //command line for the magnetometer to start reading its values
    Wire.beginTransmission(sensorAddress);
    Wire.write(0x03);
    Wire.endTransmission();
    //request 6 bytes from sensor
    Wire.requestFrom(sensorAddress, 6);
    int values[6];
    for (int i = 0; i < 6; i++)
    {
        values[i] = Wire.read();
    }
    //recorrer 8 bits (de 1 byte) y agregar los ultimos 8 bits (del segundo byte) para obtener el valor de cada eje
    int x = (values[0] << 8) + values[1];
    int y = (values[4] << 8) + values[5];
    int z = (values[2] << 8) + values[3];

    //obtener angulo en radianes mediante tangente de 2 lados de longitudes(x,y)
    float angle = (float)atan2(x, y);

    if (angle > PI * 2.0f)
    {
        angle -= PI * 2;
    }
    if (angle < 0.0f)
    {
        angle += PI * 2;
    }
    //cambiar de radianes a grados (360 grados=2PI radianes)
    /*regla de 3:
	360   -   2PI
	out   -  angle
	*/
    angle = (float)(360.0f / PI * 2.0f);
    return angle;
}

void HMC5583L::setAddress(int newAddress)
{
    sensorAddress = newAddress;
}
int HMC5583L::getAddress()
{
    return sensorAddress;
}
void HMC5583L::setStartingAngle()
{
    startingAngle = getRotationAngle();
}
float HMC5583L::getAngle()
{
    //modify the angle using the starting angle
    float angle = startingAngle - getRotationAngle();
    if (angle >= 360.0f)
    {
        angle -= 360.0f;
    }
    if (angle < 0.0f)
    {
        angle += 360.0f;
    }
    return angle;
}
float HMC5583L::getRawAngle()
{
    float angle = getRotationAngle();
    return angle;
}