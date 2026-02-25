// ili9481_parallel.c
#include "ili9481_parallel.h"

// gpiod variables
struct gpiod_chip *chip;
struct gpiod_line_request *rd_req, *wr_req, *rs_req, *cs_req, *rst_req;
struct gpiod_line_request *data_req = NULL;

unsigned int data_gpios[8] = {LCD_D0, LCD_D1, LCD_D2, LCD_D3, LCD_D4, LCD_D5, LCD_D6, LCD_D7};
uint16_t backbuffer[TFT_WIDTH * TFT_HEIGHT];

struct gpiod_line_request *req_out_line(unsigned int gpio, const char *name, int init)
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
    
    gpiod_line_config_free(config);
    gpiod_request_config_free(req_config);
    
    return request;
}

struct gpiod_line_request *req_out_lines(unsigned int *gpios, unsigned int num_gpios, const char *name, int init_val)
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
    gpiod_line_settings_set_output_value(settings, init_val ? GPIOD_LINE_VALUE_ACTIVE : GPIOD_LINE_VALUE_INACTIVE);
    
    config = gpiod_line_config_new();
    if (!config) {
        fprintf(stderr, "Failed to create line config\n");
        exit(1);
    }
    
    if (gpiod_line_config_add_line_settings(config, gpios, num_gpios, settings) < 0) {
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
        fprintf(stderr, "Failed to request lines\n");
        exit(1);
    }
    
    gpiod_line_settings_free(settings);
    gpiod_line_config_free(config);
    gpiod_request_config_free(req_config);
    
    return request;
}

void set_line(struct gpiod_line_request *req, unsigned int offset, int val)
{
    enum gpiod_line_value value = val ? GPIOD_LINE_VALUE_ACTIVE : GPIOD_LINE_VALUE_INACTIVE;
    if (gpiod_line_request_set_value(req, offset, value) < 0) {
        fprintf(stderr, "Failed to set line value\n");
        exit(1);
    }
}

// Burst write multiple data bytes with CS low throughout
// Assume RS is already set to data mode before calling this function
void burst_write_bytes(const uint8_t *data, size_t len)
{
    set_line(rs_req, LCD_RS, 1);  // Data mode
    set_line(cs_req, LCD_CS, 0);  // CS low for entire burst
    
    for (size_t i = 0; i < len; i++) {
        set_data_bus(data[i]);
        set_line(wr_req, LCD_WR, 0);
        set_line(wr_req, LCD_WR, 1);
    }
    
    set_line(cs_req, LCD_CS, 1);  // CS high
}

// Flush full backbuffer to display (converts to BGR666 bytes)
void flush_backbuffer(void) {
    size_t bytes = TFT_WIDTH * TFT_HEIGHT * 3;
    uint8_t *buf = malloc(bytes);
    if (!buf) {
        fprintf(stderr, "Malloc failed\n");
        exit(1);
    }
    
    for (int i = 0; i < TFT_WIDTH * TFT_HEIGHT; i++) {
        uint16_t color = backbuffer[i];
        uint8_t b = ((color & 0x1F) << 3);
        uint8_t g = (((color >> 5) & 0x3F) << 2);
        uint8_t r = (((color >> 11) & 0x1F) << 3);
        buf[i * 3 + 0] = b;
        buf[i * 3 + 1] = g;
        buf[i * 3 + 2] = r;
    }
    
    set_window(0, 0, TFT_WIDTH - 1, TFT_HEIGHT - 1);
    burst_write_bytes(buf, bytes);
    free(buf);
}

void set_data_bus(uint8_t value)
{
    enum gpiod_line_value values[8];
    for (int i = 0; i < 8; i++) {
        values[i] = ((value >> i) & 1) ? GPIOD_LINE_VALUE_ACTIVE : GPIOD_LINE_VALUE_INACTIVE;
    }
    if (gpiod_line_request_set_values(data_req, values) < 0) {   
        fprintf(stderr, "Failed to set data bus values\n");
        exit(1);
    }
}

// Write one 8-bit command
void write_cmd(uint8_t cmd)
{
    set_line(rs_req, LCD_RS, 0);  // RS/DC = 0 for command
    set_line(cs_req, LCD_CS, 0);  // CS low

    set_data_bus(cmd);

    set_line(wr_req, LCD_WR, 0);  // WR pulse
    set_line(wr_req, LCD_WR, 1);
    set_line(cs_req, LCD_CS, 1);  // CS high
}

