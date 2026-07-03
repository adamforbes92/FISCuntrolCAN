#pragma once

/*
 * onboot.h — Pin setup, boot sequence, connection launch, shutdown
 */

#include <Arduino.h>
#include "config.h"

void setupPins();
void launchBoot();
void launchConnections();
void saveNavState();
void loadNavState();
void refreshConnections();
void requestRestartIgnitionSequence();
void restartIgnitionSequence();
void fisDisablePrep();
void beginShutdown();
