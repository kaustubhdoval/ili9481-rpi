#define TFT_WIDTH 480
#define TFT_HEIGHT 320

// Control pins (BCM)
#define LCD_RD   2
#define LCD_WR   3
#define LCD_RS   4   // DC
#define LCD_CS   17
#define LCD_RST  27

// Data pins D0..D7 (BCM)
#define LCD_D0   15
#define LCD_D1   14
#define LCD_D2   9
#define LCD_D3   10
#define LCD_D4   22
#define LCD_D5   24
#define LCD_D6   23
#define LCD_D7   18

// COLORS
#define WHITE   0x00FFFFFF
#define BLACK   0x00000000
#define RED     0x00FF0000
#define ORANGE  0x00FF8000
#define YELLOW  0x00FFFF00
#define GREEN   0x0000FF00
#define BLUE    0x000000FF
#define INDIGO  0x004B0082
#define VIOLET  0x00EE82EE
#define CYAN    0x0000FFFF
#define MAGENTA 0x00FF00FF
#define GRAY    0x00808080
#define DARKGRAY 0x00404040

// MADCTL - Memory Data Access Control
// Flip compensations are tuned for MADCTL=0xA8 (MY=1, MV=1, RGB=1)
// If MADCTL changes, revisit the py and src_i calculations below
// MY MX MV ML RGB MH 0 0 
// 1  0  1  0   1  0        -> 0b10101000   -> 0xA8
#define TFT_MADCTL 0xA8     

// Generic commands used by init sequences
#define TFT_PARALLEL_8_BIT 1

#define TFT_SLPOUT 0x11

#define TFT_INVOFF 0x20
#define TFT_INVON 0x21

#define TFT_DISPON 0x29

#define TFT_CASET 0x2A
#define TFT_PASET 0x2B


