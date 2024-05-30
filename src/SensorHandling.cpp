
#include <Arduino.h>
#include <Wire.h>
#include <WebSerial.h>

#include "INA226.h"
#include "INA226EnumToString.h"
#include "common.h"
#include "sensorHandling.h"
#include "statusHandling.h"
#include "neotimer.h"



#if CONFIG_IDF_TARGET_ESP32S2
#define PIN_SCL SCL
#define PIN_SDA SDA
#define PIN_INTERRUPT 7
#elif defined(ESP32)
#define PIN_SCL SCL // D22
#define PIN_SDA SDA // D21
#define PIN_INTERRUPT GPIO_NUM_19
#elif defined(ESP8266)
#define PIN_SCL D1
#define PIN_SDA D2
#define PIN_INTERRUPT D5
#else
#error "Unknown Device"
#endif

#define DEBUG_SENSOR_Config
// #define DEBUG_SENSOR

float gVoltageCalibrationFactor = 1.0;
float gCurrentCalibrationFactor = 1.0;

volatile uint16_t alertCounter = 0;
double sampleTime = 0;
bool gSensorInitialized = false;
bool updateSensorConfig = true;

static INA226 ina(Wire);
Neotimer statisticsTimer = Neotimer(1000);

IRAM_ATTR void alert(void) { ++alertCounter; }

uint16_t translateConversionTime(ina226_shuntConvTime_t time) {
    uint16_t result = 0;
    switch (time) {
        case INA226_BUS_CONV_TIME_140US:
        result = 140;
        break;
        case INA226_BUS_CONV_TIME_204US:
        result = 204;
        break;
        case INA226_BUS_CONV_TIME_332US:
        result = 332;
        break;
        case INA226_BUS_CONV_TIME_588US:
        result = 558;
        break;
        case INA226_BUS_CONV_TIME_1100US:
        result = 1100;
        break;
        case INA226_BUS_CONV_TIME_2116US:
        result = 2116;
        break;
        case INA226_BUS_CONV_TIME_4156US:
        result = 4156;
        break;
        case INA226_BUS_CONV_TIME_8244US:
        result = 8244;
        break;
        default:
        result = 0;
    }
    return result;
}

uint16_t translateSampleCount(ina226_averages_t value) {
    uint16_t result = 0;
    switch (value) {
        case INA226_AVERAGES_1:
        result = 1;
        break;
        case INA226_AVERAGES_4:
        result = 4;
        break;
        case INA226_AVERAGES_16:
        result = 16;
        break;
        case INA226_AVERAGES_64:
        result = 64;
        break;
        case INA226_AVERAGES_128:
        result = 128;
        break;
        case INA226_AVERAGES_256:
        result = 256;
        break;
        case INA226_AVERAGES_512:
        result = 512;
        break;
        case INA226_AVERAGES_1024:
        result = 1024;
        break;
        default:
        result = 0;
    }

  return result;
}

void checkConfig() {
	Serial.printf("Configuration of INA226\n");
	Serial.printf("    Mode:                  %s\n", INA226EnumTypeToStr(ina.getMode()));
	Serial.printf("    Samples average:       %s\n", INA226EnumTypeToStr(ina.getAverages()));
	Serial.printf("    Bus conversion time:   %s\n", INA226EnumTypeToStr((ina226_busConvTime_t)ina.getBusConversionTime()));
	Serial.printf("    Shunt conversion time: %s\n", INA226EnumTypeToStr(ina.getShuntConversionTime()));
	Serial.printf("    Max possible current:  %.3f A\n", ina.getMaxPossibleCurrent());
	Serial.printf("    Max current:           %.3f A\n", ina.getMaxCurrent());
	Serial.printf("    Max shunt voltage:     %.3f V\n", ina.getMaxShuntVoltage());
	Serial.printf("    Max power:             %.3f W\n", ina.getMaxPower());
    Serial.printf("    Shunt resistance:      %.3f R\n", gShuntResistanceR);
    Serial.printf("    Shunt max current:     %i A\n", gMaxCurrentA);

}

