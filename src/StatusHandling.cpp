#include "common.h"
#include "statusHandling.h"

#ifdef ESP32
#include <Preferences.h>
#endif

#include <WebSerial.h>

const double MAX_FLOAT = 3.4E+38; // nax value for float
const double MIN_FLOAT = -3.4E+38; // min value forfloat

BatteryStatus gBattery;

BatteryStatus::BatteryStatus() {
    lastCurrent = 0;
    fullReachedAt = 0;
    glidingAverageCurrent = 0;
    lasStatUpdate = 0;
    lastTemperature = 0;
    isSynced = false;
}

void BatteryStatus::begin() {
	lastCurrent = 0;
	fullReachedAt = 0;
	glidingAverageCurrent = 0;
	lasStatUpdate = 0;
	lastTemperature = 0;
	isSynced = false;

	stats.init();

    if (!readStats()) {
		Serial.println("BatteryStatus::begin: No valid data found. Resetting stats...");
		stats.init();
	} else if (stats.magic != MAGICKEY) {
		Serial.println("BatteryStatus::begin: Invalid data found. Resetting stats...");
		Serial.printf("    stats.magic: %d\n", stats.magic);
		Serial.printf("    MAGICKEY: %d\n", MAGICKEY);
		stats.init();
	}
}

void BatteryStatus::setParameters(uint16_t capacityAh, uint16_t chargeEfficiencyPercent, uint16_t minPercent, uint16_t tailCurrentmA, uint16_t fullVoltagemV,uint16_t fullDelayS) {

        batteryCapacity = ((float)capacityAh) * 3600.0f; // We use it in As
        chargeEfficiency = ((float)chargeEfficiencyPercent) / 100.0f;
        tailCurrent = tailCurrentmA / 1000.0f;
        fullVoltage = fullVoltagemV / 1000.0f;
        minAs = minPercent * batteryCapacity / 100.0f;
        fullDelay = ((unsigned long)fullDelayS) *1000; 

        if (gCurrentCalibrationFactor < 0) {
            tailCurrent = tailCurrent * -1;
        }



        Serial.println("BatteryStatus::setParameters");
        Serial.printf("    capacityAh: %d\n", capacityAh);
        Serial.printf("    capacityAs: %.3f\n", batteryCapacity);
        Serial.printf("    chargeEfficiency: %.3f\n", chargeEfficiency);
        Serial.printf("    tailCurrent: %.3f\n", tailCurrent);
        Serial.printf("    fullVoltage: %.3f\n", fullVoltage);
        Serial.printf("    minAs: %.3f\n", minAs);
        Serial.printf("    fullDelay: %ld\n", fullDelay);

}

void BatteryStatus::updateSOC() {
    if (gCurrentCalibrationFactor != 0) {
        stats.socVal = stats.remainAs / batteryCapacity;;
    }
    else {
        if (lastVoltage > 12.6) {
            stats.socVal = 1.0;
        } else if(lastVoltage > 12.5){
			stats.socVal = 0.9;
		} else if(lastVoltage > 12.42) {
			stats.socVal = 0.8;
		} else if(lastVoltage > 12.33) {
			stats.socVal = 0.7;
		} else if(lastVoltage > 12.2) {
			stats.socVal = 0.6;
		} else if(lastVoltage > 12.06) {
			stats.socVal = 0.5;
		} else if(lastVoltage > 11.9) {
			stats.socVal = 0.4;
		} else if(lastVoltage > 11.75) {
			stats.socVal = 0.3;
		} else if(lastVoltage > 11.58) {
			stats.socVal = 0.2;
		} else if(lastVoltage > 11.31) {
			stats.socVal = 0.1;
        }
        else {
            stats.socVal = 0.0;
        }
    }

    //Serial.println("BatteryStatus::updateSOC");
    //Serial.printf("    socVal: %.3f\n", stats.socVal);
    //Serial.printf("    lastSoc: %.3f\n", lastSoc);
    //Serial.printf("    remainAs: %.3f\n", stats.remainAs);
    //Serial.printf("    batteryCapacity: %.3f\n", batteryCapacity);
    //Serial.printf("    fabs: %.3f\n", fabs(lastSoc - stats.socVal));
   
}

