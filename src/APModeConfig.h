#ifndef _APMODECONFIG_h
#define _APMODECONFIG_h

#include "common.h"
#include <IotWebConfAsync.h>

class APModeConfig : public iotwebconf::ParameterGroup {
public:
    APModeConfig();

    void applyDefaultValues();

    bool isStartOnlyAtBoot() const;
    void setStartOnlyAtBoot(bool value);

    uint8_t getOfflineTime() const;
    void setOfflineTime(uint8_t minutes);

private:
    char startOnlyAtBootValue[STRING_LEN];
    char offlineTimeValue[NUMBER_LEN];

    iotwebconf::CheckboxParameter startOnlyAtBootParam =
        iotwebconf::CheckboxParameter(
            "Start AP only at boot sequence",
            "APModeID",
            startOnlyAtBootValue,
            STRING_LEN,
            false  // Default: disabled (AP mode always available)
        );

    iotwebconf::NumberParameter offlineTimeParam =
        iotwebconf::NumberParameter(
            "AP offline mode after (minutes)",
            "APModeOffline",
            offlineTimeValue,
            NUMBER_LEN,
            "0",           // Default: 0 (never go offline)
            "0..30",       // Label
            "min='0' max='30' step='1'"  // HTML attributes
        );
};

extern APModeConfig apModeConfig;

#endif