#include <Arduino.h>

#include "common.h"
#include "StatusHandling.h"
#include "SensorHandling.h"
#include "webHandling.h"
#include "TemperaturHandling.h"
#include "NMEAHandling.h"

char Version[] = "0.0.0.2 (2023-07-31)"; // Manufacturer's Software version code

# define IOTWEBCONF_DEBUG_TO_SERIAL true

void setup() {
    Serial.begin(115200);

    // wait for serial port to open on native usb devices
    while (!Serial) {
        delay(1);
    }

    wifiSetup();
    sensorInit();
    TemperaturInit();
    N2kInit();
}

void loop() {
    if (gParamsChanged) {

    }

    wifiLoop();
    sensorLoop();
    TemperaturLoop();
    N2Kloop();

    gParamsChanged = false;
}