void BatteryStatus::updateTtG() {
    float _avgCurrent = getAverageConsumption() * -1;

    //Serial.println("BatteryStatus::updateTtG");
    //Serial.printf("    avgCurrent : %.3f\n", _avgCurrent);
    //Serial.printf("    TtgVal: %d\n", stats.tTgVal);
    //Serial.printf("    remainAs: %.3f\n", stats.remainAs);
    //Serial.printf("    minAs: %.3f\n", minAs);

    if (_avgCurrent > 0.0) {
        unsigned int _tTgVal = static_cast<int>(max(stats.remainAs - minAs, 0.0f) / _avgCurrent);
        
        if (_tTgVal > 359940) {
            _tTgVal = 359940;
        }
        stats.tTgVal = _tTgVal;
        isCharging = false;
    }  else if (_avgCurrent == 0.0){

    } else {
		stats.tTgVal = 0;
        if (!isCharging) {
            isCharging = true;
            stats.numChargeCycles++;
        }
	}


}

void BatteryStatus::updateConsumption(float current, float period, uint16_t numPeriods) {

    for (int _i = 0; _i < numPeriods; ++_i) {
        if (currentValues.isFull()) {
            float _oldVal;
            currentValues.pop(_oldVal);
            glidingAverageCurrent += _oldVal;
        }
        currentValues.push(current);
        // Assumtion: Consumption is negative
        glidingAverageCurrent -= current;
    }

    if (currentValues.isEmpty()) {
        // This is the first measurement, so we don't have an old current value
        lastCurrent = current;
    }

    // We use the average between the last and the current value for summation.
    float _periodConsumption = (lastCurrent + current) / 2.0 * period * numPeriods;

    // Has to be in 0.01 kWh....
    double _consumption = _periodConsumption / 3.6 / 1000.0 / 10.0 * lastVoltage;
    if (_periodConsumption > 0) {
        // We are charging
        // Verify that we don't have an overflow
        if (stats.amountChargedEnergy <= MAX_FLOAT - _consumption) {
            stats.amountChargedEnergy += _consumption;
        }
        else {
            stats.amountChargedEnergy = 0;
        }
    }
    else {
 		// We are discharging

        double _tempSumApHDrawn = _periodConsumption / -3600; // Ah
        if (stats.sumApHDrawn >= MIN_FLOAT + _tempSumApHDrawn) {
            stats.sumApHDrawn += _tempSumApHDrawn;
        }
        else {
            stats.sumApHDrawn = MIN_FLOAT;
        }

        // Verify that we don't have an overflow
        if (stats.amountDischargedEnergy <= MAX_FLOAT - (_consumption * -1)) {
            stats.amountDischargedEnergy += (_consumption * -1);
        }
        else {
            stats.amountDischargedEnergy = 0;
        }

    }

    stats.remainAs += _periodConsumption;
    stats.consumedAs += _periodConsumption;

    if (stats.remainAs > batteryCapacity) {
        stats.remainAs = batteryCapacity;
    }
    else if (stats.remainAs < 0.0f) {
        stats.remainAs = 0.0f;
    }

    lastCurrent = current;

    //Serial.println("BatteryStatus::updateConsumption");
    //Serial.printf("    current: %.3f\n", current);
    //Serial.printf("    Voltage: %.3f\n", lastVoltage);
    //Serial.printf("    period: %.3f\n", period);
    //Serial.printf("    numPeriods: %d\n", numPeriods);
    //Serial.printf("    periodConsumption: %.3f\n", _periodConsumption);
    //Serial.printf("    consumption: %.3f\n", _consumption);
    //Serial.printf("    amountChargedEnergy: %.3f kWh\n", stats.amountChargedEnergy);
    //Serial.printf("    amountDischargedEnergy: %.3f kWh\n", stats.amountDischargedEnergy);
    //Serial.printf("    sumApHDrawn: %.3f Ah\n", stats.sumApHDrawn);
    //Serial.printf("    remainAs: %.3f\n", stats.remainAs);
    //Serial.printf("    consumedAs: %.3f\n", stats.consumedAs);

}

