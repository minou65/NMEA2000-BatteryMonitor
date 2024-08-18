#include <Arduino.h>
#include <esp_task_wdt.h>

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

#define WDT_TIMEOUT 5

// # define IOTWEBCONF_DEBUG_TO_SERIAL true

// Task handle (Core 0 on ESP32)
TaskHandle_t TaskHandle;

Neotimer WDtimer = Neotimer(4000);

void setup() {

    Serial.begin(115200);
    while (!Serial) {
        delay(1);
    }
    Serial.println("NMEA2000-BatteryMonitor v" + String(VERSION) + " started");

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

    //esp_task_wdt_init(WDT_TIMEOUT, true); //enable panic so ESP32 restarts
    //esp_task_wdt_add(NULL); //add current thread to WDT watch

    //WDtimer.start();
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

    //if(WDtimer.repeat()) {
	//	esp_task_wdt_reset();
	//}
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