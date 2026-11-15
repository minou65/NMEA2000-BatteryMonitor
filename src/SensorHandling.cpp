#include <Arduino.h>
#include <Wire.h>
#include <neotimer.h>
#include <EventLogger.h>

#include "common.h"
#include "INA226.h"
#include "INA226EnumToString.h"
#include "sensorHandling.h"
#include "statusHandling.h"
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

    char buffer_[9]; // Puffer f�r "H:m:s"
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
    LOG_DEBUG("Configuration of INA226");
    LOG_DEBUG_AT(1, "Mode:                  %s", INA226EnumTypeToStr(ina.getMode()));
    LOG_DEBUG_AT(1, "Samples average:       %s", INA226EnumTypeToStr(ina.getAverages()));
    LOG_DEBUG_AT(1, "Bus conversion time:   %s", INA226EnumTypeToStr((ina226_busConvTime_t)ina.getBusConversionTime()));
    LOG_DEBUG_AT(1, "Shunt conversion time: %s", INA226EnumTypeToStr(ina.getShuntConversionTime()));
    LOG_DEBUG_AT(1, "Max possible current:  %.3f A", ina.getMaxPossibleCurrent());
    LOG_DEBUG_AT(1, "Max current:           %.3f A", ina.getMaxCurrent());
    LOG_DEBUG_AT(1, "Max shunt voltage:     %.5f V", ina.getMaxShuntVoltage());
    LOG_DEBUG_AT(1, "Max power:             %.3f W", ina.getMaxPower());
    LOG_DEBUG_AT(1, "Shunt resistance:      %.5f R", shuntConfig.ShuntResistance());
    LOG_DEBUG_AT(1, "Shunt max current:     %i A", shuntConfig.MaxCurrent());
}

void setupSensor() {
    // Default INA226 address is 0x40
    INA226Initialized = ina.begin();
    statisticsTimer.start();
    outputTimer.start();
    savePreferenceTimer.start();

    // Check if the connection was successful, stop if not
    if (!INA226Initialized) {
        LOG_ERROR("Connection to sensor failed");
        return;
    }
    // Configure INA226
    ina.configure(INA226_AVERAGES_64, INA226_BUS_CONV_TIME_2116US, INA226_SHUNT_CONV_TIME_2116US, INA226_MODE_SHUNT_BUS_CONT);
    ina.calibrate(shuntConfig.ShuntResistance(), shuntConfig.MaxCurrent());
    ina.enableConversionReadyAlert();

    LOG_DEBUG("calibrate sensor: Shunt resitance = %.5fR, max currenct = %iA", shuntConfig.ShuntResistance(), shuntConfig.MaxCurrent());
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
        LOG_DEBUG("Update Ah counter");
        LOG_DEBUG_AT(1, "current: %.2f", current_);
        LOG_DEBUG_AT(1, "voltage: %.2f", batteryStatus.voltage());
        LOG_DEBUG_AT(1, "sampletime: %.2f", sampleTime);
        LOG_DEBUG_AT(1, "count: %i", count_);
        LOG_DEBUG_AT(1, "CurrentCalibrationFactor: %.2f", shuntConfig.CurrentCalibrationFactor());

        if (count_ > 1) {
            LOG_DEBUG_AT(1, "Overflow %d", count_);
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
            LOG_WARN("Sensor not initialized");
        }
        return;
    }

    if (updateINA226Config) {
        LOG_DEBUG("calibrate sensor: Shunt resitance = %.5fR, max currenct = %iA", shuntConfig.ShuntResistance(), shuntConfig.MaxCurrent());
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
        LOG_DEBUG("Battery status");
        LOG_DEBUG_AT(1, "voltage:      %.3fV", batteryStatus.voltage());
        LOG_DEBUG_AT(1, "current:      %.3fA", batteryStatus.current());
        LOG_DEBUG_AT(1, "average:      %.3fA", batteryStatus.averageCurrent());
        LOG_DEBUG_AT(1, "SOC:          %.3f", batteryStatus.soc());
        LOG_DEBUG_AT(1, "TTG:          %ds", batteryStatus.tTg());
        LOG_DEBUG_AT(1, "remaining As: %.3f", batteryStatus.remainingAs());
        LOG_DEBUG_AT(1, "Bus voltage:   %.3fV", ina.readBusVoltage());
        LOG_DEBUG_AT(1, "Bus power:     %.3fW", ina.readBusPower());
        LOG_DEBUG_AT(1, "Shunt voltage: %.3fV", ina.readShuntVoltage());
        LOG_DEBUG_AT(1, "Shunt current: %.3fA", ina.readShuntCurrent());
    }
}
