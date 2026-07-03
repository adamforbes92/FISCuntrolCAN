#pragma once

/*
 * logging.h — Categorised serial/UI logging
 *
 * SpeedPulserPro-style component logging. Every log line is tagged with a
 * category. Each category has a single runtime on/off toggle that gates
 * emission to BOTH the serial console AND the on-screen / web UI log ring.
 * Toggles are persisted in NVS and editable from the web UI.
 *
 * A dedicated FreeRTOS task drains a queue so producers never block on Serial.
 */

#include <Arduino.h>

enum LogCat : uint8_t {
    LOG_SYS = 0,   // [SYS]   boot, FIS, RTC, menu, general
    LOG_CAN,       // [CAN]   TWAI driver + chassis/Haldex frames
    LOG_KLINE,     // [KLINE] K-line polling + menu reconnect
    LOG_IO,        // [IO]    ignition, buttons, oil sensor, GPIO
    LOG_WIFI,      // [WIFI]  WiFi / web server
    LOG_CAT_COUNT
};

// Per-category enable (runtime, NVS-persisted). Index by LogCat.
extern bool logCatEnabled[LOG_CAT_COUNT];

// On-screen / web UI log ring (read by the dashboard + diagnostics tab).
extern String serialLogLines[8];

void        logInit();                                  // create queue + start serial task
void        logPrintf(LogCat cat, const char* fmt, ...); // categorised printf
const char* logCatName(LogCat cat);                     // "SYS","CAN","KLINE","IO","WIFI"
