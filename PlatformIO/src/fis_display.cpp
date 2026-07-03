/*
 * fis_display.cpp — TLBFISLib initialisation and rendering
 */

#include "fis_display.h"
#include "config.h"
#include "can_bus.h"
#include "cards.h"
#include "bootLogos.h"

#include <SPI.h>

// ---------------------------------------------------------------------------
// parseFIS — chooses the data source and builds the active FIS card.
// The card content itself is built by the owning data module (can_bus).
// ---------------------------------------------------------------------------
void parseFIS() {
    if (showHaldex) {
        renderHaldexCard();
    } else {
        renderCanCard();
    }
}

// ---------------------------------------------------------------------------
// CJ74LVC4T245 Channel 2 direction control (ENA line only)
// 1DIR is hardwired to 3.3V — Channel 1 (CLK, DATA) is always A→B.
// GPIO fisENADIR controls 2DIR via a 10kΩ pull-up (default HIGH = A→B):
//   HIGH = ESP32 drives ENA to FIS (A→B, output)
//   LOW  = FIS drives ENA back to ESP32 (B→A, input / listen)
// ---------------------------------------------------------------------------
static inline void fisEnaDir_output() { digitalWrite(fisENADIR, HIGH); }
static inline void fisEnaDir_input()  { digitalWrite(fisENADIR, LOW);  }

// ---------------------------------------------------------------------------
// SPI callbacks required by TLBFISLib
// ---------------------------------------------------------------------------
static void sendFunctionFIS(uint8_t data) {
    // Assert output direction before driving the bus.
    // DIR stays HIGH briefly after the transfer; TLBLib releases ENA (stop_ENA)
    // within a few microseconds, so the overlap is harmless.
    fisEnaDir_output();
    SPI_INSTANCE.beginTransaction(SPISettings(125000, MSBFIRST, SPI_MODE3));
    SPI_INSTANCE.transfer(data);
    SPI_INSTANCE.endTransaction();
    // Small hold: ensures DIR stays HIGH while TLBLib completes stop_ENA()
    // (digitalWrite LOW + pinMode INPUT), then release to input direction.
    delayMicroseconds(10);
    fisEnaDir_input();
}

static void beginFunctionFIS() {
    // Configure 2DIR (ENA channel) as output, default to listen direction.
    // The 10kΩ pull-up on 2DIR means the chip defaults A→B; drive LOW here
    // so ENA is in input mode until an actual transmission begins.
    pinMode(fisENADIR, OUTPUT);
    fisEnaDir_input();
    SPI_INSTANCE.begin();
}

// ---------------------------------------------------------------------------
// Global FIS object
// ---------------------------------------------------------------------------
TLBFISLib FIS(fisENA, sendFunctionFIS, beginFunctionFIS);

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------
bool bootFISWithTimeout(uint32_t timeoutMs) {
    (void)timeoutMs;

    DEBUG("Booting FIS...");
    delay(fisWakeDelay);
    FIS.errorFunction(custom_error_function);
    FIS.begin();
    FIS.initScreen(screenSize);
    DEBUG("Booting FIS Complete!");
    return true;
}

void displayFIS() {
    FIS.update();
    FIS.setTextAlignment(globalTextAlignment);
    FIS.setFont(TLBFISLib::COMPACT);

    const char* cardTitle = getCardTitle();
    if (cardTitle != nullptr && cardTitle[0] != '\0') {
        FIS.writeText(0, 1, cardTitle);
        FIS.drawLine(0, 9, 64);
    }

    static char combinedArray[500];
    combinedArray[0] = '\0';
    char buf[32];
    char padded[33];  // must hold up to 31 chars from buf plus null (%-12s pads but never truncates)

    // Take mutex before reading fisLine[] to avoid race with klineTask (core 0)
    SemaphoreHandle_t mtx = getFisLineMutex();
    if (mtx) xSemaphoreTake(mtx, portMAX_DELAY);

    for (uint8_t i = 0; i < 8; i++) {
        fisLine[i].toUpperCase();
        fisLine[i].toCharArray(buf, sizeof(buf));
        snprintf(padded, sizeof(padded), "%-12s", buf);
        if (i == 0) {
            snprintf(combinedArray, sizeof(combinedArray), "%s", padded);
        } else {
            size_t used = strlen(combinedArray);
            snprintf(combinedArray + used, sizeof(combinedArray) - used, "\n%s", padded);
        }
    }

    if (mtx) xSemaphoreGive(mtx);

    FIS.writeMultiLineText(0, 27, combinedArray, false);
}

void drawScreen() {
    // Use the selected rev/shift indicator image (danger preset or custom upload).
    const uint8_t* bmp = getRpmLogoBitmap(rpmLogoSelection);
    if (bmp == nullptr) bmp = MK4Golf;
    FIS.clear();
    FIS.drawBitmap(0, 0, 64, 88, bmp, true);
    FIS.update();
}

void custom_error_function(unsigned long duration) {
    (void)duration;
    // TLBFISLib invokes this when a transfer to the FIS fails (the cluster did
    // not acknowledge on the ENA line). The render-task watchdog counts these
    // to detect a hung/unresponsive FIS and force a re-boot of the display.
    fisErrorEventCount = fisErrorEventCount + 1;
    FIS.initScreen();
    FIS.writeMultiLineText(0, 16, "Error\nevent");
}
