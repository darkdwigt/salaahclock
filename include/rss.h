#pragma once
#include <Arduino.h>
#include "config.h"

struct RssHeadlines {
    bool valid = false;
    String headlines[RSS_MAX_HEADLINES];
    int count = 0;
};

// Fetches the sport, business, and economy feeds (config.h) and interleaves
// their headlines (sport, business, economy, sport, business, economy...)
// into one rotation so each section gets even airtime in MODE_NEWS.
// Headlines are pulled from each <item>'s <title> by string search - no XML
// library, matching the scraping approach already used for prayer times.
// Returns true if at least one headline was found across all feeds.
bool fetchRssHeadlines(RssHeadlines &out);
