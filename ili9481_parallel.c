// ili9481_parallel.c
#include "ili9481_parallel.h"

// nop count a variable you can change at runtime
#define WR_SETUP_NOPS  4    
#define WR_HOLD_NOPS   4    
#define WR_CYCLE_NOPS  4 

unsigned int data_gpios[8] = {LCD_D0, LCD_D1, LCD_D2, LCD_D3, LCD_D4, LCD_D5, LCD_D6, LCD_D7};

volatile uint8_t *gpio_base = NULL;
uint32_t data_lut[256];

uint16_t backbuffer[TFT_WIDTH * TFT_HEIGHT];
static uint16_t dirty_x0, dirty_y0, dirty_x1, dirty_y1; // dirty region tracking

static void gpio_set_output(uint8_t pin)
{
    // Each GPFSEL register controls 10 pins, 3 bits each
    uint32_t reg_offset = (pin / 10) * 4;          // which GPFSEL register (offset in bytes)
    uint32_t bit_offset = (pin % 10) * 3;          // which 3-bit field within that register
    volatile uint32_t *fsel = (volatile uint32_t*)(gpio_base + reg_offset);
    *fsel = (*fsel & ~(7 << bit_offset)) | (1 << bit_offset);  // set to 001 = output
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

    // Set all our pins to output mode
    uint8_t all_pins[] = {LCD_RD, LCD_WR, LCD_RS, LCD_CS, LCD_RST,
                          LCD_D0, LCD_D1, LCD_D2, LCD_D3,
                          LCD_D4, LCD_D5, LCD_D6, LCD_D7};
    for (int i = 0; i < 13; i++)
        gpio_set_output(all_pins[i]);

    // Drive control lines high (idle state)
    GPIO_SET = (1<<LCD_RD)|(1<<LCD_WR)|(1<<LCD_RS)|(1<<LCD_CS)|(1<<LCD_RST);

    // Precompute 256-entry LUT for data bus bit scatter
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

// Flush full backbuffer to display (converts to BGR666 bytes)
void flush_backbuffer(void) {
    // if nothing dirty -> return
    if (dirty_x0 >= dirty_x1 || dirty_y0 >= dirty_y1) return;
    
    static uint8_t buf[TFT_WIDTH * TFT_HEIGHT * 3];  // 3 bytes per pixel for BGR666
    
    uint16_t w = dirty_x1 - dirty_x0;
    uint16_t h = dirty_y1 - dirty_y0;
    int idx = 0;

    for (int j = 0; j < h; j++) {
        for (int i = 0; i < w; i++) {
            uint16_t color = backbuffer[(dirty_y0 + j) * TFT_WIDTH + (dirty_x0 + i)];
            uint8_t b = (color & 0x1F) << 3;
            uint8_t g = ((color >> 5) & 0x3F) << 2;
            uint8_t r = ((color >> 11) & 0x1F) << 3;
            buf[idx++] = r;
            buf[idx++] = g;
            buf[idx++] = b;
        }
    }

    set_window(dirty_x0, dirty_y0, dirty_x1 - 1, dirty_y1 - 1);
    burst_write_bytes(buf, (size_t)w * h * 3);
    reset_dirty();
}

// Write one 8-bit command
void write_cmd(uint8_t cmd)
{
    set_line(LCD_RS, 0);  // RS/DC = 0 for command
    __asm__ volatile("nop"); __asm__ volatile("nop");  
    set_line(LCD_CS, 0);  // CS low
    __asm__ volatile("nop"); __asm__ volatile("nop");  

    set_data_bus(cmd);

    for(int n = 0; n < WR_SETUP_NOPS; n++) __asm__ volatile("nop");
    set_line(LCD_WR, 0);  // WR pulse
    __asm__ volatile("nop"); __asm__ volatile("nop");      
    set_line(LCD_WR, 1);
    set_line(LCD_CS, 1);  // CS high
}

// Write one 8-bit data value
void write_data(uint8_t data)
{
    set_line(LCD_RS, 1);  // RS/DC = 1 for data
    __asm__ volatile("nop"); __asm__ volatile("nop");  
    set_line(LCD_CS, 0);  // CS low
    __asm__ volatile("nop"); __asm__ volatile("nop");  

    set_data_bus(data);

    for(int n = 0; n < WR_SETUP_NOPS; n++) __asm__ volatile("nop");
    set_line(LCD_WR, 0);
    __asm__ volatile("nop"); __asm__ volatile("nop");  
    set_line(LCD_WR, 1);
    set_line(LCD_CS, 1);  // CS high
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

void delay(uint32_t ms){
    usleep(ms * 1000);
}

void reset_dirty(void) {
    dirty_x0 = TFT_WIDTH;
    dirty_y0 = TFT_HEIGHT;
    dirty_x1 = 0;
    dirty_y1 = 0;
}

void expand_dirty(uint16_t x, uint16_t y, uint16_t w, uint16_t h) {
    if (x < dirty_x0) dirty_x0 = x;
    if (y < dirty_y0) dirty_y0 = y;
    if ((x + w) > dirty_x1) dirty_x1 = x + w;
    if ((y + h) > dirty_y1) dirty_y1 = y + h;
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

void set_pixel(uint16_t x, uint16_t y, uint16_t color) {
    if (x >= TFT_WIDTH || y >= TFT_HEIGHT) return;
    backbuffer[y * TFT_WIDTH + x] = color;
}

void fill_screen(uint16_t color) {
    expand_dirty(0, 0, TFT_WIDTH, TFT_HEIGHT); 
    for (int i = 0; i < TFT_WIDTH * TFT_HEIGHT; i++) {
        backbuffer[i] = color;
    }
}

void fill_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color) {
    expand_dirty(x, y, w, h);
    for (int j = 0; j < h; j++) {
        for (int i = 0; i < w; i++) {
            set_pixel(x + i, y + j, color);
        }
    }
}

void draw_line(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, uint16_t color) {
    // add dirty region
    uint16_t bx = x0 < x1 ? x0 : x1;
    uint16_t by = y0 < y1 ? y0 : y1;
    uint16_t bw = (x0 < x1 ? x1 - x0 : x0 - x1) + 1;
    uint16_t bh = (y0 < y1 ? y1 - y0 : y0 - y1) + 1;
    expand_dirty(bx, by, bw, bh);
    
    // Bresenham's line algorithm
    int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1; 
    int err = dx + dy, e2;

    while (1) {
        backbuffer[y0 * TFT_WIDTH + x0] = color;
        if (x0 == x1 && y0 == y1) break;
        e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

void draw_char(uint16_t x, uint16_t y, char c, uint16_t fg)
{
    if ((unsigned char)c > FONT_LAST) c = '?';

    const unsigned char *glyph = &cp437font8x8[6 + ((unsigned char)c * FONT_HEIGHT)];

    expand_dirty(x, y, FONT_WIDTH, FONT_HEIGHT);

    for (int row = 0; row < FONT_HEIGHT; row++) {
        unsigned char bits = glyph[row];
        for (int col = 0; col < FONT_WIDTH; col++) {
            if (bits & (0x80 >> col)) {
                set_pixel(x + row, y + col, fg);
            }
        }
    }
}

void draw_char_scaled(uint16_t x, uint16_t y, char c, uint8_t scale, uint16_t fg)
{
    if (scale == 0) return;
    if ((unsigned char)c > FONT_LAST) c = '?';

    const unsigned char *glyph = &cp437font8x8[6 + ((unsigned char)c * FONT_HEIGHT)];

    expand_dirty(x, y, FONT_WIDTH * scale, FONT_HEIGHT * scale);

    for (int row = 0; row < FONT_HEIGHT; row++) {
        unsigned char bits = glyph[row];
        for (int col = 0; col < FONT_WIDTH; col++) {
            if (bits & (0x80 >> col)) {
                // fill a scale×scale block
                fill_rect(x + row * scale, y + col * scale, scale, scale, fg);
            }
        }
    }
}

void draw_string(uint16_t x, uint16_t y, const char *str, uint16_t fg)
{
    uint16_t cursor_x = x;

    while (*str) {
        // Handle newline
        if (*str == '\n') {
            cursor_x = x;
            y += FONT_HEIGHT;
            str++;
            continue;
        }

        // Stop if we'd go off screen horizontally — wrap to next line
        if (cursor_x + FONT_WIDTH > TFT_WIDTH) {
            cursor_x = x;
            y += FONT_HEIGHT;
        }

        // Stop if we'd go off screen vertically
        if (y + FONT_HEIGHT > TFT_HEIGHT) break;

        draw_char(cursor_x, y, *str, fg);
        cursor_x += FONT_WIDTH;
        str++;
    }
}

void draw_string_scaled(uint16_t x, uint16_t y, const char *str,
                        uint8_t scale, uint16_t fg)
{
    uint16_t cursor_x = x;
    uint16_t line_height = FONT_HEIGHT * scale;

    while (*str) {
        if (*str == '\n') {
            cursor_x = x;
            y += line_height;
            str++;
            continue;
        }
        if (cursor_x + FONT_WIDTH * scale > TFT_WIDTH) {
            cursor_x = x;
            y += line_height;
        }
        if (y + line_height > TFT_HEIGHT) break;

        draw_char_scaled(cursor_x, y, *str, scale, fg);
        cursor_x += FONT_WIDTH * scale;
        str++;
    }
}

void draw_bitmap(uint16_t x, uint16_t y, uint16_t w, uint16_t h,
                 const uint16_t *bitmap, uint16_t transparent_color)
{
    expand_dirty(x, y, w, h);

    for (uint16_t j = 0; j < h; j++) {
        for (uint16_t i = 0; i < w; i++) {
            uint16_t color = bitmap[j * w + i];
            if (color != transparent_color) {          // -1 = no transparency
                set_pixel(x + i, y + j, color);
            }
        }
    }
}

void draw_bitmap_mono(uint16_t x, uint16_t y,
                      uint16_t w, uint16_t h,
                      const uint8_t *bitmap,
                      uint16_t color)
{
    for (uint16_t j = 0; j < h; j++)
    {
        for (uint16_t i = 0; i < w; i++)
        {
            uint32_t bit_index = j * w + i;
            uint32_t byte_index = bit_index / 8;
            uint8_t bit = 7 - (bit_index % 8);

            if (bitmap[byte_index] & (1 << bit))
            {
                set_pixel(x + i, y + j, color);
            }
        }
    }
}

void ili9481_reset(void)
{
    // VCI power stable
    usleep(10000);  // Wait 10ms after power on
    
    set_line(LCD_RST, 1);
    usleep(1000);   // RESX high for 1ms
    
    set_line(LCD_RST, 0);
    usleep(10000);  // RESX low for at least 10μs 
    
    set_line(LCD_RST, 1);
    usleep(120000); // Wait 120ms before sending commands 
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