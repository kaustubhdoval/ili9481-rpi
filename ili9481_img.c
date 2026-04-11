#include "ili9481_parallel.h"
#include "ili9481_img.h"

// Static row buffer — 3 bytes per pixel, max screen width
static uint8_t row_out[TFT_WIDTH * 3];
static uint8_t row_in[TFT_WIDTH * 3];  

// Add a gamma correction LUT
// Why? : JPEG data is stored in sRGB (gamma ~2.2) but we treat it as linear when converting to RGB565
uint8_t gamma_lut[256];

// BMP header structs (packed to match on-disk layout)
#pragma pack(push, 1)
typedef struct {
    uint16_t signature;   // 'BM'
    uint32_t file_size;
    uint16_t reserved1;
    uint16_t reserved2;
    uint32_t pixel_offset; // offset to pixel data from start of file
} BmpFileHeader;

typedef struct {
    uint32_t header_size;
    int32_t  width;
    int32_t  height;       // negative = top-down, positive = bottom-up
    uint16_t planes;
    uint16_t bits_per_pixel;
    uint32_t compression;  // 0 = BI_RGB (uncompressed)
    uint32_t image_size;
    int32_t  x_pixels_per_meter;
    int32_t  y_pixels_per_meter;
    uint32_t colors_used;
    uint32_t colors_important;
} BmpDibHeader;
#pragma pack(pop)

void init_gamma() {
    for (int i = 0; i < 256; i++) {
        gamma_lut[i] = (uint8_t)(pow(i / 255.0, 2.2) * 255.0 + 0.5);
    }
}

int draw_jpeg_file(uint16_t x, uint16_t y, const char *filepath, bool grayscale)
{
    FILE *f = fopen(filepath, "rb");
    if (!f) { perror("draw_jpeg_file: fopen"); return -1; }

    struct jpeg_decompress_struct cinfo;
    struct jpeg_error_mgr jerr;

    cinfo.err = jpeg_std_error(&jerr);
    jpeg_create_decompress(&cinfo);
    jpeg_stdio_src(&cinfo, f);

    if (jpeg_read_header(&cinfo, TRUE) != JPEG_HEADER_OK) {
        fprintf(stderr, "draw_jpeg_file: bad JPEG header\n");
        goto err;
    }

    // Let libjpeg do the grayscale conversion natively when requested
    cinfo.out_color_space = grayscale ? JCS_GRAYSCALE : JCS_RGB;
    jpeg_start_decompress(&cinfo);

    // Initialize Gamma LUT
    init_gamma();

    uint32_t img_w = cinfo.output_width;
    uint32_t img_h = cinfo.output_height;

    uint32_t draw_w = img_w;
    uint32_t draw_h = img_h;
    if (x + draw_w > TFT_WIDTH)  draw_w = TFT_WIDTH  - x;
    if (y + draw_h > TFT_HEIGHT) draw_h = TFT_HEIGHT - y;

    // One scanline buffer — full image width, 1 or 3 bytes per pixel
    int components = cinfo.output_components;  // 1 for grayscale, 3 for RGB
    uint8_t *row_buf = malloc(img_w * components);
    if (!row_buf) {
        fprintf(stderr, "draw_jpeg_file: OOM\n");
        goto err;
    }

    JSAMPROW row_ptr = row_buf;
    uint32_t scanline = 0;

    while (cinfo.output_scanline < cinfo.output_height) {
        jpeg_read_scanlines(&cinfo, &row_ptr, 1);

        if (scanline < draw_h) {
            // Flip vertically: write bottom-up into backbuffer
            uint16_t py = y + (draw_h - 1 - scanline);

            for (uint32_t px_i = 0; px_i < draw_w; px_i++) {
                // Flip horizontally: mirror pixel order
                uint32_t src_i = (img_w - 1 - px_i);
                uint8_t r, g, b;

                if (components == 3){
                    r = gamma_lut[row_buf[src_i * 3 + 0]];
                    g = gamma_lut[row_buf[src_i * 3 + 1]];
                    b = gamma_lut[row_buf[src_i * 3 + 2]];
                }
                else {
                    uint8_t gray = row_buf[src_i];
                    r = g = b = gray;
                }
                

                uint16_t color = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
                set_pixel(x + px_i, py, color);
            }
        }
        scanline++;
    }

    // Mark the drawn region dirty so flush_backbuffer picks it up
    expand_dirty(x, y, draw_w, draw_h);

    free(row_buf);
    jpeg_finish_decompress(&cinfo);
    jpeg_destroy_decompress(&cinfo);
    fclose(f);
    return 0;

err:
    jpeg_destroy_decompress(&cinfo);
    fclose(f);
    return -1;
}

