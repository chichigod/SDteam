#pragma once
#include <stdint.h>
#include <stdbool.h>

typedef struct {
    uint16_t version;
    uint8_t  of_num;
    uint8_t  strip_num;

    uint8_t  *led_num;      // [strip_num]
    uint32_t frame_num;
    uint32_t *timestamps;   // [frame_num]
} control_info_t;

bool control_reader_load(const char *path, control_info_t *out);
void control_reader_free(control_info_t *info);
