// ili9481_parallel.c
// ILI9481 driver using libgpiod API on Raspberry Pi Zero 2 W

#include "ili9481_parallel.h"

// gpiod variables
static struct gpiod_chip *chip;
static struct gpiod_line_request *rd_req, *wr_req, *rs_req, *cs_req, *rst_req;
static struct gpiod_line_request *data_req = NULL;

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

static struct gpiod_line_request *req_out_lines(unsigned int *gpios, unsigned int num_gpios, const char *name, int init_val)
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

static void set_line(struct gpiod_line_request *req, unsigned int offset, int val)
{
    enum gpiod_line_value value = val ? GPIOD_LINE_VALUE_ACTIVE : GPIOD_LINE_VALUE_INACTIVE;
    if (gpiod_line_request_set_value(req, offset, value) < 0) {
        fprintf(stderr, "Failed to set line value\n");
        exit(1);
    }
}

// Burst write multiple data bytes with CS low throughout
// Assume RS is already set to data mode before calling this function
static void burst_write_bytes(const uint8_t *data, size_t len)
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
static void flush_backbuffer(void) {
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

static void set_data_bus(uint8_t value)
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
static void write_cmd(uint8_t cmd)
{
    set_line(rs_req, LCD_RS, 0);  // RS/DC = 0 for command
    set_line(cs_req, LCD_CS, 0);  // CS low

    set_data_bus(cmd);

    set_line(wr_req, LCD_WR, 0);  // WR pulse
    set_line(wr_req, LCD_WR, 1);
    set_line(cs_req, LCD_CS, 1);  // CS high
}

static void delay(uint32_t ms){
    usleep(ms * 1000);
}

// Write one 8-bit data value
static void write_data(uint8_t data)
{
    set_line(rs_req, LCD_RS, 1);  // RS/DC = 1 for data
    set_line(cs_req, LCD_CS, 0);  // CS low

    set_data_bus(data);

    set_line(wr_req, LCD_WR, 0);
    set_line(wr_req, LCD_WR, 1);
    set_line(cs_req, LCD_CS, 1);  // CS high
}

// 18-bit color mode (BGR666) - sends 3 bytes per pixel
static void write_data16(uint16_t color)
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
static void write_coord16(uint16_t value)
{
    write_data((value >> 8) & 0xFF);   // High byte
    write_data(value & 0xFF);           // Low byte
}

static void ili9481_reset(void)
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
static void set_window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1)
{
    write_cmd(0x2A);             // Column address set
    write_coord16(x0);
    write_coord16(x1);

    write_cmd(0x2B);             // Page address set
    write_coord16(y0);
    write_coord16(y1);

    write_cmd(0x2C);             // Memory write
}

static void fill_screen(uint16_t color) {
    for (int i = 0; i < TFT_WIDTH * TFT_HEIGHT; i++) {
        backbuffer[i] = color;
    }
    flush_backbuffer();
}

 // Function to reverse bits
static uint8_t reverse_bits(uint8_t b) {
    uint8_t r = 0;
    for (int i = 0; i < 8; i++) {
        r = (r << 1) | ((b >> i) & 1);
    }
    return r;
}

static void test_data_bus_pattern(void)
{
    printf("Testing data bus pattern...\n");
    printf("Connect logic analyzer or LED bar to D0-D7\n");
    printf("You should see a walking bit pattern:\n");
    
    // turn all LEDs on for a second (verify wiring)
    set_data_bus(0xFF);
    sleep(1);
    set_data_bus(0x00);

    // Walking 1s pattern (Check Pattern)
    for (int i = 0; i < 8; i++) {
        uint8_t pattern = (1 << i);
        printf("Setting data bus to 0x%02X (bit %d high)\n", pattern, i);
        set_data_bus(pattern);
        sleep(1);
    }
}

