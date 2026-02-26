// ili9481_parallel.c
#include "ili9481_parallel.h"

unsigned int data_gpios[8] = {LCD_D0, LCD_D1, LCD_D2, LCD_D3, LCD_D4, LCD_D5, LCD_D6, LCD_D7};

volatile uint8_t *gpio_base = NULL;
uint32_t data_lut[256];

uint16_t backbuffer[TFT_WIDTH * TFT_HEIGHT];

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
        GPIO_CLR = (1 << LCD_WR);     // WR low
        GPIO_SET = (1 << LCD_WR);     // WR high
    }

    set_line(LCD_CS, 1);
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

// Write one 8-bit command
void write_cmd(uint8_t cmd)
{
    set_line(LCD_RS, 0);  // RS/DC = 0 for command
    set_line(LCD_CS, 0);  // CS low

    set_data_bus(cmd);

    set_line(LCD_WR, 0);  // WR pulse
    set_line(LCD_WR, 1);
    set_line(LCD_CS, 1);  // CS high
}

// Write one 8-bit data value
void write_data(uint8_t data)
{
    set_line(LCD_RS, 1);  // RS/DC = 1 for data
    set_line(LCD_CS, 0);  // CS low

    set_data_bus(data);

    set_line(LCD_WR, 0);
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
    gpio_mmap_init();       // replaces all the gpiod chip/line setup
    ili9481_reset();
    ili9481_init();
}

void ili9481_stop(void)
{
    gpio_mmap_cleanup();
}