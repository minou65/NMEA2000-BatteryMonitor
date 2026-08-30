#include "PowerSavingConfig.h"

PowerSavingConfig powerSavingConfig;

PowerSavingConfig::PowerSavingConfig() : ParameterGroup("powersaving", "Power Saving") {
    // Initialize default values
    strcpy(enabledValue, "selected");  // "selected" = checked by default
    strcpy(timeoutValue, "5");         // 5 minutes default
    
    // Add parameters to group
    addItem(&enabledParam);
    addItem(&timeoutParam);
}

void PowerSavingConfig::applyDefaultValues() {
    strcpy(enabledValue, "selected");  // "selected" = checked by default
    strcpy(timeoutValue, "5");         // 5 minutes default
}

bool PowerSavingConfig::isEnabled() const {
    // Checkbox is checked if value is "selected"
    return strcmp(enabledValue, "selected") == 0;
}

void PowerSavingConfig::setEnabled(bool enabled) {
    if (enabled) {
        strcpy(enabledValue, "selected");
    } else {
        enabledValue[0] = '\0';  // Empty string
    }
}

unsigned long PowerSavingConfig::getTimeout() const {
    unsigned long timeout = atol(timeoutValue);

    // Validate range
    if (timeout < 1) timeout = 1;
    if (timeout > 60) timeout = 60;

    return timeout;
}

void PowerSavingConfig::setTimeout(unsigned long timeoutMinutes) {
    if (timeoutMinutes < 1) timeoutMinutes = 1;
    if (timeoutMinutes > 60) timeoutMinutes = 60;

    snprintf(timeoutValue, NUMBER_LEN, "%lu", timeoutMinutes);
}