static void test_suite(void)
{
    printf("\n");
    printf("================================================================\n");
    printf("    ILI9481 COMPREHENSIVE DIAGNOSTIC TEST SUITE\n");
    printf("================================================================\n");
    printf("\n");

    // Keep RD high always
    set_line(rd_req, LCD_RD, 1);

    // ====================================================================
    // TEST 1: Data Bus Bit Pattern Test
    // ====================================================================
    test_data_bus_pattern();

    // ====================================================================
    // TEST 2: Hardware Reset
    // ====================================================================
    printf("TEST 2: HARDWARE RESET\n");
    printf("----------------------\n");
    printf("Performing hardware reset sequence...\n");
    
    set_line(rst_req, LCD_RST, 1);
    printf("  RST = HIGH\n");
    usleep(10000);
    
    set_line(rst_req, LCD_RST, 0);
    printf("  RST = LOW (50ms)\n");
    usleep(50000);
    
    set_line(rst_req, LCD_RST, 1);
    printf("  RST = HIGH (waiting 200ms)\n");
    usleep(200000);
    
    printf("Reset complete.\n");
    printf("Press Enter to continue...");
    getchar();
    printf("\n");

    // ====================================================================
    // TEST 3: Basic Command Test (Normal WR Polarity)
    // ====================================================================
    printf("TEST 3: BASIC COMMAND TEST (Normal WR Polarity)\n");
    printf("------------------------------------------------\n");
    printf("Testing with WR idle HIGH, pulse LOW (standard)\n\n");
    
    printf("Sending Software Reset (0x01)...\n");
    write_cmd(0x01);
    usleep(200000);
    
    printf("Sending Sleep Out (0x11)...\n");
    write_cmd(0x11);
    usleep(120000);
    
    printf("Sending Display ON (0x29)...\n");
    write_cmd(0x29);
    usleep(100000);
    
    printf("\nWatch the display! Testing inversion toggle (5 cycles)...\n");
    printf("You should see flashing or color changes if working.\n\n");
    
    for (int i = 0; i < 5; i++) {
        printf("  Cycle %d: Inversion ON (0x21)...\n", i+1);
        write_cmd(0x21);
        sleep(1);
        
        printf("  Cycle %d: Inversion OFF (0x20)...\n", i+1);
        write_cmd(0x20);
        sleep(1);
    }
    
    printf("\nDid you see ANY change on the display? (y/n): ");
    char response;
    scanf(" %c", &response);
    getchar(); // consume newline
    
    if (response == 'y' || response == 'Y') {
        printf("\n✓ GREAT! Communication is working!\n");
        printf("  Moving to full initialization...\n\n");
        goto full_init;
    } else {
        printf("\n✗ No change detected. Trying alternative tests...\n\n");
    }

    // ====================================================================
    // TEST 4: Inverted RS/DC Polarity Test
    // ====================================================================
    printf("TEST 4: INVERTED RS/DC POLARITY TEST\n");
    printf("-------------------------------------\n");
    printf("Some shields have inverted RS/DC logic.\n");
    printf("Testing RS=1 for command (inverted)...\n\n");
    
    // Hardware reset again
    set_line(rst_req, LCD_RST, 0);
    usleep(50000);
    set_line(rst_req, LCD_RST, 1);
    usleep(200000);
    
    // Send commands with INVERTED RS
    printf("Sending commands with RS inverted...\n");
    
    // Sleep Out with inverted RS
    set_line(rs_req, LCD_RS, 1);  // INVERTED (normally would be 0)
    set_line(cs_req, LCD_CS, 0);
    usleep(10);
    set_data_bus(0x11);
    usleep(10);
    set_line(wr_req, LCD_WR, 0);
    usleep(10);
    set_line(wr_req, LCD_WR, 1);
    usleep(10);
    set_line(cs_req, LCD_CS, 1);
    usleep(120000);
    
    // Display ON with inverted RS
    set_line(rs_req, LCD_RS, 1);  // INVERTED
    set_line(cs_req, LCD_CS, 0);
    usleep(10);
    set_data_bus(0x29);
    usleep(10);
    set_line(wr_req, LCD_WR, 0);
    usleep(10);
    set_line(wr_req, LCD_WR, 1);
    usleep(10);
    set_line(cs_req, LCD_CS, 1);
    usleep(100000);
    
    printf("\nDid you see ANY change now? (y/n): ");
    scanf(" %c", &response);
    getchar();
    
    if (response == 'y' || response == 'Y') {
        printf("\n✓ RS/DC is INVERTED on your shield!\n");
        printf("  You need to flip RS logic in your code.\n");
        printf("  Change: RS=0 for cmd → RS=1 for cmd\n");
        printf("          RS=1 for data → RS=0 for data\n\n");
        goto cleanup;
    } else {
        printf("\n✗ Still no change. Trying inverted WR...\n\n");
    }

    // ====================================================================
    // TEST 5: Inverted WR Polarity Test
    // ====================================================================
    printf("TEST 5: INVERTED WR POLARITY TEST\n");
    printf("----------------------------------\n");
    printf("Some shields use WR idle LOW, pulse HIGH.\n");
    printf("Testing inverted WR...\n\n");
    
    // Hardware reset again
    set_line(rst_req, LCD_RST, 0);
    usleep(50000);
    set_line(rst_req, LCD_RST, 1);
    usleep(200000);
    
    // Sleep Out with inverted WR
    printf("Sending Sleep Out (0x11) with WR inverted...\n");
    set_line(rs_req, LCD_RS, 0);
    set_line(cs_req, LCD_CS, 0);
    set_line(wr_req, LCD_WR, 0);  // WR idle LOW (inverted)
    usleep(10);
    set_data_bus(0x11);
    usleep(10);
    set_line(wr_req, LCD_WR, 1);  // WR pulse HIGH (inverted)
    usleep(10);
    set_line(wr_req, LCD_WR, 0);  // Back to idle LOW
    usleep(10);
    set_line(cs_req, LCD_CS, 1);
    usleep(120000);
    
    // Display ON with inverted WR
    printf("Sending Display ON (0x29) with WR inverted...\n");
    set_line(rs_req, LCD_RS, 0);
    set_line(cs_req, LCD_CS, 0);
    set_line(wr_req, LCD_WR, 0);  // WR idle LOW
    usleep(10);
    set_data_bus(0x29);
    usleep(10);
    set_line(wr_req, LCD_WR, 1);  // WR pulse HIGH
    usleep(10);
    set_line(wr_req, LCD_WR, 0);  // Back to idle LOW
    usleep(10);
    set_line(cs_req, LCD_CS, 1);
    usleep(100000);
    
    printf("\nDid you see ANY change now? (y/n): ");
    scanf(" %c", &response);
    getchar();
    
    if (response == 'y' || response == 'Y') {
        printf("\n✓ WR is INVERTED on your shield!\n");
        printf("  You need to flip WR logic in your code.\n");
        printf("  Change: WR idle=1, pulse to 0 → WR idle=0, pulse to 1\n\n");
        goto cleanup;
    } else {
        printf("\n✗ Still no change.\n\n");
    }

    // ====================================================================
    // TEST 6: Both RS and WR Inverted
    // ====================================================================
    printf("TEST 6: BOTH RS AND WR INVERTED TEST\n");
    printf("-------------------------------------\n");
    printf("Testing with both RS and WR inverted...\n\n");
    
    // Hardware reset again
    set_line(rst_req, LCD_RST, 0);
    usleep(50000);
    set_line(rst_req, LCD_RST, 1);
    usleep(200000);
    
    // Sleep Out with BOTH inverted
    set_line(rs_req, LCD_RS, 1);  // RS INVERTED
    set_line(cs_req, LCD_CS, 0);
    set_line(wr_req, LCD_WR, 0);  // WR idle LOW
    usleep(10);
    set_data_bus(0x11);
    usleep(10);
    set_line(wr_req, LCD_WR, 1);  // WR pulse HIGH
    usleep(10);
    set_line(wr_req, LCD_WR, 0);
    usleep(10);
    set_line(cs_req, LCD_CS, 1);
    usleep(120000);
    
    // Display ON with BOTH inverted
    set_line(rs_req, LCD_RS, 1);  // RS INVERTED
    set_line(cs_req, LCD_CS, 0);
    set_line(wr_req, LCD_WR, 0);  // WR idle LOW
    usleep(10);
    set_data_bus(0x29);
    usleep(10);
    set_line(wr_req, LCD_WR, 1);  // WR pulse HIGH
    usleep(10);
    set_line(wr_req, LCD_WR, 0);
    usleep(10);
    set_line(cs_req, LCD_CS, 1);
    usleep(100000);
    
    printf("\nDid you see ANY change now? (y/n): ");
    scanf(" %c", &response);
    getchar();
    
    if (response == 'y' || response == 'Y') {
        printf("\n✓ BOTH RS and WR are INVERTED!\n\n");
        goto cleanup;
    }

    // ====================================================================
    // TEST 7: Data Bus Bit Order Check
    // ====================================================================
    printf("\n");
    printf("TEST 7: DATA BUS BIT ORDER VERIFICATION\n");
    printf("----------------------------------------\n");
    printf("Your data bus might be wired in reverse or scrambled.\n");
    printf("Common configurations:\n");
    printf("  Normal:   D0=LSB, D7=MSB\n");
    printf("  Reversed: D0=MSB, D7=LSB\n\n");
    
    printf("Try these commands with REVERSED bit order:\n\n");
    
    // Hardware reset
    set_line(rst_req, LCD_RST, 0);
    usleep(50000);
    set_line(rst_req, LCD_RST, 1);
    usleep(200000);
    
    // Sleep Out (0x11) with reversed bits
    uint8_t cmd_reversed = reverse_bits(0x11);
    printf("Sending 0x11 as 0x%02X (reversed bits)...\n", cmd_reversed);
    set_line(rs_req, LCD_RS, 0);
    set_line(cs_req, LCD_CS, 0);
    usleep(10);
    set_data_bus(cmd_reversed);
    usleep(10);
    set_line(wr_req, LCD_WR, 0);
    usleep(10);
    set_line(wr_req, LCD_WR, 1);
    usleep(10);
    set_line(cs_req, LCD_CS, 1);
    usleep(120000);
    
    printf("\nDid you see ANY change? (y/n): ");
    scanf(" %c", &response);
    getchar();
    
    if (response == 'y' || response == 'Y') {
        printf("\n✓ Your data bus is BIT-REVERSED!\n");
        printf("  You need to reverse the bit order in set_data_bus()\n\n");
        goto cleanup;
    }

    // ====================================================================
    // If we got here, nothing worked
    // ====================================================================
    printf("\n");
    printf("================================================================\n");
    printf("  DIAGNOSTIC SUMMARY: NO COMMUNICATION DETECTED\n");
    printf("================================================================\n");
    printf("\n");
    printf("Possible issues:\n");
    printf("  1. Wrong GPIO pin assignments (D0-D7, RS, WR, CS, RST)\n");
    printf("  2. Loose or incorrect wiring\n");
    printf("  3. Display needs different power sequence\n");
    printf("  4. Shield is in SPI mode, not parallel mode\n");
    printf("  5. Data bus is scrambled (not just reversed)\n");
    printf("\n");
    printf("Next steps:\n");
    printf("  - Verify pin mapping matches your shield\n");
    printf("  - Check continuity with multimeter\n");
    printf("  - Look for IM0-IM3 configuration pins on shield\n");
    printf("  - Compare with working Arduino wiring\n");
    printf("\n");
    goto cleanup;

    // ====================================================================
    // Full Initialization (if basic test worked)
    // ====================================================================
full_init:
    printf("\n");
    printf("================================================================\n");
    printf("  FULL INITIALIZATION SEQUENCE\n");
    printf("================================================================\n");
    printf("\n");
    
    printf("Running complete ILI9481 initialization...\n");
    ili9481_init();
    
    printf("Filling screen with RED...\n");
    fill_screen(0xF800);  // Red
    sleep(2);
    
    printf("Filling screen with GREEN...\n");
    fill_screen(0x07E0);  // Green
    sleep(2);
    
    printf("Filling screen with BLUE...\n");
    fill_screen(0x001F);  // Blue
    sleep(2);
    
    printf("Filling screen with WHITE...\n");
    fill_screen(0xFFFF);  // White
    sleep(2);
    
    printf("Filling screen with BLACK...\n");
    fill_screen(0x0000);  // Black
    sleep(2);
    
    printf("\n");
    printf("================================================================\n");
    printf("  SUCCESS! Display is working!\n");
    printf("================================================================\n");
    printf("\n");

    // ====================================================================
    // Cleanup
    // ====================================================================
cleanup:
    printf("\nCleaning up GPIO...\n");
    gpiod_line_request_release(rd_req);
    gpiod_line_request_release(wr_req);
    gpiod_line_request_release(rs_req);
    gpiod_line_request_release(cs_req);
    gpiod_line_request_release(rst_req);
    if (data_req) {
        gpiod_line_request_release(data_req);
    data_req = NULL;
}
    gpiod_chip_close(chip);
    
    printf("Done.\n\n");
}

static void ili9481_start(void){
    chip = gpiod_chip_open(GPIOCHIP);
    if (!chip) {
        perror("gpiod_chip_open");
    }

    unsigned int data_gpios[8] = {LCD_D0, LCD_D1, LCD_D2, LCD_D3, LCD_D4, LCD_D5, LCD_D6, LCD_D7};

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