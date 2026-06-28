// common.h

#pragma once

#ifndef _COMMON_h
#define _COMMON_h

#if defined(ARDUINO) && ARDUINO >= 100
#include "arduino.h"
#else
#include "WProgram.h"
#endif

#include <Arduino.h>
#include <N2kTypes.h>

extern bool debugMode;

#define DEBUG_PRINT(x) if (debugMode) Serial.print(x) 
#define DEBUG_PRINTLN(x) if (debugMode) Serial.println(x)
#define DEBUG_PRINTF(...) if (debugMode) Serial.printf(__VA_ARGS__)

#define STRING_LEN 50
#define NUMBER_LEN 10
#define DATE_LEN 11
#define TIME_LEN 6

extern bool gParamsChanged;
extern bool gSaveParams;

extern bool gSensorInitialized;


extern char gCustomName[64];
extern char Version[];

extern uint8_t gN2KSource;
extern uint8_t gN2KSID;
extern uint8_t gN2KInstance;

#define UPDATE_INTERVAL 1000

extern String getCurrentTime();

#endif
