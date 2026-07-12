# AGENTS.md

Guide for anyone (human or AI agent) picking up work on a specific part
of this project. `CLAUDE.md` has current status and history; `README.md`
has setup/wiring/architecture. This file is a map of *where to work* for
a given kind of change, plus the gotchas specific to each area.

## Ponytail, lazy senior dev mode

You are a lazy senior developer. Lazy means efficient, not careless. The best code is the code never written.

Before writing any code, stop at the first rung that holds:

1. Does this need to be built at all? (YAGNI)
2. Does it already exist in this codebase? Reuse the helper, util, or pattern that's already here, don't re-write it.
3. Does the standard library already do this? Use it.
4. Does a native platform feature cover it? Use it.
5. Does an already-installed dependency solve it? Use it.
6. Can this be one line? Make it one line.
7. Only then: write the minimum code that works.

The ladder runs after you understand the problem, not instead of it: read the task and the code it touches, trace the real flow end to end, then climb.

Bug fix = root cause, not symptom: a report names a symptom. Grep every caller of the function you touch and fix the shared function once — one guard there is a smaller diff than one per caller, and patching only the path the ticket names leaves a sibling caller still broken.

Rules:

- No abstractions that weren't explicitly requested.
- No new dependency if it can be avoided.
- No boilerplate nobody asked for.
- Deletion over addition. Boring over clever. Fewest files possible.
- Shortest working diff wins, but only once you understand the problem. The smallest change in the wrong place isn't lazy, it's a second bug.
- Question complex requests: "Do you actually need X, or does Y cover it?"
- Pick the edge-case-correct option when two stdlib approaches are the same size, lazy means less code, not the flimsier algorithm.
- Mark intentional simplifications with a `ponytail:` comment. If the shortcut has a known ceiling (global lock, O(n²) scan, naive heuristic), the comment names the ceiling and the upgrade path.

Not lazy about: understanding the problem (read it fully and trace the real flow before picking a rung, a small diff you don't understand is just laziness dressed up as efficiency), input validation at trust boundaries, error handling that prevents data loss, security, accessibility, the calibration real hardware needs (the platform is never the spec ideal, a clock drifts, a sensor reads off), anything explicitly requested. Lazy code without its check is unfinished: non-trivial logic leaves ONE runnable check behind, the smallest thing that fails if the logic breaks (an assert-based demo/self-check or one small test file; no frameworks, no fixtures). Trivial one-liners need no test.

## Firmware / control flow — `src/main.cpp`

Owns WiFi connection, NTP sync, and the clock ⟷ countdown mode switch.

- The clock face and the scrolling countdown are switched by two
  different mechanisms: the clock uses a fixed timer
  (`DISPLAY_SWITCH_MS`), the countdown switches back only when its
  scroll animation completes a full pass
  (`displayAnimateScroll()` returning `true`). Don't collapse these back
  into one fixed timer — that was tried and cut the scroll off mid-pass
  every cycle.
- `countdownText` is a file-scope `static String`, not a local variable,
  because MD_Parola keeps a raw pointer into whatever string you hand
  it and reads from that pointer for the whole scroll duration. If you
  change how the countdown text is built, keep it in a variable that
  outlives the mode-switch block, or the scroll will silently look like
  it "finishes" almost instantly (dangling pointer read as an empty
  string).
- `secondsToNextPrayer()` / `formatCountdown()` are where to change the
  countdown logic or wording (currently "PrayerName: X min").

## Display driver — `src/display.cpp`, `include/display.h`

Thin wrapper around MD_Parola. Keep it thin — mode-switching and text
logic belong in `main.cpp`, not here.

- `HARDWARE_TYPE` and `CS_PIN`/`MAX_DEVICES` live in `include/config.h`,
  not here.
- If you touch scrolling behavior, remember Parola's animation
  completes an IN effect, a pause, then an OUT effect before
  `displayAnimate()` returns `true` — both effects are currently
  `PA_SCROLL_LEFT`.

## Vendored library — `lib/MD_MAX72XX`

A patched copy of the MD_MAX72XX driver, **not** fetched from the
PlatformIO registry (see the comment in `platformio.ini`). The patch
lowers the SPI clock from the upstream 8MHz to 1MHz in
`MD_MAX72xx.cpp`'s `spiSend()` — needed for reliable scrolling text over
this build's jumper-wire connections. If you ever need to update this
library:

1. Re-apply the same clock-speed patch after updating.
2. Don't re-add `majicdesigns/MD_MAX72xx` to `platformio.ini`'s
   `lib_deps` — a project `lib/` copy takes priority over a registry
   fetch, but having both is confusing and PlatformIO will still fetch
   the unpatched one into `.pio/libdeps` (harmless but wasteful).

## Prayer time scraping — `src/prayer_times.cpp`, `include/prayer_times.h`

Scrapes `muaadhbinjabal.org.za`'s homepage HTML directly (no JSON API
exists). Locates each prayer's block via a class-name marker and reads
the Azaan/Salaah `HH:MM` pair out of it.

