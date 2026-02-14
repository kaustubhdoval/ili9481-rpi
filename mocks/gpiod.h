#pragma once

#include <stddef.h>
#include <stdint.h>

/* ================================
 * Opaque structs
 * ================================ */
struct gpiod_chip { int dummy; };
struct gpiod_line_request { int dummy; };
struct gpiod_line_settings { int dummy; };
struct gpiod_line_config { int dummy; };
struct gpiod_request_config { int dummy; };

/* ================================
 * Enums
 * ================================ */
enum gpiod_line_direction {
    GPIOD_LINE_DIRECTION_INPUT = 0,
    GPIOD_LINE_DIRECTION_OUTPUT = 1
};

enum gpiod_line_value {
    GPIOD_LINE_VALUE_INACTIVE = 0,
    GPIOD_LINE_VALUE_ACTIVE = 1
};

/* ================================
 * Chip
 * ================================ */
struct gpiod_chip *
gpiod_chip_open(const char *path);

struct gpiod_line_request *
gpiod_chip_request_lines(struct gpiod_chip *chip,
                          struct gpiod_request_config *req_cfg,
                          struct gpiod_line_config *line_cfg);

void
gpiod_chip_close(struct gpiod_chip *chip);

/* ================================
 * Line settings
 * ================================ */
struct gpiod_line_settings *
gpiod_line_settings_new(void);

void
gpiod_line_settings_free(struct gpiod_line_settings *settings);

void
gpiod_line_settings_set_direction(struct gpiod_line_settings *settings,
                                  enum gpiod_line_direction direction);

void
gpiod_line_settings_set_output_value(struct gpiod_line_settings *settings,
                                     enum gpiod_line_value value);

/* ================================
 * Line config
 * ================================ */
struct gpiod_line_config *
gpiod_line_config_new(void);

void
gpiod_line_config_free(struct gpiod_line_config *config);

int
gpiod_line_config_add_line_settings(struct gpiod_line_config *config,
                                    const unsigned int *offsets,
                                    size_t num_offsets,
                                    struct gpiod_line_settings *settings);

/* ================================
 * Request config
 * ================================ */
struct gpiod_request_config *
gpiod_request_config_new(void);

void
gpiod_request_config_free(struct gpiod_request_config *config);

void
gpiod_request_config_set_consumer(struct gpiod_request_config *config,
                                  const char *consumer);

/* ================================
 * Line request I/O
 * ================================ */
int
gpiod_line_request_set_value(struct gpiod_line_request *request,
                             unsigned int offset,
                             enum gpiod_line_value value);

void
gpiod_line_request_release(struct gpiod_line_request *request);
