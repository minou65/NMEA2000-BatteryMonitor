#include <Arduino.h>
#include <esp_task_wdt.h>
#include <RebootManager.h>

#include "common.h"
#include "StatusHandling.h"
#include "SensorHandling.h"
#include "webHandling.h"
#include "TemperaturHandling.h"
#include "NMEAHandling.h"
#include "version.h"
#include "neotimer.h"

// Manufacturer's Software version code
char Version[] = VERSION_STR;

// # define IOTWEBCONF_DEBUG_TO_SERIAL true

// Task handle (Core 0 on ESP32)
TaskHandle_t TaskHandle;

void setup() {
    Serial.begin(115200);
    while (!Serial) {
        delay(1);
    }
    Serial.println("NMEA2000-BatteryMonitor v" + String(VERSION) + " started");

    RebootManager::begin();
	Serial.printf("Reboot count: %d\n", RebootManager::getRebootCount());
	Serial.printf("Last reboot reason: %s\n", RebootManager::getLastRebootReasonText().c_str());

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

    esp_task_wdt_add(NULL);
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

    esp_task_wdt_reset();
}

void loop2(void* parameter) {
    esp_task_wdt_add(NULL);
    for (;;) {   // Endless loop
        TemperaturLoop();
        esp_task_wdt_reset();
        vTaskDelay(1000);
    }
}