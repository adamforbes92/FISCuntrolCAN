#pragma once

/*
 * can_bus.h — ESP32 TWAI (native CAN) interface
 *
 * Replaces the ESP32_CAN library with the ESP-IDF TWAI driver exposed through
 * the Arduino ESP32 core (driver/twai.h).
 */

#include <Arduino.h>
#include "config.h"

void canInit();
void canDeinit();

void nextCanCard();
void prevCanCard();
void resetCanCards();
const char* getCurrentCanCardName();
uint8_t getCurrentCanCardIndex();
void setCurrentCanCardIndex(uint8_t index);
uint8_t getCanCardCount();
const char* getCanCardName(uint8_t index);
void updateCanCardFrame(uint32_t id, uint8_t dlc, const uint8_t* data);

// FIS card rendering for CAN-sourced data (called by parseFIS() in fis_display).
void renderHaldexCard();
void renderCanCard();

// Receive task lives in can_bus.cpp.
void canTask(void* pvParameters);
void startCANReceiveTask();

void broadcastOpenHaldex();
