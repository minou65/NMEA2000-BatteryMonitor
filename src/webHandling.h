// webhandling.h

#ifndef _WEBHANDLING_h
#define _WEBHANDLING_h

#if defined(ARDUINO) && ARDUINO >= 100
#include "arduino.h"
#else
#include "WProgram.h"
#endif

#include <IotWebConf.h>

// -- Initial password to connect to the Thing, when it creates an own Access Point.
const char wifiInitialApPassword[] = "123456789";

// -- Configuration specific key. The value should be modified if config structure was changed.
#define CONFIG_VERSION "C3"

// -- When CONFIG_PIN is pulled to ground on startup, the Thing will use the initial
//      password to buld an AP. (E.g. in case of lost password)
#define CONFIG_PIN  -1

// -- Status indicator pin.
//      First it will light up (kept LOW), on Wifi connection it will blink,
//      when connected to the Wifi it will turn off (kept HIGH).
#define STATUS_PIN LED_BUILTIN
#if ESP32 
#define ON_LEVEL HIGH
#else
#define ON_LEVEL LOW
#endif

static char BatTypeValues[][STRING_LEN] = { "0", "1", "2" };
static char BatTypeNames[][STRING_LEN] = { "flooded", "gel", "AGM" };
static char BatNomVoltValues[][STRING_LEN] = { "0", "1", "2", "3", "4", "5", "6" };
static char BatNomVoltNames[][STRING_LEN] = { "6V", "12V", "24V", "32V", "62V", "42V", "48V" };
static char BatChemValues[][STRING_LEN] = { "0", "1", "2", "3" };
static char BatChemNames[][STRING_LEN] = { "lead acid", "LiIon", "NiCad", "NiMh" };

extern void wifiSetup();
extern void wifiLoop();

extern void wifiStoreConfig();

extern IotWebConf iotWebConf;

class NMEAConfig : public iotwebconf::ParameterGroup {
public:
    NMEAConfig() : ParameterGroup("nmeaconfig", "NMEA configuration") {
        snprintf(instanceID, STRING_LEN, "%s-instance", this->getId());
        snprintf(sidID, STRING_LEN, "%s-sid", this->getId());
        snprintf(sourceID, STRING_LEN, "%s-source", this->getId());

        this->addItem(&this->InstanceParam);
        this->addItem(&this->SIDParam);

        iotWebConf.addHiddenParameter(&SourceParam);

    }

    uint8_t Instance() { return atoi(InstanceValue); };
    uint8_t SID() { return atoi(SIDValue); };
    uint8_t Source() { return atoi(SourceValue); };

    void SetSource(uint8_t source_) {
        String s;
        s = (String)source_;
        strncpy(SourceParam.valueBuffer, s.c_str(), NUMBER_LEN);
    }

private:
    iotwebconf::NumberParameter InstanceParam = iotwebconf::NumberParameter("Instance", instanceID, InstanceValue, NUMBER_LEN, "255", "1..255", "min='1' max='254' step='1'");
    iotwebconf::NumberParameter SIDParam = iotwebconf::NumberParameter("SID", sidID, SIDValue, NUMBER_LEN, "255", "1..255", "min='1' max='255' step='1'");
    iotwebconf::NumberParameter SourceParam = iotwebconf::NumberParameter("Source", sourceID, SourceValue, NUMBER_LEN, "22", nullptr, nullptr);

    char InstanceValue[NUMBER_LEN];
    char SIDValue[NUMBER_LEN];
    char SourceValue[NUMBER_LEN];


    char instanceID[STRING_LEN];
    char sidID[STRING_LEN];
    char sourceID[STRING_LEN];

    // additional sources
    iotwebconf::NumberParameter SourcePressureParam = iotwebconf::NumberParameter("SourcePressure", sourceIDPressure, SourcePressureValue, NUMBER_LEN, "23", nullptr, nullptr);
    iotwebconf::NumberParameter SourceHumidityParam = iotwebconf::NumberParameter("SourceHumidity", sourceIDHumidity, SourceHumidityValue, NUMBER_LEN, "24", nullptr, nullptr);
    char SourcePressureValue[NUMBER_LEN];
    char SourceHumidityValue[NUMBER_LEN];

    char sourceIDPressure[STRING_LEN];
    char sourceIDHumidity[STRING_LEN];

};

extern NMEAConfig Config;

#endif