float BatteryStatus::getAverageConsumption() {
    uint16_t _count = currentValues.size();
    if (_count != 0) {
		return  (glidingAverageCurrent / _count) * -1;
        
    } else {
        return 0;
    }
}

void BatteryStatus::setVoltage(float currVoltage) {
    lastVoltage = currVoltage;
}

void BatteryStatus::setTemperatur(float currTemperatur) {
    lastTemperature = currTemperatur;
}

bool BatteryStatus::checkFull() {

    if (lastVoltage - fullVoltage >= -0.05) {
        
        if (stats.socVal < 0.99) {
            // This is just to indicate that we will be close to full
            setBatterySoc(0.999);
        }
        float _current = getAverageConsumption();

        if (_current <= tailCurrent) {
            unsigned long _now = millis();
            if (fullReachedAt == 0) {
                fullReachedAt = _now;
            }

            unsigned long _delay = _now - fullReachedAt;
            if (_delay >= fullDelay) {
                // And here we are. 100 %
                setBatterySoc(1.0);
                if (!isSynced) {
                    setDeepestDischarge();
                    isSynced = true;
                }
                stats.secsSinceLastFull = -1;
                stats.numAutoSyncs++;
                stats.lastDischarge = roundf(stats.remainAs / 3.6);
                stats.consumedAs = 0.0;
                return true;
            }
        } else {
            fullReachedAt = 0;
            setBatterySoc(0.999);
        }
    } else {
        fullReachedAt = 0;
    }
    return false;
}

void BatteryStatus::setBatterySoc(float val) {
    stats.socVal = val;
    stats.remainAs = batteryCapacity * val;
    if(val >= 1.0) {
        fullReachedAt = millis();
        stats.secsSinceLastFull = -1;
    }
    updateTtG();
    //writeStats();
}

void BatteryStatus::setDeepestDischarge() {
   // stats.deepestDischarge = stats.remainAs / 3.6;
}

void BatteryStatus::resetStats() {
    stats.amountChargedEnergy = 0;
    stats.amountDischargedEnergy = 0;
    stats.minBatVoltage = INT32_MAX;
    stats.maxBatVoltage = 0;
    stats.lastDischarge = INT32_MAX;
    stats.deepestDischarge = INT32_MAX;
    stats.averageDischarge = 0;

    stats.consumedAs = 0;
    stats.numChargeCycles = 0;
    stats.numFullDischarge = 0;
    stats.sumApHDrawn = 0;
    stats.numAutoSyncs = 0;
    stats.numLowVoltageAlarms = 0;
    stats.numHighVoltageAlarms = 0;
    stats.deepestTemperatur = INT32_MAX;
    stats.highestTemperatur = 0;

	writeStats();

}

void BatteryStatus::updateStats(unsigned long now) {
    int _timeDeltaSec = (now - lasStatUpdate) / 1000;
    lasStatUpdate = now;
    if (_timeDeltaSec < 0) {
        // We had an overflow, so let's assume the last call was 1 sec ago (default interval)
        _timeDeltaSec = 1;
    }
    if (stats.secsSinceLastFull >= 0) {
        stats.secsSinceLastFull += _timeDeltaSec;
    }

    if ((stats.tTgVal != INFINITY) && (!isCharging)) {
        unsigned int _mAh = stats.remainAs / 3.6;

        if (stats.secsSinceLastFull == -1) {
            stats.secsSinceLastFull = 0;
        }

        if ((_mAh > 0) && (stats.deepestDischarge > _mAh)) {
            stats.deepestDischarge = _mAh;
        }
        else if (stats.deepestDischarge == 0) {
            stats.deepestDischarge = _mAh;
        }

        stats.lastDischarge = _mAh;
        stats.averageDischarge = stats.lastDischarge;

    }

    uint32_t _voltageV = lastVoltage * 1000;
    if (stats.minBatVoltage > _voltageV) {
        stats.minBatVoltage = _voltageV;
    }
    if (stats.maxBatVoltage < _voltageV) {
        stats.maxBatVoltage = _voltageV;
    }   

    if ((stats.deepestTemperatur > lastTemperature) || (stats.deepestTemperatur == -127.00) || (stats.deepestTemperatur == 0.00)) {
        stats.deepestTemperatur = lastTemperature;
    }
    if (stats.highestTemperatur < lastTemperature) {
        stats.highestTemperatur = lastTemperature;
    }
}

