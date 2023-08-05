/*
INA226.cpp - Class file for the INA226 Bi-directional Current/Power Monitor Arduino Library.

(c) 2014 Korneliusz Jarzebski, modified 2020 by Peter Buchegger
www.jarzebski.pl

This program is free software: you can redistribute it and/or modify
it under the terms of the version 3 GNU General Public License as
published by the Free Software Foundation.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#if ARDUINO >= 100
#include <Arduino.h>
#else
#include <WProgram.h>
#endif

#include <Wire.h>
#include "INA226.h"

INA226::INA226(TwoWire &w) : inaAddress(0), currentLSB(0), powerLSB(0), vShuntMax(0.08192f), vBusMax(36), rShunt(0) {
  wire = &w;
}

bool INA226::begin(uint8_t address)
{
    inaAddress = address;
    return configure(
        INA226_AVERAGES_1,
        INA226_BUS_CONV_TIME_1100US,
        INA226_SHUNT_CONV_TIME_1100US,
        INA226_MODE_SHUNT_BUS_CONT
    );
}

bool INA226::configure(ina226_averages_t avg, ina226_busConvTime_t busConvTime, ina226_shuntConvTime_t shuntConvTime, ina226_mode_t mode)
{
    uint16_t config = 0;

    config |= (avg << 9 | busConvTime << 6 | shuntConvTime << 3 | mode);

    vBusMax = 36;
    vShuntMax = 0.08192f;

    return writeRegister16(INA226_REG_CONFIG, config);
}

bool INA226::calibrate(float rShuntValue, float iMaxCurrentExcepted)
{
    uint16_t calibrationValue;
    rShunt = rShuntValue;

    float minimumLSB;

    minimumLSB = iMaxCurrentExcepted / 32768;

    currentLSB = (uint32_t)(minimumLSB * 100000000);
    currentLSB /= 100000000;
    currentLSB /= 0.0001;
    currentLSB = ceil(currentLSB);
    currentLSB *= 0.0001;

    powerLSB = currentLSB * 25;

    calibrationValue = (uint16_t)((0.00512) / (currentLSB * rShunt));

    return writeRegister16(INA226_REG_CALIBRATION, calibrationValue);
}

float INA226::getMaxPossibleCurrent(void)
{
    return (vShuntMax / rShunt);
}

float INA226::getMaxCurrent(void)
{
    float maxCurrent = (currentLSB * 32767);
    float maxPossible = getMaxPossibleCurrent();

    if (maxCurrent > maxPossible)
    {
        return maxPossible;
    } else
    {
        return maxCurrent;
    }
}

float INA226::getMaxShuntVoltage(void)
{
    float maxVoltage = getMaxCurrent() * rShunt;

    if (maxVoltage >= vShuntMax)
    {
        return vShuntMax;
    } else
    {
        return maxVoltage;
    }
}

float INA226::getMaxPower(void)
{
    return (getMaxCurrent() * vBusMax);
}

float INA226::readBusPower(void)
{
    return (readRegister16(INA226_REG_POWER) * powerLSB);
}

float INA226::readShuntCurrent(void)
{
    return (readRegister16(INA226_REG_CURRENT) * currentLSB);
}

float INA226::readShuntVoltage(void)
{
    float voltage;

    voltage = readRegister16(INA226_REG_SHUNTVOLTAGE);

    return (voltage * 0.0000025);
}

float INA226::readBusVoltage(void)
{
    int16_t voltage;

    voltage = readRegister16(INA226_REG_BUSVOLTAGE);

    return (voltage * 0.00125);
}

ina226_averages_t INA226::getAverages(void)
{
    uint16_t value;

    value = readRegister16(INA226_REG_CONFIG);
    value &= 0b0000111000000000;
    value >>= 9;

    return (ina226_averages_t)value;
}

ina226_busConvTime_t INA226::getBusConversionTime(void)
{
    uint16_t value;

    value = readRegister16(INA226_REG_CONFIG);
    value &= 0b0000000111000000;
    value >>= 6;

    return (ina226_busConvTime_t)value;
}

ina226_shuntConvTime_t INA226::getShuntConversionTime(void)
{
    uint16_t value;

    value = readRegister16(INA226_REG_CONFIG);
    value &= 0b0000000000111000;
    value >>= 3;

    return (ina226_shuntConvTime_t)value;
}

ina226_mode_t INA226::getMode(void)
{
    uint16_t value;

    value = readRegister16(INA226_REG_CONFIG);
    value &= 0b0000000000000111;

    return (ina226_mode_t)value;
}

bool INA226::setMaskEnable(uint16_t mask)
{
    return writeRegister16(INA226_REG_MASKENABLE, mask);
}

uint16_t INA226::getMaskEnable(void)
{
    return readRegister16(INA226_REG_MASKENABLE);
}

bool INA226::addMaskEnableBit(uint16_t mask) 
{
    uint16_t temp = getMaskEnable();
    temp |= mask;
    return setMaskEnable(temp);
}

bool INA226::enableShuntOverLimitAlert(void) {
    return addMaskEnableBit(INA226_BIT_SOL);
}

bool INA226::enableShuntUnderLimitAlert(void)
{
    return addMaskEnableBit(INA226_BIT_SUL);
}

bool INA226::enableBusOvertLimitAlert(void)
{
    return addMaskEnableBit(INA226_BIT_BOL);
}

bool INA226::enableBusUnderLimitAlert(void)
{
    return addMaskEnableBit(INA226_BIT_BUL);
}

bool INA226::enableOverPowerLimitAlert(void)
{
    return addMaskEnableBit(INA226_BIT_POL);
}

bool INA226::enableConversionReadyAlert(void)
{
    return addMaskEnableBit(INA226_BIT_CNVR);    
}

bool INA226::setBusVoltageLimit(float voltage)
{
    uint16_t value = voltage / 0.00125;
    return writeRegister16(INA226_REG_ALERTLIMIT, value);
}

bool INA226::setShuntVoltageLimit(float voltage)
{
    uint16_t value = voltage / 0.0000025;
    return writeRegister16(INA226_REG_ALERTLIMIT, value);
}

bool INA226::setPowerLimit(float watts)
{
    uint16_t value = watts / powerLSB;
    return writeRegister16(INA226_REG_ALERTLIMIT, value);
}

bool INA226::setAlertInvertedPolarity(bool inverted)
{
    uint16_t temp = getMaskEnable();

    if (inverted)
    {
        temp |= INA226_BIT_APOL;
    } else
    {
        temp &= ~INA226_BIT_APOL;
    }

    return setMaskEnable(temp);
}

bool INA226::setAlertLatch(bool latch)
{
    uint16_t temp = getMaskEnable();

    if (latch)
    {
        temp |= INA226_BIT_LEN;
    } else
    {
        temp &= ~INA226_BIT_LEN;
    }

    return setMaskEnable(temp);
}

bool INA226::isMathOverflow(void)
{
    return ((getMaskEnable() & INA226_BIT_OVF) == INA226_BIT_OVF);
}

bool INA226::isAlert(void)
{
    return ((getMaskEnable() & INA226_BIT_AFF) == INA226_BIT_AFF);
}

bool INA226::isConversionReady(void) 
{
    return ((getMaskEnable() & INA226_BIT_CVRF) == INA226_BIT_CVRF);
}


int16_t INA226::readRegister16(uint8_t reg) 
{
    int16_t value;

    wire->beginTransmission(inaAddress);
    wire->write(reg);
    wire->endTransmission();

    wire->requestFrom(inaAddress, 2);
    uint8_t vha = wire->read();
    uint8_t vla = wire->read();
    value = vha << 8 | vla;

    return value;
}

bool INA226::writeRegister16(uint8_t reg, uint16_t val)
{
    uint8_t vla;
    vla = (uint8_t)val;
    val >>= 8;

    wire->beginTransmission(inaAddress);
    wire->write(reg);
    wire->write((uint8_t)val);
    wire->write(vla);
    return wire->endTransmission() == 0;
}
