#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "table_frame.h"

typedef enum {
    FRAME_READER_OK = 0,
    FRAME_READER_EOF,
    FRAME_READER_IO_ERROR,
} frame_reader_result_t;

bool frame_reader_init(const char *path);
frame_reader_result_t frame_reader_read(table_frame_t *out, uint32_t timestamp);
bool frame_reader_seek(uint32_t offset);
uint32_t frame_reader_frame_size(void);
void frame_reader_deinit(void);
