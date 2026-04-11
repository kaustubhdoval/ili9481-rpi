// ili9481_parallel.c
#include "ili9481_parallel.h"

#define WR_SETUP_NOPS  4
#define WR_HOLD_NOPS   4
#define WR_CYCLE_NOPS  4

unsigned int data_gpios[8] = {LCD_D0, LCD_D1, LCD_D2, LCD_D3, LCD_D4, LCD_D5, LCD_D6, LCD_D7};

volatile uint8_t *gpio_base = NULL;
uint32_t data_lut[256];

// 18-bit backbuffer: 3 bytes per pixel, stored as R, G, B
uint8_t backbuffer[TFT_WIDTH * TFT_HEIGHT * 3];
static uint16_t dirty_x0, dirty_y0, dirty_x1, dirty_y1;

static void gpio_set_output(uint8_t pin)
{
    // Each GPFSEL register controls 10 pins, 3 bits each
    uint32_t reg_offset = (pin / 10) * 4;                                           // which GPFSEL register (offset in bytes)
    uint32_t bit_offset = (pin % 10) * 3;                                           // which 3-bit field within that register
    volatile uint32_t *fsel = (volatile uint32_t*)(gpio_base + reg_offset);
    *fsel = (*fsel & ~(7 << bit_offset)) | (1 << bit_offset);                       // set to 001 = output
}   

void gpio_mmap_init(void)
{
    int fd = open("/dev/gpiomem", O_RDWR | O_SYNC);
    if (fd < 0) {
        perror("open /dev/gpiomem");
        exit(1);
    }

    gpio_base = (volatile uint8_t*)mmap(NULL, GPIO_MAP_SIZE,
                                         PROT_READ | PROT_WRITE,
                                         MAP_SHARED, fd, GPIO_BASE);
    close(fd);

    if (gpio_base == MAP_FAILED) {
        perror("mmap");
        exit(1);
    }

    uint8_t all_pins[] = {LCD_RD, LCD_WR, LCD_RS, LCD_CS, LCD_RST,
                          LCD_D0, LCD_D1, LCD_D2, LCD_D3,
                          LCD_D4, LCD_D5, LCD_D6, LCD_D7};
    for (int i = 0; i < 13; i++)
        gpio_set_output(all_pins[i]);

    GPIO_SET = (1<<LCD_RD)|(1<<LCD_WR)|(1<<LCD_RS)|(1<<LCD_CS)|(1<<LCD_RST);

    for (int val = 0; val < 256; val++) {
        uint32_t mask = 0;
        if (val & (1<<0)) mask |= (1<<LCD_D0);
        if (val & (1<<1)) mask |= (1<<LCD_D1);
        if (val & (1<<2)) mask |= (1<<LCD_D2);
        if (val & (1<<3)) mask |= (1<<LCD_D3);
        if (val & (1<<4)) mask |= (1<<LCD_D4);
        if (val & (1<<5)) mask |= (1<<LCD_D5);
        if (val & (1<<6)) mask |= (1<<LCD_D6);
        if (val & (1<<7)) mask |= (1<<LCD_D7);
        data_lut[val] = mask;
    }
}

void gpio_mmap_cleanup(void)
{
    if (gpio_base && gpio_base != MAP_FAILED) {
        munmap((void*)gpio_base, GPIO_MAP_SIZE);
        gpio_base = NULL;
    }
}

static inline void set_line(uint8_t pin, int val)
{
    if (val) GPIO_SET = (1 << pin);
    else     GPIO_CLR = (1 << pin);
}

static inline void set_data_bus(uint8_t value)
{
    GPIO_CLR = DATA_PIN_MASK;          // clear all data pins
    GPIO_SET = data_lut[value];        // set the required ones high
}

