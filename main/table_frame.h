#pragma once
#include <stdint.h>
#include <stdbool.h>

/* compile-time upper bounds (document constraints: OF<=40, Strip<=8, LED<=100) */
#define WS2812B_MAX_STRIP   8
#define WS2812B_MAX_LED     100
#define PCA9955B_MAX_CH     40

typedef struct {
    uint8_t g;
    uint8_t r;
    uint8_t b;
} grb8_t;

typedef struct {
    grb8_t ws2812b[WS2812B_MAX_STRIP][WS2812B_MAX_LED];
    grb8_t pca9955b[PCA9955B_MAX_CH];
} frame_data;

typedef struct {
    uint64_t timestamp;   // from frame.dat: start_time (uint32 extended)
    bool     fade;        // from frame.dat
    frame_data data;
} table_frame_t;
