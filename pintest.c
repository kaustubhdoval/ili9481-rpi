#ifdef COMPILE_CHECK
#include "gpiod.h"
#else
#include <gpiod.h>
#endif

#include <stdio.h>
#include <unistd.h>

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

int main(void)
{
    // Test each control pin
    int test_pins[] = {2, 3, 4, 17, 27};  // RD, WR, RS, CS, RST
    const char *names[] = {"RD", "WR", "RS", "CS", "RST"};
    
    struct gpiod_chip *chip = gpiod_chip_open("/dev/gpiochip0");
    
    for (int i = 0; i < 5; i++) {
        printf("Testing pin %d (%s) - measure with multimeter\n", test_pins[i], names[i]);
        
        struct gpiod_line_request *req = req_out_line(test_pins[i], "test", 0);
        
        for (int j = 0; j < 5; j++) {
            set_line(req, test_pins[i], 1);
            sleep(1);
            set_line(req, test_pins[i], 0);
            sleep(1);
        }
        
        gpiod_line_request_release(req);
        printf("Done with %s\n", names[i]);
        sleep(2);
    }
    
    gpiod_chip_close(chip);
    return 0;
}