// Burst write multiple data bytes with CS low throughout
// Assume RS is already set to data mode before calling this function
void burst_write_bytes(const uint8_t *data, size_t len)
{
    set_line(LCD_RS, 1);
    set_line(LCD_CS, 0);

    for (size_t i = 0; i < len; i++) {
        GPIO_CLR = DATA_PIN_MASK;
        GPIO_SET = data_lut[data[i]];

        for(int n = 0; n < WR_SETUP_NOPS; n++) __asm__ volatile("nop");
        GPIO_CLR = (1 << LCD_WR);
        for(int n = 0; n < WR_HOLD_NOPS; n++) __asm__ volatile("nop");

        GPIO_SET = (1 << LCD_WR);
        for(int n = 0; n < WR_CYCLE_NOPS; n++) __asm__ volatile("nop");
    }

    set_line(LCD_CS, 1);
}

// Write one 8-bit command
void write_cmd(uint8_t cmd)
{
    set_line(LCD_RS, 0);
    __asm__ volatile("nop"); __asm__ volatile("nop");
    set_line(LCD_CS, 0);
    __asm__ volatile("nop"); __asm__ volatile("nop");

    set_data_bus(cmd);

    for(int n = 0; n < WR_SETUP_NOPS; n++) __asm__ volatile("nop");
    set_line(LCD_WR, 0);
    __asm__ volatile("nop"); __asm__ volatile("nop");
    set_line(LCD_WR, 1);
    set_line(LCD_CS, 1);
}

// Write one 8-bit data value
void write_data(uint8_t data)
{
    set_line(LCD_RS, 1);
    __asm__ volatile("nop"); __asm__ volatile("nop");
    set_line(LCD_CS, 0);
    __asm__ volatile("nop"); __asm__ volatile("nop");

    set_data_bus(data);

    for(int n = 0; n < WR_SETUP_NOPS; n++) __asm__ volatile("nop");
    set_line(LCD_WR, 0);
    __asm__ volatile("nop"); __asm__ volatile("nop");
    set_line(LCD_WR, 1);
    set_line(LCD_CS, 1);
}

// Sends a 24-bit colour (0x00RRGGBB) as 3 bytes to the display directly
// (bypasses backbuffer — use for direct hardware writes only)
void write_color24(uint32_t color)
{
    write_data((color >> 16) & 0xFF);  // R
    write_data((color >> 8)  & 0xFF);  // G
    write_data( color        & 0xFF);  // B
}

// Legacy helper kept for any callers that still pass RGB565.
// Expands to 24-bit and writes directly to hardware (no backbuffer).
void write_data16(uint16_t color)
{
    uint8_t r5 = (color >> 11) & 0x1F;
    uint8_t g6 = (color >> 5)  & 0x3F;
    uint8_t b5 =  color        & 0x1F;

    write_data((r5 << 3) | (r5 >> 2));
    write_data((g6 << 2) | (g6 >> 4));
    write_data((b5 << 3) | (b5 >> 2));
}

void write_coord16(uint16_t value)
{
    write_data((value >> 8) & 0xFF);
    write_data(value & 0xFF);
}

void delay(uint32_t ms)
{
    usleep(ms * 1000);
}

void reset_dirty(void)
{
    dirty_x0 = TFT_WIDTH;
    dirty_y0 = TFT_HEIGHT;
    dirty_x1 = 0;
    dirty_y1 = 0;
}

void expand_dirty(uint16_t x, uint16_t y, uint16_t w, uint16_t h)
{
    if (x < dirty_x0) dirty_x0 = x;
    if (y < dirty_y0) dirty_y0 = y;
    if ((x + w) > dirty_x1) dirty_x1 = x + w;
    if ((y + h) > dirty_y1) dirty_y1 = y + h;
}

