#pragma once

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

typedef enum {
    CARD_SOURCE_NONE = 0,
    CARD_SOURCE_KLINE,
    CARD_SOURCE_CAN,
    CARD_SOURCE_HALDEX,
} card_source_id;

void initCardMutex();
SemaphoreHandle_t getFisLineMutex();

void beginCard(card_source_id source, const char* title);
void setCardLine(uint8_t lineIndex, const String& value);
void commitCard();
void clearCard();

card_source_id getSelectedCardSource();
card_source_id getCardSource();
const char* getCardTitle();
