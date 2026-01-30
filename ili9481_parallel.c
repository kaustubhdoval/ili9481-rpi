// ili9481_parallel.c
// Minimal 8-bit parallel ILI9481 driver using libgpiod on Raspberry Pi Zero 2 W.
// Pin mapping uses BCM numbers corresponding to your wiring.

#include <gpiod.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>

#define GPIOCHIP "/dev/gpiochip0"

// Control pins (BCM)
#define LCD_RD   2
#define LCD_WR   3
#define LCD_RS   4   // DC
#define LCD_CS   17
#define LCD_RST  27

// Data pins D0..D7 (BCM)
#define LCD_D0   15
#define LCD_D1   14
#define LCD_D2   9
#define LCD_D3   10
#define LCD_D4   22
#define LCD_D5   24
#define LCD_D6   23
#define LCD_D7   18

static struct gpiod_chip *chip;
static struct gpiod_line *rd_line, *wr_line, *rs_line, *cs_line, *rst_line;
static struct gpiod_line *d_lines[8];

// Simple helper to request one output line
static struct gpiod_line *req_out_line(int gpio, const char *name, int init)
{
    struct gpiod_line *l = gpiod_chip_get_line(chip, gpio);
    if (!l) {
        fprintf(stderr, "Failed to get line %d\n", gpio);
        exit(1);
    }
    if (gpiod_line_request_output(l, name, init) < 0) {
        fprintf(stderr, "Failed to request line %d as output\n", gpio);
        exit(1);
    }
    return l;
}

static void set_line(struct gpiod_line *l, int val)
{
    if (gpiod_line_set_value(l, val) < 0) {
        fprintf(stderr, "Failed to set line value\n");
        exit(1);
    }
}

static void set_data_bus(uint8_t value)
{
    // Map bits 0..7 to your D0..D7 lines
    int vals[8];
    vals[0] = (value >> 0) & 1; // D0
    vals[1] = (value >> 1) & 1; // D1
    vals[2] = (value >> 2) & 1; // D2
    vals[3] = (value >> 3) & 1; // D3
    vals[4] = (value >> 4) & 1; // D4
    vals[5] = (value >> 5) & 1; // D5
    vals[6] = (value >> 6) & 1; // D6
    vals[7] = (value >> 7) & 1; // D7

    for (int i = 0; i < 8; i++) {
        if (gpiod_line_set_value(d_lines[i], vals[i]) < 0) {
            fprintf(stderr, "Failed to set data line %d\n", i);
            exit(1);
        }
    }
}

// Write one 8-bit command
static void write_cmd(uint8_t cmd)
{
    set_line(rs_line, 0);        // RS/DC = 0 for command
    set_line(cs_line, 0);        // CS low
    set_data_bus(cmd);
    // Pulse WR low
    set_line(wr_line, 0);
    // Short delay – adjust if needed
    // usleep(1);
    set_line(wr_line, 1);
    set_line(cs_line, 1);        // CS high
}

// Write one 8-bit data value
static void write_data(uint8_t data)
{
    set_line(rs_line, 1);        // RS/DC = 1 for data
    set_line(cs_line, 0);        // CS low
    set_data_bus(data);
    set_line(wr_line, 0);
    // usleep(1);
    set_line(wr_line, 1);
    set_line(cs_line, 1);        // CS high
}

// Write 16-bit data as two bytes (MSB first) for RGB565
static void write_data16(uint16_t data)
{
    write_data((data >> 8) & 0xFF);
    write_data(data & 0xFF);
}

static void ili9481_reset(void)
{
    // Hardware reset
    set_line(rst_line, 0);
    usleep(20000);  // 20 ms
    set_line(rst_line, 1);
    usleep(120000); // 120 ms
}

