#ifndef ili9481_parallel_h
#define ili9481_parallel_h

#include "ili9481_constants.h"
#include "ili9481_init.h"   // Plethora of other init sequences

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


extern volatile uint8_t *gpio_base;
extern uint32_t data_lut[256];

// Back Buffer
extern uint16_t backbuffer[TFT_WIDTH * TFT_HEIGHT];

// Public Functions
void gpio_mmap_init(void);
void gpio_mmap_cleanup(void);

void burst_write_bytes(const uint8_t *data, size_t len);
void flush_backbuffer(void);

void write_cmd(uint8_t cmd);
void write_data(uint8_t data);
void write_data16(uint16_t color);
void write_coord16(uint16_t value);
void delay(uint32_t ms);

void reset_dirty(void);
void expand_dirty(uint16_t x, uint16_t y, uint16_t w, uint16_t h);

void set_window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1);
void fill_screen(uint16_t color);
void fill_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color);
void draw_line(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, uint16_t color);

void draw_char(uint16_t x, uint16_t y, char c, uint16_t fg);
void draw_char_scaled(uint16_t x, uint16_t y, char c, uint8_t scale, uint16_t fg);
void draw_string(uint16_t x, uint16_t y, const char *str, uint16_t fg);
void draw_string_scaled(uint16_t x, uint16_t y, const char *str, uint8_t scale, uint16_t fg);
void draw_bitmap(uint16_t x, uint16_t y, uint16_t w, uint16_t h, const uint16_t *bitmap, uint16_t transparent_color);
void draw_bitmap_mono(uint16_t x, uint16_t y, uint16_t w, uint16_t h, const uint8_t *bitmap, uint16_t color);

void ili9481_reset(void);
void ili9481_start(void);
void ili9481_stop(void);

#endif