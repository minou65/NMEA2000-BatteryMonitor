#ifndef _POWERSAVINGCONFIG_h
#define _POWERSAVINGCONFIG_h

#include "common.h"
#include <IotWebConfAsync.h>

class PowerSavingConfig : public iotwebconf::ParameterGroup {
public:
    PowerSavingConfig();

	void applyDefaultValues();

    bool isEnabled() const;
    void setEnabled(bool enabled);

    unsigned long getTimeout() const;
    void setTimeout(unsigned long timeoutMinutes);

private:
    char enabledValue[STRING_LEN];
    char timeoutValue[NUMBER_LEN];

    iotwebconf::CheckboxParameter enabledParam =
        iotwebconf::CheckboxParameter(
            "Enable Power Saving Mode",
            "powerSavingEnabled",
            enabledValue,
            STRING_LEN,
            true  // Default: enabled
        );

    iotwebconf::NumberParameter timeoutParam =
        iotwebconf::NumberParameter(
            "Inactivity Timeout (minutes)",
            "powerSavingTimeout",
            timeoutValue,
            NUMBER_LEN,
            "5",           // Default: 5 minutes
            "1..60",       // Label
            "min='1' max='60' step='1'"  // HTML attributes
        );
};

extern PowerSavingConfig powerSavingConfig;

#endif