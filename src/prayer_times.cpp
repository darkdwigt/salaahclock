#include "prayer_times.h"
#include "config.h"
#include <HTTPClient.h>
#include <WiFiClientSecure.h>

const char *PRAYER_NAMES[P_COUNT] = {"Fajr", "Zuhr", "Asr", "Maghrib", "Isha"};

// The masjid site renders each prayer's Azaan/Salaah times as plain
// server-side HTML, e.g.:
//   <div class="... box-shadow-m Fajr">
//     <h6 ...>Fajr Azaan</h6><div ...>05:35</div>
//     <h6 ...>Fajr Salaah</h6><div ...>05:50</div>
//   </div>
// so we don't need a JSON API - just locate each prayer's block by its
// unique class marker and pull the first two HH:MM values out of it.
// The 2nd value (Salaah) is the actual congregation start time, which is
// what matters for a "time to next prayer" countdown.
static const char *BLOCK_MARKERS[P_COUNT + 1] = {
    "box-shadow-m Fajr\"",
    "box-shadow-m Zuhr\"",
    "box-shadow-m Asr\"",
    "box-shadow-m Maghrib\"",
    "box-shadow-m Isha\"",
    "box-shadow-m Jumuah\"", // bounds the end of the Isha block
};

// Finds the next HH:MM occurrence in `s` at or after `from`.
// Returns the index just past the match, or -1 if none found.
static int findNextTime(const String &s, int from, int &hh, int &mm) {
    for (int i = from; i + 4 < (int)s.length(); i++) {
        if (isDigit(s[i]) && isDigit(s[i + 1]) && s[i + 2] == ':' &&
            isDigit(s[i + 3]) && isDigit(s[i + 4])) {
            hh = (s[i] - '0') * 10 + (s[i + 1] - '0');
            mm = (s[i + 3] - '0') * 10 + (s[i + 4] - '0');
            if (hh >= 0 && hh < 24 && mm >= 0 && mm < 60) {
                return i + 5;
            }
        }
    }
    return -1;
}

bool fetchPrayerTimes(PrayerTimes &out) {
    if (WiFi.status() != WL_CONNECTED) return false;

    WiFiClientSecure client;
    client.setInsecure(); // public, non-sensitive data - skip cert pinning

    HTTPClient http;
    if (!http.begin(client, PRAYER_TIMES_URL)) return false;
    http.setTimeout(10000);

    int code = http.GET();
    if (code != HTTP_CODE_OK) {
        Serial.printf("Prayer times fetch failed, HTTP %d\n", code);
        http.end();
        return false;
    }

    String html = http.getString();
    http.end();

    int blockStart[P_COUNT + 1];
    for (int i = 0; i <= P_COUNT; i++) {
        blockStart[i] = html.indexOf(BLOCK_MARKERS[i]);
        if (blockStart[i] < 0) {
            Serial.printf("Could not find marker for %s\n", BLOCK_MARKERS[i]);
            return false;
        }
    }

    PrayerTimes result;
    for (int i = 0; i < P_COUNT; i++) {
        int blockEnd = blockStart[i + 1];
        String block = html.substring(blockStart[i], blockEnd);

        int hh1, mm1, hh2, mm2;
        int pos = findNextTime(block, 0, hh1, mm1);
        if (pos < 0) return false;
        pos = findNextTime(block, pos, hh2, mm2);
        if (pos < 0) return false; // fall back to azaan time if no salaah value found

        result.minutesOfDay[i] = hh2 * 60 + mm2;
        char buf[6];
        snprintf(buf, sizeof(buf), "%02d:%02d", hh2, mm2);
        result.hhmm[i] = buf;
    }

    result.valid = true;
    out = result;
    return true;
}
