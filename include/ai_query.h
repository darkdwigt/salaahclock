#pragma once
#include <Arduino.h>

// Sends `question` to Pollinations.ai's free text API (no API key/signup
// required: https://text.pollinations.ai/{prompt}) and fills `answer` with
// a matrix-safe (ASCII, length-capped) response. Returns true on success.
bool askAI(const String &question, String &answer);