// Flush dirty region of backbuffer directly to display.
void flush_backbuffer(void)
{
    if (dirty_x0 >= dirty_x1 || dirty_y0 >= dirty_y1) return;

    uint16_t w = dirty_x1 - dirty_x0;
    uint16_t h = dirty_y1 - dirty_y0;

    set_window(dirty_x0, dirty_y0, dirty_x1 - 1, dirty_y1 - 1);

    // If the dirty region is the full width we can burst the whole block at once
    if (dirty_x0 == 0 && w == TFT_WIDTH) {
        size_t offset = (size_t)dirty_y0 * TFT_WIDTH * 3;
        burst_write_bytes(backbuffer + offset, (size_t)w * h * 3);
    } else {
        // Partial width — send row by row
        for (uint16_t j = 0; j < h; j++) {
            size_t offset = (size_t)(dirty_y0 + j) * TFT_WIDTH * 3 + dirty_x0 * 3;
            burst_write_bytes(backbuffer + offset, (size_t)w * 3);
        }
    }

    reset_dirty();
}

void set_window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1)
{
    write_cmd(0x2A);
    write_coord16(x0);
    write_coord16(x1);

    write_cmd(0x2B);
    write_coord16(y0);
    write_coord16(y1);

    write_cmd(0x2C);
}

// color is 0x00RRGGBB
void set_pixel(uint16_t x, uint16_t y, uint32_t color)
{
    if (x >= TFT_WIDTH || y >= TFT_HEIGHT) return;
    size_t idx = ((size_t)y * TFT_WIDTH + x) * 3;
    backbuffer[idx + 0] = (color >> 16) & 0xFF;  // R
    backbuffer[idx + 1] = (color >> 8)  & 0xFF;  // G
    backbuffer[idx + 2] =  color        & 0xFF;  // B
}

void fill_screen(uint32_t color)
{
    expand_dirty(0, 0, TFT_WIDTH, TFT_HEIGHT);
    uint8_t r = (color >> 16) & 0xFF;
    uint8_t g = (color >> 8)  & 0xFF;
    uint8_t b =  color        & 0xFF;
    size_t total = (size_t)TFT_WIDTH * TFT_HEIGHT;
    for (size_t i = 0; i < total; i++) {
        backbuffer[i * 3 + 0] = r;
        backbuffer[i * 3 + 1] = g;
        backbuffer[i * 3 + 2] = b;
    }
}

void fill_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint32_t color)
{
    expand_dirty(x, y, w, h);
    uint8_t r = (color >> 16) & 0xFF;
    uint8_t g = (color >> 8)  & 0xFF;
    uint8_t b =  color        & 0xFF;
    for (int j = 0; j < h; j++) {
        size_t row_start = ((size_t)(y + j) * TFT_WIDTH + x) * 3;
        for (int i = 0; i < w; i++) {
            backbuffer[row_start + i * 3 + 0] = r;
            backbuffer[row_start + i * 3 + 1] = g;
            backbuffer[row_start + i * 3 + 2] = b;
        }
    }
}

