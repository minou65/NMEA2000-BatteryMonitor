// common.h

#pragma once

#ifndef _COMMON_h
#define _COMMON_h

#if defined(ARDUINO) && ARDUINO >= 100
#include "arduino.h"
#else
#include "WProgram.h"
#endif

#define BOARD_WEMOS_D1_MINI_ESP32       // Wemos D1 Mini ESP32
// #define BOARD_NODE32S                // NodeMCU-32S
// #define BOARD_ESP32_DEVKIT           // ESP32 DevKit V1
// #define BOARD_CUSTOM                 // Custom board (define pins below)


#if defined(BOARD_WEMOS_D1_MINI_ESP32)
    // ----------------------------------------------------
    // Wemos D1 Mini ESP32 Pin Configuration
    // ----------------------------------------------------
#define ESP32_CAN_TX_PIN GPIO_NUM_23    // CAN Bus TX Pin
#define ESP32_CAN_RX_PIN GPIO_NUM_5     // CAN Bus RX Pin

#define ONE_WIRE_BUS     GPIO_NUM_36    // DS18B20 OneWire Data Pin

#define INA226_ALERT_PIN GPIO_NUM_19      // INA226 Alert Pin
#define INA226_SCL_PIN   GPIO_NUM_22      // INA226 SCL Pin
#define INA226_SDA_PIN   GPIO_NUM_21      // INA226 SDA Pin

#define BOARD_NAME "Wemos D1 Mini ESP32"
#define BOARD_INFO "Standard Wemos D1 Mini ESP32 configuration"

#elif defined(BOARD_NODE32S)
    // ----------------------------------------------------
    // NodeMCU-32S Pin Configuration
    // ----------------------------------------------------
#define ESP32_CAN_TX_PIN GPIO_NUM_5     // CAN Bus TX Pin
#define ESP32_CAN_RX_PIN GPIO_NUM_4     // CAN Bus RX Pin

#define ONE_WIRE_BUS     GPIO_NUM_22    // DS18B20 OneWire Data Pin

#define INA226_ALERT_PIN GPIO_NUM_19      // INA226 Alert Pin
#define INA226_SCL_PIN   GPIO_NUM_22      // INA226 SCL Pin
#define INA226_SDA_PIN   GPIO_NUM_21      // INA226 SDA Pin

#define BOARD_NAME "NodeMCU-32S"
#define BOARD_INFO "NodeMCU-32S standard configuration"
#elif defined(BOARD_CUSTOM)
    // ----------------------------------------------------
    // Custom Board Pin Configuration
    // ----------------------------------------------------
    // Define your custom pin assignments here
#define ESP32_CAN_TX_PIN GPIO_NUM_17    // CAN Bus TX Pin (customize)
#define ESP32_CAN_RX_PIN GPIO_NUM_16    // CAN Bus RX Pin (customize)
#define ONE_WIRE_BUS     GPIO_NUM_25    // DS18B20 OneWire Data Pin (customize)

#define BOARD_NAME "Custom ESP32 Board"
#define BOARD_INFO "User-defined custom pin configuration"

#else
    // ----------------------------------------------------
    // No Board Selected - Compilation Error
    // ----------------------------------------------------
#error "ERROR: No valid board configuration selected! Please uncomment ONE board definition at the top of this file."
#error "Available options: BOARD_WEMOS_D1_MINI_ESP32, BOARD_NODE32S, BOARD_ESP32_DEVKIT, BOARD_CUSTOM"

#endif



#include <Arduino.h>
#include <N2kTypes.h>

extern bool debugMode;

#define DEBUG_PRINT(x) if (debugMode) Serial.print(x) 
#define DEBUG_PRINTLN(x) if (debugMode) Serial.println(x)
#define DEBUG_PRINTF(...) if (debugMode) Serial.printf(__VA_ARGS__)

#define STRING_LEN 50
#define NUMBER_LEN 10
#define DATE_LEN 11
#define TIME_LEN 6

extern bool ParamsChanged;
extern bool SaveParams;

extern bool INA226Initialized;


extern char CustomName[64];
extern char Version[];

#define UPDATE_INTERVAL 1000

extern String getCurrentTime();

#endif