// Very minimal init sequence for ILI9481
static void ili9481_init(void)
{
    ili9481_reset();

    write_cmd(0x11);             // Sleep Out (TFT_SLPOUT)
    usleep(20000);               // 20ms delay

    // Power setting - match ESP32 values
    write_cmd(0xD0);
    write_data(0x07);
    write_data(0x42);         
    write_data(0x18);           

    // VCOM control
    write_cmd(0xD1);
    write_data(0x00);
    write_data(0x07);            
    write_data(0x10);            

    // Power setting for normal mode 
    write_cmd(0xD2);
    write_data(0x01);
    write_data(0x02);            

    // Panel driving setting 
    write_cmd(0xC0);
    write_data(0x10);
    write_data(0x3B);
    write_data(0x00);
    write_data(0x02);
    write_data(0x11);

    // Frame rate and inversion - same
    write_cmd(0xC5);
    write_data(0x03);

    // Gamma 
    write_cmd(0xC8);
    write_data(0x00);
    write_data(0x32);
    write_data(0x36);
    write_data(0x45);
    write_data(0x06);
    write_data(0x16);
    write_data(0x37);
    write_data(0x75);
    write_data(0x77);
    write_data(0x54);
    write_data(0x0C);
    write_data(0x00);

    // Memory Access Control (0x36 = TFT_MADCTL)
    write_cmd(0x36);
    write_data(0x0A);           

    // Interface pixel format: 16-bit (0x3A)
    write_cmd(0x3A);
    write_data(0x55);            // 16bpp for 8-bit parallel

    // Column Address Set (0x2A = TFT_CASET)
    write_cmd(0x2A);
    write_data(0x00);
    write_data(0x00);
    write_data(0x01);
    write_data(0x3F);            // 0x013F = 319

    // Page Address Set (0x2B = TFT_PASET)
    write_cmd(0x2B);
    write_data(0x00);
    write_data(0x00);
    write_data(0x01);
    write_data(0xDF);            

    usleep(120000);              // 120ms delay

    // Display ON (0x29 = TFT_DISPON)
    write_cmd(0x29);
    usleep(25000);               
}

// Set full window (0..319, 0..479)
static void ili9481_set_window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1)
{
    write_cmd(0x2A);             // Column address set
    write_data16(x0);
    write_data16(x1);

    write_cmd(0x2B);             // Page address set
    write_data16(y0);
    write_data16(y1);

    write_cmd(0x2C);             // Memory write
}

// Fill entire screen with one color
static void ili9481_fill_screen(uint16_t color)
{
    const uint16_t width  = 320;
    const uint16_t height = 480;
    uint32_t pixels = (uint32_t)width * (uint32_t)height;

    ili9481_set_window(0, 0, width - 1, height - 1);

    for (uint32_t i = 0; i < pixels; i++) {
        write_data16(color);
    }
}

int main(void)
{
    chip = gpiod_chip_open(GPIOCHIP);
    if (!chip) {
        perror("gpiod_chip_open");
        return 1;
    }

    // Request control lines
    rd_line  = req_out_line(LCD_RD,  "lcd-rd", 1);  // Keep RD high (no reads)
    wr_line  = req_out_line(LCD_WR,  "lcd-wr", 1);
    rs_line  = req_out_line(LCD_RS,  "lcd-rs", 1);
    cs_line  = req_out_line(LCD_CS,  "lcd-cs", 1);
    rst_line = req_out_line(LCD_RST, "lcd-rst", 1);

    // Request data lines
    int data_gpios[8] = {
        LCD_D0, LCD_D1, LCD_D2, LCD_D3,
        LCD_D4, LCD_D5, LCD_D6, LCD_D7
    };
    for (int i = 0; i < 8; i++) {
        d_lines[i] = req_out_line(data_gpios[i], "lcd-d", 0);
    }

    // Ensure default states
    set_line(rd_line, 1);
    set_line(wr_line, 1);
    set_line(cs_line, 1);
    set_line(rs_line, 1);

    printf("Initializing ILI9481...\n");
    ili9481_init();
    printf("Filling screen...\n");

    // RGB565: blue = 0x001F, red = 0xF800, green = 0x07E0
    ili9481_fill_screen(0x001F);

    printf("Done.\n");

    // Keep process alive a bit so you can see result, then exit
    sleep(2);

    gpiod_chip_close(chip);
    return 0;
}
