# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

A bare-metal C driver for an ILI9481 TFT panel (480×320, 18-bit colour) over an 8-bit parallel interface, running on Raspberry Pi. There is no OS-level display abstraction: GPIO is controlled via direct `/dev/gpiomem` register mapping, not `pigpio`/`wiringPi`/sysfs GPIO. This only builds/runs meaningfully on a Raspberry Pi (BCM2835/BCM2711 SoC) with the panel wired up — it will compile elsewhere but `gpio_mmap_init()` will fail at runtime without `/dev/gpiomem`.

## Build

```bash
make          # builds ./main from main.c, ili9481_parallel.c, ili9481_img.c
make clean    # removes objects and the main binary
```

Requires `gcc` and `libjpeg-dev` (JPEG decoding only). CFLAGS are `-Wall -Wextra -O0`.

`diagnostic.c` is **not** wired into the Makefile — it must be compiled manually if needed, e.g.:
```bash
gcc -Wall -Wextra -O0 diagnostic.c ili9481_parallel.c ili9481_img.c -o diagnostic -ljpeg -lm
```

Run on-device (GPIO access requires root):
```bash
sudo ./main
sudo ./diagnostic
```

There is no test suite; `diagnostic.c` serves as the hardware bring-up/verification tool (see below).

## Architecture

**File layout is a flat single-driver-library shape, not a general graphics framework:**

- `ili9481_constants.h` — pin assignments (BCM numbering), panel dimensions, colour macros, MADCTL value, and controller command bytes. Pin numbers and `TFT_MADCTL` here are display-wiring-specific; changing panel orientation means revisiting the flip-compensation math in `ili9481_parallel.c` (see comment there referencing `TFT_MADCTL=0xA8`).
- `ili9481_init.h` — 8 alternative controller init command sequences (including several ported from TFT_eSPI for cross-panel testing). The default sequence used by `ili9481_start()` was hand-derived from the raw ILI9481 datasheet because no off-the-shelf sequence produced correct colour on the panel this was built against.
- `ili9481_parallel.c`/`.h` — the core driver: GPIO mmap setup/teardown, the 8080-style parallel bus write cycle, the backbuffer, dirty-region tracking, and all drawing primitives (pixels, rects, lines, text, bitmaps).
- `ili9481_img.c`/`.h` — JPEG (via libjpeg) and BMP file decoding/rendering into the backbuffer, including Floyd-Steinberg dithering when quantizing decoded 8-bit RGB down to the panel's 18-bit (6-bit/channel) colour depth.
- `main.c` — example/demo program exercising the primitives.
- `diagnostic.c` — standalone interactive hardware bring-up tool, run *before* trusting `main.c`. Walks through wiring/polarity checks in order (data bus pattern → reset timing → normal polarity → inverted RS/DC → inverted WR → both inverted → bit-reversed bus → full init + colour flood), prompting for visual/logic-analyzer confirmation at each step. Use this to diagnose wiring issues (e.g. inverted RS/WR lines, D0/D7 swapped) independently of whether the init sequence itself is correct.
- `assets/` — embedded 8×8 CP437 font header (`cp437font8x8.h`) and demo image files used by `main.c`.

**Key design points to preserve when modifying:**

- GPIO register access (`GPIO_SET`/`GPIO_CLR` at `GPIO_BASE = 0x3F200000`) is hardcoded for BCM2835/BCM2711 (Pi Zero 2W, Pi 3, Pi 4). Supporting a Pi 5 (BCM2712, different GPIO base) requires updating `GPIO_BASE` in `ili9481_parallel.h` and rechecking `gpio_mmap_init()`.
- Byte writes to the 8-bit data bus go through a precomputed 256-entry LUT (`data_lut`) mapping each byte value to a GPIO SET bitmask, avoiding per-bit branching on the hot path. Any change to `LCD_D0..D7` pin assignments requires regenerating this LUT logic, not just the constants.
- Parallel bus timing (setup/hold around the WR strobe) is enforced with inline `__asm__ volatile("nop")` padding, not `usleep`. Don't replace this with timer-based delays on the hot write path — it will change timing margins against the datasheet.
- All drawing primitives write to `backbuffer` (480×320×3 bytes, 18-bit RGB) — nothing reaches the physical display until `flush_backbuffer()` is called. Primitives are responsible for calling `expand_dirty()` to grow the tracked dirty rectangle; `flush_backbuffer()` only transmits that region (full-width dirty regions go out as one burst, partial-width regions row-by-row) and then calls `reset_dirty()`. New primitives must maintain this contract.
- Colours are `0x00RRGGBB` 32-bit values everywhere (see `RGB(r,g,b)` macro and the named colour constants in `ili9481_constants.h`), not RGB565 — `write_data16()` exists only as a legacy RGB565→24-bit expansion path for direct (non-backbuffer) writes.
