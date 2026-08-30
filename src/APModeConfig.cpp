#include "APModeConfig.h"

APModeConfig apModeConfig;

APModeConfig::APModeConfig() : ParameterGroup("apmode", "AP Mode") {
    // Initialize default values
    startOnlyAtBootValue[0] = '\0';  // Empty = not checked
    strcpy(offlineTimeValue, "0");    // 0 = never go offline
    
    // Add parameters to group
    addItem(&startOnlyAtBootParam);
    addItem(&offlineTimeParam);
}

void APModeConfig::applyDefaultValues() {
    startOnlyAtBootValue[0] = '\0';  // Empty = not checked
    strcpy(offlineTimeValue, "0");    // 0 = never go offline
}

bool APModeConfig::isStartOnlyAtBoot() const {
    // Checkbox is checked if value is "selected"
    return strcmp(startOnlyAtBootValue, "selected") == 0;
}

void APModeConfig::setStartOnlyAtBoot(bool value) {
    if (value) {
        strcpy(startOnlyAtBootValue, "selected");
    } else {
        startOnlyAtBootValue[0] = '\0';  // Empty string
    }
}

uint8_t APModeConfig::getOfflineTime() const {
    uint8_t time = atoi(offlineTimeValue);
    
    // Validate range
    if (time > 30) time = 30;
    
    return time;
}

void APModeConfig::setOfflineTime(uint8_t minutes) {
    if (minutes > 30) minutes = 30;
    
    snprintf(offlineTimeValue, NUMBER_LEN, "%u", minutes);
}