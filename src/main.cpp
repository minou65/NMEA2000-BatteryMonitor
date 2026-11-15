#include <Arduino.h>
#include <esp_task_wdt.h>
#include <RebootManager.h>
#include <IotWebConfAsyncTab.h>
#include <EventLogger.h>
#include <neotimer.h>
#include <build_version.h>

#include "common.h"
#include "StatusHandling.h"
#include "SensorHandling.h"
#include "webHandling.h"
#include "TemperaturHandling.h"
#include "NMEAHandling.h"

//Foward declarations
void loop2(void* parameter);

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

    LOG_INFO("========================================");
    LOG_INFO("NMEA2000 battery monitor");
    LOG_INFO("========================================");
    LOG_INFO("Firmware version: %s", Version);
    LOG_INFO("Sensor type: INA226 and DS1820 sensor");
    LOG_DEBUG("INA226 Alert pin:  GPIO_%d", INA226_ALERT_PIN);
    LOG_DEBUG("INA226 SCL pin:    GPIO_%d", INA226_SCL_PIN);
    LOG_DEBUG("INA226 SDA pin:    GPIO_%d", INA226_SDA_PIN);
    LOG_DEBUG("DS1820 Sensor pin: GPIO_%d", ONE_WIRE_BUS);
    LOG_INFO("========================================");

#ifdef DISABLE_BROWNOUT_DETECTOR
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"
    WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);
    LOG_DEBUG("Brownout detector disabled via build flag");
#endif

    RebootManager::begin();
    LOG_DEBUG("Reboot count: %d", RebootManager::getRebootCount());
    LOG_DEBUG("Last reboot reason: %s", RebootManager::getLastRebootReasonText().c_str());

    wifiSetup();
    LOG_OK("WiFi initialized");
    
    sensorInit();
    TemperaturInit();
    LOG_OK("Sensors initialized");

    LOG_INFO("Setting up NMEA2000...");
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

    LOG_INFO("========================================");
    LOG_INFO("NMEA2000 started");
    LOG_INFO("Listening for GPS time on NMEA2000 bus");
    LOG_INFO("Setup complete");
    LOG_INFO("========================================");
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