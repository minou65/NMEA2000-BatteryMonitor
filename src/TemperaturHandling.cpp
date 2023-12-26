
#include "TemperaturHandling.h"
#include <OneWire.h>
#include <DallasTemperature.h>

#include "common.h"
#include "sensorHandling.h"
#include "statusHandling.h"

// #define DEBUG_Temperatur

static const float TemperatureCalibrationFactor = 1;

// Data wire is plugged into pin 18 on the Arduino
#define ONE_WIRE_BUS 18

// Setup a OneWire instance to communicate with any OneWire devices
OneWire oneWire(ONE_WIRE_BUS);

// Pass OneWire reference to Dallas Temperature
DallasTemperature sensors(&oneWire);

void TemperaturInit() {
    sensors.begin(); // Start up the library
}

void TemperaturLoop() {
    static unsigned long lastUpdate = 0;
    unsigned long now = millis();

    if (gParamsChanged) {

    }
    sensors.requestTemperatures(); // Send the command to get temperatures

    if (now - lastUpdate >= UPDATE_INTERVAL) {
        gBattery.setTemperatur(sensors.getTempCByIndex(0) * TemperatureCalibrationFactor);
        lastUpdate = now;
    }

#ifdef DEBUG_Temperatur
    Serial.print("Temperatur:   ");
    Serial.println(double(gBattery.temperatur()));
#endif // DEBUG_Temperatur
}
