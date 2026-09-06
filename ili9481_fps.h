// ili9481_fps.h
#ifndef ili9481_fps_h
#define ili9481_fps_h

#include <stdbool.h>
#include <stdint.h>

// Enable/disable FPS counting. Disabled by default (near-zero cost when off).
void fps_enable(bool on);
bool fps_is_enabled(void);

// How often (in ms) the averaged FPS is printed to the terminal. Default: 5000.
void fps_set_report_interval_ms(uint32_t ms);

// Called once per completed flush_backbuffer(). No-op when disabled.
void fps_tick(void);

// Last averaged FPS value computed at the most recent report (0 until the first report).
float fps_get_last(void);

#endif