void draw_line(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, uint32_t color)
{
    uint16_t bx = x0 < x1 ? x0 : x1;
    uint16_t by = y0 < y1 ? y0 : y1;
    uint16_t bw = (x0 < x1 ? x1 - x0 : x0 - x1) + 1;
    uint16_t bh = (y0 < y1 ? y1 - y0 : y0 - y1) + 1;
    expand_dirty(bx, by, bw, bh);

    uint8_t r = (color >> 16) & 0xFF;
    uint8_t g = (color >> 8)  & 0xFF;
    uint8_t b =  color        & 0xFF;

    int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy, e2;

    while (1) {
        size_t idx = ((size_t)y0 * TFT_WIDTH + x0) * 3;
        backbuffer[idx + 0] = r;
        backbuffer[idx + 1] = g;
        backbuffer[idx + 2] = b;
        if (x0 == x1 && y0 == y1) break;
        e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

void draw_char(uint16_t x, uint16_t y, char c, uint32_t fg)
{
    if ((unsigned char)c > FONT_LAST) c = '?';
    
    const unsigned char *glyph = &cp437font8x8[6 + ((unsigned char)c * FONT_HEIGHT)];
    expand_dirty(x, y, FONT_WIDTH, FONT_HEIGHT);
    
    for (int row = 0; row < FONT_HEIGHT; row++) {
        unsigned char bits = glyph[row];
        for (int col = 0; col < FONT_WIDTH; col++) {
            if (bits & (0x80 >> col))
                set_pixel(x + row, y + col, fg);
        }
    }
}

void draw_char_scaled(uint16_t x, uint16_t y, char c, uint8_t scale, uint32_t fg)
{
    if (scale == 0) return;
    if ((unsigned char)c > FONT_LAST) c = '?';
    
    const unsigned char *glyph = &cp437font8x8[6 + ((unsigned char)c * FONT_HEIGHT)];
    expand_dirty(x, y, FONT_WIDTH * scale, FONT_HEIGHT * scale);
    
    for (int row = 0; row < FONT_HEIGHT; row++) {
        unsigned char bits = glyph[row];
        for (int col = 0; col < FONT_WIDTH; col++) {
            if (bits & (0x80 >> col))
                fill_rect(x + row * scale, y + col * scale, scale, scale, fg);
        }
    }
}

void draw_string(uint16_t x, uint16_t y, const char *str, uint32_t fg)
{
    uint16_t cursor_x = x;
    while (*str) {
        if (*str == '\n') { cursor_x = x; y += FONT_HEIGHT; str++; continue; }
        if (cursor_x + FONT_WIDTH > TFT_WIDTH) { cursor_x = x; y += FONT_HEIGHT; }
        if (y + FONT_HEIGHT > TFT_HEIGHT) break;
        draw_char(cursor_x, y, *str, fg);
        cursor_x += FONT_WIDTH;
        str++;
    }
}

void draw_string_scaled(uint16_t x, uint16_t y, const char *str, uint8_t scale, uint32_t fg)
{
    uint16_t cursor_x = x;
    uint16_t line_height = FONT_HEIGHT * scale;
    while (*str) {
        if (*str == '\n') { cursor_x = x; y += line_height; str++; continue; }
        if (cursor_x + FONT_WIDTH * scale > TFT_WIDTH) { cursor_x = x; y += line_height; }
        if (y + line_height > TFT_HEIGHT) break;
        draw_char_scaled(cursor_x, y, *str, scale, fg);
        cursor_x += FONT_WIDTH * scale;
        str++;
    }
}

// bitmap pixels are 0x00RRGGBB; transparent_color = 0xFFFFFFFF to disable
void draw_bitmap(uint16_t x, uint16_t y, uint16_t w, uint16_t h,
                 const uint32_t *bitmap, uint32_t transparent_color)
{
    expand_dirty(x, y, w, h);
    for (uint16_t j = 0; j < h; j++) {
        for (uint16_t i = 0; i < w; i++) {
            uint32_t color = bitmap[j * w + i];
            if (color != transparent_color)
                set_pixel(x + i, y + j, color);
        }
    }
}

void draw_bitmap_mono(uint16_t x, uint16_t y, uint16_t w, uint16_t h,
                      const uint8_t *bitmap, uint32_t color)
{
    for (uint16_t j = 0; j < h; j++) {
        for (uint16_t i = 0; i < w; i++) {
            uint32_t bit_index  = j * w + i;
            uint32_t byte_index = bit_index / 8;
            uint8_t  bit        = 7 - (bit_index % 8);
            if (bitmap[byte_index] & (1 << bit))
                set_pixel(x + i, y + j, color);
        }
    }
}

void ili9481_reset(void)
{
    usleep(10000);
    set_line(LCD_RST, 1);
    usleep(1000);
    set_line(LCD_RST, 0);
    usleep(10000);
    set_line(LCD_RST, 1);
    usleep(120000);
}

void ili9481_start(void)
{
    gpio_mmap_init();
    ili9481_reset();
    ili9481_init();
    reset_dirty();
}

void ili9481_stop(void)
{
    gpio_mmap_cleanup();
}