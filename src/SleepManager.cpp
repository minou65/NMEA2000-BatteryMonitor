#include "SleepManager.h"
#include "common.h"
#include "Config.h"
#include "statusHandling.h"
#include "INA226.h"
#include <EventLogger.h>
#include <driver/gpio.h>
#include <driver/rtc_io.h>

SleepManager sleepManager;

SleepManager::SleepManager() {
    lastActivity = 0;
    sleepTimeout = SLEEP_TIMEOUT_MS;
    sleepEnabled = true;
}

void SleepManager::begin() {
    lastActivity = millis();
    
    eventLogger.log(F("Sleep Manager initialized"));
    eventLogger.logf("    Sleep timeout: %lu minutes", sleepTimeout / 60000);
    eventLogger.logf("    Sleep enabled: %s", sleepEnabled ? "YES" : "NO");
    
    // Check if we woke up from deep sleep
    esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();
    
    switch(wakeup_reason) {
        case ESP_SLEEP_WAKEUP_EXT0:
            eventLogger.log(F("Wakeup from CAN bus activity"));
            break;
        case ESP_SLEEP_WAKEUP_EXT1:
            eventLogger.log(F("Wakeup from external interrupt"));
            break;
        case ESP_SLEEP_WAKEUP_TIMER:
            eventLogger.log(F("Wakeup from timer"));
            break;
        default:
            eventLogger.log(F("Normal startup (not from deep sleep)"));
            break;
    }
}

void SleepManager::loop() {
    if (!sleepEnabled) {
        return;
    }
    
    // Check if sleep timeout has been reached
    if (isSleepDue()) {
        eventLogger.logf("No NMEA2000 bus activity for %lu minutes", sleepTimeout / 60000);
        eventLogger.log(F("Entering deep sleep mode..."));
        enterSleep();
    }
}

void SleepManager::resetActivityTimer() {
    lastActivity = millis();
}

bool SleepManager::isSleepDue() {
    if (!sleepEnabled) {
        return false;
    }
    
    unsigned long currentTime = millis();
    
    // Handle millis() overflow
    if (currentTime < lastActivity) {
        lastActivity = currentTime;
        return false;
    }
    
    return (currentTime - lastActivity) >= sleepTimeout;
}

void SleepManager::enable() {
    if (!sleepEnabled) {
        sleepEnabled = true;
        lastActivity = millis(); // Reset timer when enabling
        eventLogger.log(F("Sleep mode ENABLED"));
        eventLogger.logf("    Timeout: %lu minutes", sleepTimeout / 60000);
    }
}

void SleepManager::disable() {
    if (sleepEnabled) {
        sleepEnabled = false;
        eventLogger.log(F("Sleep mode DISABLED"));
    }
}

void SleepManager::setEnabled(bool enabled) {
    if (enabled) {
        enable();
    } else {
        disable();
    }
}

bool SleepManager::isEnabled() const {
    return sleepEnabled;
}

unsigned long SleepManager::getRemainingTime() const {
    if (!sleepEnabled) {
        return 0xFFFFFFFF; // Maximum value when disabled
    }
    
    unsigned long currentTime = millis();
    
    // Handle millis() overflow
    if (currentTime < lastActivity) {
        return sleepTimeout;
    }
    
    unsigned long elapsed = currentTime - lastActivity;
    
    if (elapsed >= sleepTimeout) {
        return 0;
    }
    
    return sleepTimeout - elapsed;
}

void SleepManager::setTimeout(unsigned long timeoutMs) {
    sleepTimeout = timeoutMs;
    eventLogger.logf("Sleep timeout changed to %lu minutes", sleepTimeout / 60000);
}

unsigned long SleepManager::getTimeout() const {
    return sleepTimeout;
}

void SleepManager::prepareSleep() {
    // Save battery statistics to non-volatile storage
    eventLogger.log(F("Saving battery statistics before sleep..."));
    
    // Only save to Preferences (public method)
    // RTC memory will be preserved automatically by ESP32
    batteryStatus.writeStats();

    // Power down INA226 to save energy
	ina.powerDown(); 
    
    // Give the system time to complete any pending operations
    delay(100);
}

void SleepManager::configureWakeup() {
    // Configure CAN RX pin as wakeup source
    // The CAN transceiver will pull this pin low when there's bus activity
    
    // First, ensure the pin is configured as input with pullup
    gpio_num_t wakeup_pin = ESP32_CAN_RX_PIN;
    
    // Configure the pin for RTC GPIO wakeup
    if (rtc_gpio_is_valid_gpio(wakeup_pin)) {
        rtc_gpio_init(wakeup_pin);
        rtc_gpio_set_direction(wakeup_pin, RTC_GPIO_MODE_INPUT_ONLY);
        rtc_gpio_pullup_en(wakeup_pin);
        rtc_gpio_pulldown_dis(wakeup_pin);
        
        // Configure wakeup on LOW level (CAN activity pulls line low)
        esp_sleep_enable_ext0_wakeup(wakeup_pin, 0); // 0 = LOW, 1 = HIGH
        
        eventLogger.logf("Wakeup configured on CAN RX pin (GPIO %d)", wakeup_pin);
    } else {
        eventLogger.log(F("WARNING: CAN RX pin is not RTC-capable!"));
        eventLogger.log(F("Using timer wakeup as fallback"));
        
        // Fallback: Wake up every 10 minutes to check
        esp_sleep_enable_timer_wakeup(10 * 60 * 1000000ULL); // 10 minutes in microseconds
    }
}

void SleepManager::enterSleep() {
    // Prepare for sleep
    prepareSleep();
    
    // Configure wakeup sources
    configureWakeup();
    
    // Shutdown INA226 if possible
    // The INA226 will be powered off when ESP32 cuts power to peripherals
    eventLogger.log(F("Powering down sensors..."));
    
    // Log final message
    eventLogger.log(F("Entering deep sleep NOW"));
    
    // Give serial time to finish
    Serial.flush();
    delay(100);
    
    // Enter deep sleep
    esp_deep_sleep_start();
    
    // Code never reaches here - ESP32 is in deep sleep
}