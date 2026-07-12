# Feature drafts (not yet implemented)

Ideas discussed but parked for later. Revisit and flesh out before building.

## RSS news headline mode

Add a third display mode alongside the clock and prayer countdown, cycling
in the same round-robin the loop already uses (clock → countdown →
headline → clock → …), so it never crowds out the time or countdown.

Key design points agreed so far:
- RSS mode gets a **fixed slot frequency** in the rotation (e.g. 1-in-4
  cycles), independent of how many headlines the feed has that day — a
  slow news day shouldn't starve it of airtime, a busy day shouldn't flood
  the rotation.
- **Headline selection: round-robin** through the fetched item list, one
  headline per RSS turn, advancing an index (mod list length) each time.
  Preferred over "always show newest" (repetitive if fetch is slower than
  rotation) or "newest-unseen" (needs more state to track).
- Each RSS turn scrolls exactly one headline to completion, same pattern
  as `displayAnimateScroll()` returning `true` for the countdown.

Open questions: which feed URL (blocked on this - user still deciding),
fetch interval, whether to skip headline mode during prayer-adjacent time
windows. Once a URL is picked, extend the clock's mode-toggle in
`main.cpp` (currently a `nextIsWeather` bool alternating countdown/weather)
into a mod-3 rotation so countdown/weather/headline each get an even,
fixed-frequency slot the same way countdown/weather do today.

## Custom messages via WiFi

Idea: push an ad-hoc text message (e.g. "Happy birthday") to the display
remotely. Two architectures discussed, not yet decided - user is
researching:

- **Push**: ESP32 runs a small web server; POST a message directly (e.g.
  `http://salaahclock.local/message`) and it displays instantly. Only
  reachable on the home LAN without port-forwarding/tunneling. Would be
  the first *inbound* listener in this project - everything else
  (prayer times, weather) is outbound-only fetches.
- **Pull**: ESP32 polls a small external endpoint (Google Sheet cell,
  Gist, hosted JSON file) on an interval for a pending message, same
  pattern as `prayer_times`/`weather`. Works from anywhere, no inbound
  port needed, but has fetch-interval lag and needs somewhere to host/
  edit the message.

Leaning consideration: pull fits the existing all-outbound architecture;
push gives instant delivery. Revisit once the user has a preference.

## Weather forecast (wttr.in) — done, 2026-07-04

Implemented: `include/weather.h` / `src/weather.cpp` (mirrors the
`prayer_times` pattern), wired into `main.cpp` as `MODE_WEATHER`, third
mode in the rotation. Builds clean (`pio run`).

- Location: IP-geolocated (no city in the URL) — `https://wttr.in/?format=...`,
  per user preference over hardcoding a city.
- Format string `%C+%t` (condition + temp) via `WEATHER_FORMAT` in
  `config.h`; response is stripped of the UTF-8 degree symbol
  (`0xC2 0xB0`) since it's not in the MAX7219 font.
- Request sends `User-Agent: curl/8.0` — wttr.in serves the plain-text
  one-liner only to curl-like clients, otherwise returns an HTML page.
- Refetched hourly (`WEATHER_FETCH_INTERVAL_MS` in `config.h`), same
  stale-while-refreshing behavior as prayer times (keeps last known value
  on fetch failure).
- Mode rotation: clock face alternates which scrolling mode follows it
  (`nextIsWeather` toggle in `main.cpp`) — countdown, weather, countdown,
  weather, … — so both get even airtime regardless of how often either
  refreshes. If a mode's data isn't valid yet, `enterMode()` falls
  straight back to the clock instead of scrolling something empty.

Tested on real hardware 2026-07-04: WiFi connect, NTP sync, prayer times,
and weather (`"Clear +16C"` observed) all fetched successfully; mode
rotation confirmed clock → countdown → clock → weather → clock → …
Also fixed a pre-existing bug found during this test: if WiFi didn't
connect within `connectWiFi()`'s 20s window in `setup()`, the board hung
forever in the NTP-wait loop with no retry path. Fixed by calling
`connectWiFi()` again inside that loop when `WiFi.status() != WL_CONNECTED`.

## Weather forecast v2 (high/low + rain/wind callouts) — in progress, 2026-07-05

