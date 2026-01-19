#pragma once
#include <stdint.h>
#include "esp_err.h"
#include "table_frame.h"

esp_err_t frame_system_init(const char *control_path,
                            const char *frame_path);

esp_err_t read_frame(table_frame_t *playerbuffer);
esp_err_t read_frame_ts(table_frame_t *playerbuffer, uint64_t ts);

esp_err_t frame_reset(void);
void      frame_system_deinit(void);
