#include <Arduino.h>

#include "common.h"
#include "StatusHandling.h"
#include "SensorHandling.h"
#include "webHandling.h"
#include "TemperaturHandling.h"
#include "NMEAHandling.h"

char Version[] = "1.1.0.7 (2024-05-25)"; // Manufacturer's Software version code

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
        16384,  /* Stack size in words */
        NULL,  /* Task input parameter */
        0,  /* Priority of the task */
        &TaskHandle,  /* Task handle. */
        0 /* Core where the task should run */
    );
}

void loop() {
    if (gParamsChanged) {
        updateSensorConfig = true;
        gParamsChanged = false;
    }

    wifiLoop();
    sensorLoop();
    N2Kloop();

    gBattery.setTemperatur(GetTemperatur());

}

void loop2(void* parameter) {
    for (;;) {   // Endless loop

        TemperaturLoop();
        vTaskDelay(1000);

        //UBaseType_t stackHighWaterMark = uxTaskGetStackHighWaterMark(NULL);
        //size_t remainingStackBytes = stackHighWaterMark * sizeof(StackType_t);
        //Serial.print("Remaining stack space (bytes): "); Serial.println(remainingStackBytes);

    }
}