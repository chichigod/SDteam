#include "frame_reader.h"

#include <stdio.h>
#include <string.h>
#include "esp_log.h"

static const char *TAG = "frame_reader";
static FILE *fp = NULL;

extern uint16_t LED_bulbs[WS2812B_NUM];

static bool read_u24_le(FILE *f, uint32_t *out)
{
    uint8_t b[3];
    if (fread(b, 1, 3, f) != 3) {
        return false; 
    }
    *out = (uint32_t)b[0]
         | ((uint32_t)b[1] << 8)
         | ((uint32_t)b[2] << 16);
    return true;
}

bool frame_reader_init(const char *path)
{
    fp = fopen(path, "rb");
    if (!fp) {
        ESP_LOGE(TAG, "Failed to open %s", path);
        return false;
    }
    ESP_LOGI(TAG, "Opened %s", path);
    return true;
}

bool readframe_from_sd(table_frame_t *p)
{
    if (!fp || !p) return false;

    memset(p, 0, sizeof(*p));

    /* 1) header */
    uint32_t start_time;
    if (!read_u24_le(fp, &start_time)) {
        ESP_LOGW(TAG, "EOF/read error at timestamp");
        return false;
    }

    uint8_t fade_u8;
    if (fread(&fade_u8, 1, 1, fp) != 1) {
        ESP_LOGW(TAG, "EOF/read error at fade");
        return false;
    }

    p->timestamp = (uint64_t)start_time;
    p->fade = (fade_u8 != 0);

    /* 2) OF */
    for (int i = 0; i < PCA9955B_CH_NUM; i++) {
        uint8_t rgb[3];
        if (fread(rgb, 1, 3, fp) != 3) {
            ESP_LOGW(TAG, "EOF/read error at OF[%d]", i);
            return false;
        }

        /* file : RGB ; struct : GRB */
        p->data.pca9955b[i].r = rgb[0];
        p->data.pca9955b[i].g = rgb[1];
        p->data.pca9955b[i].b = rgb[2];
    }

    /* 3) LED */
    for (int ch = 0; ch < WS2812B_NUM; ch++) {
        uint16_t n = LED_bulbs[ch];

        if (n > 100) {
            ESP_LOGE(TAG, "LED_bulbs[%d]=%u exceeds limit 100", ch, (unsigned)n);
            return false;
        }

        for (int i = 0; i < n; i++) {
            uint8_t rgb[3];
            if (fread(rgb, 1, 3, fp) != 3) {
                ESP_LOGW(TAG, "EOF/read error at LED[ch=%d][%d]", ch, i);
                return false;
            }

            p->data.ws2812b[ch][i].r = rgb[0];
            p->data.ws2812b[ch][i].g = rgb[1];
            p->data.ws2812b[ch][i].b = rgb[2];
        }
    }

    return true;
}
