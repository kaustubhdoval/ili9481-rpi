// ili9481_img.c
#include "ili9481_parallel.h"
#include "ili9481_img.h"

static uint8_t row_out[TFT_WIDTH * 3];
static uint8_t row_in[TFT_WIDTH * 4];   // 4 bytes for 32bpp BMP

#pragma pack(push, 1)
typedef struct {
    uint16_t signature;
    uint32_t file_size;
    uint16_t reserved1;
    uint16_t reserved2;
    uint32_t pixel_offset;
} BmpFileHeader;

typedef struct {
    uint32_t header_size;
    int32_t  width;
    int32_t  height;
    uint16_t planes;
    uint16_t bits_per_pixel;
    uint32_t compression;
    uint32_t image_size;
    int32_t  x_pixels_per_meter;
    int32_t  y_pixels_per_meter;
    uint32_t colors_used;
    uint32_t colors_important;
} BmpDibHeader;
#pragma pack(pop)

// ---------------------------------------------------------------------------
// JPEG — decoded directly into the 18-bit backbuffer.
//
// libjpeg outputs 8-bit R, G, B per pixel. We write those bytes straight into
// backbuffer[idx+0..2] with no intermediate RGB565 conversion, preserving the
// full colour depth that the ILI9481's 18-bit interface can display.
// ---------------------------------------------------------------------------
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

    cinfo.out_color_space      = grayscale ? JCS_GRAYSCALE : JCS_RGB;
    cinfo.do_fancy_upsampling  = FALSE;   // eliminates chroma bleed at hard edges
    jpeg_start_decompress(&cinfo);

    uint32_t img_w = cinfo.output_width;
    uint32_t img_h = cinfo.output_height;

    uint32_t draw_w = img_w;
    uint32_t draw_h = img_h;
    if (x + draw_w > TFT_WIDTH)  draw_w = TFT_WIDTH  - x;
    if (y + draw_h > TFT_HEIGHT) draw_h = TFT_HEIGHT - y;

    int components = cinfo.output_components;   // 1 = grayscale, 3 = RGB
    uint8_t *row_buf = malloc(img_w * components);
    if (!row_buf) {
        fprintf(stderr, "draw_jpeg_file: OOM\n");
        goto err;
    }

    JSAMPROW row_ptr  = row_buf;
    uint32_t scanline = 0;

    while (cinfo.output_scanline < cinfo.output_height) {
        jpeg_read_scanlines(&cinfo, &row_ptr, 1);

        if (scanline < draw_h) {
            // Vertical flip: JPEG is top-down, display expects bottom-up
            uint16_t py = y + (draw_h - 1 - scanline);

            for (uint32_t px_i = 0; px_i < draw_w; px_i++) {
                // Horizontal flip
                uint32_t src_i = img_w - 1 - px_i;
                uint8_t r, g, b;

                if (components == 3) {
                    r = row_buf[src_i * 3 + 0];
                    g = row_buf[src_i * 3 + 1];
                    b = row_buf[src_i * 3 + 2];
                } else {
                    r = g = b = row_buf[src_i];
                }

                // Convert 8 bit color to 6-bit with simple dithering
                r = (r & 0xFC) | (r >> 6);   // Keep top 6 bits, add 1 if bottom 2 bits are >= 128
                g = (g & 0xFC) | (g >> 6);
                b = (b & 0xFC) | (b >> 6);

                // Write to backbuffer 
                size_t idx = ((size_t)py * TFT_WIDTH + (x + px_i)) * 3;
                backbuffer[idx + 0] = b;
                backbuffer[idx + 1] = g;
                backbuffer[idx + 2] = r;
            }
        }
        scanline++;
    }

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

// ---------------------------------------------------------------------------
// BMP — direct to display (bypasses backbuffer, same as before).
// BMP stores pixels as BGR on disk which matches what the panel wants, so
// the conversion is just masking to 6-bit precision per channel.
// ---------------------------------------------------------------------------
int draw_bmp_file(uint16_t x, uint16_t y, const char *filepath)
{
    FILE *f = fopen(filepath, "rb");
    if (!f) { perror("draw_bmp_file: fopen"); return -1; }

    BmpFileHeader fhdr;
    BmpDibHeader  dhdr;

    if (fread(&fhdr, sizeof(fhdr), 1, f) != 1) goto err;
    if (fhdr.signature != 0x4D42) { fprintf(stderr, "Not a BMP file\n"); goto err; }

    if (fread(&dhdr, sizeof(dhdr), 1, f) != 1) goto err;
    if (dhdr.compression != 0) {
        fprintf(stderr, "Only uncompressed BMP (BI_RGB) supported\n"); goto err;
    }
    if (dhdr.bits_per_pixel != 24 && dhdr.bits_per_pixel != 32) {
        fprintf(stderr, "Only 24bpp and 32bpp BMP supported (got %d)\n",
                dhdr.bits_per_pixel);
        goto err;
    }

    int32_t bmp_w    = dhdr.width;
    int32_t bmp_h    = dhdr.height;
    int     top_down = (bmp_h < 0);
    if (top_down) bmp_h = -bmp_h;

    uint8_t  bytes_per_px = dhdr.bits_per_pixel / 8;
    uint32_t row_stride   = (bmp_w * bytes_per_px + 3) & ~3u;

    int32_t draw_w = bmp_w;
    int32_t draw_h = bmp_h;
    if (x + draw_w > TFT_WIDTH)  draw_w = TFT_WIDTH  - x;
    if (y + draw_h > TFT_HEIGHT) draw_h = TFT_HEIGHT - y;
    if (draw_w <= 0 || draw_h <= 0) { fclose(f); return 0; }

    set_window(x, y, x + draw_w - 1, y + draw_h - 1);

    for (int32_t row = 0; row < draw_h; row++) {
        int32_t src_row = top_down ? row : (bmp_h - 1 - row);
        long    offset  = (long)fhdr.pixel_offset + (long)src_row * row_stride;

        if (fseek(f, offset, SEEK_SET) != 0) goto err;
        if (fread(row_in, bytes_per_px, bmp_w, f) != (size_t)bmp_w) goto err;

        // BMP is BGR on disk — panel wants BGR too, just clip to top 6 bits
        uint8_t *src = row_in;
        uint8_t *dst = row_out;
        for (int32_t px = 0; px < draw_w; px++) {
            dst[0] = src[0] & 0xFC;   // B
            dst[1] = src[1] & 0xFC;   // G
            dst[2] = src[2] & 0xFC;   // R
            dst += 3;
            src += bytes_per_px;
        }

        burst_write_bytes(row_out, (size_t)draw_w * 3);
    }

    fclose(f);
    return 0;

err:
    fclose(f);
    return -1;
}