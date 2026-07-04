# Salaah Clock — project notes

An ESP32 + LED dot-matrix clock that alternates between the current time
and a countdown to the next prayer, scraped from the local masjid's
website. See `README.md` for hardware/wiring/setup details and gotchas —
this file is about project status and where to pick up next. See also
`AGENTS.md` for a map of which files/areas to touch for which kind of
change.

## Status: working end-to-end on real hardware (as of 2026-07-04)

Parts arrived (invoice: `DIYElectronics_Invoice_REDACTED-ORDER_2026-07-03.pdf`,
order #REDACTED-ORDER, 2026-07-03 — FireBeetle ESP32, 4x MAX7219 8x8 dot
matrix, F-F jumper wires). Header pins have been soldered onto the
FireBeetle, wiring is solid, and the clock is fully functional:

- WiFi connect (SSID "REDACTED-SSID"), NTP sync, and prayer-time scraping
  all confirmed working over serial.
- Display shows the current time, then scrolls a "PrayerName: X min"
  countdown to the next prayer, then switches back — repeating
  indefinitely. Orientation is correct and the scroll no longer garbles
  or cuts off early.
- Git repo initialized locally (`git init`, one accumulating commit
  history). Remote `origin` is set to
  `https://github.com/darkdwigt/salaahclock.git` but **not yet
  authenticated/pushed** — `gh auth login`'s device flow kept failing
  in this sandboxed shell (network resets, interrupted polling). Revisit
  with a personal access token or from a normal (non-sandboxed)
  terminal when ready to push.

### What it took to get here (debugging history, useful if regressions appear)

1. **Physical connection**: originally friction-fit jumpers on an
   unsoldered board — extremely fragile, caused blank display and
   all-LEDs-on (classic MAX7219 garbage-on-CS/DIN/CLK symptom). Fixed by
   soldering the header pins on.
2. **Display orientation**: came out rotated 90°. Fixed by physically
   flipping the whole 4-module matrix strip 180° in its mount — not a
   firmware/`HARDWARE_TYPE` issue in the end, though that's the other
   axis to check if this recurs (see README's "Display orientation").
   `HARDWARE_TYPE` settled on `FC16_HW` (the original default).
3. **Text cut off / shifted horizontally**: was actually a symptom of
   the physical flip above being the real fix; a pixel-shift hack was
   tried and reverted (not needed once oriented correctly).
4. **Intermittent garbling during scrolling only (never during the
   static clock)**: root-caused to the MD_MAX72XX library's hardcoded
   8MHz SPI clock being too fast for the jumper-wire run. Fixed by
   vendoring the library into `lib/MD_MAX72XX` with the clock patched to
   1MHz (see README's "Known-good SPI clock speed" — this patch is easy
   to lose if the library is ever re-fetched from the registry instead).
5. **Countdown text format**: changed from "PrayerName in H:MM:SS" to
   "PrayerName: X min" per user preference (also sidestepped an early,
   ultimately-misdiagnosed theory that the colon glyph itself was
   causing corruption — it wasn't; see point 4).
6. **Countdown mode restarting/never finishing its scroll**: two
   compounding bugs, both fixed:
   - Mode switching used to flip on a fixed `DISPLAY_SWITCH_MS` timer
     even for the scrolling countdown, cutting it off mid-pass every
     cycle. Fixed by switching modes based on scroll-completion
     (`displayAnimateScroll()` returning `true`) instead, for the
     countdown side specifically; the clock face still uses the fixed
     timer.
   - A dangling-pointer bug: MD_Parola's `displayText()` stores a raw
     pointer into the string you pass it rather than copying it, so a
     function-local `String` went out of scope before the scroll
     finished reading it. Fixed with a file-scope
     `static String countdownText` in `main.cpp`. This bug likely
     contributed to some of the earlier "intermittent garbling" before
     it was isolated — worth remembering if similar corruption
     resurfaces after touching the scrolling code path.

## What's built

- `src/main.cpp` — WiFi connect, NTP time sync, mode-switching loop
  (static clock for a fixed duration ⟷ scrolling countdown until its
  scroll completes).
- `src/prayer_times.cpp` — scrapes `muaadhbinjabal.org.za` homepage
  (no JSON API exists) by locating each prayer's HTML block via a class
  marker and reading the Azaan/Salaah `HH:MM` pair out of it.
- `src/display.cpp` — thin wrapper around MD_Parola for the 4x MAX7219
  8x8 chain.
- `include/config.h` — pins, timezone, refresh intervals, brightness,
  hardware type.
- `include/secrets.h` — WiFi credentials (gitignored; `secrets.h.example`
  is the template).
- `lib/MD_MAX72XX` — vendored, patched copy of the display driver
  library (1MHz SPI clock instead of upstream's 8MHz). Not fetched from
  the PlatformIO registry — see README.

## Next steps

1. **Push to GitHub.** Remote is configured; auth isn't. Easiest path:
   generate a personal access token at
   https://github.com/settings/tokens/new (`repo` scope) and either
   paste it for a one-off push, or set it up via `git credential` /
   GitHub Desktop for ongoing use. `gh auth login`'s device flow has
   not worked reliably in this sandboxed environment.
2. Sanity-check the scraped prayer times against what's currently shown
   on https://www.muaadhbinjabal.org.za — the scraper depends on the
   site's exact HTML structure and will silently need updating if the
   site is redesigned.
3. Cosmetic/optional, from `INVENTORY.md`: diffuser material and an
   enclosure haven't been sourced yet. Purely visual, not blocking.
4. If corruption or misbehavior reappears after future changes, check
   the debugging history above first — several of these symptoms look
   superficially similar (garbling, cutoff, wrong orientation) but had
   unrelated root causes.

## Known limitations (by design, not bugs)

- TLS cert validation is skipped (`setInsecure()`) — acceptable since
  the fetched data isn't sensitive.
- Jumu'ah time is scraped but unused; countdown only covers the 5 daily
  prayers.
- After Isha, the countdown to tomorrow's Fajr uses *today's* Fajr time
  as an approximation until the next scheduled scrape (every 30 min,
  and forced right after local midnight) picks up the real value.
