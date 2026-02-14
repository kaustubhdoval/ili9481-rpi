#include "pin_definitions.h"
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

// Create Back Buffer
static uint16_t backbuffer[TFT_WIDTH * TFT_HEIGHT];

static struct gpiod_line_request *req_out_line(unsigned int gpio, const char *name, int init);
static struct gpiod_line_request *req_out_lines(unsigned int *gpios, unsigned int num_gpios, const char *name, int init_val);
static void set_line(struct gpiod_line_request *req, unsigned int offset, int val);
static void set_data_bus(uint8_t value);    

static void burst_write_bytes(const uint8_t *data, size_t len);
static void flush_backbuffer(void);

static void write_cmd(uint8_t cmd);
static void write_data(uint8_t data);
static void write_data16(uint16_t color);
static void write_coord16(uint16_t value);

static void set_window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1);

static void delay(uint32_t ms);

static void fill_screen(uint16_t color);

static void test_suite(void);
static void test_data_bus_pattern(void);

static void ili9481_reset(void);
static void ili9481_start(void);
