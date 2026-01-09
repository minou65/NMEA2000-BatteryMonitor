// TemperaturHandling.h

#ifndef _TEMPERATURHANDLING_h
#define _TEMPERATURHANDLING_h

#if defined(ARDUINO) && ARDUINO >= 100
	#include "arduino.h"
#else
	#include "WProgram.h"
#endif

#include <OneWire.h>
#include <DallasTemperature.h>

extern void TemperaturInit();
extern void TemperaturLoop();
extern volatile float GetTemperatur();

#endif