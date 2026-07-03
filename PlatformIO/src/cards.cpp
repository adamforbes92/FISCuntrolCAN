#include "cards.h"
#include "config.h"

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

static card_source_id activeCardSource = CARD_SOURCE_NONE;
static char activeCardTitle[24] = "";
static String activeCardLines[8];

// Protects fisLine[] between tasks on different cores (commitCard writes, displayFIS reads)
static SemaphoreHandle_t s_fisLineMutex = nullptr;

void initCardMutex() {
    if (s_fisLineMutex == nullptr) {
        s_fisLineMutex = xSemaphoreCreateMutex();
    }
}

void beginCard(card_source_id source, const char* title) {
    activeCardSource = source;

    if (title == nullptr) {
        activeCardTitle[0] = '\0';
    } else {
        snprintf(activeCardTitle, sizeof(activeCardTitle), "%s", title);
    }

    for (uint8_t i = 0; i < 8; i++) {
        activeCardLines[i] = "";
    }
}

void setCardLine(uint8_t lineIndex, const String& value) {
    if (lineIndex >= 8) {
        return;
    }
    activeCardLines[lineIndex] = value;
}

void commitCard() {
    if (s_fisLineMutex && xSemaphoreTake(s_fisLineMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        for (uint8_t i = 0; i < 8; i++) {
            fisLine[i] = activeCardLines[i];
        }
        xSemaphoreGive(s_fisLineMutex);
    } else {
        // Mutex not yet initialised or timeout — write anyway (startup path)
        for (uint8_t i = 0; i < 8; i++) {
            fisLine[i] = activeCardLines[i];
        }
    }
}

SemaphoreHandle_t getFisLineMutex() {
    return s_fisLineMutex;
}

void clearCard() {
    beginCard(CARD_SOURCE_NONE, "");
    commitCard();
}

card_source_id getSelectedCardSource() {
    if (showHaldex && hasHaldex) {
        return CARD_SOURCE_HALDEX;
    }

    switch (bootSource) {
        case BOOT_SOURCE_KLINE:
            if (hasK && isConnectedK) {
                return CARD_SOURCE_KLINE;
            }
            break;

        case BOOT_SOURCE_CAN:
            if ((hasCAN || hasHaldex) && isConnectedCAN) {
                return CARD_SOURCE_CAN;
            }
            break;

        case BOOT_SOURCE_NONE:
            if (hasHaldex) {
                return CARD_SOURCE_HALDEX;
            }
            break;
    }

    if (hasK && isConnectedK) {
        return CARD_SOURCE_KLINE;
    }

    if ((hasCAN || hasHaldex) && isConnectedCAN) {
        return CARD_SOURCE_CAN;
    }

    return CARD_SOURCE_NONE;
}

card_source_id getCardSource() {
    return activeCardSource;
}

const char* getCardTitle() {
    return activeCardTitle;
}
