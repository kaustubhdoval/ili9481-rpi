#define TFT_WIDTH 320
#define TFT_HEIGHT 480

#define MADCTL 0x0
#define TFT_PARALLEL_8_BIT 1

// Delay between some initialisation commands
#define TFT_INIT_DELAY 0x02

// COLORS
#define WHITE   0xFFFF  // White
#define RED     0xF800  // Red
#define ORANGE  0xFD20  // Orange
#define YELLOW  0xFFE0  // Yellow
#define GREEN   0x07E0  // Green
#define BLUE    0x001F  // Blue
#define INDIGO  0x781F  // Indigo
#define VIOLET  0xF81F   // Violet

// Generic commands used by TFT_eSPI.cpp init sequences
#define TFT_SLPOUT 0x11

#define TFT_INVOFF 0x20
#define TFT_INVON 0x21

#define TFT_DISPON 0x29

#define TFT_CASET 0x2A
#define TFT_PASET 0x2B

#define TFT_MADCTL 0x36

