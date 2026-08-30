#include <Arduino.h>
#include <Wire.h>
#include <EventLogger.h>

#include "common.h"
#include "INA226.h"
#include "INA226EnumToString.h"
#include "sensorHandling.h"
#include "statusHandling.h"
#include "neotimer.h"
#include "BatteryConfig.h"

bool debugAhCounter = false;
bool debugMode = false;

volatile uint16_t alertCounter = 0;
double sampleTime = 0;
bool INA226Initialized = false;
bool updateINA226Config = true;

Neotimer statisticsTimer = Neotimer(1000);
Neotimer outputTimer = Neotimer(15000);
Neotimer savePreferenceTimer = Neotimer(180 * 60 * 1000); // 3 hours

IRAM_ATTR void alert(void) { ++alertCounter; }

String getCurrentTime() {
    time_t now_ = time(nullptr);
    struct tm* timeInfo_ = localtime(&now_);

    char buffer_[9]; // Puffer für "H:m:s"
    strftime(buffer_, sizeof(buffer_), "%H:%M:%S", timeInfo_);

    return String(buffer_);
}

uint16_t translateConversionTime(ina226_shuntConvTime_t time) {
    uint16_t result_ = 0;
    switch (time) {
        case INA226_BUS_CONV_TIME_140US:
        result_ = 140;
        break;
        case INA226_BUS_CONV_TIME_204US:
        result_ = 204;
        break;
        case INA226_BUS_CONV_TIME_332US:
        result_ = 332;
        break;
        case INA226_BUS_CONV_TIME_588US:
        result_ = 588;
        break;
        case INA226_BUS_CONV_TIME_1100US:
        result_ = 1100;
        break;
        case INA226_BUS_CONV_TIME_2116US:
        result_ = 2116;
        break;
        case INA226_BUS_CONV_TIME_4156US:
        result_ = 4156;
        break;
        case INA226_BUS_CONV_TIME_8244US:
        result_ = 8244;
        break;
        default:
        result_ = 0;
    }
    return result_;
}

uint16_t translateSampleCount(ina226_averages_t value) {
    uint16_t result_ = 0;
    switch (value) {
        case INA226_AVERAGES_1:
        result_ = 1;
        break;
        case INA226_AVERAGES_4:
        result_ = 4;
        break;
        case INA226_AVERAGES_16:
        result_ = 16;
        break;
        case INA226_AVERAGES_64:
        result_ = 64;
        break;
        case INA226_AVERAGES_128:
        result_ = 128;
        break;
        case INA226_AVERAGES_256:
        result_ = 256;
        break;
        case INA226_AVERAGES_512:
        result_ = 512;
        break;
        case INA226_AVERAGES_1024:
        result_ = 1024;
        break;
        default:
        result_ = 0;
    }

  return result_;
}

void checkConfig() {
    eventLogger.log(F("Configuration of INA226"));
    eventLogger.logf("    Mode:                  %s", INA226EnumTypeToStr(ina.getMode()));
    eventLogger.logf("    Samples average:       %s", INA226EnumTypeToStr(ina.getAverages()));
    eventLogger.logf("    Bus conversion time:   %s", INA226EnumTypeToStr((ina226_busConvTime_t)ina.getBusConversionTime()));
    eventLogger.logf("    Shunt conversion time: %s", INA226EnumTypeToStr(ina.getShuntConversionTime()));
    eventLogger.logf("    Max possible current:  %.3f A", ina.getMaxPossibleCurrent());
    eventLogger.logf("    Max current:           %.3f A", ina.getMaxCurrent());
    eventLogger.logf("    Max shunt voltage:     %.5f V", ina.getMaxShuntVoltage());
    eventLogger.logf("    Max power:             %.3f W", ina.getMaxPower());
    eventLogger.logf("    Shunt resistance:      %.5f R", shuntConfig.ShuntResistance());
    eventLogger.logf("    Shunt max current:     %i A", shuntConfig.MaxCurrent());
}

