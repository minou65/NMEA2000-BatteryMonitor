#ifndef _NEMAHANDLING_h
#define _NEMAHANDLING_h

#if defined(ARDUINO) && ARDUINO >= 100
#include "arduino.h"
#else
#include "WProgram.h"
#endif



extern void N2kInit();
extern void N2Kloop();
extern void SendN2kBattery();
extern void OnN2kOpen();



#endif