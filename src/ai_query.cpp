#include "ai_query.h"
#include <HTTPClient.h>
#include <WiFiClientSecure.h>

// Percent-encodes everything except unreserved characters, same set as
// JS encodeURIComponent - needed because the question becomes a URL path
// segment for Pollinations' GET-based API.
static String urlEncode(const String &s) {
    String out;
    out.reserve(s.length() * 3);
    const char *hex = "0123456789ABCDEF";
    for (size_t i = 0; i < s.length(); i++) {
        uint8_t c = (uint8_t)s[i];
        bool unreserved = isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~';
        if (unreserved) {
            out += (char)c;
        } else {
            out += '%';
            out += hex[c >> 4];
            out += hex[c & 0x0F];
        }
    }
    return out;
}

bool askAI(const String &question, String &answer) {
    if (WiFi.status() != WL_CONNECTED) return false;

    WiFiClientSecure client;
    client.setInsecure(); // public API, no sensitive data - skip cert pinning

    HTTPClient http;
    String url = "https://text.pollinations.ai/" + urlEncode(question);
    if (!http.begin(client, url)) return false;
    http.setTimeout(20000); // AI response can take a few seconds

    int code = http.GET();
    if (code != HTTP_CODE_OK) {
        Serial.printf("AI query failed, HTTP %d\n", code);
        http.end();
        return false;
    }

    String body = http.getString();
    http.end();
    body.trim();

    // Matrix font is ASCII-only; strip anything outside printable ASCII
    // and cap length so a rambling answer doesn't scroll forever.
    String clean;
    clean.reserve(body.length());
    for (size_t i = 0; i < body.length(); i++) {
        char c = body[i];
        if (c >= 32 && c < 127) clean += c;
        else if (c == '\n' || c == '\r') clean += ' ';
    }
    clean.trim();
    if (clean.length() > 300) clean = clean.substring(0, 300) + "...";

    answer = clean;
    return clean.length() > 0;
}
