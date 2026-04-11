// ili9481_img.h
#pragma once
#include <stdint.h>
#include <stdbool.h>

int draw_bmp_file(uint16_t x, uint16_t y, const char *filepath);
int draw_jpeg_file(uint16_t x, uint16_t y, const char *filepath, bool grayscale);