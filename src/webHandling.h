// webhandling.h

#ifndef _WEBHANDLING_h
#define _WEBHANDLING_h

#if defined(ARDUINO) && ARDUINO >= 100
#include "arduino.h"
#else
#include "WProgram.h"
#endif

#include <IotWebConfAsync.h>
#include <WebSerial.h>

// -- Initial password to connect to the Thing, when it creates an own Access Point.
const char wifiInitialApPassword[] = "123456789";

// -- Configuration specific key. The value should be modified if config structure was changed.
#define CONFIG_VERSION "C4"

// -- When CONFIG_PIN is pulled to ground on startup, the Thing will use the initial
//      password to buld an AP. (E.g. in case of lost password)
#define CONFIG_PIN  GPIO_NUM_13

// -- Status indicator pin.
//      First it will light up (kept LOW), on Wifi connection it will blink,
//      when connected to the Wifi it will turn off (kept HIGH).
#define STATUS_PIN LED_BUILTIN
#if defined(ESP32)
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

extern AsyncIotWebConf iotWebConf;

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
};

class ShuntConfig : public iotwebconf::ParameterGroup {
public:
    ShuntConfig() : ParameterGroup("shuntconfig", "Shunt configuration") {
        snprintf(_maxCurrentID, STRING_LEN, "%s-maxcurrent", this->getId());
        snprintf(_VoltageCalibrationFactorID, STRING_LEN, "%s-voltagecalibrationfactor", this->getId());
        snprintf(_CurrentCalibrationFactorID, STRING_LEN, "%s-currentcalibrationfactor", this->getId());
        snprintf(_shuntResistanceID, STRING_LEN, "%s-shuntresistance", this->getId());

        this->addItem(&_shuntResistance);
        this->addItem(&_maxCurrent);
        this->addItem(&_VoltageCalibrationFactor);
        this->addItem(&_CurrentCalibrationFactor);

    }

    float ShuntResistance() { return atof(_shuntResistanceValue); };
    float MaxCurrent() { return atof(_maxCurrentValue); };
    float VoltageCalibrationFactor() { return atof(_VoltageCalibrationFactorValue); };
    float CurrentCalibrationFactor() { return atof(_CurrentCalibrationFactorValue); };

private:
    char _maxCurrentValue[NUMBER_LEN];
    char _VoltageCalibrationFactorValue[NUMBER_LEN];
    char _CurrentCalibrationFactorValue[NUMBER_LEN];
    char _shuntResistanceValue[NUMBER_LEN];

    char _maxCurrentID[STRING_LEN];
    char _VoltageCalibrationFactorID[STRING_LEN];
    char _CurrentCalibrationFactorID[STRING_LEN];
    char _shuntResistanceID[STRING_LEN];

    iotwebconf::NumberParameter _shuntResistance = iotwebconf::NumberParameter("Shunt resistance [&#8486;]", _shuntResistanceID, _shuntResistanceValue, NUMBER_LEN, "0.00075", "0..100", "min='0.00001' max='100' step='0.00001'");
    iotwebconf::NumberParameter _maxCurrent = iotwebconf::NumberParameter("Expected max current [A]", _maxCurrentID, _maxCurrentValue, NUMBER_LEN, "100", "1..500", "min='1' max='500' step='1'");
    iotwebconf::NumberParameter _VoltageCalibrationFactor = iotwebconf::NumberParameter("Voltage calibration factor", _VoltageCalibrationFactorID, _VoltageCalibrationFactorValue, NUMBER_LEN, "1.0000", "-5.00000 - 5.00000", "min='-5.00000' max='5.00000' step='0.00001'");
    iotwebconf::NumberParameter _CurrentCalibrationFactor = iotwebconf::NumberParameter("Current calibration factor", _CurrentCalibrationFactorID, _CurrentCalibrationFactorValue, NUMBER_LEN, "1.0000", "-5.00000 - 5.00000", "min='-5.00000' max='5.00000' step='0.00001'");

};

