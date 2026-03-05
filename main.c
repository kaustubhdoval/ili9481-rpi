#include "ili9481_parallel.h"

void demo_rainbow_bars(void) {
     uint16_t colors[] = {
        0xF800, 0xFD20, 0xFFE0, 0x07E0, 0x001F, 0x781F, 0xF81F
    };
    int bar_height = TFT_HEIGHT / 7;
    for (int i = 0; i < 7; i++) {
        int y0 = i * bar_height;
        int h  = (i == 6) ? (TFT_HEIGHT - y0) : bar_height;
        fill_rect(0, y0, TFT_WIDTH, h, colors[i]);
    }
}

int main(void){
    ili9481_start();

    demo_rainbow_bars();
    flush_backbuffer();  
    sleep(2);
    
    fill_screen(WHITE);
    draw_char(120, 160, 'K', VIOLET);
    flush_backbuffer();
    sleep(2);
    
    fill_rect(50, 50, 380, 220, GREEN);
    flush_backbuffer();
    draw_string(60, 100, "Hello World!", VIOLET);
    flush_backbuffer();

    ili9481_stop();
    return 0;
}