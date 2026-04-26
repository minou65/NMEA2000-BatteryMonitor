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
    uint32_t rebootCount_ = prefs_.getUInt("reboots", 0);
    rebootCount_++;
    prefs_.putUInt("reboots", rebootCount_);

	// store last reboot reason
    esp_reset_reason_t reason_ = esp_reset_reason();
    prefs_.putUInt("last_reason", (uint32_t)reason_);

    prefs_.end();

	Serial.print("Reboot reason: ");
    switch (reason_) {
        case ESP_RST_UNKNOWN:    Serial.println("Unknown"); break;
        case ESP_RST_POWERON:    Serial.println("Power on"); break;
        case ESP_RST_EXT:        Serial.println("External reset"); break;
        case ESP_RST_SW:         Serial.println("Software reset"); break;
        case ESP_RST_PANIC:      Serial.println("Panic reset"); break;
        case ESP_RST_INT_WDT:    Serial.println("Interrupt watchdog reset"); break;
        case ESP_RST_TASK_WDT:   Serial.println("Task watchdog reset"); break;
        case ESP_RST_WDT:        Serial.println("Other watchdog reset"); break;
        case ESP_RST_DEEPSLEEP:  Serial.println("Deep sleep reset"); break;
        case ESP_RST_BROWNOUT:   Serial.println("Brownout reset"); break;
        case ESP_RST_SDIO:       Serial.println("SDIO reset"); break;
        case ESP_RST_USB:        Serial.println("USB reset"); break;
        case ESP_RST_JTAG:       Serial.println("JTAG reset"); break;
        case ESP_RST_EFUSE:      Serial.println("EFUSE reset"); break;
        case ESP_RST_PWR_GLITCH: Serial.println("Power glitch reset"); break;
        case ESP_RST_CPU_LOCKUP: Serial.println("CPU lockup reset"); break;
        default:                Serial.println("Unknown reason code " + String((uint32_t)reason_)); break;
	}
	Serial.println("Reboot count: " + String(rebootCount_));
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
    esp_task_wdt_add(NULL);
    for (;;) {   // Endless loop
        TemperaturLoop();
        esp_task_wdt_reset();
        vTaskDelay(1000);
    }
}