/*
 * bootmessage.cpp — Personalised boot greeting / special-date message
 *
 * Data is now user-configurable from the web UI and persisted in NVS:
 *   - welcomeGreetingEnabled + welcomeGreetName  → "GOOD MORNING/AFTERNOON/EVENING <name>"
 *   - specialDatesEnabled + specialDateValue[]/specialDateText[] → custom day messages
 *
 * The wall-clock time comes from the timekeeper module (browser-set, optionally
 * backed by a DS1307 RTC). A matching special date overrides the boot logo.
 */

#include "bootmessage.h"
#include "config.h"
#include "timekeeper.h"

// Fill fisLine[] from a '\n'-separated message, starting on line 2.
static bool buildSpecialDate(const struct tm& t) {
    for (int i = 0; i < 5; i++) {
        if (specialDateValue[i].length() < 10) continue;
        int y = specialDateValue[i].substring(0, 4).toInt();
        int m = specialDateValue[i].substring(5, 7).toInt();
        int d = specialDateValue[i].substring(8, 10).toInt();
        if (y <= 1970) continue;                       // unset sentinel
        if (d != t.tm_mday || m != (t.tm_mon + 1)) continue;

        String txt = specialDateText[i];
        if (txt.length() == 0) continue;

        uint8_t line = 2;
        int start = 0;
        while (line < 8) {
            int nl = txt.indexOf('\n', start);
            fisLine[line++] = (nl < 0) ? txt.substring(start) : txt.substring(start, nl);
            if (nl < 0) break;
            start = nl + 1;
        }
        return true;
    }
    return false;
}

bool returnBootMessage() {
    for (int i = 0; i < 8; i++) fisLine[i] = "";

    struct tm t;
    bool haveTime = timeNow(&t);

    // Special date wins and overrides the logo for the day.
    if (specialDatesEnabled && haveTime && buildSpecialDate(t)) {
        return true;
    }

    if (welcomeGreetingEnabled) {
        const char* greet = "WELCOME";
        if (haveTime) {
            uint8_t h = t.tm_hour;
            if (h < 12)      greet = "GOOD MORNING";
            else if (h < 18) greet = "GOOD AFTERNOON";
            else             greet = "GOOD EVENING";
        }
        fisLine[3] = greet;
        fisLine[4] = welcomeGreetName;
        fisLine[6] = ":)";
        return true;
    }

    return false;
}
