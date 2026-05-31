<div align="center">

<h1>ili9481-rpi</h1>

</div>

<p align="center">
  A bare-metal C display driver for the ILI9481 TFT panel over an 8-bit parallel interface on Raspberry Pi - built from scratch with no OS-level display abstractions.
  <br />
  <a href="https://github.com/kaustubhdoval/ili9481-rpi"><strong>Explore the docs »</strong></a>
  <br />
  <br />
  ·
  <a href="https://github.com/kaustubhdoval/ili9481-rpi/issues">Report Bug</a>
  ·
  <a href="https://github.com/kaustubhdoval/ili9481-rpi/issues">Request Feature</a>
  ·
</p>
</p>

---

## Overview

This project is a pure C driver for an ILI9481-based 3.5" TFT display (480×320, 18-bit colour) driven over an 8-bit parallel interface. Every layer was written from scratch: GPIO control via direct `/dev/gpiomem` memory mapping, a hand-tuned custom init sequence, a double-buffered rendering pipeline with dirty-region tracking, and a full primitives library.

The motivation: I had a generic Arduino Uno TFT shield lying around and wanted to drive it from a Raspberry Pi Zero 2W. The parallel interface (vs SPI) is significantly faster but requires simultaneous control of 13 GPIO lines - something existing tools handle poorly or not at all.

| Commit bb12be9                                        | Commit d095598                                      | Commit e94f437                                     |
| ----------------------------------------------------- | --------------------------------------------------- | -------------------------------------------------- |
| <img src="/assets/docs/memory_map.gif" width="400" /> | <img src="/assets/docs/batching.gif" width="200" /> | <img src="/assets/docs/initial.gif" width="200" /> |
| Direct memory-mapped GPIO + full primitive suite      | Burst batching implemented                          | Initial unoptimized bit-banging                    |

---

## Key Implementation Details

### Direct GPIO Memory Mapping

Rather than going through `/dev/gpio`, `pigpio`, or any other userspace abstraction, the driver maps the BCM2835/BCM2711 GPIO peripheral registers directly into process memory via `/dev/gpiomem`. This gives nanosecond-level register access with no kernel round-trips.

```c
gpio_base = (volatile uint8_t*)mmap(NULL, GPIO_MAP_SIZE,
                                     PROT_READ | PROT_WRITE,
                                     MAP_SHARED, fd, GPIO_BASE);
```

Pin direction and set/clear operations write directly to `GPFSEL`, `GPSET`, and `GPCLR` registers, bypassing all OS overhead entirely.

### 256-Entry Data Bus LUT

Writing a byte to the 8-bit parallel bus requires mapping each data bit to a potentially non-contiguous GPIO pin. A naive approach does 8 conditional branches per byte. This driver pre-computes a 256-entry lookup table at startup that maps every possible byte value to the exact 32-bit GPIO SET mask — reducing each bus write to two register writes and zero branches:

```c
GPIO_CLR = DATA_PIN_MASK;       // clear all data pins
GPIO_SET = data_lut[value];     // set the required ones — no branching
```

### Burst Write with NOP-Calibrated Timing

The 8080-style parallel write cycle (RS → CS → data → WR strobe) is implemented with inline `__asm__ volatile("nop")` padding to satisfy the ILI9481's setup/hold timing requirements without relying on `usleep` or any timer. A `burst_write_bytes()` function keeps CS low across an entire pixel block, minimising per-byte overhead for bulk transfers.

### 18-bit Backbuffer with Lazy Rendering and Dirty-Region Tracking

The driver maintains a 460,800-byte (480×320×3) backbuffer in 18-bit RGB format. All drawing primitives write to this buffer - nothing reaches the display until `flush_backbuffer()` is called. The driver tracks the minimal axis-aligned bounding rectangle of all changes since the last flush (the "dirty region") and only transmits that sub-image, dramatically reducing bus traffic for partial-screen updates.

If the dirty region spans the full display width, the flush path sends the pixel data in a single contiguous burst. Partial-width regions are sent row-by-row.

```c
// Full-width dirty region: one burst
if (dirty_x0 == 0 && w == TFT_WIDTH) {
    burst_write_bytes(backbuffer + offset, w * h * 3);
} else {
    // Partial width: row-by-row
    for (uint16_t j = 0; j < h; j++) { ... }
}
```

### Custom ILI9481 Initialization Sequence

The driver ships with 8 init sequences (including several sourced from TFT_eSPI for cross-panel compatibility testing), but the default is a custom sequence developed from the raw ILI9481 datasheet — necessary because no existing sequence produced correct colour on the specific panel variant in use. The sequence configures power control, VCOM, gamma correction, panel driving, and sets the interface to 18-bit (0x66) colour mode.

