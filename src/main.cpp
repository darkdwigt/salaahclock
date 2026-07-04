#include <Arduino.h>
#include <WiFi.h>
#include <time.h>

#include "config.h"
#include "display.h"
#include "prayer_times.h"
#include "secrets.h"

enum DisplayMode { MODE_CLOCK, MODE_COUNTDOWN };

static PrayerTimes prayerTimes;
static unsigned long lastPrayerFetch = 0;
static unsigned long lastModeSwitch = 0;
static DisplayMode mode = MODE_CLOCK;
static int lastShownMinute = -1;
static int lastFetchedDay = -1;

static void connectWiFi() {
    Serial.printf("Connecting to WiFi \"%s\"...\n", WIFI_SSID);
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < 20000) {
        delay(250);
        Serial.print(".");
    }
    Serial.println();

    if (WiFi.status() == WL_CONNECTED) {
        Serial.printf("Connected, IP: %s\n", WiFi.localIP().toString().c_str());
    } else {
        Serial.println("WiFi connect timed out, will retry in loop()");
    }
}

// Returns seconds-until-next-prayer and writes its name into `label`.
// Falls back to tomorrow's Fajr (using today's Fajr time as a same-day
// approximation) if every prayer for today has already passed.
static long secondsToNextPrayer(const struct tm &now, String &label) {
    long nowSec = now.tm_hour * 3600L + now.tm_min * 60L + now.tm_sec;

    long best = -1;
    int bestIdx = -1;
    for (int i = 0; i < P_COUNT; i++) {
        long prayerSec = prayerTimes.minutesOfDay[i] * 60L;
        long diff = prayerSec - nowSec;
        if (diff > 0 && (best < 0 || diff < best)) {
            best = diff;
            bestIdx = i;
        }
    }

    if (bestIdx >= 0) {
        label = PRAYER_NAMES[bestIdx];
        return best;
    }

    // Past Isha: count down to tomorrow's Fajr.
    label = PRAYER_NAMES[P_FAJR];
    long secondsLeftToday = 24L * 3600L - nowSec;
    return secondsLeftToday + prayerTimes.minutesOfDay[P_FAJR] * 60L;
}

static String formatCountdown(long totalSeconds, const String &label) {
    long h = totalSeconds / 3600;
    long m = (totalSeconds % 3600) / 60;
    long s = totalSeconds % 60;
    char buf[32];
    if (h > 0) {
        snprintf(buf, sizeof(buf), "%s in %ld:%02ld:%02ld", label.c_str(), h, m, s);
    } else {
        snprintf(buf, sizeof(buf), "%s in %ld:%02ld", label.c_str(), m, s);
    }
    return String(buf);
}

void setup() {
    Serial.begin(115200);
    delay(200);

    displayInit();
    displayShowStatic("...");

    connectWiFi();

    configTzTime(TZ_INFO, NTP_SERVER);
    Serial.println("Waiting for NTP time sync...");
    struct tm timeinfo;
    while (!getLocalTime(&timeinfo, 10000)) {
        Serial.println("NTP sync retrying...");
    }
    Serial.println("Time synced.");

    if (fetchPrayerTimes(prayerTimes)) {
        Serial.println("Prayer times fetched.");
    } else {
        Serial.println("Initial prayer times fetch failed, will retry in loop()");
    }
    lastPrayerFetch = millis();
    lastFetchedDay = timeinfo.tm_yday;

    lastModeSwitch = millis();
}

void loop() {
    if (WiFi.status() != WL_CONNECTED) {
        connectWiFi();
    }

    struct tm now;
    bool haveTime = getLocalTime(&now, 100);

    // Re-scrape periodically, and force a refresh right after midnight so
    // the next day's times are picked up promptly.
    bool dayRolled = haveTime && now.tm_yday != lastFetchedDay;
    if (!prayerTimes.valid || dayRolled ||
        millis() - lastPrayerFetch >= PRAYER_FETCH_INTERVAL_MS) {
        if (fetchPrayerTimes(prayerTimes)) {
            Serial.println("Prayer times refreshed.");
            if (haveTime) lastFetchedDay = now.tm_yday;
        } else {
            Serial.println("Prayer times refresh failed, keeping last known values.");
        }
        lastPrayerFetch = millis();
    }

    if (!haveTime) {
        delay(50);
        return;
    }

    if (millis() - lastModeSwitch >= DISPLAY_SWITCH_MS) {
        mode = (mode == MODE_CLOCK) ? MODE_COUNTDOWN : MODE_CLOCK;
        lastModeSwitch = millis();
        lastShownMinute = -1; // force redraw

        if (mode == MODE_COUNTDOWN && prayerTimes.valid) {
            String label;
            long secs = secondsToNextPrayer(now, label);
            displayShowScrolling(formatCountdown(secs, label));
        }
    }

    if (mode == MODE_CLOCK) {
        if (now.tm_min != lastShownMinute) {
            char buf[6];
            snprintf(buf, sizeof(buf), "%02d:%02d", now.tm_hour, now.tm_min);
            displayShowStatic(buf);
            lastShownMinute = now.tm_min;
        }
    } else {
        displayAnimateScroll();
    }

    delay(10);
}
