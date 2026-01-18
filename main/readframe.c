#include "readframe.h"
#include "control_reader.h"
#include "frame_reader.h"
#include "frame_config.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "readframe";

static control_info_t g_ctrl;
static uint32_t frame_size;
static uint32_t cur_idx;
static bool inited = false;

static uint32_t find_idx(uint64_t ts)
{
    uint32_t l = 0, r = g_ctrl.frame_num - 1, ans = 0;
    while (l <= r) {
        uint32_t m = (l + r) >> 1;
        if (g_ctrl.timestamps[m] <= ts) {
            ans = m;
            l = m + 1;
        } else {
            if (m == 0) break;
            r = m - 1;
        }
    }
    return ans;
}

bool frame_system_init(void)
{
    memset(&g_ctrl, 0, sizeof(g_ctrl));
    cur_idx = 0;

    if (!control_reader_load("/sd/control.dat", &g_ctrl))
        return false;

    g_strip_num = g_ctrl.strip_num;
    g_of_num    = g_ctrl.of_num;
    g_led_num   = g_ctrl.led_num;

    if (!frame_reader_init("/sd/frame.dat")) {
        control_reader_free(&g_ctrl);
        return false;
    }

    frame_size = frame_reader_frame_size();
    inited = true;
    ESP_LOGI(TAG, "frame system ready");
    return true;
}

esp_err_t read_frame(table_frame_t *buf)
{
    if (!inited) return ESP_ERR_INVALID_STATE;
    if (!buf) return ESP_ERR_INVALID_ARG;
    if (cur_idx >= g_ctrl.frame_num) return ESP_ERR_NOT_FOUND;

    if (frame_reader_read(buf, g_ctrl.timestamps[cur_idx]) != FRAME_READER_OK)
        return ESP_FAIL;

    cur_idx++;
    return ESP_OK;
}

esp_err_t read_frame_ts(table_frame_t *buf, uint64_t ts)
{
    if (!inited) return ESP_ERR_INVALID_STATE;
    if (!buf) return ESP_ERR_INVALID_ARG;

    uint32_t idx = find_idx(ts);
    if (!frame_reader_seek(idx * frame_size))
        return ESP_FAIL;

    if (frame_reader_read(buf, g_ctrl.timestamps[idx]) != FRAME_READER_OK)
        return ESP_FAIL;

    cur_idx = idx + 1;
    return ESP_OK;
}

esp_err_t frame_reset(void)
{
    if (!inited) return ESP_ERR_INVALID_STATE;
    if (!frame_reader_seek(0)) return ESP_FAIL;
    cur_idx = 0;
    return ESP_OK;
}

void frame_system_deinit(void)
{
    if (!inited) return;

    frame_reader_deinit();
    control_reader_free(&g_ctrl);

    g_strip_num = g_of_num = 0;
    g_led_num = NULL;

    memset(&g_ctrl, 0, sizeof(g_ctrl));
    inited = false;
    ESP_LOGI(TAG, "frame system deinit");
}
