
#pragma once

#include <Arduino.h>
#include "RingBuf.h"


static const int MAGICKEY = 0x343332;
struct Statistics {
    void init() {
        memset(this, 0, sizeof(*this));
        secsSinceLastFull = -1;
        minBatVoltage = INT32_MAX;
        lastDischarge = INT32_MAX;
        deepestDischarge = INT32_MAX;

    }
    const uint32_t magic = MAGICKEY;
    // The following values are the main values we want
    float socVal;
    float remainAs;
    unsigned int tTgVal;  // Time to go in seconds
    // Here the statistics start
    float consumedAs; // As
    unsigned int deepestDischarge; // mAh
    unsigned int lastDischarge; // mAh
    unsigned int averageDischarge; // mAh
    unsigned int numChargeCycles;
    unsigned int numFullDischarge;
    float sumApHDrawn; // Ah
    unsigned int minBatVoltage; // mV
    unsigned int maxBatVoltage; // mv
    int secsSinceLastFull; // s
    unsigned int numAutoSyncs;
    unsigned int numLowVoltageAlarms;
    unsigned int numHighVoltageAlarms;
    float amountDischargedEnergy; // kWh
    float amountChargedEnergy; // kWh
    float deepestTemperatur; // °C
    float highestTemperatur; // °C
};


class BatteryStatus {
protected:
    static const uint16_t GlidingAverageWindow = 60;
public:
    BatteryStatus();
    ~BatteryStatus() {}

    void begin();
    void setParameters(uint16_t capacityAh, uint16_t chargeEfficiencyPercent, uint16_t minPercent, uint16_t tailCurrentmA, uint16_t fullVoltagemV, uint16_t fullDelayS);
    void updateSOC();
    void updateTtG();
    void setVoltage(float currVoltage);
    void setTemperatur(float currTemperatur);
    bool checkFull();
    void updateConsumption(float current, float period, uint16_t numPeriods);
    void updateStats(unsigned long now);

    //Getters
    unsigned int tTg() {
        return stats.tTgVal;
    }
    float soc() {
        return stats.socVal;
    }
    bool isFull() {
        return (fullReachedAt != 0);
    }

    float voltage() {
        return lastVoltage;
    }
    float current() {
        return lastCurrent;
    }
    float averageCurrent() {
        return getAverageConsumption();
    }
    float temperatur(){
      return lastTemperature;
    }

    float remainingAs() {
		return stats.remainAs;
	}

    void setBatterySoc(float val);
    void resetStats();
    void writeStats();
    bool readStats();
    const Statistics& statistics() {return stats;}

    protected:                
        float getAverageConsumption();
        // This is called when we become synced for the first time;
        void setDeepestDischarge();
        void writeStatsToRTC();
        bool readStatsFromRTC();

        RingBuf<float, GlidingAverageWindow> currentValues;
        float batteryCapacity;
        float chargeEfficiency; // Value between 0 and 1 (representing percent)       
        float tailCurrent; // For full detection, A going ointo the battery
        float fullVoltage; // Voltage when Battery ois assumed to be full
        float minAs; // Amount of As that are in the battery when we assume it to be empty
        unsigned long fullDelay; // For how long do we need Full Voltage and current < tailCurrent to assume battery is full

        bool isCharging = false;

        float lastVoltage;
        float lastCurrent;   
        float lastTemperature;     
        unsigned long fullReachedAt;        
        float glidingAverageCurrent;
        unsigned long lasStatUpdate;
        bool isSynced;
        Statistics stats;
};

extern BatteryStatus gBattery;
