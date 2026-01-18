#include "frame_reader.h"
#include "frame_config.h"
#include "ff.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "frame_reader";

static FIL fp;
static bool opened = false;
static uint32_t frame_size = 0;

static inline void rgb_to_grb(grb8_t *d, const uint8_t rgb[3])
{
    d->r = rgb[0];
    d->g = rgb[1];
    d->b = rgb[2];
}

bool frame_reader_init(const char *path)
{
    if (f_open(&fp, path, FA_READ) != FR_OK) {
        ESP_LOGE(TAG, "open frame.dat failed");
        return false;
    }

    frame_size = 1;                     // fade
    frame_size += g_of_num * 3;         // OF
    for (int i = 0; i < g_strip_num; i++)
        frame_size += g_led_num[i] * 3; // WS2812B

    opened = true;
    ESP_LOGI(TAG, "frame payload size=%u", frame_size);
    return true;
}

frame_reader_result_t frame_reader_read(table_frame_t *out, uint32_t ts)
{
    if (!opened || !out) return FRAME_READER_IO_ERROR;

    UINT br;
    uint8_t rgb[3];
    memset(out, 0, sizeof(*out));
    out->timestamp = ts;

    uint8_t fade;
    if (f_read(&fp, &fade, 1, &br) != FR_OK || br != 1)
        return FRAME_READER_EOF;
    out->fade = (fade != 0);

    for (int i = 0; i < g_of_num; i++) {
        if (f_read(&fp, rgb, 3, &br) != FR_OK || br != 3)
            return FRAME_READER_EOF;
        rgb_to_grb(&out->data.pca9955b[i], rgb);
    }

    for (int ch = 0; ch < g_strip_num; ch++) {
        uint8_t n = g_led_num[ch];
        if (n > WS2812B_MAX_LED) n = WS2812B_MAX_LED;
        for (int i = 0; i < n; i++) {
            if (f_read(&fp, rgb, 3, &br) != FR_OK || br != 3)
                return FRAME_READER_EOF;
            rgb_to_grb(&out->data.ws2812b[ch][i], rgb);
        }
    }

    return FRAME_READER_OK;
}

bool frame_reader_seek(uint32_t offset)
{
    return opened && (f_lseek(&fp, offset) == FR_OK);
}

uint32_t frame_reader_frame_size(void)
{
    return frame_size;
}

void frame_reader_deinit(void)
{
    if (!opened) return;
    f_close(&fp);
    opened = false;
}
