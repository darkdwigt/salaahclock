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
