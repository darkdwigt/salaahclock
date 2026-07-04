#pragma once

// ---- Display wiring (hardware SPI on FireBeetle ESP32) ----
// MAX7219 DIN  -> GPIO23 (MOSI)
// MAX7219 CLK  -> GPIO18 (SCK)
// MAX7219 CS   -> GPIO25 (D2)
// MAX7219 VCC  -> 5V, GND -> GND
#define MAX_DEVICES 4
#define CS_PIN      25

// Confirmed working on this build's physical matrix: FC16_HW, with the
// 4-module strip physically mounted flipped 180 degrees from how it
// arrived (long edge horizontal, rotated so the IN header ends up on
// the correct side). If you swap the matrix hardware or re-wire it,
// text orientation problems can come from either this constant
// (PAROLA_HW / GENERIC_HW / ICSTATION_HW are the alternatives) or from
// the physical mounting rotation -- try flipping the block before
// cycling through every HARDWARE_TYPE value.
#define HARDWARE_TYPE MD_MAX72XX::FC16_HW

// ---- Prayer times source ----
#define PRAYER_TIMES_URL "https://www.muaadhbinjabal.org.za/"

// ---- Weather source (wttr.in) ----
// No city set - wttr.in geolocates the ESP32's public IP. Percent-notation
// format string, kept ASCII-only (no degree symbol/emoji) for the matrix
// font: %C = condition text, %t = temperature.
#define WEATHER_FORMAT "%C+%t"
#define WEATHER_FETCH_INTERVAL_MS (60UL * 60UL * 1000UL) // re-fetch hourly

// ---- Timezone ----
// South Africa Standard Time, no DST.
#define TZ_INFO "SAST-2"
#define NTP_SERVER "pool.ntp.org"

// ---- Refresh intervals ----
#define PRAYER_FETCH_INTERVAL_MS (30UL * 60UL * 1000UL) // re-scrape every 30 min
#define DISPLAY_SWITCH_MS 10000UL                         // clock face shown for 10s

// ---- Display appearance ----
#define DISPLAY_INTENSITY 2 // 0 (dim) - 15 (max brightness)
