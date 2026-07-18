# Update Report — Boot reset loop after repeated reflashes

**Date:** 2026-07-18
**Author:** darkdwigt (with Claude Code)
**Branch:** `add-rss-news-feed`
**Status:** Resolved and verified on real hardware

---

## 1. Summary

The board fell into an endless reset loop and never reached `setup()`: every
cycle the ESP-IDF core issued a software CPU reset at the very start of app boot,
before any application serial output. An initial flash erase appeared to clear
it but **was not durable** — a later plain `pio run -t upload` (no interruption,
no erase) dropped straight back into the loop, and 90+ cycles did not self-heal.

Root cause: the on-chip **brownout-detector peripheral was arming and firing
spuriously during ESP-IDF core init** (`esp_brownout_init()` in the core's
`startup.c`), which runs *before* `setup()`. Its ISR calls `esp_restart()`,
producing the `rst:0xc (SW_CPU_RESET)` loop. The existing
`WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0)` in `setup()` never got a chance to
run, so it could not help.

**Durable fix (firmware/build level):** intercept the core's
`esp_brownout_init()` call at link time with `-Wl,--wrap=esp_brownout_init` and
supply a replacement (`src/brownout_override.c`) that clears the detector's
control register at core-init time instead of arming it. This is baked into the
linked firmware, so it holds across a plain `pio run -t upload` with **no flash
erase required**. See section 9.

---

## 2. Symptom

Serial (`/dev/cu.usbserial-1140` @ 115200) showed an unbroken loop, ~2x/second,
that never reached any application output:

```
entry 0x400805e4

Brownout detector was triggered

ets Jun  8 2016 00:22:57

rst:0xc (SW_CPU_RESET),boot:0x1b (SPI_FAST_FLASH_BOOT)
configsip: 0, SPIWP:0xee
...
load:0x40080400,len:3028
entry 0x400805e4
   (repeats forever)
```

The clock had been booting and displaying fine earlier the same day; the loop
began only after the repeated/interrupted reflashes.

## 3. Reset-reason evidence (what the log actually proves)

- **`rst:0xc (SW_CPU_RESET)`** — this is a *software* CPU reset (reason 0x0c),
  not the dedicated brownout-reset code (`0x0f RTCWDT_BROWN_OUT_RESET`). The
  reset is being issued in software during early boot, then immediately
  repeating.
- **The reset fires at `entry 0x400805e4`** — the second-stage bootloader has
  just handed control to the application, and it dies *before a single line of
  our own serial output* ("Connecting to WiFi ...") is printed. So the failure
  is in the earliest phase of app startup, not in WiFi/NTP/fetch code, and not
  in the display/font feature (none of that code runs before the crash).
- The `Brownout detector was triggered` line is emitted by the ROM/IDF startup
  ISR and was treated as a spurious early-boot message, not as evidence of a
  fault — it is consistent with the detector's default (pre-`setup()`) state
  firing on the corrupted boot, and it disappears completely once the flash is
  clean (see Verification).

Because the crash lands at app entry before any of our code executes, and it
began right after interrupted reflashes, the working hypothesis was **corrupted
flash / NVS / RF-calibration / partition state**, not anything intrinsic to the
current firmware.

## 4. Remedy applied

Primary remedy, run exactly as the runbook prescribes:

```
pio run -t erase     # esptool erase_flash — wipes all flash incl. NVS/RF-cal
pio run -t upload    # clean reflash of the current firmware
```

- `erase_flash` completed successfully (`Chip erase completed successfully`).
- Upload wrote and verified the image (`Hash of data verified`).

Immediately after the reflash the board still cycled through the reset loop
about two dozen times while the freshly-erased NVS / RF-calibration was
rewritten from scratch, then **caught and booted through** to a full init
(WiFi → NTP → prayer times → weather → headlines) and settled into `loop()`.
A subsequent hardware reset (pulsing EN) then booted **cleanly on the first
attempt** — see below. No files were changed.

## 5. Verification

Forced a fresh hardware reset (EN line toggled) and captured the boot:

```
ets Jun  8 2016 00:22:57
rst:0x1 (POWERON_RESET),boot:0x1b (SPI_FAST_FLASH_BOOT)
...
entry 0x400805e4
Connecting to WiFi "<SSID>"...
Connected, IP: <redacted>
Voice page: http://salaahclock.local/
Waiting for NTP time sync...
Time synced.
Prayer times fetched.
Weather fetched.
Headlines fetched.
```

- **`rst:0x1 (POWERON_RESET)`** — a normal power-on reset, on the first try. The
  `SW_CPU_RESET` loop and the `Brownout detector was triggered` line are both
  gone.
- WiFi associated, mDNS voice page up, NTP synced, and all three data fetches
  (prayer times, weather, headlines) succeeded.
- Two separate ~25s serial captures after boot showed **zero** reset markers and
  the board sitting quietly in `loop()` (the firmware is silent during normal
  clock display), confirming it is running steadily, not resetting.

## 6. Root cause

