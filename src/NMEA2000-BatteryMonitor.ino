#include <Arduino.h>

#include "common.h"
#include "StatusHandling.h"
#include "SensorHandling.h"
#include "webHandling.h"
#include "TemperaturHandling.h"
#include "NMEAHandling.h"

<<<<<<< HEAD
char Version[] = "1.1.0.6 (2024-05-21)"; // Manufacturer's Software version code
=======
char Version[] = "1.1.0.5 (2024-05-19)"; // Manufacturer's Software version code
>>>>>>> 3bdfd4878abd1023bd62f2882f8212403dc01d9f

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
        2000,  /* Stack size in words */
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

        //UBaseType_t stackHighWaterMark = uxTaskGetStackHighWaterMark(NULL);
        //size_t remainingStackBytes = stackHighWaterMark * sizeof(StackType_t);
        //Serial.print("Remaining stack space (bytes): "); Serial.println(remainingStackBytes);

    }
}