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

#define GPIOCHIP "/dev/gpiochip0"
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
static void gpio_set_output(uint8_t pin);
void gpio_mmap_init(void);
void gpio_mmap_cleanup(void);

static inline void set_line(uint8_t pin, int val);
static inline void set_data_bus(uint8_t value);

void burst_write_bytes(const uint8_t *data, size_t len);
void flush_backbuffer(void);

void write_cmd(uint8_t cmd);
void write_data(uint8_t data);
void write_data16(uint16_t color);
void write_coord16(uint16_t value);
void delay(uint32_t ms);

void set_window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1);
void fill_screen(uint16_t color);

uint8_t reverse_bits(uint8_t x);

void ili9481_reset(void);
void ili9481_start(void);
void ili9481_stop(void);

#endif