#include "weather.h"
#include "config.h"
#include <HTTPClient.h>
#include <WiFiClientSecure.h>

bool fetchWeather(WeatherData &out) {
    if (WiFi.status() != WL_CONNECTED) return false;

    WiFiClientSecure client;
    client.setInsecure(); // public, non-sensitive data - skip cert pinning

    HTTPClient http;
    // No location in the URL: wttr.in geolocates the requesting IP.
    String url = String("https://wttr.in/?format=") + WEATHER_FORMAT;
    if (!http.begin(client, url)) return false;
    http.setTimeout(10000);
    // wttr.in only returns the plain-text one-liner for curl-like clients;
    // browser user agents get back a full HTML page instead.
    http.addHeader("User-Agent", "curl/8.0");

    int code = http.GET();
    if (code != HTTP_CODE_OK) {
        Serial.printf("Weather fetch failed, HTTP %d\n", code);
        http.end();
        return false;
    }

    String line = http.getString();
    http.end();
    line.trim();

    // Strip the UTF-8 degree symbol (bytes 0xC2 0xB0) - not in the MAX7219 font.
    String clean;
    clean.reserve(line.length());
    for (int i = 0; i < (int)line.length(); i++) {
        if ((uint8_t)line[i] == 0xC2 && i + 1 < (int)line.length() &&
            (uint8_t)line[i + 1] == 0xB0) {
            i++; // skip both bytes of the degree symbol
            continue;
        }
        clean += line[i];
    }

    out.text = clean;
    out.valid = clean.length() > 0;
    return out.valid;
}
