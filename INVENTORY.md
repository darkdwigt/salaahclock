# Hardware inventory

Status as of 2026-07-04. Core hardware arrived, soldered, wired, and
confirmed working end-to-end (see `CLAUDE.md`).

## In hand (received, soldered/wired, confirmed working)

- [x] LED Dot Matrix Display Module — 8x8 x4, MAX7219 driver (chained
      4-in-1 board)
- [x] FireBeetle ESP32 IoT Microcontroller — header pins soldered
- [x] Female-to-Female SIL jumper wires — 40-wire ribbon, 10 repeating
      colours
- [x] USB cable matching the FireBeetle's port — confirmed working for
      flashing and serial (`/dev/cu.usbserial-140`)

## Still need to buy (cosmetic only, not blocking)

- [ ] 5V USB power supply / wall adapter (or power bank) — only needed
      if the clock will run standalone away from a laptop
- [ ] Diffuser material (thin white acrylic, tracing paper, or similar)
      — optional, makes the bare LED matrix much easier to look at
      directly
- [ ] Enclosure/case (project box or 3D-printed frame) — optional,
      cosmetic

## Not needed

- Breadboard — not required; jumper wires connect the display's male
  header pins directly to the FireBeetle's male header pins
- Separate power supply just to drive the display — the FireBeetle's 5V
  pin can handle it at the moderate brightness configured in the
  firmware (`DISPLAY_INTENSITY = 4` of 15 in `include/config.h`)