class BatteryFullConfig : public iotwebconf::ParameterGroup {
public:
    BatteryFullConfig() : ParameterGroup("BatteryFullConfig", "Battery full detection") {
        snprintf(_tailCurrentID, STRING_LEN, "%s-tailcurrent", this->getId());
        snprintf(_fullVoltageID, STRING_LEN, "%s-fullvoltage", this->getId());
        snprintf(_fullDelayID, STRING_LEN, "%s-fulldelay", this->getId());
        snprintf(_CurrentThresholdID, STRING_LEN, "%s-currentthreshold", this->getId());

        addItem(&_tailCurrent);
        addItem(&_fullVoltage);
        addItem(&_fullDelay);
        addItem(&_CurrentThreshold);
    }

    uint16_t TailCurrent_mA() { return (uint16_t)(atof(_tailCurrentValue) * 1000); };
    uint16_t FullVoltage_mV() { return (uint16_t)(atof(_fullVoltageValue) * 1000); };
    uint16_t FullDelay_s() { return (uint16_t)atoi(_fullDelayValue); };
    float CurrentThreshold_A() { return atof(_CurrentThresholdValue); };

private:
    // Value buffers
    char _tailCurrentValue[NUMBER_LEN];
    char _fullVoltageValue[NUMBER_LEN];
    char _fullDelayValue[NUMBER_LEN];
    char _CurrentThresholdValue[NUMBER_LEN];

    // ID buffers - MUST be declared BEFORE the NumberParameter members
    char _tailCurrentID[STRING_LEN];
    char _fullVoltageID[STRING_LEN];
    char _fullDelayID[STRING_LEN];
    char _CurrentThresholdID[STRING_LEN];

    // NumberParameter members - initialized in declaration, using the ID buffers above
    iotwebconf::NumberParameter _tailCurrent = iotwebconf::NumberParameter("Tail current [A]", _tailCurrentID, _tailCurrentValue, NUMBER_LEN, "1.500", "0..65", "min='0.001' max='65' step='0.001'");
    iotwebconf::NumberParameter _fullVoltage = iotwebconf::NumberParameter("Voltage when full [V]", _fullVoltageID, _fullVoltageValue, NUMBER_LEN, "12.85", "0..38", "min='0.01' max='38' step='0.01'");
    iotwebconf::NumberParameter _fullDelay = iotwebconf::NumberParameter("Delay before full [s]", _fullDelayID, _fullDelayValue, NUMBER_LEN, "60", "1..7200", "min='1' max='7200' step='1'");
    iotwebconf::NumberParameter _CurrentThreshold = iotwebconf::NumberParameter("Current threshold [A]", _CurrentThresholdID, _CurrentThresholdValue, NUMBER_LEN, "0.10", "0.00..2.00", "min='0.00' max='2.00' step='0.01'");
};

class BatteryConfig : public iotwebconf::ParameterGroup {
public:
    BatteryConfig() : ParameterGroup("BatteryConfig", "Battery") {
        snprintf(_BatTypeID, STRING_LEN, "%s-battype", this->getId());
        snprintf(_BatNomVoltID, STRING_LEN, "%s-batnomvolt", this->getId());
        snprintf(_BatChemID, STRING_LEN, "%s-batchem", this->getId());
        snprintf(_battCapacityID, STRING_LEN, "%s-battcapacity", this->getId());
        snprintf(_chargeEfficiencyID, STRING_LEN, "%s-chargeefficiency", this->getId());
        snprintf(_minSocID, STRING_LEN, "%s-minsoc", this->getId());
        snprintf(_BatteryReplacmentDateID, STRING_LEN, "%s-batteryreplacmentdate", this->getId());
        snprintf(_BatteryReplacmentTimeID, STRING_LEN, "%s-batteryreplacmenttime", this->getId());
        snprintf(_BatteryManufacturerID, STRING_LEN, "%s-batterymanufacturer", this->getId());

        addItem(&_BatType);
        addItem(&_BatNomVoltType);
        addItem(&_BatChemType);
        addItem(&_battCapacity);
        addItem(&_chargeEfficiency);
        addItem(&_minSoc);
        addItem(&_BatteryReplacmentDateParameter);
        addItem(&_BatterymanufacturerParameter);
    }

