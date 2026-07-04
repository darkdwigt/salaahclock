#include <Arduino.h>
#include <WiFi.h>
#include <time.h>

#include "config.h"
#include "display.h"
#include "prayer_times.h"
#include "secrets.h"
#include "weather.h"

enum DisplayMode { MODE_CLOCK, MODE_COUNTDOWN, MODE_WEATHER };

static PrayerTimes prayerTimes;
static WeatherData weather;
static unsigned long lastPrayerFetch = 0;
static unsigned long lastWeatherFetch = 0;
static unsigned long lastModeSwitch = 0;
static DisplayMode mode = MODE_CLOCK;
// Alternates which scrolling mode follows the clock face, so countdown and
// weather each get an even share of airtime regardless of fetch cadence.
static bool nextIsWeather = false;
static int lastShownMinute = -1;
static int lastFetchedDay = -1;
// MD_Parola's displayText() stores a pointer into this buffer rather than
// copying it, and keeps reading from it for the whole scroll animation, so
// it must outlive the loop() call that starts the scroll.
static String countdownText;
static String weatherText;

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
    long m = totalSeconds / 60;
    char buf[48];
    if (m > 0) {
        snprintf(buf, sizeof(buf), "%s: %ld min", label.c_str(), m);
    } else {
        snprintf(buf, sizeof(buf), "%s: under a min", label.c_str());
    }
    return String(buf);
}

// Switches to `m` and kicks off its scroll. If the mode's data isn't ready
// yet (e.g. weather hasn't been fetched successfully), falls straight back
// to the clock instead of scrolling something empty.
static void enterMode(DisplayMode m, const struct tm &now) {
    mode = m;
    lastModeSwitch = millis();
    lastShownMinute = -1;

    if (m == MODE_COUNTDOWN) {
        if (!prayerTimes.valid) {
            mode = MODE_CLOCK;
            return;
        }
        String label;
        long secs = secondsToNextPrayer(now, label);
        countdownText = formatCountdown(secs, label);
        displayShowScrolling(countdownText);
    } else if (m == MODE_WEATHER) {
        if (!weather.valid) {
            mode = MODE_CLOCK;
            return;
        }
        weatherText = weather.text;
        displayShowScrolling(weatherText);
    }
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
        if (WiFi.status() != WL_CONNECTED) {
            connectWiFi();
        }
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

    if (fetchWeather(weather)) {
        Serial.println("Weather fetched.");
    } else {
        Serial.println("Initial weather fetch failed, will retry in loop()");
    }
    lastWeatherFetch = millis();

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

    if (!weather.valid || millis() - lastWeatherFetch >= WEATHER_FETCH_INTERVAL_MS) {
        if (fetchWeather(weather)) {
            Serial.println("Weather refreshed.");
        } else {
            Serial.println("Weather refresh failed, keeping last known value.");
        }
        lastWeatherFetch = millis();
    }

    if (!haveTime) {
        delay(50);
        return;
    }

    bool switchFromClock = mode == MODE_CLOCK &&
                            millis() - lastModeSwitch >= DISPLAY_SWITCH_MS;
    bool switchFromScroll = (mode == MODE_COUNTDOWN || mode == MODE_WEATHER) &&
                             displayAnimateScroll();

    if (switchFromClock) {
        DisplayMode next = nextIsWeather ? MODE_WEATHER : MODE_COUNTDOWN;
        nextIsWeather = !nextIsWeather;
        enterMode(next, now);
    } else if (switchFromScroll) {
        mode = MODE_CLOCK;
        lastModeSwitch = millis();
        lastShownMinute = -1; // force redraw
    }

    if (mode == MODE_CLOCK && now.tm_min != lastShownMinute) {
        char buf[6];
        snprintf(buf, sizeof(buf), "%02d:%02d", now.tm_hour, now.tm_min);
        displayShowStatic(buf);
        lastShownMinute = now.tm_min;
    }

    delay(10);
}
