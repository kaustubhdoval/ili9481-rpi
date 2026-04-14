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

    cinfo.out_color_space     = grayscale ? JCS_GRAYSCALE : JCS_RGB;
    cinfo.do_fancy_upsampling = FALSE;
    jpeg_start_decompress(&cinfo);

    uint32_t img_w = cinfo.output_width;
    uint32_t img_h = cinfo.output_height;
    uint32_t draw_w = (x + img_w > TFT_WIDTH)  ? TFT_WIDTH  - x : img_w;
    uint32_t draw_h = (y + img_h > TFT_HEIGHT) ? TFT_HEIGHT - y : img_h;

    int components = cinfo.output_components;
    uint8_t *row_buf = malloc(img_w * components);
    if (!row_buf) { fprintf(stderr, "draw_jpeg_file: OOM\n"); goto err; }

    // Claude hard-carry begins....
    // Floyd-Steinberg error buffers: one per channel, width+2 (padded for edge safety)
    // err_cur = errors to add to the current row being written
    // err_nxt = errors accumulating for the next row
    int *err_cur = calloc((img_w + 2) * 3, sizeof(int));
    int *err_nxt = calloc((img_w + 2) * 3, sizeof(int));
    if (!err_cur || !err_nxt) {
        fprintf(stderr, "draw_jpeg_file: OOM dither buffers\n");
        free(row_buf); free(err_cur); free(err_nxt);
        goto err;
    }

    // Helper macros — index into error buffer (offset by 1 so we can safely
    // write to col-1 without a bounds check)
    #define ERR_R(col) err_cur[((col)+1)*3 + 0]
    #define ERR_G(col) err_cur[((col)+1)*3 + 1]
    #define ERR_B(col) err_cur[((col)+1)*3 + 2]
    #define NXT_R(col) err_nxt[((col)+1)*3 + 0]
    #define NXT_G(col) err_nxt[((col)+1)*3 + 1]
    #define NXT_B(col) err_nxt[((col)+1)*3 + 2]

    JSAMPROW row_ptr = row_buf;
    uint32_t scanline = 0;

    while (cinfo.output_scanline < cinfo.output_height) {
        jpeg_read_scanlines(&cinfo, &row_ptr, 1);

        if (scanline < draw_h) {
            // Remap scanline to display row
            uint16_t py = y + (draw_h - 1 - scanline);

            // Reset next-row error buffer for this pass
            memset(err_nxt, 0, (img_w + 2) * 3 * sizeof(int));

            for (uint32_t px_i = 0; px_i < draw_w; px_i++) {
                // Horizontal flip
                uint32_t src_i = img_w - 1 - px_i;

                int r, g, b;
                if (components == 3) {
                    r = row_buf[src_i * 3 + 0];
                    g = row_buf[src_i * 3 + 1];
                    b = row_buf[src_i * 3 + 2];
                } else {
                    r = g = b = row_buf[src_i];
                }

                // 1. Add accumulated error from neighbours
                r = r + ERR_R(px_i);
                g = g + ERR_G(px_i);
                b = b + ERR_B(px_i);

                // 2. Clamp to valid 8-bit range
                r = r < 0 ? 0 : r > 255 ? 255 : r;
                g = g < 0 ? 0 : g > 255 ? 255 : g;
                b = b < 0 ? 0 : b > 255 ? 255 : b;

                // 3. Quantize to 6-bit (round to nearest multiple of 4)
                uint8_t rq = (r + 2) & 0xFC;
                uint8_t gq = (g + 2) & 0xFC;
                uint8_t bq = (b + 2) & 0xFC;

                // Clamp quantized value (adding 2 can push 254/255 to 256)
                if (rq > 252) rq = 252;
                if (gq > 252) gq = 252;
                if (bq > 252) bq = 252;

                // 4. Compute error
                int er = r - (int)rq;
                int eg = g - (int)gq;
                int eb = b - (int)bq;

                // 5. Distribute Floyd-Steinberg error:
                //    right:       7/16
                //    below-left:  3/16
                //    below:       5/16
                //    below-right: 1/16
                ERR_R(px_i + 1) += (er * 7) >> 4;
                ERR_G(px_i + 1) += (eg * 7) >> 4;
                ERR_B(px_i + 1) += (eb * 7) >> 4;

                NXT_R(px_i - 1) += (er * 3) >> 4;
                NXT_G(px_i - 1) += (eg * 3) >> 4;
                NXT_B(px_i - 1) += (eb * 3) >> 4;

                NXT_R(px_i)     += (er * 5) >> 4;
                NXT_G(px_i)     += (eg * 5) >> 4;
                NXT_B(px_i)     += (eb * 5) >> 4;

                NXT_R(px_i + 1) += (er * 1) >> 4;
                NXT_G(px_i + 1) += (eg * 1) >> 4;
                NXT_B(px_i + 1) += (eb * 1) >> 4;

                // 6. Write quantized RGB to backbuffer
                size_t idx = ((size_t)py * TFT_WIDTH + (x + px_i)) * 3;
                backbuffer[idx + 0] = rq;
                backbuffer[idx + 1] = gq;
                backbuffer[idx + 2] = bq;
            }

            // Swap error buffers — next row's errors become current
            int *tmp = err_cur;
            err_cur  = err_nxt;
            err_nxt  = tmp;
        }
        scanline++;
    }

    #undef ERR_R
    #undef ERR_G
    #undef ERR_B
    #undef NXT_R
    #undef NXT_G
    #undef NXT_B

    expand_dirty(x, y, draw_w, draw_h);

    free(row_buf);
    free(err_cur);
    free(err_nxt);
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