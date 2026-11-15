

#include <OneWire.h>
#include <DallasTemperature.h>
#include <neotimer.h>
#include <EventLogger.h>

#include "common.h"
#include "neotimer.h"
#include "TemperaturHandling.h"

static const float TemperatureCalibrationFactor = 1;

// Setup a OneWire instance to communicate with any OneWire devices
OneWire oneWire(ONE_WIRE_BUS);

// Pass OneWire reference to Dallas Temperature
DallasTemperature sensors(&oneWire);

Neotimer sensorTimer = Neotimer(UPDATE_INTERVAL);

volatile float Temperatur = 0.0f;

void TemperaturInit() {
    sensors.begin(); // Start up the library
	sensors.setResolution(10); // Set the resolution to 10 bit (can be 9, 10, 11, or 12)
    sensorTimer.start();
}

void TemperaturLoop() {
    
    if (sensorTimer.repeat()) {
		sensors.requestTemperatures(); // Send the command to get temperatures
		Temperatur = sensors.getTempCByIndex(0) * TemperatureCalibrationFactor;
	}


    if (debugMode) {
        LOG_DEBUG("Temperatur: %f", double(Temperatur));
    }
}

volatile float GetTemperatur() {
	return Temperatur;
}
