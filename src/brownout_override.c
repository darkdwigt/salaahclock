// Link-time override of the ESP-IDF core's brownout-detector init.
//
// The stock arduino-esp32 core calls esp_brownout_init() from startup.c
// (do_core_init) during ESP-IDF bring-up, BEFORE app_main() -> setup() runs.
// On this board that on-chip brownout-detector peripheral trips spuriously the
// instant it is armed and its ISR issues an esp_restart(), which shows up as
// "Brownout detector was triggered" + rst:0xc (SW_CPU_RESET) on an endless loop
// that never reaches any application code. Because it fires during core init,
// disabling the detector from inside setup() is too late.
//
// platformio.ini adds `-Wl,--wrap=esp_brownout_init`, so the linker redirects
// the core's startup call here. We deliberately do NOT arm the detector; we
// clear its control register instead, which disables both the detector and its
// reset path. This runs at the earliest possible point (core init) and is
// baked into the linked firmware, so it survives a plain `pio run -t upload`
// without needing a flash erase. __real_esp_brownout_init (the original) is
// intentionally never called.
#include "soc/rtc_cntl_reg.h"
#include "soc/soc.h"

void __wrap_esp_brownout_init(void) {
    // Writing 0 clears RTC_CNTL_BROWN_OUT_ENA and RTC_CNTL_BROWN_OUT_RST_ENA,
    // leaving the detector off and unable to reset the chip.
    WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);
}
