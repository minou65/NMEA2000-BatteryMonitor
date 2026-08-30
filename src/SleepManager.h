#ifndef _SLEEPMANAGER_h
#define _SLEEPMANAGER_h

#if defined(ARDUINO) && ARDUINO >= 100
#include "arduino.h"
#else
#include "WProgram.h"
#endif

#include <esp_sleep.h>

// Sleep configuration constants
#define SLEEP_TIMEOUT_MS        (1 * 60 * 1000)  // 1 minute in milliseconds
#define SLEEP_CHECK_INTERVAL_MS 1000              // Check every second

class SleepManager {
public:
    SleepManager();
    
    void begin();
    void loop();
    void resetActivityTimer();
    bool isSleepDue();
    void enterSleep();
    
    // Enable/Disable sleep mode
    void enable();
    void disable();
    void setEnabled(bool enabled);
    bool isEnabled() const;
    
    // Get remaining time until sleep
    unsigned long getRemainingTime() const;
    
    // Set custom timeout (in milliseconds)
    void setTimeout(unsigned long timeoutMs);
    unsigned long getTimeout() const;
    
private:
    unsigned long lastActivity;
    unsigned long sleepTimeout;
    bool sleepEnabled;
    
    void prepareSleep();
    void configureWakeup();
};

extern SleepManager sleepManager;

#endif