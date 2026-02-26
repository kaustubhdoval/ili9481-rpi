#ifndef ili9481_parallel_h
#define ili9481_parallel_h

#include "ili9481_constants.h"
#include "ili9481_init.h"   // Plethora of other init sequences

#ifdef COMPILE_CHECK
#include "gpiod.h"
#else
#include <gpiod.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>

#define GPIOCHIP "/dev/gpiochip0"

// Back Buffer
extern uint16_t backbuffer[TFT_WIDTH * TFT_HEIGHT];

// Public Functions
void set_line(struct gpiod_line_request *req, unsigned int offset, int val);
void set_data_bus(uint8_t value);
void write_cmd(uint8_t cmd);
void write_data(uint8_t data);
void write_data16(uint16_t color);
void write_coord16(uint16_t value);
void delay(uint32_t ms);

void set_window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1);
void fill_screen(uint16_t color);

void ili9481_reset(void);
void ili9481_start(void);
void ili9481_stop(void);

uint8_t reverse_bits(uint8_t x);

#endif