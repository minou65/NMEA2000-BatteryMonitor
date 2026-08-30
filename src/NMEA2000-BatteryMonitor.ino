#include "APModeConfig.h"
#include "PowerSavingConfig.h"
#include <Arduino.h>
#include <esp_task_wdt.h>
#include <RebootManager.h>
#include <EventLogger.h>

#include "common.h"
#include "StatusHandling.h"
#include "SensorHandling.h"
#include "webHandling.h"
#include "TemperaturHandling.h"
#include "NMEAHandling.h"
#include "version.h"
#include "neotimer.h"
#include "SleepManager.h"

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

    eventLogger.log(F("========================================"));
    eventLogger.log(F("NMEA2000 battery monitor"));
    eventLogger.log(F("========================================"));
    eventLogger.logf("Firmware version: %s", Version);
    eventLogger.log(F("Sensor type: INA226 and DS1820 sensor"));
    eventLogger.logf("INA226 Alert pin:  GPIO_%d", INA226_ALERT_PIN);
    eventLogger.logf("INA226 SCL pin:    GPIO_%d", INA226_SCL_PIN);
    eventLogger.logf("INA226 SDA pin:    GPIO_%d", INA226_SDA_PIN);
    eventLogger.logf("DS1820 Sensor pin: GPIO_%d", ONE_WIRE_BUS);
    eventLogger.log(F("========================================"));

#ifdef DISABLE_BROWNOUT_DETECTOR
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"
    WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);
    eventLogger.log(F("Brownout detector disabled via build flag"));
#endif

    RebootManager::begin();
    eventLogger.logf("Reboot count: %d", RebootManager::getRebootCount());
    eventLogger.logf("Last reboot reason: %s", RebootManager::getLastRebootReasonText().c_str());

    eventLogger.log(F("Initializing WiFi..."));
    wifiSetup();

    eventLogger.log(F("Initializing sensor..."));
    sensorInit();
    TemperaturInit();

    eventLogger.log(F("Setting up NMEA2000..."));
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

    // Initialize sleep manager after all other initializations
    sleepManager.begin();
    
    // If we woke from deep sleep, restore RTC data
    if (esp_sleep_get_wakeup_cause() != ESP_SLEEP_WAKEUP_UNDEFINED) {
        batteryStatus.readStats();
    }

    eventLogger.log(F("========================================"));
    eventLogger.log(F("NMEA2000 started"));
    eventLogger.log(F("Listening for GPS time on NMEA2000 bus"));
    eventLogger.log(F("Setup complete"));
    eventLogger.log(F("========================================"));

   // sleepManager.disable();

}

void loop() {
    if (ParamsChanged) {
        updateINA226Config = true;
        ParamsChanged = false;
    }

    wifiLoop();
    sensorLoop();
    N2Kloop();

    batteryStatus.setTemperatur(GetTemperatur());

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