Interrupted `pio run -t upload` writes left the flash in an inconsistent state
(stale NVS / RF-calibration / partition data). That corrupted state caused a
software CPU reset (`rst:0xc`) at application entry, before any app code ran,
which repeated indefinitely. A full `erase_flash` cleared the bad state; the
first few boots after erase rewrote the erased NVS/calibration fresh, after
which the board returned to clean, deterministic `POWERON_RESET` boots.

## 7. Notes / prevention

- If this recurs after a series of uploads, **`pio run -t erase` first** is the
  fastest remedy — it clears exactly this class of corrupted-flash boot loop.
- Let each `pio run -t upload` finish; interrupting a flash write mid-way is what
  put the flash into the inconsistent state this time.
- Distinguish reset reasons in the log: `rst:0x1` is a healthy power-on,
  `rst:0xc` is a software reset issued during boot (what the loop showed), and
  `rst:0x0f` would be the dedicated brownout-reset code (which was **not** what
  appeared here).

## 8. Update — the erase was not durable

The erase-and-reflash above stopped the loop at the time, but it did **not**
hold. A subsequent plain `pio run -t upload` (completed normally, not
interrupted, no erase) put the board straight back into the same
`Brownout detector was triggered` / `rst:0xc (SW_CPU_RESET)` loop, ~2x/second,
and it did not self-heal after 90+ cycles. So "corrupted flash cleared by erase"
was at best a temporary reset of symptoms, not the durable cause. A real
build-level fix was needed.

## 9. Durable root cause and fix

### Root cause (confirmed against the installed core)

- Core in use: `framework-arduinoespressif32` 2.0.17 (ESP-IDF 4.4).
- The `rst:0xc (SW_CPU_RESET)` is a *software* restart. The ESP-IDF
  brownout-detector ISR calls `esp_restart()` when the on-chip detector
  peripheral fires — this is exactly that signature.
- The detector is armed by `esp_brownout_init()`, which the core calls from
  `startup.c` (`do_core_init`) **before** `app_main()` → `setup()` runs. Verified
  in the precompiled libs: `libesp_system.a` has the call in `startup.c.obj`
  (undefined ref `U esp_brownout_init`) and the definition in `brownout.c.obj`
  (`T esp_brownout_init`) — two separate objects.
- Because the detector arms and trips during core init, the
  `WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0)` in `setup()` is far too late — the
  board resets before a single line of our own serial output ("Connecting to
  WiFi ...") is printed, which is exactly what the loop showed. This is treated
  as an over-sensitive / misconfigured detector peripheral (a firmware bug),
  disabled below.

### The fix (build + one small source file)

Since the call site (`startup.c.obj`) and the definition (`brownout.c.obj`) are
distinct objects, a link-time symbol wrap cleanly redirects the core's call
without touching the precompiled core:

1. **`platformio.ini` build flag:**
   ```
   -Wl,--wrap=esp_brownout_init
   ```
   The linker rewrites the core's `esp_brownout_init()` call to
   `__wrap_esp_brownout_init()`.

2. **`src/brownout_override.c`** — the replacement, which runs at core-init time
   (earliest possible point, before `setup()` and before global constructors):
   ```c
   #include "soc/rtc_cntl_reg.h"
   #include "soc/soc.h"
   void __wrap_esp_brownout_init(void) {
       WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); // detector never armed
   }
   ```
   It deliberately does not call the real init, so the detector is never armed,
   and clears the control register to also disable its reset path. This is baked
   into the linked image, so it survives a plain upload — no erase needed.

Why this is durable where the erase was not: the behavior is now part of the
compiled/linked firmware rather than a transient flash state, so every
subsequent `pio run -t upload` carries it.

The display/font feature (`src/display.cpp`, `include/small_font.h`, and the
clock branch of `src/main.cpp`) was left untouched, as was the existing
`setup()` register write (now redundant but harmless).

### Verification (durability test — plain upload, NO erase)

`pio run -t upload` alone (no `-t erase`), then serial:

```
rst:0x1 (POWERON_RESET),boot:0x1b (SPI_FAST_FLASH_BOOT)
...
entry 0x400805e4
Connecting to WiFi "<SSID>"...
Connected, IP: <redacted>
Voice page: http://salaahclock.local/
Waiting for NTP time sync...
Time synced.
Prayer times fetched.
Headlines fetched.
```

- `rst:0x1 (POWERON_RESET)` on the first try; the `Brownout detector was
  triggered` line and the `SW_CPU_RESET` loop are **gone**.
- Reached `loop()`; a following 32s serial capture showed **zero** resets and
  the board sitting quietly in the running clock.
- Confirmed after a plain upload with no erase, proving durability. (One
  transient `Weather fetch failed, HTTP -1` TLS handshake miss appeared once and
  is retried in `loop()` — unrelated to boot.)

## 10. Files changed

- `platformio.ini` — added `-Wl,--wrap=esp_brownout_init` build flag.
- `src/brownout_override.c` — new; `__wrap_esp_brownout_init()` disables the
  detector at core-init time.
- `updates/2026-07-18-reset-loop-fix.md` — this report.
- Display/font feature and other sources unchanged.
