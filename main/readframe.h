#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifndef WS2812B_NUM
#define WS2812B_NUM  4     
#endif

#ifndef PCA9955B_CH_NUM
#define PCA9955B_CH_NUM  16
#endif


typedef struct {
    uint8_t g;
    uint8_t r;
    uint8_t b;
} grb8_t;

typedef struct {
    grb8_t ws2812b[WS2812B_NUM][100];
    grb8_t pca9955b[PCA9955B_CH_NUM];
} frame_data;

typedef struct {
    uint64_t timestamp;   // start_time from frame.bin (24-bit expanded)
    bool    fade;        // fade flag
    frame_data data;
} table_frame_t;

/*  Player API */

/**
 *  Initialize frame system.
 *       Must be called once before read_frame().
 */
bool frame_init(const char *sd_path);

/**
 * Get next frame.
 *
 * memcpy into playerbuffer
 *       
 *        
 */
bool read_frame(table_frame_t *playerbuffer);

/* 
 

*/
