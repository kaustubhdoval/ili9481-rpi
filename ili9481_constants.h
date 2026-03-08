#define TFT_WIDTH 480
#define TFT_HEIGHT 320

// Delay between some initialisation commands
#define TFT_INIT_DELAY 0x02

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
#define WHITE   0xFFFF  // White
#define RED     0xF800  // Red
#define ORANGE  0xFD20  // Orange
#define YELLOW  0xFFE0  // Yellow
#define GREEN   0x07E0  // Green
#define BLUE    0x001F  // Blue
#define INDIGO  0x781F  // Indigo
#define VIOLET  0xF81F   // Violet

// MADCTL - Memory Data Access Control
// MY MX MV ML RGB MH 0 0 
// 1  0  1  0   1  0        -> 0b10101000
#define TFT_MADCTL 0xA8     

// Generic commands used by TFT_eSPI.cpp init sequences
#define TFT_PARALLEL_8_BIT 1

#define TFT_SLPOUT 0x11

#define TFT_INVOFF 0x20
#define TFT_INVON 0x21

#define TFT_DISPON 0x29

#define TFT_CASET 0x2A
#define TFT_PASET 0x2B


