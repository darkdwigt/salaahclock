#pragma once
#include <Arduino.h>

void displayInit();

// Static, centered text (e.g. "12:34"). Call once per change.
void displayShowStatic(const String &text);

// Begins scrolling `text` left. Call once when the text changes.
void displayShowScrolling(const String &text);

// Must be called every loop() iteration while a scrolling message is
// active; keeps the scroll animation moving. Returns true once the
// message has fully scrolled across the display (one complete pass).
bool displayAnimateScroll();
