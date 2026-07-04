# Salaah Clock — project notes

An ESP32 + LED dot-matrix clock that alternates between the current time
and a countdown to the next prayer, scraped from the local masjid's
website. See `README.md` for hardware/wiring/setup details — this file
is about project status and where to pick up next.

## Status: hardware in hand, wired unsoldered, display not yet working

Parts arrived (invoice: `DIYElectronics_Invoice_REDACTED-ORDER_2026-07-03.pdf`,
order #REDACTED-ORDER, 2026-07-03 — FireBeetle ESP32, 4x MAX7219 8x8 dot
matrix, F-F jumper wires). Firmware builds and has been flashed and run
successfully on the actual board:

- USB-serial: board enumerates as `/dev/cu.usbserial-xxxx` (first cable
  tried was power-only, no data lines — swap cable if the board doesn't
  show up in `pio device list` / `ls /dev/cu.*`).
- Flash + monitor both work from this sandboxed shell, but
  `pio device monitor` fails (`termios.error`) because there's no real
  TTY here — use a short pyserial script instead (with a DTR/RTS toggle
  to force a reset and catch the boot banner):
  `/Users/user/.local/pipx/venvs/platformio/bin/python3` has pyserial
  bundled (the system `python3` doesn't).
- Confirmed working end-to-end over serial: WiFi connect (SSID "REDACTED-SSID"), NTP sync, and a successful prayer-time scrape — the whole
  software side of `main.cpp` is verified correct.
- **Blocked on physical wiring**, not firmware. The FireBeetle's pin
  headers are not soldered — jumpers are just friction-fit into bare
  through-holes borrowed from a spare header strip, and the connection
  is extremely fragile (the smallest touch breaks it). Symptoms seen
  so far: display blank when data lines aren't making contact, and
  **all-LEDs-solid-on** (a classic MAX7219 symptom of the chip getting
  garbage on CS/DIN/CLK and landing in its display-test register) even
  after a full firmware reboot — meaning it's not a software state, it's
  a bad connection. VCC/GND are confirmed good (matrix visibly powers
  on). CS/DIN/CLK are the suspects.
- **Next physical step: solder the header pins onto the FireBeetle**
  (user doesn't have a soldering iron yet as of 2026-07-03) before
  further debugging the data-line issue — not worth continuing to
  diagnose on a friction-fit connection.
- A wiring quick-reference was generated as a Claude Artifact during
  this session (not saved to the repo — regenerate if needed).

## What's built

- `src/main.cpp` — WiFi connect, NTP time sync, mode-switching loop
  (clock ⟷ countdown every 4s).
- `src/prayer_times.cpp` — scrapes `muaadhbinjabal.org.za` homepage
  (no JSON API exists) by locating each prayer's HTML block via a class
  marker and reading the Azaan/Salaah `HH:MM` pair out of it.
- `src/display.cpp` — thin wrapper around MD_Parola for the 4x MAX7219
  8x8 chain.
- `include/config.h` — pins, timezone, refresh intervals, brightness.
- `include/secrets.h` — WiFi credentials (gitignored; `secrets.h.example`
  is the template).

## Next steps

1. **Solder the male header strip onto the FireBeetle's two 18-hole
   rows** (short/embedded pin end goes down through the board from the
   top — same side as the silkscreen labels and the charging port —
   long end stays up for jumper wires). Redo the 5 jumper connections
   into the soldered pins afterward.
2. Re-check the MAX7219 IN-header connections once solid:
   VCC→VCC/5V, GND→GND, DIN→D23/GPIO23, CLK→D18/GPIO18, CS→D2/GPIO25.
   (LED matrix has an IN header and an OUT header — only IN is used;
   the board is 4 modules pre-chained internally, so there's exactly
   one true IN at one end of the chain, identifiable as the header with
   nothing feeding into its input side.)
3. Flash + monitor: `pio run -t upload` then read serial (see note
   above about using pyserial directly instead of `pio device monitor`
   in a non-TTY shell).
4. **Expect to need to flip `HARDWARE_TYPE`** in `include/config.h`
   between `FC16_HW` / `PAROLA_HW` / `GENERIC_HW` if text comes out
   mirrored/reversed/split once the display is reliably receiving data.
5. Sanity-check the scraped prayer times against what's currently shown
   on https://www.muaadhbinjabal.org.za — the scraper depends on the
   site's exact HTML structure and will silently need updating if the
   site is redesigned.

## Known limitations (by design, not bugs)

- TLS cert validation is skipped (`setInsecure()`) — acceptable since
  the fetched data isn't sensitive.
- Jumu'ah time is scraped but unused; countdown only covers the 5 daily
  prayers.
- After Isha, the countdown to tomorrow's Fajr uses *today's* Fajr time
  as an approximation until the next scheduled scrape (every 30 min,
  and forced right after local midnight) picks up the real value.
