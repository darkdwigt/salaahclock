#pragma once
#include <Arduino.h>

enum PrayerIndex { P_FAJR = 0, P_ZUHR, P_ASR, P_MAGHRIB, P_ISHA, P_COUNT };

extern const char *PRAYER_NAMES[P_COUNT];

struct PrayerTimes {
    bool valid = false;
    int minutesOfDay[P_COUNT] = {0}; // congregation (salaah/iqamah) time, minutes since midnight
    String hhmm[P_COUNT];
};

// Scrapes the masjid homepage and fills `out`. Returns true on success.
bool fetchPrayerTimes(PrayerTimes &out);
