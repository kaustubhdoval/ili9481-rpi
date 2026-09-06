// benchmark.c
//
// Standalone FPS benchmark suite. Not wired into the Makefile (like
// diagnostic.c) — build manually on-device:
//
//   gcc -Wall -Wextra -O0 benchmark.c ili9481_parallel.c ili9481_img.c ili9481_fps.c -o benchmark -ljpeg -lm
//   sudo ./benchmark
//
// Three isolated cases, each reported separately, because FPS here depends
// on two independent costs that a single "play a video" test would conflate:
//
//   1. Full-screen fill   — bus-transfer ceiling. No decode/dither cost,
//                           maximum dirty region every frame. This is the
//                           hard upper bound nothing else can beat.
//   2. Small moving rect  — dirty-region tracking payoff. The rect moves in
//                           small steps so each flush's dirty bounding box
//                           stays small (old + new position are adjacent),
//                           showing what partial updates actually buy you.
//   3. JPEG decode+dither — realistic decode-bound workload. Repeatedly
//                           decodes the same JPEG to stand in for video
//                           (decode+dither cost per frame is the same
//                           whether the source is a single image looped or
//                           a real video stream).

#include "ili9481_parallel.h"
#include "ili9481_img.h"

#include <stdio.h>
#include <time.h>

#define FULLSCREEN_ITERATIONS 100
#define RECT_ITERATIONS       200
#define JPEG_ITERATIONS       30

static double now_seconds(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
}

static void report(const char *name, int frames, double elapsed)
{
    printf("%-32s %5d frames in %6.2fs -> %8.2f FPS\n",
           name, frames, elapsed, elapsed > 0.0 ? frames / elapsed : 0.0);
}

// Case 1: full-screen solid fill, alternating colors — bus ceiling.
static void bench_full_screen_fill(void)
{
    uint32_t colors[2] = {RED, BLUE};

    double start = now_seconds();
    for (int i = 0; i < FULLSCREEN_ITERATIONS; i++) {
        fill_screen(colors[i % 2]);
        flush_backbuffer();
    }
    double elapsed = now_seconds() - start;

    report("Full-screen fill (bus ceiling)", FULLSCREEN_ITERATIONS, elapsed);
}

// Case 2: small rect sweeping the screen in small steps — dirty-region payoff.
static void bench_small_moving_rect(void)
{
    const uint16_t rect_w = 32, rect_h = 32;
    const int16_t  step   = 4;
    int16_t x = 0;
    int16_t y = (TFT_HEIGHT - rect_h) / 2;
    int16_t dir = 1;

    fill_screen(BLACK);
    flush_backbuffer();

    double start = now_seconds();
    for (int i = 0; i < RECT_ITERATIONS; i++) {
        fill_rect(x, y, rect_w, rect_h, BLACK);   // erase old position

        x += dir * step;
        if (x <= 0 || x >= TFT_WIDTH - rect_w) dir = -dir;

        fill_rect(x, y, rect_w, rect_h, WHITE);   // draw new position
        flush_backbuffer();
    }
    double elapsed = now_seconds() - start;

    report("32x32 moving rect (dirty-region)", RECT_ITERATIONS, elapsed);
}

// Case 3: repeated JPEG decode+dither+flush — realistic decode-bound workload.
static void bench_jpeg_decode(void)
{
    const char *path = "assets/helloThere.jpg";
    int frames = 0;

    double start = now_seconds();
    for (int i = 0; i < JPEG_ITERATIONS; i++) {
        if (draw_jpeg_file(0, 0, path, false) != 0) {
            fprintf(stderr, "bench_jpeg_decode: failed to decode %s, aborting case\n", path);
            break;
        }
        flush_backbuffer();
        frames++;
    }
    double elapsed = now_seconds() - start;

    if (frames > 0) report("JPEG decode+dither (realistic)", frames, elapsed);
}

int main(void)
{
    ili9481_start();

    printf("\n=== ILI9481 driver FPS benchmark ===\n\n");

    bench_full_screen_fill();
    bench_small_moving_rect();
    bench_jpeg_decode();

    printf("\n");

    ili9481_stop();
    return 0;
}
