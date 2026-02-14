#include "ili9481_parallel.c"

void demo_rainbow_bars(void)
{
    printf("Drawing rainbow bars...\n");
    uint16_t colors[] = {
        0xF800,  // Red
        0xFD20,  // Orange
        0xFFE0,  // Yellow
        0x07E0,  // Green
        0x001F,  // Blue
        0x781F,  // Indigo
        0xF81F   // Violet
    };
    
    int bar_height = 480 / 7;
    
    for (int i = 0; i < 7; i++) {
        int y0 = i * bar_height;
        int y1 = (i + 1) * bar_height - 1;
        if (i == 6) y1 = 479;  // Last bar fills to bottom
        
        set_window(0, y0, 319, y1);
        int pixels = 320 * (y1 - y0 + 1);
        for (int p = 0; p < pixels; p++) {
            write_data16(colors[i]);
        }
    }
}

int main(void){
    ili9481_start();

    while (1) {
        fill_screen(RED);
        sleep(5);
        fill_screen(GREEN);
        sleep(5);
    }

    return 0;
}