### Floyd-Steinberg Dithering for JPEG Rendering

JPEG images are decoded via `libjpeg` into 8-bit RGB and then quantized to 18-bit (6 bits per channel) for the display. To minimize the perceptual loss from quantization, the JPEG decoder path implements per-pixel Floyd-Steinberg error diffusion, distributing quantization error to adjacent pixels using the standard 7/16, 3/16, 5/16, 1/16 weights. This produces significantly smoother gradients than simple rounding.

---

## Graphics Primitive Library

All primitives write to the backbuffer and automatically update the dirty region.

| Primitive        | Function                                             |
| ---------------- | ---------------------------------------------------- |
| Pixel            | `set_pixel(x, y, color)`                             |
| Rectangle fill   | `fill_rect(x, y, w, h, color)`                       |
| Screen fill      | `fill_screen(color)`                                 |
| Line (Bresenham) | `draw_line(x0, y0, x1, y1, color)`                   |
| Character        | `draw_char(x, y, c, fg)`                             |
| Scaled character | `draw_char_scaled(x, y, c, scale, fg)`               |
| String           | `draw_string(x, y, str, fg)`                         |
| Scaled string    | `draw_string_scaled(x, y, str, scale, fg)`           |
| RGBA bitmap      | `draw_bitmap(x, y, w, h, bitmap, transparent_color)` |
| 1-bit bitmap     | `draw_bitmap_mono(x, y, w, h, bitmap, color)`        |
| JPEG file        | `draw_jpeg_file(x, y, filepath, grayscale)`          |
| BMP file         | `draw_bmp_file(x, y, filepath)`                      |

Text rendering uses an embedded 8×8 CP437 font. Colours are `0x00RRGGBB` 32-bit values; an `RGB(r, g, b)` macro is provided for convenience.

---

## Diagnostic Test Suite

`diagnostic.c` is a standalone interactive program for hardware bring-up. It walks through hardware-level signal checks before attempting any initialization, so wiring issues are caught and diagnosed before any init sequence is sent. The suite covers:

1. **Data bus pattern test** — outputs a walking-bit pattern (0x01, 0x02, 0x04…) to D0–D7 for verification with an LED bar or logic analyzer
2. **Hardware reset sequence** — controlled RST line toggle with correct timing margins
3. **Standard polarity command test** — sends Sleep Out + Display On and prompts for visual confirmation
4. **Inverted RS/DC test** — retries with DC polarity flipped (some shields invert this line)
5. **Inverted WR test** — retries with WR idle-low / pulse-high (non-standard shields)
6. **Combined RS + WR inversion** — covers the remaining polarity permutation
7. **Bit-reversed data bus test** — detects D0/D7 MSB/LSB swap wiring errors
8. **Full init + colour flood** — if communication is confirmed, runs the complete init sequence and floods the screen with red, green, blue, white, and black

Each test pauses for user confirmation before proceeding, and the suite prints actionable corrective steps if a fault is identified.

---

## Usage

Requires `make`, `gcc`, and `libjpeg-dev` (for JPEG support only).

```bash
# Clone and build
git clone https://github.com/kaustubhdoval/ili9481-rpi
cd ili9481-rpi
make

# Run the example
sudo ./main

# Run hardware diagnostics
sudo ./diagnostic
```

> **Note:** The GPIO memory map is hardcoded to `0x3F200000` (BCM2835/BCM2711 — Pi Zero 2W, Pi 3, Pi 4). For a Pi 5 or other variants, update `GPIO_BASE` in `ili9481_parallel.h` and verify `gpio_mmap_init()` covers all required pins.

---

## Dependencies

| Library       | Purpose                                                       |
| ------------- | ------------------------------------------------------------- |
| `libjpeg-dev` | JPEG decoding (optional — only required for `draw_jpeg_file`) |

No other external dependencies. Everything else - GPIO control, timing, rendering, font rendering - is implemented directly.

---

## References

- [ILI9481 Datasheet](https://www.displayfuture.com/Display/datasheet/controller/ILI9481.pdf)
- [BCM2835 ARM Peripherals Manual](https://datasheets.raspberrypi.com/bcm2835/bcm2835-peripherals.pdf)
- [TFT_eSPI](https://github.com/Bodmer/TFT_eSPI) — source of several alternative init sequences used during development

---

## Roadmap

- Fix JPEG rendering edge cases
- Finer-grained dirty region tracking (multiple non-overlapping rectangles)
- DMA-based pixel transfers for higher sustained frame rates
- Video playback (MP4/AVI via ffmpeg decode)
- Resistive touchscreen support