void setupSensor() {
    // Default INA226 address is 0x40
    INA226Initialized = ina.begin();
    statisticsTimer.start();
    outputTimer.start();
    savePreferenceTimer.start();

    // Check if the connection was successful, stop if not
    if (!INA226Initialized) {
        eventLogger.log(F("Connection to sensor failed"));
        return;
    }
    // Configure INA226
    ina.configure(INA226_AVERAGES_64, INA226_BUS_CONV_TIME_2116US, INA226_SHUNT_CONV_TIME_2116US, INA226_MODE_SHUNT_BUS_CONT);
    ina.calibrate(shuntConfig.ShuntResistance(), shuntConfig.MaxCurrent());
    ina.enableConversionReadyAlert();

    eventLogger.logf("calibrate sensor: Shunt resitance = %.5fR, max currenct = %iA", shuntConfig.ShuntResistance(), shuntConfig.MaxCurrent());
    batteryStatus.setParameters(
        batteryConfig.Capacity_Ah(),
        batteryConfig.ChargeEfficiency_Percent(),
        batteryConfig.MinSoc_Percent(),
        batteryFullConfig.TailCurrent_mA(),
        batteryFullConfig.FullVoltage_mV(),
        batteryFullConfig.FullDelay_s()
    );

    uint16_t conversionTimeShunt_ = translateConversionTime(ina.getShuntConversionTime());
    uint16_t conversionTimeBus_ = translateConversionTime((ina226_shuntConvTime_t)ina.getBusConversionTime());
    uint16_t samples_ = translateSampleCount(ina.getAverages());

    // This is the time it takes to create a new measurement
    sampleTime = (conversionTimeShunt_ + conversionTimeBus_) * samples_ * 0.000001;
}

void sensorInit() {
    Wire.begin(INA226_SDA_PIN, INA226_SCL_PIN); 
    attachInterrupt(digitalPinToInterrupt(INA226_ALERT_PIN), alert, FALLING);

    setupSensor();

    batteryStatus.begin();

	if (!INA226Initialized) {
		return;
	}
}

void updateAhCounter() {
    int count_;
    noInterrupts();
    // If we missed an interrupt, we assume we had the same value all the time.
    count_ = alertCounter;
    alertCounter = 0;
    interrupts();

    float current_ = ina.readShuntCurrent() * shuntConfig.CurrentCalibrationFactor();
    batteryStatus.updateConsumption(current_, sampleTime, count_);

    if (debugAhCounter) {
        eventLogger.log(F("Update Ah counter"));
        eventLogger.logf("    current: %.2f", current_);
        eventLogger.logf("    voltage: %.2f", batteryStatus.voltage());
        eventLogger.logf("    sampletime: %.2f", sampleTime);
        eventLogger.logf("    count: %i", count_);
        eventLogger.logf("    CurrentCalibrationFactor: %.2f", shuntConfig.CurrentCalibrationFactor());

        if (count_ > 1) {
            eventLogger.logf("Overflow %d", count_);
        }
    }
}

void sensorLoop() {
    unsigned long now_ = millis();

    if (savePreferenceTimer.repeat()) {
        batteryStatus.writeStats();
    }

    if (!INA226Initialized) {
        if (statisticsTimer.repeat() && debugMode) {
            eventLogger.log(F("Sensor not initialized"));
        }
        return;
    }

    if (updateINA226Config) {
        eventLogger.logf("calibrate sensor: Shunt resitance = %.5fR, max currenct = %iA", shuntConfig.ShuntResistance(), shuntConfig.MaxCurrent());
        ina.calibrate(shuntConfig.ShuntResistance(), shuntConfig.MaxCurrent());
        batteryStatus.setParameters(
            batteryConfig.Capacity_Ah(),
            batteryConfig.ChargeEfficiency_Percent(),
            batteryConfig.MinSoc_Percent(),
            batteryFullConfig.TailCurrent_mA(),
            batteryFullConfig.FullVoltage_mV(),
            batteryFullConfig.FullDelay_s()
        );

        checkConfig();
        updateINA226Config = false;
    }

    while (alertCounter && ina.isConversionReady()) {
        updateAhCounter();
    }

    if (statisticsTimer.repeat()) {
        batteryStatus.setVoltage(ina.readBusVoltage() * shuntConfig.VoltageCalibrationFactor());
        batteryStatus.checkFull();
        batteryStatus.updateSOC();
        batteryStatus.updateTtG();
        batteryStatus.updateStats(now_);
    }

    if (outputTimer.repeat() && debugMode) {
        eventLogger.log(F("Battery status"));
        eventLogger.logf("    voltage:      %.3fV", batteryStatus.voltage());
        eventLogger.logf("    current:      %.3fA", batteryStatus.current());
        eventLogger.logf("    average:      %.3fA", batteryStatus.averageCurrent());
        eventLogger.logf("    SOC:          %.3f", batteryStatus.soc());
        eventLogger.logf("    TTG:          %ds", batteryStatus.tTg());
        eventLogger.logf("    remaining As: %.3f", batteryStatus.remainingAs());
        eventLogger.logf("    Bus voltage:   %.3fV", ina.readBusVoltage());
        eventLogger.logf("    Bus power:     %.3fW", ina.readBusPower());
        eventLogger.logf("    Shunt voltage: %.3fV", ina.readShuntVoltage());
        eventLogger.logf("    Shunt current: %.3fA", ina.readShuntCurrent());
    }
}