void delay(uint32_t ms){
    usleep(ms * 1000);
}

// Write one 8-bit data value
void write_data(uint8_t data)
{
    set_line(rs_req, LCD_RS, 1);  // RS/DC = 1 for data
    set_line(cs_req, LCD_CS, 0);  // CS low

    set_data_bus(data);

    set_line(wr_req, LCD_WR, 0);
    set_line(wr_req, LCD_WR, 1);
    set_line(cs_req, LCD_CS, 1);  // CS high
}

// 18-bit color mode (BGR666) - sends 3 bytes per pixel
void write_data16(uint16_t color)
{
    // Extract RGB565 channels
    uint8_t r = ((color >> 11) & 0x1F);  // 5-bit red
    uint8_t g = ((color >> 5) & 0x3F);   // 6-bit green
    uint8_t b = (color & 0x1F);          // 5-bit blue
    
    // Expand to 6 bits and send in BGR order
    write_data(b << 3);  // BLUE first (xxxxx000 -> xxxxxx00)
    write_data(g << 2);  // GREEN second (xxxxxx00)
    write_data(r << 3);  // RED third (xxxxx000 -> xxxxxx00)
}

// Add a separate function for coordinates
void write_coord16(uint16_t value)
{
    write_data((value >> 8) & 0xFF);   // High byte
    write_data(value & 0xFF);           // Low byte
}

void ili9481_reset(void)
{
    // VCI power stable
    usleep(10000);  // Wait 10ms after power on
    
    set_line(rst_req, LCD_RST, 1);
    usleep(1000);   // RESX high for 1ms
    
    set_line(rst_req, LCD_RST, 0);
    usleep(10000);  // RESX low for at least 10μs 
    
    set_line(rst_req, LCD_RST, 1);
    usleep(120000); // Wait 120ms before sending commands 
}

// Set full window (0..319, 0..479)
void set_window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1)
{
    write_cmd(0x2A);             // Column address set
    write_coord16(x0);
    write_coord16(x1);

    write_cmd(0x2B);             // Page address set
    write_coord16(y0);
    write_coord16(y1);

    write_cmd(0x2C);             // Memory write
}

void fill_screen(uint16_t color) {
    for (int i = 0; i < TFT_WIDTH * TFT_HEIGHT; i++) {
        backbuffer[i] = color;
    }
    flush_backbuffer();
}

// Function to reverse bits
uint8_t reverse_bits(uint8_t b) {
    uint8_t r = 0;
    for (int i = 0; i < 8; i++) {
        r = (r << 1) | ((b >> i) & 1);
    }
    return r;
}

void ili9481_start(void){
    chip = gpiod_chip_open(GPIOCHIP);
    if (!chip) {
        perror("gpiod_chip_open");
    }

    // Request control lines (using single-line function)
    rd_req  = req_out_line(LCD_RD,  "lcd-rd", 1);
    wr_req  = req_out_line(LCD_WR,  "lcd-wr", 1);
    rs_req  = req_out_line(LCD_RS,  "lcd-rs", 1);
    cs_req  = req_out_line(LCD_CS,  "lcd-cs", 1);
    rst_req = req_out_line(LCD_RST, "lcd-rst", 1);

    // Request data lines (using multi-line function)
    data_req = req_out_lines(data_gpios, 8, "lcd_data", 0);  // Init all low

    ili9481_reset();
    ili9481_init();
}

void ili9481_stop(void)
{
    // Release control lines
    if (rd_req)  { gpiod_line_request_release(rd_req);  rd_req  = NULL; }
    if (wr_req)  { gpiod_line_request_release(wr_req);  wr_req  = NULL; }
    if (rs_req)  { gpiod_line_request_release(rs_req);  rs_req  = NULL; }
    if (cs_req)  { gpiod_line_request_release(cs_req);  cs_req  = NULL; }
    if (rst_req) { gpiod_line_request_release(rst_req); rst_req = NULL; }

    // Release data bus 
    if (data_req) { gpiod_line_request_release(data_req); data_req = NULL; }

    // Close Chip
    if (chip) { gpiod_chip_close(chip); chip = NULL; }
}