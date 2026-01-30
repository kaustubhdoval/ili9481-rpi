#include <gpiod.h>
#include <stdio.h>
#include <unistd.h>

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