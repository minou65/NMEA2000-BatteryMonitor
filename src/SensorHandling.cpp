#include <Arduino.h>
#include <Wire.h>
#include <WebSerial.h>

#include "common.h"
#include "INA226.h"
#include "INA226EnumToString.h"
#include "sensorHandling.h"
#include "statusHandling.h"
#include "neotimer.h"
#include "BatteryConfig.h"

#define DEBUG_SENSOR_Config
// #define DEBUG_SENSOR

volatile uint16_t alertCounter = 0;
double sampleTime = 0;
bool INA226Initialized = false;
bool updateINA226Config = true;

static INA226 ina(Wire);
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
	Serial.printf("Configuration of INA226\n");
	Serial.printf("    Mode:                  %s\n", INA226EnumTypeToStr(ina.getMode()));
	Serial.printf("    Samples average:       %s\n", INA226EnumTypeToStr(ina.getAverages()));
	Serial.printf("    Bus conversion time:   %s\n", INA226EnumTypeToStr((ina226_busConvTime_t)ina.getBusConversionTime()));
	Serial.printf("    Shunt conversion time: %s\n", INA226EnumTypeToStr(ina.getShuntConversionTime()));
	Serial.printf("    Max possible current:  %.3f A\n", ina.getMaxPossibleCurrent());
	Serial.printf("    Max current:           %.3f A\n", ina.getMaxCurrent());
	Serial.printf("    Max shunt voltage:     %.5f V\n", ina.getMaxShuntVoltage());
	Serial.printf("    Max power:             %.3f W\n", ina.getMaxPower());
    Serial.printf("    Shunt resistance:      %.5f R\n", shuntConfig.ShuntResistance());
    Serial.printf("    Shunt max current:     %i A\n", shuntConfig.MaxCurrent());

}

void setupSensor() {
    // Default INA226 address is 0x40
    INA226Initialized = ina.begin();
	statisticsTimer.start();
    outputTimer.start();
    savePreferenceTimer.start();

    // Check if the connection was successful, stop if not
    if (!INA226Initialized) {
        Serial.println("Connection to sensor failed");
		return;
    }
    // Configure INA226
    ina.configure(INA226_AVERAGES_64, INA226_BUS_CONV_TIME_2116US, INA226_SHUNT_CONV_TIME_2116US, INA226_MODE_SHUNT_BUS_CONT);
    ina.calibrate(shuntConfig.ShuntResistance(), shuntConfig.MaxCurrent());
    ina.enableConversionReadyAlert();

    Serial.printf("calibrate sensor: Shunt resitance = %.5fR, max currenct = %iA\n", shuntConfig.ShuntResistance(), shuntConfig.MaxCurrent());
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
    sampleTime = (conversionTimeShunt_ + conversionTimeBus_) * samples_ * 0.000001  ;
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

    //float _shuntVoltage = ina.readShuntVoltage();
    float current_  = ina.readShuntCurrent() * shuntConfig.CurrentCalibrationFactor();
    batteryStatus.updateConsumption(current_, sampleTime, count_);

    //WebSerial.printf("%s : Update Ah counter\n", getCurrentTime());
    //WebSerial.printf("    current: %.2f\n", current_);
    //WebSerial.printf("    voltage: %.2f\n", batteryStatus.voltage());
    //WebSerial.printf("    sampletime: %.2f\n    count: %i\n", sampleTime, count_);

#ifdef DEBUG_SENSOR
    Serial.println(F("Update Ah counter"));

    Serial.printf("current is: %.2f (CurrentCalibrationFactor = %.2f)\n", _current, shuntConfig.CurrentCalibrationFactor());
    Serial.printf("    sampletime: %.2f\n    count: %i\n", sampleTime, _count);
    if(_count > 1) {
        Serial.printf("Overflow %d\n", _count);
		WebSerial.printf("%s : Overflow %d\n", getCurretnTime(),  _count);
    } 
#endif // DEBUG_SENSOR

}

void sensorLoop() {
    unsigned long now_ = millis();

    if (savePreferenceTimer.repeat()) {
        batteryStatus.writeStats();
    }

    if(!INA226Initialized) {
		if (statisticsTimer.repeat()) {
			//WebSerial.printf("%s : Sensor not initialized\n", getCurrentTime());
		}
        return;
    }

    if(updateINA226Config) {

        WebSerial.printf("%s : calibrate sensor: Shunt resitance = %.5fR, max currenct = %iA\n", getCurrentTime(), shuntConfig.ShuntResistance(), shuntConfig.MaxCurrent());
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

    if (outputTimer.repeat()) {
        WebSerial.printf("%s : Battery status\n", getCurrentTime());
        WebSerial.printf("    voltage:      %.3fV\n", batteryStatus.voltage());
        WebSerial.printf("    current:      %.3fA\n", batteryStatus.current());
        WebSerial.printf("    average:      %.3fA\n", batteryStatus.averageCurrent());
        WebSerial.printf("    SOC:          %.3f\n", batteryStatus.soc());
        WebSerial.printf("    TTG:          %ds\n", batteryStatus.tTg());
        WebSerial.printf("    remaining As: %.3f\n", batteryStatus.remainingAs());

#ifdef DEBUG_SENSOR
        Serial.printf("Bus voltage:   %.3fV\n", ina.readBusVoltage());
        Serial.printf("Bus power:     %.3fW\n", ina.readBusPower());
        Serial.printf("Shunt voltage: %.3fV\n", ina.readShuntVoltage());
        Serial.printf("Shunt current: %.3fA\n", ina.readShuntCurrent());
#endif // DEBUG_SENSOR
	}
}
