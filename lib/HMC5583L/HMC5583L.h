// Library copied from https://github.com/CFOP/L16/tree/master/Code/Libraries/HMC5583L
#include "Arduino.h"
#include "Wire.h"

#ifndef HMC5583L_h
#define HMC5583L_h
#define HMC5583L_DEFAULT_ADDRESS 0x1E
class HMC5583L
{
private:
    int sensorAddress;
    float startingAngle;
    float getRotationAngle();

public:
    /*
		Constructor method: sets the initial I^2C address for the sensor 
		and sets the initial rotation angle based on magnetic north.
		method args: I^2C address
		*/
    HMC5583L(int);
    /*
		*/
    void initialize();
    /*
		Constructor method: sets the initial I^2C address for the sensor 
		and sets the initial rotation angle based on magnetic north.
		method args: I^2C address
		*/
    void setAddress(int);
    int getAddress();
    /*
		reset angle method: resets the initial rotation angle
		method args: none
		*/
    void setStartingAngle();
    float getAngle();
    float getRawAngle();
};
#endif