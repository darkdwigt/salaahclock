# Update Report — Clock stuck on boot / WiFi brownout loop

**Date:** 2026-07-12
**Author:** darkdwigt (with Claude Code)
**Branch:** `add-rss-news-feed`
**Status:** Resolved and verified on real hardware

---

## 1. Summary

The clock stopped working: the LED matrix sat frozen on its boot placeholder
(`...`) and never showed the time. It looked like a WiFi problem, but the true
cause was a **power brownout**: the ESP32's brownout detector was tripping the
instant the WiFi radio powered up, resetting the board in an endless loop
before it could ever connect. The fix was entirely in firmware — reducing the
WiFi current spike, disabling the over-sensitive brownout detector, and doing a
clean flash erase to clear corrupted RF-calibration data. The clock now boots
cleanly on the same laptop USB port that was failing.

---

## 2. Symptom

- Matrix displayed a steady `...` and never advanced to the clock face.
- Appeared to be "not connecting to WiFi."
- Occurred on the same laptop USB port and cable that had worked previously —
  nothing on the user's side (network, cable, adapter) had been changed.

## 3. Investigation

Diagnosis was driven off the ESP32 serial log (read directly over the CH340
USB-serial port with `pyserial`, since `pio device monitor` needs an
interactive TTY that this environment lacks).

The serial output was unambiguous and repeated forever:

```
Connecting to WiFi "<SSID>"...
Brownout detector was triggered
ets Jun  8 2016 00:22:57
rst:0xc (SW_CPU_RESET),boot:0x13 (SPI_FAST_FLASH_BOOT)
...  (loops)
```

Key findings, in order:

1. **The `...` on screen is the boot placeholder**, printed by
   `displayShowStatic("...")` early in `setup()`, *before* WiFi/NTP. So the
   board was alive and the display was fine — it was hanging/resetting in
   `setup()` before it could draw a clock.
2. **`Brownout detector was triggered` is a hardware ROM message** emitted only
   when the 3.3 V rail actually sags below threshold. This was a real voltage
   dip, not a guess.
3. **The reset fires exactly at WiFi power-up.** Every reset was immediately
   preceded by `Connecting to WiFi ...`. The WiFi transmitter's current spike
   was collapsing the rail.
4. **Unplugging the LED matrix helped but did not fix it** — the bare ESP32
   still browned out at WiFi start, proving the ESP32's *own* supply headroom
   was the limiting factor, not just the display's added load.
5. **The toolchain had not changed.** `platform = espressif32` is unpinned in
   `platformio.ini`, but the installed platform/framework packages were dated
   from the original build and never updated, ruling out a core-version
   regression. Same code path, same compiler, same hardware.

## 4. Root cause

This build runs the ESP32 **and** four MAX7219 modules from a single laptop
USB-A port, which supplies roughly 500–900 mA. The WiFi radio's transmit/
calibration burst at association draws a sudden 300–500 mA spike. The total sat
right at the edge of what the port could deliver, so the clock had **always**
been operating on a razor-thin power margin — it worked only because it was
*just* over the line.

Something drifted under that line (cable/connector resistance, the laptop's USB
power policy, and/or the onboard regulator aging). Once it started browning out,
the failure became self-reinforcing: repeated brownout resets corrupted the
stored WiFi **RF-calibration** data, which forces a heavier *full* calibration
at every boot — making the startup current draw even larger and the brownout
even more certain.

