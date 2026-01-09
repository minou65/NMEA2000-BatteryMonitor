#ifndef _SENSORHANDLING_h
#define _SENSORHANDLING_h

#if defined(ARDUINO) && ARDUINO >= 100
#include "arduino.h"
#else
#include "WProgram.h"
#endif

#include <Wire.h>

extern bool updateSensorConfig;

void sensorInit();
void sensorLoop();

#endif