- This is the most fragile part of the project long-term: it depends on
  exact HTML structure that could change without notice on a site this
  project doesn't control. If prayer times ever look wrong, check the
  live site's markup against the parsing logic here first.
- Jumu'ah is parsed but intentionally unused (see `CLAUDE.md`'s known
  limitations) — don't wire it into the countdown without checking with
  the user first, since Friday prayer scheduling/logic may differ from
  the 5 daily prayers.

## Hardware / wiring / physical build

Not a code area, but worth flagging for whoever's debugging a "nothing
shows up" or "text looks wrong" report:

- **Blank display or all-LEDs-on**: almost always a physical connection
  problem (bad solder joint, jumper not fully seated), not firmware.
  Check DIN/CLK/CS continuity before touching code.
- **Wrong text orientation**: two independent causes, either of which
  can look like the other — physical mounting rotation of the matrix
  strip, or a `HARDWARE_TYPE` mismatch in `include/config.h`. See
  README's "Display orientation" section.
- **Garbling only during scrolling, never during the static clock**:
  historically an SPI clock speed issue (see `lib/MD_MAX72XX` above),
  not a wiring issue — don't jump straight to "check your connections"
  without first confirming whether the static clock ever glitches too.

## Voice Q&A — `src/web_ui.cpp`, `src/ai_query.cpp`

- Voice capture is browser-side (Web Speech API), not on-device — the
  ESP32 has no mic. Don't try to add on-device audio capture without
  checking with the user first; that's a separate, much bigger hardware
  project (I2S mic module).
- `ai_query.cpp` calls Pollinations.ai's keyless text API specifically
  because the user asked for free-with-no-signup. Don't swap in
  Groq/OpenAI/xAI Grok without checking — those need an API key/account,
  which was explicitly ruled out.
- `web_ui.cpp` uses the ESP32 core's bundled `WebServer`/`ESPmDNS` —
  don't add ArduinoJson or another HTTP library for this; the `/ask`
  endpoint deliberately takes a raw `text/plain` body (`server.arg
  ("plain")`) instead of JSON to avoid the dependency.
- `askAI()` runs synchronously inside the `/ask` HTTP handler, blocking
  that request for a few seconds. This matches the existing blocking
  fetch pattern for weather/RSS/prayer times elsewhere in the codebase
  — don't introduce async/RTOS tasking just for this.
- See `CLAUDE.md`'s "Voice Q&A feature" section for the full request
  flow and its real-hardware test status.

## Config / tunables — `include/config.h`

Pins, timezone, refresh intervals, brightness, hardware type, all in
one place. Check here before assuming something needs a code change —
most physical/behavioral tuning (brightness, refresh cadence, display
timing) is a constant here, not logic elsewhere.

## Repo / infra

- `secrets.h` is gitignored; never commit real WiFi credentials.
  `secrets.h.example` is the template new setups should copy.
- Remote is `https://github.com/darkdwigt/salaahclock.git`, added but
  not yet authenticated for push from this environment — see
  `CLAUDE.md`'s next steps for the current blocker.
