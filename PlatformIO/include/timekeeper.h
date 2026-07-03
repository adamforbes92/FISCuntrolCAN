#pragma once

/*
 * timekeeper.h — Device clock
 *
 * The device runs as a WiFi Access Point (no internet / NTP), so the wall-clock
 * time is supplied by the connected browser via /api/time on every page load.
 * It is then advanced by the ESP32's internal timekeeper (time()/settimeofday)
 * between updates and persisted to NVS so a reboot has an approximate time.
 *
 * If "Use RTC" is enabled and a DS1307 is wired (SDA=GPIO21, SCL=GPIO22), the
 * RTC is preferred on boot and kept in sync whenever the browser sets the time.
 *
 * All stored/read components are LOCAL time (no timezone maths on-device).
 */

#include <Arduino.h>
#include <time.h>

void     timeInit();   // (re)initialise — applies the current useRTC setting
void     timeSetLocal(int year, int month, int day, int hour, int minute, int second);
bool     timeNow(struct tm* out);   // true if a plausible time is known
bool     timeIsValid();
bool     timeRtcPresent();
bool     timeRtcHealthy();           // RTC present AND running
uint32_t timeNowEpoch();
String   timeNowIso();   // "YYYY-MM-DD HH:MM:SS" (empty if unknown)
String   timeRtcIso();   // time read directly from the RTC (empty if none)