void setupSensor() {
    // Default INA226 address is 0x40
    gSensorInitialized = ina.begin();
	statisticsTimer.start();

    // Check if the connection was successful, stop if not
    if (!gSensorInitialized) {
        Serial.println("Connection to sensor failed");
		return;
    }
    // Configure INA226
    ina.configure(INA226_AVERAGES_64, INA226_BUS_CONV_TIME_2116US, INA226_SHUNT_CONV_TIME_2116US, INA226_MODE_SHUNT_BUS_CONT);
    ina.calibrate(gShuntResistanceR / 1000, gMaxCurrentA);
    ina.enableConversionReadyAlert();

    Serial.printf("calibrate sensor: Shunt resitance = %.3fR, max currenct = %iA\n", gShuntResistanceR, gMaxCurrentA);
    gBattery.setParameters(gCapacityAh, gChargeEfficiencyPercent, gMinPercent, gTailCurrentmA, gFullVoltagemV, gFullDelayS);

    uint16_t conversionTimeShunt = translateConversionTime(ina.getShuntConversionTime());
    uint16_t conversionTimeBus = translateConversionTime((ina226_shuntConvTime_t)ina.getBusConversionTime());
    uint16_t samples = translateSampleCount(ina.getAverages());

    // This is the time it takes to create a new measurement
    sampleTime = (conversionTimeShunt + conversionTimeBus) * samples * 0.000001  ;
}

void sensorInit() {
    Wire.begin(PIN_SDA, PIN_SCL); 
    attachInterrupt(digitalPinToInterrupt(PIN_INTERRUPT), alert, FALLING);

    setupSensor();

	if (!gSensorInitialized) {
		return;
	}
}

void updateAhCounter() {
    int _count;
    noInterrupts();
    // If we missed an interrupt, we assume we had the same value all the time.
    _count = alertCounter;
    alertCounter = 0;
    interrupts();

    //float _shuntVoltage = ina.readShuntVoltage();
    float _current = ina.readShuntCurrent() * gCurrentCalibrationFactor;
    gBattery.updateConsumption(_current, sampleTime, _count);

#ifdef DEBUG_SENSOR
    Serial.println(F("Update Ah counter"));

    WebSerial.printf("current is: %.2f (CurrentCalibrationFactor = %.2f)\n", _current, gCurrentCalibrationFactor);
    WebSerial.printf("sampletime is: %.2d; count is: %i\n", sampleTime, _count);
    if(_count > 1) {
        Serial.printf("Overflow %d\n", _count);
		WebSerial.printf("Overflow %d\n", _count);
    } 
#endif // DEBUG_SENSOR

}

void sensorLoop() {
    unsigned long _now = millis();

    if(!gSensorInitialized) {
		if (statisticsTimer.repeat()) {
			WebSerial.println("Sensor not initialized");
            Serial.println("Sensor not initialized");
		}
        return;
    }

    if(updateSensorConfig) {

        WebSerial.printf("calibrate sensor: Shunt resitance = %.3fR, max currenct = %iA\n", gShuntResistanceR, gMaxCurrentA);
        ina.calibrate(gShuntResistanceR / 1000, gMaxCurrentA);    
        gBattery.setParameters(gCapacityAh, gChargeEfficiencyPercent, gMinPercent, gTailCurrentmA, gFullVoltagemV, gFullDelayS);

        checkConfig();
        updateSensorConfig = false;
    }

    while (alertCounter && ina.isConversionReady()) { 
        updateAhCounter();
    }

	if (statisticsTimer.repeat()) {
        gBattery.setVoltage(ina.readBusVoltage() * gVoltageCalibrationFactor);
        gBattery.checkFull();
        gBattery.updateSOC();
        gBattery.updateTtG();
        gBattery.updateStats(_now);

        WebSerial.printf("Bus voltage:   %.3fV\n", ina.readBusVoltage());
        WebSerial.printf("Bus power:     %.3fW\n", ina.readBusPower());
        WebSerial.printf("Shunt voltage: %.3fV\n", ina.readShuntVoltage());
        WebSerial.printf("Shunt current: %.3fA\n", ina.readShuntCurrent());

#ifdef DEBUG_SENSOR
        Serial.printf("Bus voltage:   %.3fV\n", ina.readBusVoltage());
        Serial.printf("Bus power:     %.3fW\n", ina.readBusPower());
        Serial.printf("Shunt voltage: %.3fV\n", ina.readShuntVoltage());
        Serial.printf("Shunt current: %.3fA\n", ina.readShuntCurrent());
#endif // DEBUG_SENSOR
	}
}
