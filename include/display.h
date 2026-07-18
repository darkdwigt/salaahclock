#pragma once
#include <Arduino.h>

void displayInit();

// Static, centered text (e.g. "12:34"). Call once per change.
void displayShowStatic(const String &text);

// Boot splash: "loading" with a progress bar filled to `percent` (0..100).
void displayLoading(uint8_t percent);

// Clock face: small weekday label + larger time (e.g. small "FRI" next
// to "13:22"), centered as one block. Call once per change.
void displayShowDayTime(const String &day, const String &time);

// Begins scrolling `text` left. Call once when the text changes.
void displayShowScrolling(const String &text);

// Must be called every loop() iteration while a scrolling message is
// active; keeps the scroll animation moving. Returns true once the
// message has fully scrolled across the display (one complete pass).
bool displayAnimateScroll();