    tN2kBatType BatType() { return (tN2kBatType)atoi(_BatTypeValue); };
    tN2kBatNomVolt BatNomVolt() { return (tN2kBatNomVolt)atoi(_BatNomVoltValue); };
    tN2kBatChem BatChem() { return (tN2kBatChem)atoi(_BatChemValue); };
    uint16_t BattCapacity_Ah() { return (uint16_t)atoi(_battCapacityValue); };
    uint16_t ChargeEfficiency_Percent() { return (uint16_t)atoi(_chargeEfficiencyValue); };
    uint16_t MinSoc_Percent() { return (uint16_t)atoi(_minSocValue); };
    const char* BatteryReplacmentDate() { return _BatteryReplacmentDateValue; };
    const char* BatteryReplacmentTime() { return _BatteryReplacmentTimeValue; };
    const char* BatteryManufacturer() { return _BatteryManufacturerValue; };

private:
    // Value buffers
    char _BatTypeValue[STRING_LEN];
    char _BatNomVoltValue[STRING_LEN];
    char _BatChemValue[STRING_LEN];
    char _battCapacityValue[NUMBER_LEN];
    char _chargeEfficiencyValue[NUMBER_LEN];
    char _minSocValue[NUMBER_LEN];
    char _BatteryReplacmentDateValue[DATE_LEN];
    char _BatteryReplacmentTimeValue[TIME_LEN];
    char _BatteryManufacturerValue[STRING_LEN];

    // ID buffers - MUST be declared BEFORE the Parameter members
    char _BatTypeID[STRING_LEN];
    char _BatNomVoltID[STRING_LEN];
    char _BatChemID[STRING_LEN];
    char _battCapacityID[STRING_LEN];
    char _chargeEfficiencyID[STRING_LEN];
    char _minSocID[STRING_LEN];
    char _BatteryReplacmentDateID[STRING_LEN];
    char _BatteryReplacmentTimeID[STRING_LEN];
    char _BatteryManufacturerID[STRING_LEN];

    // Parameter members - initialized in declaration
    iotwebconf::SelectParameter _BatType = iotwebconf::SelectParameter("Type", _BatTypeID, _BatTypeValue, STRING_LEN, (char*)BatTypeValues, (char*)BatTypeNames, sizeof(BatTypeValues) / STRING_LEN, STRING_LEN, "2");
    iotwebconf::SelectParameter _BatNomVoltType = iotwebconf::SelectParameter("Voltage", _BatNomVoltID, _BatNomVoltValue, STRING_LEN, (char*)BatNomVoltValues, (char*)BatNomVoltNames, sizeof(BatNomVoltValues) / STRING_LEN, STRING_LEN, "1");
    iotwebconf::SelectParameter _BatChemType = iotwebconf::SelectParameter("Chemistrie", _BatChemID, _BatChemValue, STRING_LEN, (char*)BatChemValues, (char*)BatChemNames, sizeof(BatChemValues) / STRING_LEN, STRING_LEN, "0");
    iotwebconf::NumberParameter _battCapacity = iotwebconf::NumberParameter("Capacity [Ah]", _battCapacityID, _battCapacityValue, NUMBER_LEN, "100", "1..300", "min='1' max='300' step='1'");
    iotwebconf::NumberParameter _chargeEfficiency = iotwebconf::NumberParameter("charge efficiency [%]", _chargeEfficiencyID, _chargeEfficiencyValue, NUMBER_LEN, "95", "1..100", "min='1' max='100' step='1'");
    iotwebconf::NumberParameter _minSoc = iotwebconf::NumberParameter("Minimun SOC [%]", _minSocID, _minSocValue, NUMBER_LEN, "10", "1..100", "min='1' max='100' step='1'");
    iotwebconf::DateParameter _BatteryReplacmentDateParameter = iotwebconf::DateParameter("Replacment date", _BatteryReplacmentDateID, _BatteryReplacmentDateValue, DATE_LEN, "2024-01-01");
    iotwebconf::TextParameter _BatterymanufacturerParameter = iotwebconf::TextParameter("Manufacturer", _BatteryManufacturerID, _BatteryManufacturerValue, STRING_LEN);
};

extern NMEAConfig nmeaConfig;
extern ShuntConfig shuntConfig;
extern BatteryFullConfig batteryFullConfig;
extern BatteryConfig batteryConfig;

#endif