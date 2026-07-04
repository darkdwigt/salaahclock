#pragma once
#include <Arduino.h>

struct WeatherData {
    bool valid = false;
    String text; // matrix-safe single line, e.g. "Partlycloudy +22C"
};

// Fetches a one-line forecast from wttr.in (IP-geolocated) and fills `out`.
// Returns true on success.
bool fetchWeather(WeatherData &out);
