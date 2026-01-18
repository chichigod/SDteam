#include "control_reader.h"
#include "ff.h"
#include "esp_log.h"
#include <stdlib.h>
#include <string.h>

static const char *TAG = "control_reader";

bool control_reader_load(const char *path, control_info_t *out)
{
    memset(out, 0, sizeof(*out));

    FIL fp;
    UINT br;

    if (f_open(&fp, path, FA_READ) != FR_OK) {
        ESP_LOGE(TAG, "open control.dat failed");
        return false;
    }

    f_read(&fp, &out->version,   2, &br);
    f_read(&fp, &out->of_num,    1, &br);
    f_read(&fp, &out->strip_num, 1, &br);

    out->led_num = malloc(out->strip_num);
    if (!out->led_num) goto fail;
    f_read(&fp, out->led_num, out->strip_num, &br);

    f_read(&fp, &out->frame_num, 4, &br);

    out->timestamps = malloc(out->frame_num * sizeof(uint32_t));
    if (!out->timestamps) goto fail;
    f_read(&fp, out->timestamps, out->frame_num * 4, &br);

    f_close(&fp);
    ESP_LOGI(TAG, "control.dat loaded, frames=%u", out->frame_num);
    return true;

fail:
    f_close(&fp);
    control_reader_free(out);
    return false;
}

void control_reader_free(control_info_t *info)
{
    if (!info) return;
    free(info->led_num);
    free(info->timestamps);
    memset(info, 0, sizeof(*info));
}
