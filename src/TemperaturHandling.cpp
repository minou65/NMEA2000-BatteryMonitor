
#include "TemperaturHandling.h"
#include <OneWire.h>
#include <DallasTemperature.h>

#include "common.h"
#include "sensorHandling.h"
#include "statusHandling.h"
#include "neotimer.h"

// #define DEBUG_Temperatur

static const float TemperatureCalibrationFactor = 1;

// Data wire is plugged into pin 18 on the Arduino
#define ONE_WIRE_BUS 18

// Setup a OneWire instance to communicate with any OneWire devices
OneWire oneWire(ONE_WIRE_BUS);

// Pass OneWire reference to Dallas Temperature
DallasTemperature sensors(&oneWire);

Neotimer sensorTimer = Neotimer(UPDATE_INTERVAL);

volatile float Temperatur = 0.0f;

void TemperaturInit() {
    sensors.begin(); // Start up the library
    sensorTimer.start();
}

void TemperaturLoop() {
    
    if (sensorTimer.repeat()) {
		sensors.requestTemperatures(); // Send the command to get temperatures
		Temperatur = sensors.getTempCByIndex(0) * TemperatureCalibrationFactor;
	}

#ifdef DEBUG_Temperatur
    Serial.print("Temperatur:   ");
    Serial.println(double(gBattery.temperatur()));
#endif // DEBUG_Temperatur
}

volatile float GetTemperatur() {
	return Temperatur;
}
