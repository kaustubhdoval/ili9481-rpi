// ili9481_parallel.c
// ILI9481 driver using libgpiod API on Raspberry Pi Zero 2 W

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
static struct gpiod_line_request *rd_req, *wr_req, *rs_req, *cs_req, *rst_req;
static struct gpiod_line_request *d_req[8];

// Helper to request one output line with libgpiod v2
static struct gpiod_line_request *req_out_line(unsigned int gpio, const char *name, int init)
{
    struct gpiod_line_settings *settings;
    struct gpiod_line_config *config;
    struct gpiod_request_config *req_config;
    struct gpiod_line_request *request;
    
    settings = gpiod_line_settings_new();
    if (!settings) {
        fprintf(stderr, "Failed to create line settings\n");
        exit(1);
    }
    
    gpiod_line_settings_set_direction(settings, GPIOD_LINE_DIRECTION_OUTPUT);
    gpiod_line_settings_set_output_value(settings, init ? GPIOD_LINE_VALUE_ACTIVE : GPIOD_LINE_VALUE_INACTIVE);
    
    config = gpiod_line_config_new();
    if (!config) {
        fprintf(stderr, "Failed to create line config\n");
        exit(1);
    }
    
    if (gpiod_line_config_add_line_settings(config, &gpio, 1, settings) < 0) {
        fprintf(stderr, "Failed to add line settings\n");
        exit(1);
    }
    
    req_config = gpiod_request_config_new();
    if (!req_config) {
        fprintf(stderr, "Failed to create request config\n");
        exit(1);
    }
    gpiod_request_config_set_consumer(req_config, name);
    
    request = gpiod_chip_request_lines(chip, req_config, config);
    if (!request) {
        fprintf(stderr, "Failed to request line %d\n", gpio);
        exit(1);
    }
    
    gpiod_line_settings_free(settings);
    gpiod_line_config_free(config);
    gpiod_request_config_free(req_config);
    
    return request;
}

static void set_line(struct gpiod_line_request *req, unsigned int offset, int val)
{
    enum gpiod_line_value value = val ? GPIOD_LINE_VALUE_ACTIVE : GPIOD_LINE_VALUE_INACTIVE;
    if (gpiod_line_request_set_value(req, offset, value) < 0) {
        fprintf(stderr, "Failed to set line value\n");
        exit(1);
    }
}

static void set_data_bus(uint8_t value)
{
    // Set each data line individually
    int data_gpios[8] = {LCD_D0, LCD_D1, LCD_D2, LCD_D3, LCD_D4, LCD_D5, LCD_D6, LCD_D7};
    
    for (int i = 0; i < 8; i++) {
        int bit_val = (value >> i) & 1;
        set_line(d_req[i], data_gpios[i], bit_val);
    }
}

// Write one 8-bit command
static void write_cmd(uint8_t cmd)
{
    set_line(rs_req, LCD_RS, 0);  // RS/DC = 0 for command
    set_line(cs_req, LCD_CS, 0);  // CS low

    //DEBUGGING - add delay 
    usleep(1);

    set_data_bus(cmd);

    //DEBUGGING - add delay 
    usleep(1);

    set_line(wr_req, LCD_WR, 0);  // WR pulse
    //DEBUGGING - add delay 
    usleep(1);
    set_line(wr_req, LCD_WR, 1);
    //DEBUGGING - add delay 
    usleep(1);
    set_line(cs_req, LCD_CS, 1);  // CS high
}

// Write one 8-bit data value
static void write_data(uint8_t data)
{
    set_line(rs_req, LCD_RS, 1);  // RS/DC = 1 for data
    set_line(cs_req, LCD_CS, 0);  // CS low

    //DEBUGGING - add delay 
    usleep(1);

    set_data_bus(data);
    //DEBUGGING - add delay 
    usleep(1);
    set_line(wr_req, LCD_WR, 0);
    //DEBUGGING - add delay 
    usleep(1);
    set_line(wr_req, LCD_WR, 1);
    //DEBUGGING - add delay 
    usleep(1);
    set_line(cs_req, LCD_CS, 1);  // CS high
}

// Write 16-bit data as two bytes (MSB first) for RGB565
static void write_data16(uint16_t data)
{
    write_data((data >> 8) & 0xFF);
    write_data(data & 0xFF);
}

static void ili9481_reset(void)
{
    set_line(rst_req, LCD_RST, 0);
    usleep(20000);  // 20 ms
    set_line(rst_req, LCD_RST, 1);
    usleep(120000); // 120 ms
}

// Very minimal init sequence for ILI9481
static void ili9481_init(void)
{
    ili9481_reset();

    write_cmd(0x11);             // Sleep Out (TFT_SLPOUT)
    usleep(20000);               // 20ms delay

    // Power setting 
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
    rd_req  = req_out_line(LCD_RD,  "lcd-rd", 1);
    wr_req  = req_out_line(LCD_WR,  "lcd-wr", 1);
    rs_req  = req_out_line(LCD_RS,  "lcd-rs", 1);
    cs_req  = req_out_line(LCD_CS,  "lcd-cs", 1);
    rst_req = req_out_line(LCD_RST, "lcd-rst", 1);

    // Request data lines
    int data_gpios[8] = {LCD_D0, LCD_D1, LCD_D2, LCD_D3, LCD_D4, LCD_D5, LCD_D6, LCD_D7};
    for (int i = 0; i < 8; i++) {
        d_req[i] = req_out_line(data_gpios[i], "lcd-d", 0);
    }

    printf("Initializing ILI9481...\n");
    ili9481_init();
    printf("Filling screen with blue...\n");

    ili9481_fill_screen(0x001F);  // Blue

    printf("Done.\n");
    sleep(2);

    // Cleanup
    gpiod_line_request_release(rd_req);
    gpiod_line_request_release(wr_req);
    gpiod_line_request_release(rs_req);
    gpiod_line_request_release(cs_req);
    gpiod_line_request_release(rst_req);
    for (int i = 0; i < 8; i++) {
        gpiod_line_request_release(d_req[i]);
    }
    gpiod_chip_close(chip);
    
    return 0;
}