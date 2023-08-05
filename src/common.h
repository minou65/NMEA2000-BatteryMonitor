#pragma once

#include <Arduino.h>
#include <N2kTypes.h>

#define STRING_LEN 40
#define NUMBER_LEN 32

extern bool gParamsChanged;

extern uint16_t gCapacityAh;
extern uint16_t gChargeEfficiencyPercent;
extern uint16_t gMinPercent;
extern uint16_t gTailCurrentmA;
extern uint16_t gFullVoltagemV;
extern uint16_t gFullDelayS;
extern float gShuntResistancemR;
extern uint16_t gMaxCurrentA;
extern bool gSensorInitialized;

extern tN2kBatType gBatteryType;

extern float gVoltageCalibrationFactor;
extern float gCurrentCalibrationFactor;

extern char gCustomName[64];
extern char Version[];

#define UPDATE_INTERVAL 990


