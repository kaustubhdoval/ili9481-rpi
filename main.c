#include "ili9481_parallel.h"
#include "ili9481_bmp.h"

static const uint8_t monoBitmap[] = {0x01,0x00,0x00,0x21,0x08,0x02,0x10,0x10,0x05,0x03,0x80,0x02,0x8c,0x62,0x38,0x48,0x24,0x60,0x10,0x10,0x40,0x10,0x10,0x40,0x10,0x10,0x60,0x48,0x24,0x38,0x8c,0x62,0x00,0x03,0x80,0x00,0x10,0x10,0x00,0x21,0x08,0x00,0x01,0x00,0x00};

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
    
    fill_screen(GREEN);
    draw_bitmap_mono(50, 50, 24, 15, monoBitmap, 0xFFFF);
    flush_backbuffer();
    sleep(2);
    
    fill_rect(50, 50, 380, 220, WHITE);    
    draw_string_scaled(60, 100, "Hello World!", 3, BLACK);
    flush_backbuffer();
    sleep(2);

    draw_bmp_file(0, 0, "assets/helloThere.bmp");    // this function also flushes backbuffer 

    ili9481_stop();
    return 0;
}