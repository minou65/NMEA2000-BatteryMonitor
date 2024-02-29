#include <Arduino.h>

#include "common.h"
#include "StatusHandling.h"
#include "SensorHandling.h"
#include "webHandling.h"
#include "TemperaturHandling.h"
#include "NMEAHandling.h"

char Version[] = "1.0.0.0 (2024-01-03)"; // Manufacturer's Software version code

// # define IOTWEBCONF_DEBUG_TO_SERIAL true

// Task handle (Core 0 on ESP32)
TaskHandle_t TaskHandle;

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

    xTaskCreatePinnedToCore(
        loop2, /* Function to implement the task */
        "TaskHandle", /* Name of the task */
        10000,  /* Stack size in words */
        NULL,  /* Task input parameter */
        0,  /* Priority of the task */
        &TaskHandle,  /* Task handle. */
        0 /* Core where the task should run */
    );
}

void loop() {
    if (gParamsChanged) {

    }
    wifiLoop();
    sensorLoop();
    N2Kloop();

    gParamsChanged = false;
}

void loop2(void* parameter) {
    for (;;) {   // Endless loop

        TemperaturLoop();
        vTaskDelay(100);

    }
}