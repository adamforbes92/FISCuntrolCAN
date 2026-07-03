#include "timekeeper.h"
#include "config.h"

#include <sys/time.h>
#include <Wire.h>
#include <RTClib.h>
#include <Preferences.h>

static RTC_DS1307 rtc;
static bool       rtcPresent = false;
static Preferences timePrefs;

static const time_t TIME_PLAUSIBLE = 1577836800; // 2020-01-01 00:00:00

static void saveEpoch(uint32_t e) {
    timePrefs.begin("time", false);
    timePrefs.putUInt("epoch", e);
    timePrefs.end();
}

void timeInit() {
    // Treat the stored time as local; disable timezone shifting.
    setenv("TZ", "UTC0", 1);
    tzset();

    rtcPresent = false;
    if (useRTC) {
        Wire.begin(RTC_SDA_PIN, RTC_SCL_PIN);
        if (rtc.begin()) {
            rtcPresent = true;
            if (rtc.isrunning()) {
                DateTime dt = rtc.now();
                struct tm tmv = {};
                tmv.tm_year = dt.year() - 1900;
                tmv.tm_mon  = dt.month() - 1;
                tmv.tm_mday = dt.day();
                tmv.tm_hour = dt.hour();
                tmv.tm_min  = dt.minute();
                tmv.tm_sec  = dt.second();
                time_t e = mktime(&tmv);
                struct timeval tv = { e, 0 };
                settimeofday(&tv, nullptr);
                LOGSYS("Time: loaded from RTC %04d-%02d-%02d %02d:%02d",
                       dt.year(), dt.month(), dt.day(), dt.hour(), dt.minute());
                return;
            }
            LOGSYS("Time: RTC present but not running");
        } else {
            LOGSYS("Time: RTC not found on I2C (SDA=%d SCL=%d)", RTC_SDA_PIN, RTC_SCL_PIN);
        }
    }

    // Fallback: restore the last epoch the browser gave us.
    timePrefs.begin("time", true);
    uint32_t saved = timePrefs.getUInt("epoch", 0);
    timePrefs.end();
    if (saved > (uint32_t)TIME_PLAUSIBLE) {
        struct timeval tv = { (time_t)saved, 0 };
        settimeofday(&tv, nullptr);
        LOGSYS("Time: restored from NVS (epoch=%lu)", (unsigned long)saved);
    } else {
        LOGSYS("Time: no stored time available");
    }
}

void timeSetLocal(int year, int month, int day, int hour, int minute, int second) {
    struct tm tmv = {};
    tmv.tm_year = year - 1900;
    tmv.tm_mon  = month - 1;
    tmv.tm_mday = day;
    tmv.tm_hour = hour;
    tmv.tm_min  = minute;
    tmv.tm_sec  = second;
    time_t e = mktime(&tmv);

    struct timeval tv = { e, 0 };
    settimeofday(&tv, nullptr);

    if (useRTC && rtcPresent) {
        rtc.adjust(DateTime((uint16_t)year, (uint8_t)month, (uint8_t)day,
                            (uint8_t)hour, (uint8_t)minute, (uint8_t)second));
    }
    saveEpoch((uint32_t)e);
    LOGSYS("Time set: %04d-%02d-%02d %02d:%02d:%02d (rtc=%d)",
           year, month, day, hour, minute, second, (int)(useRTC && rtcPresent));
}

bool timeIsValid() {
    return time(nullptr) > TIME_PLAUSIBLE;
}

bool timeRtcPresent() {
    return rtcPresent;
}

bool timeRtcHealthy() {
    return rtcPresent && rtc.isrunning();
}

String timeRtcIso() {
    if (!rtcPresent || !rtc.isrunning()) return String("");
    DateTime dt = rtc.now();
    char buf[24];
    snprintf(buf, sizeof(buf), "%04d-%02d-%02d %02d:%02d:%02d",
             dt.year(), dt.month(), dt.day(), dt.hour(), dt.minute(), dt.second());
    return String(buf);
}

bool timeNow(struct tm* out) {
    if (!out) return false;
    time_t now = time(nullptr);
    gmtime_r(&now, out);
    return now > TIME_PLAUSIBLE;
}

uint32_t timeNowEpoch() {
    return (uint32_t)time(nullptr);
}

String timeNowIso() {
    struct tm t;
    if (!timeNow(&t)) return String("");
    char buf[24];
    snprintf(buf, sizeof(buf), "%04d-%02d-%02d %02d:%02d:%02d",
             t.tm_year + 1900, t.tm_mon + 1, t.tm_mday,
             t.tm_hour, t.tm_min, t.tm_sec);
    return String(buf);
}
