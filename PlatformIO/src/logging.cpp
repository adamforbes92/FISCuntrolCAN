/*
 * logging.cpp — Categorised serial/UI logging implementation
 *
 * Producers call logPrintf(cat, fmt, ...). The formatted line is queued and a
 * dedicated task (LOG_SERIAL) drains it, emitting "[TAG] message" to Serial and
 * appending it to the UI log ring — but only when that category is enabled.
 *
 * Before logInit() runs (early boot), logPrintf() emits inline so nothing is
 * lost during startup.
 */

#include "logging.h"

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#include <stdarg.h>
#include <string.h>

// SYS + KLINE + IO on by default; CAN + WIFI off (they are high-volume).
bool logCatEnabled[LOG_CAT_COUNT] = { true, false, true, true, false };

String serialLogLines[8];
static uint8_t serialLogFillCount = 0;

static const char* const kCatName[LOG_CAT_COUNT] = { "SYS", "CAN", "KLINE", "IO", "WIFI" };

const char* logCatName(LogCat cat) {
    return (cat < LOG_CAT_COUNT) ? kCatName[cat] : "?";
}

struct LogMsg {
    uint8_t cat;
    char    text[120];
};

static QueueHandle_t logQueue = nullptr;

static void appendUiLine(const char* line) {
    if (line == nullptr || line[0] == '\0') {
        return;
    }
    // Clear the on-screen log once all visible rows are filled.
    if (serialLogFillCount >= 8) {
        for (uint8_t i = 0; i < 8; i++) {
            serialLogLines[i] = "";
        }
        serialLogFillCount = 0;
    }
    serialLogLines[serialLogFillCount++] = String(line);
}

// Emit one finished line to Serial + UI, gated by the category toggle.
static void emitLine(uint8_t cat, const char* body) {
    if (cat >= LOG_CAT_COUNT || !logCatEnabled[cat]) {
        return;
    }
    char line[140];
    snprintf(line, sizeof(line), "[%s] %s", kCatName[cat], body);
    Serial.println(line);
    appendUiLine(line);
}

static void logSerialTask(void* pvParameters) {
    (void)pvParameters;
    LogMsg msg;
    for (;;) {
        if (xQueueReceive(logQueue, &msg, portMAX_DELAY) == pdTRUE) {
            emitLine(msg.cat, msg.text);
        }
    }
}

void logInit() {
    if (logQueue != nullptr) {
        return;
    }
    logQueue = xQueueCreate(32, sizeof(LogMsg));
    xTaskCreatePinnedToCore(logSerialTask, "LOG_SERIAL", 4096, nullptr, 1, nullptr, 0);
}

void logPrintf(LogCat cat, const char* fmt, ...) {
    LogMsg msg;
    msg.cat = (cat < LOG_CAT_COUNT) ? (uint8_t)cat : (uint8_t)LOG_SYS;

    va_list args;
    va_start(args, fmt);
    vsnprintf(msg.text, sizeof(msg.text), fmt, args);
    va_end(args);

    // Strip trailing newlines/carriage returns — the emitter adds its own.
    size_t length = strlen(msg.text);
    while (length > 0 && (msg.text[length - 1] == '\n' || msg.text[length - 1] == '\r')) {
        msg.text[--length] = '\0';
    }
    if (length == 0) {
        return;
    }

    if (logQueue != nullptr) {
        // Best-effort: drop the line if the queue is full rather than block.
        xQueueSend(logQueue, &msg, 0);
    } else {
        // Pre-init fallback: emit inline so early-boot logs are not lost.
        emitLine(msg.cat, msg.text);
    }
}