Replacing the current-conditions weather mode above with a forecast: high/
low for the day, plus independent rain and wind callouts. Not yet
implemented — decisions so far:

- **Data source: Open-Meteo** (`api.open-meteo.com/v1/forecast`), not
  wttr.in's JSON mode. Reasoning: Open-Meteo's daily endpoint returns just
  `temperature_2m_max`, `temperature_2m_min`, `precipitation_sum`, and
  `wind_speed_10m_max` — small, purpose-built JSON, easy to parse on the
  ESP32. wttr.in's `?format=j1` would also work but returns a much larger
  general-purpose payload requiring an ArduinoJson filter document to
  extract just the needed fields — more code, more RAM, for the same
  result. Trade-off accepted: Open-Meteo needs a hardcoded lat/lon instead
  of wttr.in's IP-geolocation, so the "no city in the URL" preference from
  v1 doesn't carry over as-is — lat/lon will need to be looked up once and
  set in `config.h`.
- **Display format** (leaning, not finalized): `H:24 L:12` normally, with
  independent optional callouts appended — e.g. `H:22 L:14 RAIN` and/or
  `H:24 L:12 WIND` — rather than always printing a condition word. No cold
  callout wanted (the low temp itself already conveys that).
- **No degree symbol / emoji** — same MAX7219 font constraint as v1.
- **Rain and wind callouts must be independent** — a rain label and a wind
  label are separate conditions, never combined into one joint label (e.g.
  no single "storm" that means both heavy rain and high wind).
- **Thresholds — not yet chosen.** Explored options:
  - Generic rain scale (daily total, India Met Dept-style, commonly cited
    internationally since it matches Open-Meteo's daily mm figure better
    than hourly-rate scales): gentle/light 1–15mm, heavy 15mm+.
  - Generic wind scale: Beaufort. Light <20 km/h, moderate 20–38 (left
    unflagged), heavy 39–61, storm 62+ km/h (Beaufort 8+).
  - Checked whether South Africa has its own standard: SAWS uses an
    "Impact-Based Severe Weather Warning System" (levels 1–10,
    yellow/orange/red) driven by a likelihood × impact matrix, not a fixed
    mm/km-h cutoff — not directly usable as a hardcoded threshold. One
    concrete SAWS number found: "damaging winds" warnings start around 35
    knots (~70 km/h), higher than generic Beaufort's 62 km/h storm cutoff.
    No official fixed SAWS mm/day figure for heavy rain surfaced.
  - Checked for an official SAWS-backed API as an alternative to
    Open-Meteo: **AfriGIS Weather API** resells SAWS forecast/lightning/
    storm data, but requires emailing them for a manual "free trial"
    signup (no self-serve key) and uses OAuth2 — too much friction/
    unknowns (no public pricing) for this project. Decided to stick with
    Open-Meteo.
  - **Not yet decided**: final numeric cutoffs to hardcode (generic scales
    above vs. anchoring the wind "heavy/storm" line to SAWS's ~70 km/h
    figure instead of Beaufort's 62). Revisit before implementing.

## AI Q&A mode via local web page — idea only, 2026-07-05

Idea: add a mode where a question typed into an HTML page (served on the
home network, usable from any computer/phone on it) gets sent to a free
AI API, and the answer scrolls on the display. Not started — architecture
not yet decided:

- **Where the web page + AI call live** — two options discussed:
  - ESP32 hosts the page itself and calls a free AI API (e.g. Groq's free
    tier, OpenAI-compatible) directly. Single device, but adds TLS/memory
    load to an already-busy microcontroller.
  - A small companion server (script on a PC/Pi already on the network)
    hosts the HTML page and makes the AI call, then pushes just the short
    final answer to a tiny local endpoint on the ESP32. Less load on the
    microcontroller, easier to iterate, but needs another machine running.
  - Leaning towards the companion-server option since a personal machine
    is already on the network, but not decided.
- **Display constraint**: the 32x8 LED matrix can only scroll short text,
  so whichever approach is used, the AI must be instructed (system prompt)
  to keep answers to roughly a sentence or less to stay legible.
- Overlaps with the "custom messages via WiFi" idea above (inbound
  push vs. outbound pull) — if that one lands on a push-style local web
  server first, this could reuse the same inbound-listener plumbing.
