#include <Arduino.h>
#include <esp_task_wdt.h>
#include <Preferences.h>
#include <esp_system.h>

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

// Watchdog timeout in seconds
#define WDT_TIMEOUT 10

// # define IOTWEBCONF_DEBUG_TO_SERIAL true

// Task handle (Core 0 on ESP32)
TaskHandle_t TaskHandle;

void logRebootReason() {
    Preferences prefs_;
    prefs_.begin("system", false);

	// increment reboot count
    uint32_t rebootCount = prefs_.getUInt("reboots", 0);
    rebootCount++;
    prefs_.putUInt("reboots", rebootCount);

	// store last reboot reason
    esp_reset_reason_t reason = esp_reset_reason();
    prefs_.putUInt("last_reason", (uint32_t)reason);

    prefs_.end();
}

void setup() {

    Serial.begin(115200);
    while (!Serial) {
        delay(1);
    }
    Serial.println("NMEA2000-BatteryMonitor v" + String(VERSION) + " started");

    logRebootReason();

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


	// Initialize the Task Watchdog Timer (TWDT) with a timeout of WDT_TIMEOUT seconds
	// and add the current task to the TWDT. The TWDT will reset the system if the task
    // fails to reset it within the specified timeout.
    esp_task_wdt_config_t wdt_config = {
        .timeout_ms = WDT_TIMEOUT * 1000, // Timeout in Millisekunden
        .idle_core_mask = (1 << portNUM_PROCESSORS) - 1, // Alle Kerne überwachen
        .trigger_panic = true
    };
    esp_task_wdt_init(&wdt_config);
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
    for (;;) {   // Endless loop
        TemperaturLoop();
        esp_task_wdt_reset();
        vTaskDelay(1000);
    }
}