#include "readframe.h"

#include "control_reader.h"
#include "frame_reader.h"

#include "esp_log.h"
#include <string.h>

static const char *TAG = "readframe";

static control_info_t g_ctrl;
static uint32_t frame_size = 0;
static uint32_t cur_idx = 0;
static bool inited = false;

static uint32_t find_idx_by_ts(uint64_t ts)
{
    if (g_ctrl.frame_num == 0) return 0;

    uint32_t l = 0;
    uint32_t r = g_ctrl.frame_num - 1;
    uint32_t ans = 0;

    while (l <= r) {
        uint32_t m = (l + r) >> 1;
        if ((uint64_t)g_ctrl.timestamps[m] <= ts) {
            ans = m;
            l = m + 1;
        } else {
            if (m == 0) break;
            r = m - 1;
        }
    }
    return ans;
}

esp_err_t frame_system_init(const char *control_path, const char *frame_path)
{
    if (inited) return ESP_ERR_INVALID_STATE;
    if (!control_path || !frame_path) return ESP_ERR_INVALID_ARG;

    memset(&g_ctrl, 0, sizeof(g_ctrl));
    cur_idx = 0;
    frame_size = 0;

    esp_err_t err = control_reader_load(control_path, &g_ctrl);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "control_reader_load failed: %s", esp_err_to_name(err));
        return err;
    }

    frame_layout_t layout = {
        .of_num    = g_ctrl.of_num,
        .strip_num = g_ctrl.strip_num,
        .led_num   = g_ctrl.led_num,
    };

    err = frame_reader_init(frame_path, &layout);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "frame_reader_init failed: %s", esp_err_to_name(err));
        control_reader_free(&g_ctrl);
        return err;
    }

    frame_size = frame_reader_frame_size();
    inited = true;

    ESP_LOGI(TAG, "frame system ready (frame_size=%u, frames=%u)",
             (unsigned)frame_size, (unsigned)g_ctrl.frame_num);
    return ESP_OK;
}

esp_err_t read_frame(table_frame_t *buf)
{
    if (!inited) return ESP_ERR_INVALID_STATE;
    if (!buf) return ESP_ERR_INVALID_ARG;

    /* If you want strict bound, use control.dat Frame_num. */
    if (g_ctrl.frame_num > 0 && cur_idx >= g_ctrl.frame_num) {
        return ESP_ERR_NOT_FOUND;
    }

    esp_err_t err = frame_reader_read(buf);
    if (err == ESP_ERR_NOT_FOUND) return ESP_ERR_NOT_FOUND;  // EOF
    if (err != ESP_OK) return err;

    cur_idx++;
    return ESP_OK;
}

esp_err_t read_frame_ts(table_frame_t *buf, uint64_t ts)
{
    if (!inited) return ESP_ERR_INVALID_STATE;
    if (!buf) return ESP_ERR_INVALID_ARG;
    if (g_ctrl.frame_num == 0) return ESP_ERR_NOT_FOUND;

    uint32_t idx = find_idx_by_ts(ts);
    if (idx >= g_ctrl.frame_num) return ESP_ERR_NOT_FOUND;

    uint32_t offset = idx * frame_size;
    esp_err_t err = frame_reader_seek(offset);
    if (err != ESP_OK) return err;

    err = frame_reader_read(buf);
    if (err != ESP_OK) return err;

    /* Optional consistency check: frame.dat start_time should match control.dat time_stamp[idx] */
    if ((uint32_t)buf->timestamp != g_ctrl.timestamps[idx]) {
        ESP_LOGW(TAG, "timestamp mismatch: control[%u]=%u frame=%u",
                 (unsigned)idx,
                 (unsigned)g_ctrl.timestamps[idx],
                 (unsigned)buf->timestamp);
        /* not fatal by default */
    }

    cur_idx = idx + 1;
    return ESP_OK;
}

esp_err_t frame_reset(void)
{
    if (!inited) return ESP_ERR_INVALID_STATE;

    esp_err_t err = frame_reader_seek(0);
    if (err != ESP_OK) return err;

    cur_idx = 0;
    return ESP_OK;
}

void frame_system_deinit(void)
{
    if (!inited) return;

    frame_reader_deinit();
    control_reader_free(&g_ctrl);

    frame_size = 0;
    cur_idx = 0;
    inited = false;

    ESP_LOGI(TAG, "frame system deinit");
}
