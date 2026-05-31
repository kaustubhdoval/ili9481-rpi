// ili9481_img.h
#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <jpeglib.h>

int draw_bmp_file(uint16_t x, uint16_t y, const char *filepath);
int draw_jpeg_file(uint16_t x, uint16_t y, const char *filepath, bool grayscale);