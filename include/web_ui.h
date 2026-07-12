#pragma once
#include <Arduino.h>

// Starts the local web server (voice-question page + /ask endpoint) and
// mDNS responder (http://salaahclock.local/). Call once from setup(),
// after WiFi is connected.
void webUIInit();

// Services pending HTTP requests. Call every loop() iteration.
void webUIHandle();

// Returns true (once) and fills `answer` when a voice question asked via
// the web page has been answered by the AI and is ready to display.
// Consumes the pending answer.
bool webUITakeAnswer(String &answer);
