#include "ili9481_parallel.h"
#include "ili9481_constants.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

void test_data_bus_pattern(void)
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

void test_suite(void)
{
    printf("\n");
    printf("================================================================\n");
    printf("    ILI9481 COMPREHENSIVE DIAGNOSTIC TEST SUITE\n");
    printf("================================================================\n");
    printf("\n");

    // Keep RD high always
    set_line(LCD_RD, 1);

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
    
    set_line(LCD_RST, 1);
    printf("  RST = HIGH\n");
    usleep(10000);
    
    set_line(LCD_RST, 0);
    printf("  RST = LOW (50ms)\n");
    usleep(50000);
    
    set_line(LCD_RST, 1);
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
    set_line(LCD_RST, 0);
    usleep(50000);
    set_line(LCD_RST, 1);
    usleep(200000);
    
    // Send commands with INVERTED RS
    printf("Sending commands with RS inverted...\n");
    
    // Sleep Out with inverted RS
    set_line(LCD_RS, 1);  // INVERTED (normally would be 0)
    set_line(LCD_CS, 0);
    usleep(10);
    set_data_bus(0x11);
    usleep(10);
    set_line(LCD_WR, 0);
    usleep(10);
    set_line(LCD_WR, 1);
    usleep(10);
    set_line(LCD_CS, 1);
    usleep(120000);
    
    // Display ON with inverted RS
    set_line(LCD_RS, 1);  // INVERTED
    set_line(LCD_CS, 0);
    usleep(10);
    set_data_bus(0x29);
    usleep(10);
    set_line(LCD_WR, 0);
    usleep(10);
    set_line(LCD_WR, 1);
    usleep(10);
    set_line(LCD_CS, 1);
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
    set_line(LCD_RST, 0);
    usleep(50000);
    set_line(LCD_RST, 1);
    usleep(200000);
    
    // Sleep Out with inverted WR
    printf("Sending Sleep Out (0x11) with WR inverted...\n");
    set_line(LCD_RS, 0);
    set_line(LCD_CS, 0);
    set_line(LCD_WR, 0);  // WR idle LOW (inverted)
    usleep(10);
    set_data_bus(0x11);
    usleep(10);
    set_line(LCD_WR, 1);  // WR pulse HIGH (inverted)
    usleep(10);
    set_line(LCD_WR, 0);  // Back to idle LOW
    usleep(10);
    set_line(LCD_CS, 1);
    usleep(120000);
    
    // Display ON with inverted WR
    printf("Sending Display ON (0x29) with WR inverted...\n");
    set_line(LCD_RS, 0);
    set_line(LCD_CS, 0);
    set_line(LCD_WR, 0);  // WR idle LOW
    usleep(10);
    set_data_bus(0x29);
    usleep(10);
    set_line(LCD_WR, 1);  // WR pulse HIGH
    usleep(10);
    set_line(LCD_WR, 0);  // Back to idle LOW
    usleep(10);
    set_line(LCD_CS, 1);
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
    set_line(LCD_RST, 0);
    usleep(50000);
    set_line(LCD_RST, 1);
    usleep(200000);
    
    // Sleep Out with BOTH inverted
    set_line(LCD_RS, 1);  // RS INVERTED
    set_line(LCD_CS, 0);
    set_line(LCD_WR, 0);  // WR idle LOW
    usleep(10);
    set_data_bus(0x11);
    usleep(10);
    set_line(LCD_WR, 1);  // WR pulse HIGH
    usleep(10);
    set_line(LCD_WR, 0);
    usleep(10);
    set_line(LCD_CS, 1);
    usleep(120000);
    
    // Display ON with BOTH inverted
    set_line(LCD_RS, 1);  // RS INVERTED
    set_line(LCD_CS, 0);
    set_line(LCD_WR, 0);  // WR idle LOW
    usleep(10);
    set_data_bus(0x29);
    usleep(10);
    set_line(LCD_WR, 1);  // WR pulse HIGH
    usleep(10);
    set_line(LCD_WR, 0);
    usleep(10);
    set_line(LCD_CS, 1);
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
    set_line(LCD_RST, 0);
    usleep(50000);
    set_line(LCD_RST, 1);
    usleep(200000);
    
    // Sleep Out (0x11) with reversed bits
    uint8_t cmd_reversed = reverse_bits(0x11);
    printf("Sending 0x11 as 0x%02X (reversed bits)...\n", cmd_reversed);
    set_line(LCD_RS, 0);
    set_line(LCD_CS, 0);
    usleep(10);
    set_data_bus(cmd_reversed);
    usleep(10);
    set_line(LCD_WR, 0);
    usleep(10);
    set_line(LCD_WR, 1);
    usleep(10);
    set_line(LCD_CS, 1);
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
    
    cleanup:
        printf("Done.\n\n");
}

int main(void)
{
    ili9481_start();

    test_suite();

    ili9481_stop();
    return 0;
}