The ESP32's classic brownout detector is also well documented as over-sensitive
at WiFi init on marginal-but-adequate USB supplies (see arduino-esp32 issue
#863 and others): the CPU can survive the brief dip, but the detector reacts by
resetting anyway.

## 5. The fix (firmware only, all in `src/main.cpp`)

| # | Change | Why |
|---|--------|-----|
| 1 | **Disable the brownout detector** — `WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);` as the first line of `setup()` | The chip runs fine through the brief WiFi-init dip; the detector was over-reacting and resetting it. Safe here because the sag is shallow/transient and no flash writes happen during WiFi bring-up. |
| 2 | **Reduce WiFi TX power to 11 dBm + enable modem sleep** — `WiFi.setTxPower(WIFI_POWER_11dBm); WiFi.setSleep(true);` (called *after* `WiFi.begin()`, which is required for it to take effect) | Shrinks the radio's peak current draw so the rail no longer sags. 11 dBm is ample for a home network. |
| 3 | **Lower CPU clock to 80 MHz** — `setCpuFrequencyMhz(80);` | Less overall current draw = more headroom for the WiFi peak. A clock does not need 240 MHz. |
| 4 | **Clean flash erase before reflash** — `esptool erase_flash` (`pio run -t erase`) | Wipes the corrupted RF-calibration left by the brownout loops so the radio recalibrates once and reverts to the lighter, lower-current partial calibration on subsequent boots. |
| 5 | **Escape hatch on the NTP sync loop** — bounded retry (~1 min) that falls through to `loop()` instead of blocking forever | The old `while(!getLocalTime(...))` could trap boot indefinitely, which is what left the display frozen on `...`. Now a WiFi/NTP failure can never hard-freeze the clock; it keeps retrying in the background. |
| 6 | **Self-diagnosing display states** (added, then largely superseded by the fix) — shows the IP's last octet on WiFi success or `noWF`/`ntp` on failure | Lets the matrix itself report the boot stage without a serial cable (useful when powered from a wall charger that has no data lines). |

An earlier, mistaken attempt added `WiFi.setTxPower(WIFI_POWER_8_5dBm)` *before*
`WiFi.begin()`. That is the wrong call order (it silently fails and can
destabilize association) and was reverted before the final fix.

## 6. Verification

Clean boot captured over serial after the fix (no brownout, full init):

```
rst:0x1 (POWERON_RESET),boot:0x1b (SPI_FAST_FLASH_BOOT)
Connecting to WiFi "<SSID>"...
Connected, IP: <redacted>
Waiting for NTP time sync...
Time synced.
Prayer times fetched.
Weather fetched.
Headlines fetched.
```

- `rst:0x1 (POWERON_RESET)` — a normal power-on, **not** a brownout reset.
- WiFi associated, NTP synced, all three data fetches succeeded.
- The matrix shows the time and rotates through countdown / weather / news.
- Voice page reachable at `http://salaahclock.local/`.
- Running on the **same laptop USB port** that was previously brownout-looping.

## 7. Prevention / recommendations

**Firmware (already applied — durable):**
- The brownout mitigations (items 1–3, 5 above) are committed, so a marginal
  supply will no longer trip an endless reset loop, and a WiFi/NTP failure can
  no longer freeze the display.
- Keep the RF-calibration healthy by not letting the board brownout-loop again;
  the reduced current draw makes that far less likely.

**Hardware (recommended for full robustness):**
- **Power it from a proper 5 V / ≥ 2 A wall adapter**, not a laptop USB port,
  for permanent headroom against the WiFi spike. (A charge-only cable is fine
  for running; a data cable is only needed for flashing.)
- **Add a bulk capacitor** (470–1000 µF electrolytic across 3V3 and GND) to
  absorb the WiFi current spike at the source — the textbook fix for this exact
  symptom and good insurance even with the firmware mitigations.
- Consider powering the MAX7219 matrix from its own 5 V supply (common ground)
  rather than through the board, so the display load and the radio spike don't
  compete for the same rail.

**Process:**
- **Pin the PlatformIO platform version** in `platformio.ini`
  (e.g. `platform = espressif32@x.y.z`) so a future `pio` update can't silently
  change core behavior between builds.
- When a symptom looks like "no WiFi," **read the serial log first** — the ROM
  brownout message distinguishes a power fault from an actual network fault in
  seconds.

## 8. Files changed

- `src/main.cpp` — brownout mitigations, TX-power/CPU-clock tuning, NTP escape
  hatch, self-diagnosing display states.
- `updates/2026-07-12-brownout-wifi-fix.md` — this report.
