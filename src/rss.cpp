#include "rss.h"
#include "config.h"
#include <HTTPClient.h>
#include <WiFiClientSecure.h>

// Maps a Unicode code point to its closest matrix-safe ASCII equivalent,
// or '\0' if it should just be dropped (no reasonable ASCII stand-in).
static char asciiForCodePoint(long cp) {
    switch (cp) {
        case 0x2013: case 0x2014: return '-';  // en/em dash
        case 0x2018: case 0x2019: return '\''; // curly single quotes
        case 0x201C: case 0x201D: return '"';  // curly double quotes
        default: return cp < 0x80 ? (char)cp : '\0';
    }
}

// Decodes a UTF-8 multi-byte sequence starting at s[i] into a code point.
// Returns the byte length consumed (1-4), or 1 (treating it as Latin-1) if
// it doesn't look like a valid UTF-8 lead byte.
static int decodeUtf8At(const String &s, int i, long &cp) {
    uint8_t b0 = (uint8_t)s[i];
    int len = (b0 & 0xE0) == 0xC0 ? 2 : (b0 & 0xF0) == 0xE0 ? 3 : (b0 & 0xF8) == 0xF0 ? 4 : 1;
    if (len == 1 || i + len > (int)s.length()) {
        cp = b0;
        return 1;
    }
    cp = b0 & (0xFF >> (len + 1));
    for (int k = 1; k < len; k++) cp = (cp << 6) | ((uint8_t)s[i + k] & 0x3F);
    return len;
}

// Converts UTF-8 text to matrix-safe ASCII: known punctuation (dashes,
// curly quotes, etc. commonly found in news headlines) is mapped to its
// closest ASCII stand-in, anything else non-ASCII is dropped since it
// isn't in the MAX7219 font.
static String toMatrixSafeAscii(const String &s) {
    String out;
    out.reserve(s.length());
    for (int i = 0; i < (int)s.length();) {
        if ((uint8_t)s[i] < 0x80) {
            out += s[i];
            i++;
        } else {
            long cp;
            i += decodeUtf8At(s, i, cp);
            char ascii = asciiForCodePoint(cp);
            if (ascii) out += ascii;
        }
    }
    return out;
}

// Decodes named entities (&amp; etc.) and numeric entities (&#8211;) - RSS
// feeds mix both depending on the CMS. Numeric entities outside ASCII are
// mapped to a plain-ASCII stand-in where a sensible one exists (dashes,
// curly quotes) since the MAX7219 font is ASCII-only, and dropped otherwise.
static String decodeEntities(const String &s) {
    String named = s;
    named.replace("&amp;", "&");
    named.replace("&quot;", "\"");
    named.replace("&apos;", "'");
    named.replace("&lt;", "<");
    named.replace("&gt;", ">");

    String out;
    out.reserve(named.length());
    for (int i = 0; i < (int)named.length(); i++) {
        if (named[i] == '&' && i + 2 < (int)named.length() && named[i + 1] == '#') {
            int semi = named.indexOf(';', i + 2);
            if (semi > 0 && semi - i < 12) {
                String numPart = named.substring(i + 2, semi);
                long cp = numPart.startsWith("x") || numPart.startsWith("X")
                              ? strtol(numPart.c_str() + 1, nullptr, 16)
                              : strtol(numPart.c_str(), nullptr, 10);
                char ascii = asciiForCodePoint(cp);
                if (ascii) out += ascii;
                i = semi;
                continue;
            }
        }
        out += named[i];
    }
    return out;
}

// Pulls the text out of a <title>...</title> element, unwrapping a
// <![CDATA[ ... ]]> section if present (RSS feeds commonly wrap titles in
// CDATA so embedded ampersands/quotes don't need entity-escaping).
static String extractTitleText(const String &block) {
    String title = block;
    title.trim();
    if (title.startsWith("<![CDATA[")) {
        int end = title.indexOf("]]>");
        title = end >= 0 ? title.substring(9, end) : title.substring(9);
    }
    title.trim();
    return decodeEntities(title);
}

// Fetches a single RSS feed and pulls up to maxCount headlines into out[].
// Returns the number of headlines found, or -1 on network/HTTP failure.
static int fetchRssFeed(const char *url, String out[], int maxCount) {
    WiFiClientSecure client;
    client.setInsecure(); // public, non-sensitive data - skip cert pinning

    HTTPClient http;
    if (!http.begin(client, url)) return -1;
    http.setTimeout(10000);

    int code = http.GET();
    if (code != HTTP_CODE_OK) {
        Serial.printf("RSS fetch failed for %s, HTTP %d\n", url, code);
        http.end();
        return -1;
    }

    String xml = http.getString();
    http.end();

    int count = 0;
    int searchFrom = 0;
    while (count < maxCount) {
        int itemStart = xml.indexOf("<item", searchFrom);
        if (itemStart < 0) break;
        int itemEnd = xml.indexOf("</item>", itemStart);
        if (itemEnd < 0) break;

        int titleStart = xml.indexOf("<title>", itemStart);
        if (titleStart < 0 || titleStart > itemEnd) {
            searchFrom = itemEnd + 7;
            continue;
        }
        titleStart += 7;
        int titleEnd = xml.indexOf("</title>", titleStart);
        if (titleEnd < 0 || titleEnd > itemEnd) {
            searchFrom = itemEnd + 7;
            continue;
        }

        String title = toMatrixSafeAscii(extractTitleText(xml.substring(titleStart, titleEnd)));
        if (title.length() > 0) {
            out[count++] = title;
        }
        searchFrom = itemEnd + 7;
    }

    return count;
}

bool fetchRssHeadlines(RssHeadlines &out) {
    if (WiFi.status() != WL_CONNECTED) return false;

    const char *feedUrls[3] = {RSS_FEED_SPORT_URL, RSS_FEED_BUSINESS_URL, RSS_FEED_ECONOMY_URL};
    String feedHeadlines[3][RSS_HEADLINES_PER_FEED];
    int feedCounts[3];
    for (int f = 0; f < 3; f++) {
        feedCounts[f] = fetchRssFeed(feedUrls[f], feedHeadlines[f], RSS_HEADLINES_PER_FEED);
        if (feedCounts[f] < 0) feedCounts[f] = 0; // one feed failing shouldn't drop the others
    }

    // Interleave sport/business/economy so consecutive news turns cycle
    // through all three sections evenly rather than draining one at a time.
    RssHeadlines result;
    for (int slot = 0; slot < RSS_HEADLINES_PER_FEED && result.count < RSS_MAX_HEADLINES; slot++) {
        for (int f = 0; f < 3; f++) {
            if (slot < feedCounts[f]) {
                result.headlines[result.count++] = feedHeadlines[f][slot];
            }
        }
    }

    result.valid = result.count > 0;
    out = result;
    return result.valid;
}
