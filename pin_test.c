#ifdef COMPILE_CHECK
#include "gpiod.h"
#else
#include <gpiod.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "pin_definitions.h"

/* =======================
   USER-TUNABLE SETTINGS
   ======================= */
static const unsigned int TEST_DELAY_SEC = 5;  // delay between toggles

/* =======================
   GPIO HELPERS
   ======================= */
static struct gpiod_chip *chip;

static struct gpiod_line_request *
req_out_line(unsigned int gpio, const char *name, int init)
{
    struct gpiod_line_settings *settings;
    struct gpiod_line_config *config;
    struct gpiod_request_config *req_config;
    struct gpiod_line_request *request;

    settings = gpiod_line_settings_new();
    if (!settings) {
        perror("line_settings_new");
        exit(1);
    }

    gpiod_line_settings_set_direction(
        settings, GPIOD_LINE_DIRECTION_OUTPUT);
    gpiod_line_settings_set_output_value(
        settings, init ? GPIOD_LINE_VALUE_ACTIVE
                       : GPIOD_LINE_VALUE_INACTIVE);

    config = gpiod_line_config_new();
    if (!config) {
        perror("line_config_new");
        exit(1);
    }

    if (gpiod_line_config_add_line_settings(
            config, &gpio, 1, settings) < 0) {
        perror("add_line_settings");
        exit(1);
    }

    req_config = gpiod_request_config_new();
    if (!req_config) {
        perror("request_config_new");
        exit(1);
    }

    gpiod_request_config_set_consumer(req_config, name);

    request = gpiod_chip_request_lines(chip, req_config, config);
    if (!request) {
        perror("request_lines");
        exit(1);
    }

    gpiod_line_settings_free(settings);
    gpiod_line_config_free(config);
    gpiod_request_config_free(req_config);

    return request;
}

static void set_line(
    struct gpiod_line_request *req,
    unsigned int offset,
    int val)
{
    enum gpiod_line_value value =
        val ? GPIOD_LINE_VALUE_ACTIVE
            : GPIOD_LINE_VALUE_INACTIVE;

    if (gpiod_line_request_set_value(req, offset, value) < 0) {
        perror("set_value");
        exit(1);
    }
}

/* =======================
   PIN DESCRIPTION TABLE
   ======================= */
typedef struct {
    const char *name;
    const char *purpose;
    unsigned int bcm;
    unsigned int phys;   // Raspberry Pi header pin
} pin_desc_t;

/* BCM → Physical pin mapping (Pi 40-pin header) */
static const pin_desc_t control_pins[] = {
    {"RD",  "Read strobe",        LCD_RD,  3},
    {"WR",  "Write strobe",       LCD_WR,  5},
    {"RS",  "Register select",    LCD_RS,  7},
    {"CS",  "Chip select",        LCD_CS, 11},
    {"RST", "Display reset",      LCD_RST,13},
};

#define NUM_PINS (sizeof(control_pins) / sizeof(control_pins[0]))

/* =======================
   MAIN
   ======================= */
int main(void)
{
    chip = gpiod_chip_open("/dev/gpiochip0");
    if (!chip) {
        perror("gpiod_chip_open");
        return 1;
    }

    printf("\n=== GPIO PIN TEST ===\n");
    printf("Toggle delay: %u second(s)\n\n", TEST_DELAY_SEC);

    for (size_t i = 0; i < NUM_PINS; i++) {
        const pin_desc_t *p = &control_pins[i];

        printf("▶ Testing %s\n", p->name);
        printf("   Purpose : %s\n", p->purpose);
        printf("   BCM     : %u\n", p->bcm);
        printf("   Header  : Physical pin %u\n", p->phys);
        printf("   Probe now with multimeter\n\n");

        struct gpiod_line_request *req =
            req_out_line(p->bcm, p->name, 0);

        for (int j = 0; j < 5; j++) {
            printf("   %s → HIGH\n", p->name);
            set_line(req, p->bcm, 1);
            sleep(TEST_DELAY_SEC);

            printf("   %s → LOW\n", p->name);
            set_line(req, p->bcm, 0);
            sleep(TEST_DELAY_SEC);
        }

        gpiod_line_request_release(req);
        printf("✔ Done with %s\n\n", p->name);
        sleep(2);
    }

    gpiod_chip_close(chip);
    printf("All pins tested.\n");
    return 0;
}
