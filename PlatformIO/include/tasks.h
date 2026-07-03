#pragma once

#include <Arduino.h>

void startTasks();
void startCANReceiveTask();
void IRAM_ATTR ignitionMonitorISR();
