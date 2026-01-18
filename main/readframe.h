#pragma once
#include "esp_err.h"
#include <stdint.h>
#include <stdbool.h>

#include "table_frame.h"
bool frame_system_init(void);
esp_err_t read_frame(table_frame_t *playerbuffer);
esp_err_t read_frame_ts(table_frame_t *playerbuffer, uint64_t ts);
esp_err_t frame_reset(void);
void frame_system_deinit(void);