#ifdef ESP32
RTC_DATA_ATTR Statistics rtcStats;

void BatteryStatus::writeStatsToRTC() {
    memcpy(&rtcStats, &stats, sizeof(stats));
}

bool BatteryStatus::readStatsFromRTC() {
    bool res = true;
    if (rtcStats.magic == MAGICKEY) {
         memcpy(&stats, &rtcStats, sizeof(stats));
    } else {
        res = false;
    }
    return res;
}

void BatteryStatus::writeStats() {
	WebSerial.printf("%s : BatteryStatus::writeStats\n", getCurrentTime());
    try {
		Preferences preferences_;
		preferences_.begin("BatteryMonitor", false);

		size_t writtenBytes_ = preferences_.putBytes("stats", &stats, sizeof(stats));

		if (writtenBytes_ != sizeof(stats)) {
			Serial.println("Error writing data. Clearing container...");
			WebSerial.printf("%s : Error writing data. Clearing container...\n", getCurrentTime());
			preferences_.clear();
		}
		else {
			Serial.printf("Successfully written %d bytes to preferences.\n", writtenBytes_);
			WebSerial.printf("%s : Successfully written %d bytes to preferences.\n", getCurrentTime(), writtenBytes_);
		}
		preferences_.end();
	}
    catch (const std::exception& e) {
        Serial.printf("Error writing data: %s\n", e.what());
        WebSerial.printf("%s : Error writing data: %s\n", getCurrentTime(), e.what());
    }
}


bool BatteryStatus::readStats() {
	WebSerial.printf("%s : BatteryStatus::readStats\n", getCurrentTime());

	try {
		bool res = false;
		Preferences preferences_;
		preferences_.begin("BatteryMonitor", true);
		size_t readBytes_ = preferences_.getBytes("stats", &stats, sizeof(stats));
		if (readBytes_ == sizeof(stats)) {
			Serial.printf("BatteryStatus::readStats: Successfully read %d bytes from preferences.\n", readBytes_);
			WebSerial.printf("%s : Successfully read %d bytes from preferences.\n", getCurrentTime(), readBytes_);
			res = true;
		}
		else {
			Serial.println("BatteryStatus::readStats: No valid data found");
			WebSerial.printf("%s : No valid data found\n", getCurrentTime());
		}
		preferences_.end();
		return res;
	}
	catch (const std::exception& e) {
		Serial.printf("Error reading data: %s\n", e.what());
		WebSerial.printf("%s : Error reading data: %s\n", getCurrentTime(), e.what());
		return false;
	}
}



#else 
void BatteryStatus::writeStatsToRTC() {
    ESP.rtcUserMemoryWrite(0, (uint32_t*)&stats, sizeof(stats));
}

bool BatteryStatus::readStatsFromRTC() {
    uint32_t magic = 0;
    if (!ESP.rtcUserMemoryRead(0, &magic, sizeof(magic)) || magic != MAGICKEY) {
        return false;
    }

    if (!ESP.rtcUserMemoryRead(0, (uint32_t*)&stats.magic, sizeof(stats))) {
        Serial.println("RTC read failed!");
        return false;
    }

    return true;
}

void BatteryStatus::writeStats() {
}

bool BatteryStatus::readStats() {
    return false;
}

#endif
