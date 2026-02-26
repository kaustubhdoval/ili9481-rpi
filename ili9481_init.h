// Yoinked from tft_espi library to test out different initialization sequences for the ILI9481 display
#ifndef ILI9481_INIT_H
#define ILI9481_INIT_H

#include <stdint.h>
#include "ili9481_constants.h"

void write_cmd(uint8_t cmd);
void write_data(uint8_t data);
void delay(uint32_t ms);

#define ILI9481_INIT_CUSTOM 
//#define ILI9481_INIT_1 // Original default
// #define ILI9481_INIT_2 // CPT29
// #define ILI9481_INIT_3 // PVI35
// #define ILI9481_INIT_4 // AUO317
// #define ILI9481_INIT_5 // CMO35 *****
// #define ILI9481_INIT_6 // RGB
//#define ILI9481_INIT_7 // From #1774
// #define ILI9481_INIT_8 // From #1774

/////////////////////////////////////////////////////////////////////////////////////////
static inline void ili9481_init(void)
{
#ifdef ILI9481_INIT_CUSTOM
    write_cmd(TFT_SLPOUT);
    delay(20);

    write_cmd(0xD0);
    write_data(0x07);
    write_data(0x42);
    write_data(0x1B);

    write_cmd(0xD1);
    write_data(0x00);
    write_data(0x14);
    write_data(0x1B);

    write_cmd(0xD2);
    write_data(0x01);
    write_data(0x12);

    write_cmd(0xC0);
    write_data(0x10);
    write_data(0x3B);
    write_data(0x00);
    write_data(0x02);
    write_data(0x01);

    write_cmd(0xC5);
    write_data(0x03);

    write_cmd(0xC8);
    write_data(0x00);
    write_data(0x46);
    write_data(0x44);
    write_data(0x50);
    write_data(0x04);
    write_data(0x16);
    write_data(0x33);
    write_data(0x13);
    write_data(0x77);
    write_data(0x05);
    write_data(0x0F);
    write_data(0x00);

    write_cmd(0x36);
    write_data(TFT_MADCTL); // Change to RGB order

    write_cmd(0x3A);
    write_data(0x66);   // 18-bit colour interface
    //write_data(0x55); // 16-bit colour interface
    
    write_data(0x00);
    write_data(0x00);
    write_data(0x01);
    write_data(0x3F);

    write_data(0x00);
    write_data(0x00);
    write_data(0x01);
    write_data(0xE0);

    delay(120);
    
    write_cmd(0x21); // Inversion 
    write_cmd(TFT_DISPON);

#elif ILI9481_INIT_1
    // Configure ILI9481 display
    write_cmd(TFT_SLPOUT);
    delay(20);

    write_cmd(0xD0);
    write_data(0x07);
    write_data(0x42);
    write_data(0x18);

    write_cmd(0xD1);
    write_data(0x00);
    write_data(0x07);
    write_data(0x10);

    write_cmd(0xD2);
    write_data(0x01);
    write_data(0x02);

    write_cmd(0xC0);
    write_data(0x10);
    write_data(0x3B);
    write_data(0x00);
    write_data(0x02);
    write_data(0x11);

    write_cmd(0xC5);
    write_data(0x03);

    write_cmd(0xC8);
    write_data(0x00);
    write_data(0x32);
    write_data(0x36);
    write_data(0x45);
    write_data(0x06);
    write_data(0x16);
    write_data(0x37);
    write_data(0x75);
    write_data(0x77);
    write_data(0x54);
    write_data(0x0C);
    write_data(0x00);

    write_cmd(TFT_MADCTL);
    write_data(0x0A);

    write_cmd(0x3A);
    write_data(0x66); // 18-bit colour interface

    write_cmd(TFT_CASET);
    write_data(0x00);
    write_data(0x00);
    write_data(0x01);
    write_data(0x3F);

    write_cmd(TFT_PASET);
    write_data(0x00);
    write_data(0x00);
    write_data(0x01);
    write_data(0xDF);

    delay(120);
    write_cmd(TFT_DISPON);

    delay(25);
// End of ILI9481 display configuration
/////////////////////////////////////////////////////////////////////////////////////////
#elif defined(ILI9481_INIT_2)
    // Configure ILI9481 display

    write_cmd(TFT_SLPOUT);
    delay(20);

    write_cmd(0xD0);
    write_data(0x07);
    write_data(0x41);
    write_data(0x1D);

    write_cmd(0xD1);
    write_data(0x00);
    write_data(0x2B);
    write_data(0x1F);

    write_cmd(0xD2);
    write_data(0x01);
    write_data(0x11);

    write_cmd(0xC0);
    write_data(0x10);
    write_data(0x3B);
    write_data(0x00);
    write_data(0x02);
    write_data(0x11);

    write_cmd(0xC5);
    write_data(0x03);

    write_cmd(0xC8);
    write_data(0x00);
    write_data(0x14);
    write_data(0x33);
    write_data(0x10);
    write_data(0x00);
    write_data(0x16);
    write_data(0x44);
    write_data(0x36);
    write_data(0x77);
    write_data(0x00);
    write_data(0x0F);
    write_data(0x00);

    write_cmd(0xB0);
    write_data(0x00);

    write_cmd(0xE4);
    write_data(0xA0);

    write_cmd(0xF0);
    write_data(0x01);

    write_cmd(0xF3);
    write_data(0x02);
    write_data(0x1A);

    write_cmd(TFT_MADCTL);
    write_data(0x0A);

    write_cmd(0x3A);
#if defined(TFT_PARALLEL_8_BIT) || defined(TFT_PARALLEL_16_BIT) || defined(RPI_DISPLAY_TYPE)
    write_data(0x55); // 16-bit colour interface
#else
    write_data(0x66); // 18-bit colour interface
#endif

#if !defined(TFT_PARALLEL_8_BIT) && !defined(TFT_PARALLEL_16_BIT)
    write_cmd(TFT_INVON);
#endif

    write_cmd(TFT_CASET);
    write_data(0x00);
    write_data(0x00);
    write_data(0x01);
    write_data(0x3F);

    write_cmd(TFT_PASET);
    write_data(0x00);
    write_data(0x00);
    write_data(0x01);
    write_data(0xDF);

    delay(120);
    write_cmd(TFT_DISPON);

    delay(25);
// End of ILI9481 display configuration
/////////////////////////////////////////////////////////////////////////////////////////
#elif defined(ILI9481_INIT_3)
    // Configure ILI9481 display

    write_cmd(TFT_SLPOUT);
    delay(20);

    write_cmd(0xD0);
    write_data(0x07);
    write_data(0x41);
    write_data(0x1D);

    write_cmd(0xD1);
    write_data(0x00);
    write_data(0x2B);
    write_data(0x1F);

    write_cmd(0xD2);
    write_data(0x01);
    write_data(0x11);

    write_cmd(0xC0);
    write_data(0x10);
    write_data(0x3B);
    write_data(0x00);
    write_data(0x02);
    write_data(0x11);

    write_cmd(0xC5);
    write_data(0x03);

    write_cmd(0xC8);
    write_data(0x00);
    write_data(0x14);
    write_data(0x33);
    write_data(0x10);
    write_data(0x00);
    write_data(0x16);
    write_data(0x44);
    write_data(0x36);
    write_data(0x77);
    write_data(0x00);
    write_data(0x0F);
    write_data(0x00);

    write_cmd(0xB0);
    write_data(0x00);

    write_cmd(0xE4);
    write_data(0xA0);

    write_cmd(0xF0);
    write_data(0x01);

    write_cmd(0xF3);
    write_data(0x40);
    write_data(0x0A);

    write_cmd(TFT_MADCTL);
    write_data(0x0A);

    write_cmd(0x3A);
#if defined(TFT_PARALLEL_8_BIT) || defined(TFT_PARALLEL_16_BIT) || defined(RPI_DISPLAY_TYPE)
    write_data(0x55); // 16-bit colour interface
#else
    write_data(0x66); // 18-bit colour interface
#endif

#if !defined(TFT_PARALLEL_8_BIT) && !defined(TFT_PARALLEL_16_BIT)
    write_cmd(TFT_INVON);
#endif

    write_cmd(TFT_CASET);
    write_data(0x00);
    write_data(0x00);
    write_data(0x01);
    write_data(0x3F);

    write_cmd(TFT_PASET);
    write_data(0x00);
    write_data(0x00);
    write_data(0x01);
    write_data(0xDF);

    delay(120);
    write_cmd(TFT_DISPON);

    delay(25);
// End of ILI9481 display configuration
/////////////////////////////////////////////////////////////////////////////////////////
#elif defined(ILI9481_INIT_4)
    // Configure ILI9481 display

    write_cmd(TFT_SLPOUT);
    delay(20);

    write_cmd(0xD0);
    write_data(0x07);
    write_data(0x40);
    write_data(0x1D);

    write_cmd(0xD1);
    write_data(0x00);
    write_data(0x18);
    write_data(0x13);

    write_cmd(0xD2);
    write_data(0x01);
    write_data(0x11);

    write_cmd(0xC0);
    write_data(0x10);
    write_data(0x3B);
    write_data(0x00);
    write_data(0x02);
    write_data(0x11);

    write_cmd(0xC5);
    write_data(0x03);

    write_cmd(0xC8);
    write_data(0x00);
    write_data(0x44);
    write_data(0x06);
    write_data(0x44);
    write_data(0x0A);
    write_data(0x08);
    write_data(0x17);
    write_data(0x33);
    write_data(0x77);
    write_data(0x44);
    write_data(0x08);
    write_data(0x0C);

    write_cmd(0xB0);
    write_data(0x00);

    write_cmd(0xE4);
    write_data(0xA0);

    write_cmd(0xF0);
    write_data(0x01);

    write_cmd(TFT_MADCTL);
    write_data(0x0A);

    write_cmd(0x3A);
#if defined(TFT_PARALLEL_8_BIT) || defined(TFT_PARALLEL_16_BIT) || defined(RPI_DISPLAY_TYPE)
    write_data(0x55); // 16-bit colour interface
#else
    write_data(0x66); // 18-bit colour interface
#endif

#if !defined(TFT_PARALLEL_8_BIT) && !defined(TFT_PARALLEL_16_BIT)
    write_cmd(TFT_INVON);
#endif

    write_cmd(TFT_CASET);
    write_data(0x00);
    write_data(0x00);
    write_data(0x01);
    write_data(0x3F);

    write_cmd(TFT_PASET);
    write_data(0x00);
    write_data(0x00);
    write_data(0x01);
    write_data(0xDF);

    delay(120);
    write_cmd(TFT_DISPON);

    delay(25);
// End of ILI9481 display configuration
/////////////////////////////////////////////////////////////////////////////////////////
#elif defined(ILI9481_INIT_5)
    // Configure ILI9481 display

    write_cmd(TFT_SLPOUT);
    delay(20);

    write_cmd(0xD0);
    write_data(0x07);
    write_data(0x41);
    write_data(0x1D);

    write_cmd(0xD1);
    write_data(0x00);
    write_data(0x1C);
    write_data(0x1F);

    write_cmd(0xD2);
    write_data(0x01);
    write_data(0x11);

    write_cmd(0xC0);
    write_data(0x10);
    write_data(0x3B);
    write_data(0x00);
    write_data(0x02);
    write_data(0x11);

    write_cmd(0xC5);
    write_data(0x03);

    write_cmd(0xC6);
    write_data(0x83);

    write_cmd(0xC8);
    write_data(0x00);
    write_data(0x26);
    write_data(0x21);
    write_data(0x00);
    write_data(0x00);
    write_data(0x1F);
    write_data(0x65);
    write_data(0x23);
    write_data(0x77);
    write_data(0x00);
    write_data(0x0F);
    write_data(0x00);

    write_cmd(0xB0);
    write_data(0x00);

    write_cmd(0xE4);
    write_data(0xA0);

    write_cmd(0xF0);
    write_data(0x01);

    write_cmd(TFT_MADCTL);
    write_data(0x0A);

    write_cmd(0x3A);
#if defined(TFT_PARALLEL_8_BIT) || defined(TFT_PARALLEL_16_BIT) || defined(RPI_DISPLAY_TYPE)
    write_data(0x55); // 16-bit colour interface
#else
    write_data(0x66); // 18-bit colour interface
#endif

#if !defined(TFT_PARALLEL_8_BIT) && !defined(TFT_PARALLEL_16_BIT)
    write_cmd(TFT_INVON);
#endif

    write_cmd(TFT_CASET);
    write_data(0x00);
    write_data(0x00);
    write_data(0x01);
    write_data(0x3F);

    write_cmd(TFT_PASET);
    write_data(0x00);
    write_data(0x00);
    write_data(0x01);
    write_data(0xDF);

    delay(120);
    write_cmd(TFT_DISPON);

    delay(25);
// End of ILI9481 display configuration
/////////////////////////////////////////////////////////////////////////////////////////
#elif defined(ILI9481_INIT_6)
    // Configure ILI9481 display

    write_cmd(TFT_SLPOUT);
    delay(20);

    write_cmd(0xD0);
    write_data(0x07);
    write_data(0x41);
    write_data(0x1D);

    write_cmd(0xD1);
    write_data(0x00);
    write_data(0x2B);
    write_data(0x1F);

    write_cmd(0xD2);
    write_data(0x01);
    write_data(0x11);

    write_cmd(0xC0);
    write_data(0x10);
    write_data(0x3B);
    write_data(0x00);
    write_data(0x02);
    write_data(0x11);
    write_data(0x00);

    write_cmd(0xC5);
    write_data(0x03);

    write_cmd(0xC6);
    write_data(0x80);

    write_cmd(0xC8);
    write_data(0x00);
    write_data(0x14);
    write_data(0x33);
    write_data(0x10);
    write_data(0x00);
    write_data(0x16);
    write_data(0x44);
    write_data(0x36);
    write_data(0x77);
    write_data(0x00);
    write_data(0x0F);
    write_data(0x00);

    write_cmd(0xB0);
    write_data(0x00);

    write_cmd(0xE4);
    write_data(0xA0);

    write_cmd(0xF0);
    write_data(0x08);

    write_cmd(0xF3);
    write_data(0x40);
    write_data(0x0A);

    write_cmd(0xF6);
    write_data(0x84);

    write_cmd(0xF7);
    write_data(0x80);

    write_cmd(0xB3);
    write_data(0x00);
    write_data(0x01);
    write_data(0x06);
    write_data(0x30);

    write_cmd(0xB4);
    write_data(0x00);

    write_cmd(0x0C);
    write_data(0x00);
    write_data(0x55);

    write_cmd(TFT_MADCTL);
    write_data(0x0A);

    write_cmd(0x3A);
#if defined(TFT_PARALLEL_8_BIT) || defined(TFT_PARALLEL_16_BIT) || defined(RPI_DISPLAY_TYPE)
    write_data(0x55); // 16-bit colour interface
#else
    write_data(0x66); // 18-bit colour interface
#endif

#if !defined(TFT_PARALLEL_8_BIT) && !defined(TFT_PARALLEL_16_BIT)
    write_cmd(TFT_INVON);
#endif

    write_cmd(TFT_CASET);
    write_data(0x00);
    write_data(0x00);
    write_data(0x01);
    write_data(0x3F);

    write_cmd(TFT_PASET);
    write_data(0x00);
    write_data(0x00);
    write_data(0x01);
    write_data(0xDF);

    delay(120);
    write_cmd(TFT_DISPON);

    delay(25);
// End of ILI9481 display configuration
/////////////////////////////////////////////////////////////////////////////////////////

// From #1774
#elif defined(ILI9481_INIT_7)
    /// ili9481+cmi3.5ips //效果不好
    //************* Start Initial Sequence **********//
    write_cmd(0x11);
    delay(20);

    write_cmd(0xD0);
    write_data(0x07);
    write_data(0x42);
    write_data(0x1B);

    write_cmd(0xD1);
    write_data(0x00);
    write_data(0x14);
    write_data(0x1B);

    write_cmd(0xD2);
    write_data(0x01);
    write_data(0x12);

    write_cmd(0xC0);
    write_data(0x10);
    write_data(0x3B);
    write_data(0x00);
    write_data(0x02);
    write_data(0x01);

    write_cmd(0xC5);
    write_data(0x03);

    write_cmd(0xC8);
    write_data(0x00);
    write_data(0x46);
    write_data(0x44);
    write_data(0x50);
    write_data(0x04);
    write_data(0x16);
    write_data(0x33);
    write_data(0x13);
    write_data(0x77);
    write_data(0x05);
    write_data(0x0F);
    write_data(0x00);

    write_cmd(0x36);
    write_data(0x02); // Change to RGB order

    write_cmd(0x3A);
#if defined(TFT_PARALLEL_8_BIT) || defined(TFT_PARALLEL_16_BIT) || defined(RPI_DISPLAY_TYPE)
    write_data(0x55); // 16-bit colour interface
#else
    write_data(0x66); // 18-bit colour interface
#endif
    write_cmd(0x22);
    write_data(0x00);
    write_data(0x00);
    write_data(0x01);
    write_data(0x3F);

    write_cmd(0x2B);
    write_data(0x00);
    write_data(0x00);
    write_data(0x01);
    write_data(0xE0);
    delay(120);
    write_cmd(0x29);

#elif defined(ILI9481_INIT_8)

    // 3.5IPS ILI9481+CMI
    write_cmd(0x01); // Soft_rese
    delay(220);

    write_cmd(0x11);
    delay(280);

    write_cmd(0xd0);  // Power_Setting
    write_data(0x07); // 07  VC[2:0] Sets the ratio factor of Vci to generate the reference voltages Vci1
    write_data(0x44); // 41  BT[2:0] Sets the Step up factor and output voltage level from the reference voltages Vci1
    write_data(0x1E); // 1f  17   1C  VRH[3:0]: Sets the factor to generate VREG1OUT from VCILVL
    delay(220);

    write_cmd(0xd1);  // VCOM Control
    write_data(0x00); // 00
    write_data(0x0C); // 1A   VCM [6:0] is used to set factor to generate VCOMH voltage from the reference voltage VREG1OUT  15    09
    write_data(0x1A); // 1F   VDV[4:0] is used to set the VCOM alternating amplitude in the range of VREG1OUT x 0.70 to VREG1OUT   1F   18

    write_cmd(0xC5);  // Frame Rate
    write_data(0x03); // 03   02

    write_cmd(0xd2);  // Power_Setting for Normal Mode
    write_data(0x01); // 01
    write_data(0x11); // 11

    write_cmd(0xE4); //?
    write_data(0xa0);
    write_cmd(0xf3);
    write_data(0x00);
    write_data(0x2a);

    // 1  OK
    write_cmd(0xc8);
    write_data(0x00);
    write_data(0x26);
    write_data(0x21);
    write_data(0x00);
    write_data(0x00);
    write_data(0x1f);
    write_data(0x65);
    write_data(0x23);
    write_data(0x77);
    write_data(0x00);
    write_data(0x0f);
    write_data(0x00);
    // GAMMA SETTING

    write_cmd(0xC0);  // Panel Driving Setting
    write_data(0x00); // 1//00  REV  SM  GS
    write_data(0x3B); // 2//NL[5:0]: Sets the number of lines to drive the LCD at an interval of 8 lines.
    write_data(0x00); // 3//SCN[6:0]
    write_data(0x02); // 4//PTV: Sets the Vcom output in non-display area drive period
    write_data(0x11); // 5//NDL: Sets the source output level in non-display area.  PTG: Sets the scan mode in non-display area.

    write_cmd(0xc6); // Interface Control
    write_data(0x83);
    // GAMMA SETTING

    write_cmd(0xf0); //?
    write_data(0x01);

    write_cmd(0xE4); //?
    write_data(0xa0);

    //////倒装设置   NG
    write_cmd(0x36);
    write_data(0x0A); //  8C:出来两行   CA：出来一个点

    write_cmd(0x3a);
#if defined(TFT_PARALLEL_8_BIT) || defined(TFT_PARALLEL_16_BIT) || defined(RPI_DISPLAY_TYPE)
    write_data(0x55); // 16-bit colour interface
#else
    write_data(0x66); // 18-bit colour interface
#endif

#if defined(TFT_PARALLEL_8_BIT) || defined(TFT_PARALLEL_16_BIT)
    write_cmd(TFT_INVON);
#endif

    write_cmd(0xb4); // Display Mode and Frame Memory Write Mode Setting
    write_data(0x02);
    write_data(0x00); //?
    write_data(0x00);
    write_data(0x01);

    delay(280);

    write_cmd(0x2a);
    write_data(0x00);
    write_data(0x00);
    write_data(0x01);
    write_data(0x3F); // 3F

    write_cmd(0x2b);
    write_data(0x00);
    write_data(0x00);
    write_data(0x01);
    write_data(0xDf); // DF

    // write_cmd(0x21);
    write_cmd(0x29);
    write_cmd(0x2c);

#endif
}

#endif // ILI9481_INIT_H