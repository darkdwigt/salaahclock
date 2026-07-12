#include <Arduino.h>
#include <WiFi.h>
#include <time.h>
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"

#include "ai_query.h"
#include "config.h"
#include "display.h"
#include "prayer_times.h"
#include "rss.h"
#include "secrets.h"
#include "weather.h"
#include "web_ui.h"

enum DisplayMode { MODE_CLOCK, MODE_COUNTDOWN, MODE_WEATHER, MODE_NEWS, MODE_AI_ANSWER };

static PrayerTimes prayerTimes;
static WeatherData weather;
static RssHeadlines rssHeadlines;
static unsigned long lastPrayerFetch = 0;
static unsigned long lastWeatherFetch = 0;
static unsigned long lastRssFetch = 0;
static unsigned long lastModeSwitch = 0;
static DisplayMode mode = MODE_CLOCK;
// Rotates which scrolling mode follows the clock face, so countdown,
// weather, and news each get an even share of airtime regardless of fetch
// cadence: clock -> countdown -> clock -> weather -> clock -> news -> repeat.
static const DisplayMode ROTATION[] = {MODE_COUNTDOWN, MODE_WEATHER, MODE_NEWS};
static int rotationIndex = 0;
// Round-robins through the fetched headlines, one per news turn.
static int newsHeadlineIndex = 0;
static int lastShownMinute = -1;
static int lastFetchedDay = -1;
// MD_Parola's displayText() stores a pointer into this buffer rather than
// copying it, and keeps reading from it for the whole scroll animation, so
// it must outlive the loop() call that starts the scroll.
static String countdownText;
static String weatherText;
static String newsText;
static String aiAnswerText;

static void connectWiFi() {
    Serial.printf("Connecting to WiFi \"%s\"...\n", WIFI_SSID);
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    // Trim the radio's peak current draw so the marginal supply doesn't sag.
    // setTxPower only takes effect after begin() has started the radio, so it
    // must be called here, not before. 11 dBm is plenty for a home network and
    // noticeably gentler on the rail than the ~19.5 dBm default. Modem sleep
    // lowers the steady-state draw between beacons too.
    WiFi.setTxPower(WIFI_POWER_11dBm);
    WiFi.setSleep(true);

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
    } else if (m == MODE_NEWS) {
        if (!rssHeadlines.valid) {
            mode = MODE_CLOCK;
            return;
        }
        newsHeadlineIndex = newsHeadlineIndex % rssHeadlines.count;
        newsText = rssHeadlines.headlines[newsHeadlineIndex];
        newsHeadlineIndex++;
        displayShowScrolling(newsText);
    } else if (m == MODE_AI_ANSWER) {
        displayShowScrolling(aiAnswerText);
    }
}

void setup() {
    // Disable the (over-sensitive) brownout detector as the very first thing:
    // this board runs on a marginal USB supply and the WiFi TX current spike
    // dips the rail just far enough to trip the detector into an endless
    // reset loop, even though the chip itself keeps running fine through the
    // dip. See connectWiFi() for the measures that shrink that spike so the
    // rail stays healthy despite the detector being off.
    WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);

    Serial.begin(115200);
    delay(200);

    // Run the core slower: less current draw overall means more headroom for
    // the WiFi radio's peak, which is the load that was tripping brownout.
    setCpuFrequencyMhz(80);

    displayInit();
    displayShowStatic("...");

    connectWiFi();
    // Make the matrix itself report the boot stage, so the failure point is
    // visible without a serial cable (useful when powered from a wall charger
    // that has no data lines): shows the IP's last octet on success, "noWF"
    // if association failed.
    if (WiFi.status() == WL_CONNECTED) {
        displayShowStatic(String(WiFi.localIP()[3]));
    } else {
        displayShowStatic("noWF");
    }
    delay(1500);

    webUIInit();

    configTzTime(TZ_INFO, NTP_SERVER);
    Serial.println("Waiting for NTP time sync...");
    displayShowStatic("ntp");
    struct tm timeinfo;
    // Bounded retry so a WiFi/NTP failure can never trap boot forever (the old
    // unbounded loop is exactly what left the display frozen on its
    // placeholder). After giving up we fall through to loop(), which keeps
    // retrying time sync and fetches on its own schedule.
    int ntpTries = 0;
    bool timeSynced = false;
    while (!(timeSynced = getLocalTime(&timeinfo, 10000))) {
        if (WiFi.status() != WL_CONNECTED) {
            connectWiFi();
        }
        Serial.println("NTP sync retrying...");
        if (++ntpTries >= 6) {
            Serial.println("NTP sync not succeeding; continuing to loop() to keep retrying.");
            break;
        }
    }
    if (timeSynced) {
        Serial.println("Time synced.");
    }

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

    if (fetchRssHeadlines(rssHeadlines)) {
        Serial.println("Headlines fetched.");
    } else {
        Serial.println("Initial headlines fetch failed, will retry in loop()");
    }
    lastRssFetch = millis();

    lastModeSwitch = millis();
}

void loop() {
    if (WiFi.status() != WL_CONNECTED) {
        connectWiFi();
    }

    webUIHandle();

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

    if (!rssHeadlines.valid || millis() - lastRssFetch >= RSS_FETCH_INTERVAL_MS) {
        if (fetchRssHeadlines(rssHeadlines)) {
            Serial.println("Headlines refreshed.");
        } else {
            Serial.println("Headlines refresh failed, keeping last known values.");
        }
        lastRssFetch = millis();
    }

    if (!haveTime) {
        delay(50);
        return;
    }

    // A voice question always pre-empts whatever's currently showing.
    String voiceAnswer;
    if (webUITakeAnswer(voiceAnswer)) {
        aiAnswerText = voiceAnswer;
        enterMode(MODE_AI_ANSWER, now);
    }

    bool switchFromClock = mode == MODE_CLOCK &&
                            millis() - lastModeSwitch >= DISPLAY_SWITCH_MS;
    bool switchFromScroll = (mode == MODE_COUNTDOWN || mode == MODE_WEATHER ||
                              mode == MODE_NEWS || mode == MODE_AI_ANSWER) &&
                             displayAnimateScroll();

    if (switchFromClock) {
        DisplayMode next = ROTATION[rotationIndex];
        rotationIndex = (rotationIndex + 1) % (sizeof(ROTATION) / sizeof(ROTATION[0]));
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
