// TemperaturHandling.h

#ifndef _TEMPERATURHANDLING_h
#define _TEMPERATURHANDLING_h

#if defined(ARDUINO) && ARDUINO >= 100
	#include "arduino.h"
#else
	#include "WProgram.h"
#endif

extern void TemperaturInit();
extern void TemperaturLoop();

#endif