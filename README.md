# Salaah Clock

An ESP32-powered LED dot-matrix clock that alternates between the current
time and a countdown to the next prayer, scraped from
[muaadhbinjabal.org.za](https://www.muaadhbinjabal.org.za).

## Hardware

- FireBeetle ESP32
- 4x 8x8 MAX7219 dot matrix module (chained, "4-in-1" board)
- Female-to-female jumper wires

## Wiring

| MAX7219 pin | FireBeetle pin | GPIO |
|---|---|---|
| VCC | 5V | - |
| GND | GND | - |
| DIN | MOSI | GPIO23 |
| CLK | SCK | GPIO18 |
| CS  | D2 | GPIO25 |

(MISO is unused - MAX7219 is write-only over SPI.)

If the FireBeetle board's silkscreen pin labels differ from the GPIO
numbers above, wire by GPIO number, not by label position. Pin choices
live in `include/config.h` (`CS_PIN`) if you need to change them.

## Display orientation

Cheap 4-in-1 MAX7219 boards ship with a few different internal wirings.
If text comes out mirrored, reversed, or split oddly across modules,
change `HARDWARE_TYPE` in `include/config.h` to `PAROLA_HW` or
`GENERIC_HW` and rebuild.

## Setup

1. Copy `include/secrets.h.example` to `include/secrets.h` and fill in
   your WiFi SSID/password. `secrets.h` is gitignored.
2. Install [PlatformIO](https://platformio.org/) (already set up in this
   checkout via `pipx install platformio`).
3. Build: `pio run`
4. Flash (with the board connected over USB): `pio run -t upload`
5. Monitor serial output: `pio device monitor`

## How it works

- `src/prayer_times.cpp` fetches the masjid homepage over HTTPS and pulls
  each prayer's Salaah (congregation) time out of the HTML by locating
  each prayer's block via a class-name marker (e.g. `box-shadow-m Fajr`)
  and reading the first two `HH:MM` values inside it (Azaan, then
  Salaah). No JSON API exists on the site, so this is plain string
  scraping - if the site's markup changes, this will need updating.
- Time comes from NTP (`pool.ntp.org`), localized to `SAST-2` (South
  Africa has no DST).
- Prayer times are re-fetched every 30 minutes and also right after local
  midnight rolls over, so the next day's times show up promptly.
- The display alternates every 4 seconds (`DISPLAY_SWITCH_MS` in
  `config.h`) between the static current time and a scrolling
  "PrayerName in H:MM:SS" countdown to whichever prayer is next. After
  Isha, it counts down to tomorrow's Fajr using today's Fajr time as an
  approximation until the next scrape confirms tomorrow's actual time.

## Known limitations

- `WiFiClientSecure::setInsecure()` is used to skip TLS certificate
  validation, since the fetched data isn't sensitive and pinning a cert
  on a hobby clock isn't worth the maintenance burden. If that ever
  changes, add proper cert validation.
- Jumu'ah is scraped but not currently used in the countdown logic -
  only the five daily prayers are (Fajr/Zuhr/Asr/Maghrib/Isha).
- If the masjid site is unreachable, the clock keeps using the last
  successfully fetched prayer times.