int draw_bmp_file(uint16_t x, uint16_t y, const char *filepath)
{
    FILE *f = fopen(filepath, "rb");
    if (!f) { perror("draw_bmp_file: fopen"); return -1; }

    // --- Read and validate headers ---
    BmpFileHeader fhdr;
    BmpDibHeader  dhdr;

    if (fread(&fhdr, sizeof(fhdr), 1, f) != 1) goto err;
    if (fhdr.signature != 0x4D42) { fprintf(stderr, "Not a BMP file\n"); goto err; }

    if (fread(&dhdr, sizeof(dhdr), 1, f) != 1) goto err;
    if (dhdr.compression != 0) {
        fprintf(stderr, "Only uncompressed BMP (BI_RGB) supported\n"); goto err;
    }
    if (dhdr.bits_per_pixel != 24 && dhdr.bits_per_pixel != 32) {
        fprintf(stderr, "Only 24bpp and 32bpp BMP supported (got %d)\n", dhdr.bits_per_pixel);
        goto err;
    }

    int32_t  bmp_w    = dhdr.width;
    int32_t  bmp_h    = dhdr.height;
    int      top_down = (bmp_h < 0);
    if (top_down) bmp_h = -bmp_h;

    uint8_t  bytes_per_px = dhdr.bits_per_pixel / 8;  // 3 or 4
    // BMP rows are padded to 4-byte alignment
    uint32_t row_stride   = (bmp_w * bytes_per_px + 3) & ~3u;

    // Clip to display
    int32_t draw_w = bmp_w;
    int32_t draw_h = bmp_h;
    if (x + draw_w > TFT_WIDTH)  draw_w = TFT_WIDTH  - x;
    if (y + draw_h > TFT_HEIGHT) draw_h = TFT_HEIGHT - y;
    if (draw_w <= 0 || draw_h <= 0) { fclose(f); return 0; }

    // Set the window once for the whole image — avoids re-issuing 0x2A/0x2B per row
    set_window(x, y, x + draw_w - 1, y + draw_h - 1);

    // Drive CS low for the entire burst — faster than toggling per row
    // We'll call burst_write_bytes per row (it handles CS itself), but
    // we can squeeze more speed by inlining the CS hold across rows.
    // For simplicity and correctness, use burst_write_bytes per row here;
    // see the "turbo" variant below for the CS-held version.

    for (int32_t row = 0; row < draw_h; row++) {
        // BMP is bottom-up unless top_down flag is set
        int32_t src_row = top_down ? row : (bmp_h - 1 - row);
        long    offset  = (long)fhdr.pixel_offset + (long)src_row * row_stride;

        if (fseek(f, offset, SEEK_SET) != 0) goto err;
        if (fread(row_in, bytes_per_px, bmp_w, f) != (size_t)bmp_w) goto err;

        // Convert BMP BGR(A) → BGR666 for the display
        // BMP stores pixels as B, G, R [, A] — conveniently already BGR order!
        uint8_t *src = row_in;
        uint8_t *dst = row_out;
        for (int32_t px = 0; px < draw_w; px++) {
            dst[0] = src[0] & 0xFC;  // Blue:  keep top 6 bits (shift up would also work)
            dst[1] = src[1] & 0xFC;  // Green: keep top 6 bits
            dst[2] = src[2] & 0xFC;  // Red:   keep top 6 bits
            dst += 3;
            src += bytes_per_px;     // skip alpha byte if 32bpp
        }

        burst_write_bytes(row_out, (size_t)draw_w * 3);
    }

    fclose(f);
    return 0;

err:
    fclose(f);
    return -1;
}