#pragma once

/*
 * bootmessage.h — Personalised boot greeting / special-date message
 *
 * returnBootMessage() fills fisLine[] and returns true when a text screen
 * should be shown (a matching special date, or the Welcome Greeting). A
 * matching special date takes priority and overrides the boot logo for the day.
 */

#include <Arduino.h>

bool returnBootMessage();
