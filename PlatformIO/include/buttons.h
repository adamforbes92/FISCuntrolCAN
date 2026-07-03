#pragma once

/*
 * buttons.h — Stalk button management (OneButton)
 */

#include <Arduino.h>
#include <OneButton.h>

typedef enum {
	BUTTON_UP = 0,
	BUTTON_DOWN,
	BUTTON_RESET,
	BUTTON_COUNT,
} button_id;

// Instances – declared here, defined in buttons.cpp; used in main.cpp for ISR
extern OneButton stalkUpButton;
extern OneButton stalkDownButton;
extern OneButton stalkResetButton;

void setupButtons();
void mimickStalkButtons();
void configureButtonOutputPins();
void serviceButtonOutputs();
void serviceExternalOutput();
bool setButtonInputPin(button_id button, uint8_t gpio);   // stub — pins fixed at compile time
bool setButtonOutputPin(button_id button, uint8_t gpio);  // stub — pins fixed at compile time
uint8_t getButtonInputPin(button_id button);
uint8_t getButtonOutputPin(button_id button);
bool getButtonInputPressed(button_id button);
bool getButtonOutputActive(button_id button);
bool getButtonOutputPhysicalLevel(button_id button);
void setButtonOutputLevel(button_id button, bool level);
void resetButtonOutputStates();
void simulateButtonPress(button_id button);

// ISR handler (attached to GPIO interrupts)
void IRAM_ATTR checkTicks();
