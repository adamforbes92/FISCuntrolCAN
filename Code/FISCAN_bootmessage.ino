#include "Wire.h"
#include "RTClib.h"

RTC_DS1307 rtc_time;

struct bootMessage {
  String specialDate;
  String bootMessage1;
  String bootMessage2;
};

// define special dates for remembering...
bootMessage specialMessage[6] = {
  { "2512", "MERRY", "CHRISTMAS!" },
  { "0101", "HAPPY NEW", "YEAR ADAM!" },
  { "1405", "HAPPY BIRTHDAY", "ADAM!" },
  { "2505", "ENJOY", "VOLKSFLING!" },
  { "2207", "HAPPY", "ANNIVERSARY!" },
  { "1709", "ENJOY", "EDITION38!" }
};

// define standard references to names/smiles etc
String strName = "ADAM";
String strSmile = ":)";

void returnBootMessage() {
  byte currentHour = 0;
  char combinedDayMonth[6];
  String combinedDayMonthStr = "";
  int i = 0;

  // launch the RTC
  if (!rtc_time.begin()) {
    DEBUG_PRINTLN("Couldn't find RTC...");
    fisLine[3] = "WELCOME";
    fisLine[4] = "ADAM!";
    fisLine[6] = strSmile;
  }
  if (!rtc_time.isrunning()) {
    DEBUG_PRINTLN("RTC is NOT running, let's set the time!");
    rtc_time.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }

  // get time values, combine/format etc...
  DateTime now_time = rtc_time.now();
  sprintf(combinedDayMonth, "%02d%02d", now_time.day(), now_time.month());
  currentHour = now_time.hour();
  combinedDayMonthStr = String(combinedDayMonth);

  // check through the array to see if there's a special date available
  for (i = 0; i < (sizeof(specialMessage) / sizeof(specialMessage[0])); i++) {
    if (specialMessage[i].specialDate == combinedDayMonthStr) {
      fisLine[3] = specialMessage[i].bootMessage1;
      fisLine[4] = specialMessage[i].bootMessage2;
      fisLine[6] = strSmile;
    }
  }

  // not found above, get time...
  if (fisLine[3] == "" || fisLine[4] == "") {
    switch (currentHour) {
      case 0 ... 12:
        fisLine[3] = "GOOD MORNING";
        fisLine[4] = strName;
        fisLine[6] = strSmile;
        break;
      case 13 ... 18:
        fisLine[3] = "GOOD AFTERNOON";
        fisLine[4] = strName;
        fisLine[6] = strSmile;
        break;
      case 19 ... 24:
        fisLine[3] = "GOOD EVENING";
        fisLine[4] = strName;
        fisLine[6] = strSmile;
        break;
    }
  }
}
