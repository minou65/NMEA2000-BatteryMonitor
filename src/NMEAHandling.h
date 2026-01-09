#ifndef _NEMAHANDLING_h
#define _NEMAHANDLING_h

#if defined(ARDUINO) && ARDUINO >= 100
#include "arduino.h"
#else
#include "WProgram.h"
#endif


#include <esp_mac.h>

#include <N2kMessages.h>
#include <NMEA2000.h>

class tNMEA2000;
extern tNMEA2000& NMEA2000;

extern void N2kInit();
extern void N2Kloop();
extern void SendN2kBattery();
extern void OnN2kOpen();



#endif