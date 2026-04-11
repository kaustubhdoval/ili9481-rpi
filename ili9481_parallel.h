#ifndef ili9481_parallel_h
#define ili9481_parallel_h

#include "ili9481_constants.h"
#include "ili9481_init.h"           // Init Sequences
#include "assets/cp437font8x8.h"    // 8x8 font data

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <fcntl.h>
#include <sys/mman.h>

#define GPIO_BASE        0x3F200000
#define GPIO_MAP_SIZE    4096

#define GPIO_SET  (*(volatile uint32_t*)(gpio_base + 0x1C))
#define GPIO_CLR  (*(volatile uint32_t*)(gpio_base + 0x28))

#define DATA_PIN_MASK ((1<<LCD_D0)|(1<<LCD_D1)|(1<<LCD_D2)|(1<<LCD_D3)|\
                       (1<<LCD_D4)|(1<<LCD_D5)|(1<<LCD_D6)|(1<<LCD_D7))


// Convenience macro: build a colour from components
#define RGB(r, g, b) ((uint32_t)(((r) & 0xFF) << 16) | (((g) & 0xFF) << 8) | ((b) & 0xFF))

extern volatile uint8_t *gpio_base;
extern uint32_t data_lut[256];

// 18-bit Back Buffer: 3 bytes per pixel
// Size: TFT_WIDTH * TFT_HEIGHT * 3 = 480 * 320 * 3 = 460,800 bytes (~450 KB)
extern uint8_t backbuffer[TFT_WIDTH * TFT_HEIGHT * 3];

// Public Functions
void gpio_mmap_init(void);
void gpio_mmap_cleanup(void);

void burst_write_bytes(const uint8_t *data, size_t len);
void flush_backbuffer(void);

void write_cmd(uint8_t cmd);
void write_data(uint8_t data);
void write_coord16(uint16_t value);
void write_color24(uint32_t color);  // sends 0x00RRGGBB as 3 bytes directly to hw
void write_data16(uint16_t color);   // legacy: RGB565 → 24-bit expand, direct to hw
void burst_write_bytes(const uint8_t *data, size_t len);
void set_window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1);
void delay(uint32_t ms);

void reset_dirty(void);
void expand_dirty(uint16_t x, uint16_t y, uint16_t w, uint16_t h);

void set_pixel(uint16_t x, uint16_t y, uint32_t color);
void fill_screen(uint32_t color);
void fill_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint32_t color);
void draw_line(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, uint32_t color);

void draw_char(uint16_t x, uint16_t y, char c, uint32_t fg);
void draw_char_scaled(uint16_t x, uint16_t y, char c, uint8_t scale, uint32_t fg);
void draw_string(uint16_t x, uint16_t y, const char *str, uint32_t fg);
void draw_string_scaled(uint16_t x, uint16_t y, const char *str, uint8_t scale, uint32_t fg);
void draw_bitmap(uint16_t x, uint16_t y, uint16_t w, uint16_t h,
                 const uint32_t *bitmap, uint32_t transparent_color);
void draw_bitmap_mono(uint16_t x, uint16_t y, uint16_t w, uint16_t h,
                      const uint8_t *bitmap, uint32_t color);
                      
void ili9481_reset(void);
void ili9481_start(void);
void ili9481_stop(void);

#endif