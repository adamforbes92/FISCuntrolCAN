#pragma once

/*
 * fis_display.h — TLBFISLib wrapper
 */

#include <Arduino.h>
#include "config.h"

#include <TLBFISLib.h>
extern TLBFISLib FIS;

bool bootFISWithTimeout(uint32_t timeoutMs);
void displayFIS();
void drawScreen();
void custom_error_function(unsigned long duration);

// Builds the active FIS card from the current data source (Haldex vs CAN).
void parseFIS();
