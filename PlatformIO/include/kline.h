#pragma once

/*
 * kline.h — KLineKWP1281Lib wrapper
 */

#include <Arduino.h>
#include "config.h"

#include <KLineKWP1281Lib_ESP32.h>
extern KLineKWP1281Lib diag;

void klineInit();
void showMeasurements(uint8_t group);

// Fault code result: code + description string
struct KlineFaultEntry {
    uint16_t code;
    char     description[48];
    char     elaboration[32];
    bool     isIntermittent;
};

// Read fault codes — fills outEntries (max maxEntries), returns count or -1 on error
int  readKlineFaults(KlineFaultEntry* outEntries, uint8_t maxEntries);
// Clear fault codes — returns true on success
bool clearKlineFaults();

// Low-level callbacks used by KLineKWP1281Lib (defined in kline.cpp)
void     beginFunction(unsigned long baud);
void     endFunction();
void     sendFunction(uint8_t data);
bool     receiveFunction(uint8_t* data, unsigned long timeout);
