// ili9481_fps.c
#include "ili9481_fps.h"

#include <stdio.h>
#include <time.h>

static bool     fps_enabled       = false;
static uint32_t report_interval_ms = 5000;

static uint64_t frame_count  = 0;
static struct timespec window_start;
static float    last_fps    = 0.0f;

static uint64_t now_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000ULL + (uint64_t)(ts.tv_nsec / 1000000ULL);
}

void fps_enable(bool on)
{
    fps_enabled = on;
    frame_count = 0;
    clock_gettime(CLOCK_MONOTONIC, &window_start);
}

bool fps_is_enabled(void)
{
    return fps_enabled;
}

void fps_set_report_interval_ms(uint32_t ms)
{
    report_interval_ms = ms;
}

void fps_tick(void)
{
    if (!fps_enabled) return;

    frame_count++;

    uint64_t start_ms = (uint64_t)window_start.tv_sec * 1000ULL + (uint64_t)(window_start.tv_nsec / 1000000ULL);
    uint64_t elapsed_ms = now_ms() - start_ms;

    if (elapsed_ms >= report_interval_ms) {
        last_fps = (float)frame_count * 1000.0f / (float)elapsed_ms;
        printf("FPS: %.2f\n", last_fps);

        frame_count = 0;
        clock_gettime(CLOCK_MONOTONIC, &window_start);
    }
}

float fps_get_last(void)
{
    return last_fps;
}
