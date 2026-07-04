#pragma once

// ---- Display wiring (hardware SPI on FireBeetle ESP32) ----
// MAX7219 DIN  -> GPIO23 (MOSI)
// MAX7219 CLK  -> GPIO18 (SCK)
// MAX7219 CS   -> GPIO25 (D2)
// MAX7219 VCC  -> 5V, GND -> GND
#define MAX_DEVICES 4
#define CS_PIN      25

// Most cheap 4-in-1 MAX7219 modules use the FC16 chain wiring.
// If the display shows garbled/mirrored/reversed text, try
// PAROLA_HW or GENERIC_HW here instead.
#define HARDWARE_TYPE MD_MAX72XX::FC16_HW

// ---- Prayer times source ----
#define PRAYER_TIMES_URL "https://www.muaadhbinjabal.org.za/"

// ---- Timezone ----
// South Africa Standard Time, no DST.
#define TZ_INFO "SAST-2"
#define NTP_SERVER "pool.ntp.org"

// ---- Refresh intervals ----
#define PRAYER_FETCH_INTERVAL_MS (30UL * 60UL * 1000UL) // re-scrape every 30 min
#define DISPLAY_SWITCH_MS 4000UL                          // alternate every 4s

// ---- Display appearance ----
#define DISPLAY_INTENSITY 4 // 0 (dim) - 15 (max brightness)
