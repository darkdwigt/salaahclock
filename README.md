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

Cheap 4-in-1 MAX7219 boards ship with a few different internal wirings,
and the module strip itself can also just be mounted the wrong way up.
If text comes out wrong, there are two independent things to try:

1. **Physically flip the matrix strip** (rotate the whole 4-module
   block 180 degrees). This build's matrix needed this — as received,
   text came out rotated 90 degrees; flipping the block fixed it with
   no code change.
2. **Change `HARDWARE_TYPE`** in `include/config.h` between `FC16_HW`
   (default, confirmed working on this build), `PAROLA_HW`,
   `GENERIC_HW`, or `ICSTATION_HW`, then rebuild and reflash.

These are independent axes — a rotation problem can look similar to a
hardware-type mismatch, so if one fix doesn't fully resolve it, try the
other before assuming the wiring itself is bad.

## Known-good SPI clock speed

`lib/MD_MAX72XX` is a vendored (not registry-fetched) copy of the
MD_MAX72XX library, patched to run the SPI clock at 1MHz instead of the
upstream default of 8MHz (`MD_MAX72xx.cpp`, `spiSend()`). At 8MHz, the
scrolling countdown text corrupted intermittently over our jumper-wire
connections — signal integrity, not a loose connection — because the
static clock face redraws only once a minute and rarely triggers a
transfer, while the scrolling animation sends hundreds of transfers a
second. 1MHz has been solid in testing. If you ever delete `lib/` or
switch back to a registry `lib_deps` entry for MD_MAX72XX, you'll lose
this patch silently — the symptom will be intermittent garbling during
scrolling only, not the static clock.

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
- The display shows the static current time for 4 seconds
  (`DISPLAY_SWITCH_MS` in `config.h`), then switches to a scrolling
  "PrayerName: X min" countdown to whichever prayer is next and holds
  that mode until the scroll has fully passed across the display once
  (not on a fixed timer — see gotcha below), then switches back. After
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

## Gotchas found the hard way

- **MD_Parola's `displayText()`/`setTextBuffer()` stores a raw pointer
  into whatever string you pass it — it does not copy the text.** It
  keeps reading from that pointer for the entire scroll animation. Any
  string passed to `displayShowScrolling()` must outlive the scroll, so
  `main.cpp` keeps it in a file-scope `static String countdownText`
  rather than a function-local temporary. Passing a temporary here
  silently produces a dangling pointer: the scroll appears to "complete"
  almost instantly because the library reads freed/garbage memory and
  sees what looks like an empty string.
- Mode switching for the countdown is driven by scroll-completion
  (`displayAnimateScroll()` returning `true`), not a fixed timer —
  `DISPLAY_SWITCH_MS` only governs how long the clock face is shown. A
  fixed timer here previously cut the scroll off mid-pass every cycle.

## Acknowledgments

- **Fonts:** the clock face and scrolling text use the "Matrix Light"
  bitmap fonts by Trip5 — https://github.com/trip5/Matrix-Fonts
  (licensed CC-BY). The `.bdf` sources are vendored under `fonts/`.
- **Glyph editing:** the hand-redrawn/adjusted glyphs (and their
  column-byte values in `include/small_font.h`) were built with the
  online dot-matrix editor at https://dotmatrixtool.com/
