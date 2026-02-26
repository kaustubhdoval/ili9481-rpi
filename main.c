#include "ili9481_parallel.h"

void demo_rainbow_bars(void) {
    int bar_height = TFT_HEIGHT / 7;
    for (int i = 0; i < 7; i++) {
        int y0 = i * bar_height;
        int h  = (i == 6) ? (TFT_HEIGHT - y0) : bar_height;
        fill_rect_buf(0, y0, TFT_WIDTH, h, colors[i]);
    }
    flush_backbuffer();  // single flush after all drawing is done
}

int main(void){
    ili9481_start();

    while (1) {
        demo_rainbow_bars();
        sleep(2);
        fill_screen(WHITE);
        flush_backbuffer();
        sleep(2);
        fill_rect(50, 50, 220, 380, GREEN);
        draw_line(0, 0, 319, 479, BLUE);
        flush_backbuffer();
        sleep(2);
    }

    ili9481_stop();
    return 0;
}