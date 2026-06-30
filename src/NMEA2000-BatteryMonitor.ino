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

    Serial.println("\n========================================");
    Serial.println("NMEA2000 battery monitor");
    Serial.println("========================================");
    Serial.printf("Firmware version: %s\n", Version);
    Serial.println("Sensor type: INA226 and DS1820 sensor");
    Serial.printf("INA226 Alert pin:  GPIO_%d\n", INA226_ALERT_PIN);
    Serial.printf("INA226 SCL pin:    GPIO_%d\n", INA226_SCL_PIN);
    Serial.printf("INA226 SDA pin:    GPIO_%d\n", INA226_SDA_PIN);
    Serial.printf("DS1820 Sensor pin: GPIO_%d\n", ONE_WIRE_BUS);
    Serial.println("========================================\n");

#ifdef DISABLE_BROWNOUT_DETECTOR
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"
    WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);
    Serial.println("Brownout detector disabled via build flag");
#endif

    RebootManager::begin();
	Serial.printf("Reboot count: %d\n", RebootManager::getRebootCount());
	Serial.printf("Last reboot reason: %s\n", RebootManager::getLastRebootReasonText().c_str());

    Serial.println("\nInitializing WiFi...");
    wifiSetup();

    Serial.println("\nInitializing sensor...");
    sensorInit();
    TemperaturInit();

    Serial.println("\nSetting up NMEA2000...");
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

    Serial.println("\n========================================");
    Serial.println("NMEA2000 started");
    Serial.println("Listening for GPS time on NMEA2000 bus");
    Serial.println("Setup complete");
    Serial.println("========================================\n");
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