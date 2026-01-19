#include "frame_reader.h"

#include "ff.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "frame_reader";

static FIL fp;
static bool opened = false;

static frame_layout_t g_layout;
static uint32_t g_frame_size = 0;

static inline void checksum_add_u8(uint32_t *sum, uint8_t b) { *sum += (uint32_t)b; }

static esp_err_t read_exact(void *dst, uint32_t n, uint32_t *sum_opt)
{
    UINT br = 0;
    FRESULT fr = f_read(&fp, dst, n, &br);
    if (fr != FR_OK) return ESP_FAIL;
    if (br != n) return ESP_ERR_NOT_FOUND; // treat short read as EOF

    if (sum_opt) {
        const uint8_t *p = (const uint8_t*)dst;
        for (uint32_t i = 0; i < n; i++) checksum_add_u8(sum_opt, p[i]);
    }
    return ESP_OK;
}

static uint32_t compute_frame_size(const frame_layout_t *ly)
{
    uint32_t s = 0;
    s += 4;                   // start_time
    s += 1;                   // fade
    s += (uint32_t)ly->of_num * 3u;  // OF GRB
    for (uint8_t i = 0; i < ly->strip_num; i++) {
        s += (uint32_t)ly->led_num[i] * 3u; // LED GRB
    }
    s += 4;                   // checksum
    return s;
}

esp_err_t frame_reader_init(const char *path, const frame_layout_t *layout)
{
    if (!path || !layout) return ESP_ERR_INVALID_ARG;
    if (layout->of_num > PCA9955B_MAX_CH) return ESP_ERR_INVALID_SIZE;
    if (layout->strip_num > WS2812B_MAX_STRIP) return ESP_ERR_INVALID_SIZE;

    for (uint8_t i = 0; i < layout->strip_num; i++) {
        if (layout->led_num[i] > WS2812B_MAX_LED) return ESP_ERR_INVALID_SIZE;
    }

    FRESULT fr = f_open(&fp, path, FA_READ);
    if (fr != FR_OK) {
        ESP_LOGE(TAG, "open %s failed (fr=%d)", path, (int)fr);
        return ESP_ERR_NOT_FOUND;
    }

    g_layout = *layout;
    g_frame_size = compute_frame_size(&g_layout);
    opened = true;

    ESP_LOGI(TAG, "opened %s, frame_size=%u", path, (unsigned)g_frame_size);
    return ESP_OK;
}

esp_err_t frame_reader_seek(uint32_t offset)
{
    if (!opened) return ESP_ERR_INVALID_STATE;
    return (f_lseek(&fp, (FSIZE_t)offset) == FR_OK) ? ESP_OK : ESP_FAIL;
}

uint32_t frame_reader_frame_size(void)
{
    return g_frame_size;
}

void frame_reader_deinit(void)
{
    if (!opened) return;
    f_close(&fp);
    opened = false;
}

esp_err_t frame_reader_read(table_frame_t *out)
{
    if (!opened) return ESP_ERR_INVALID_STATE;
    if (!out) return ESP_ERR_INVALID_ARG;

    memset(out, 0, sizeof(*out));

    static uint8_t raw[4096];
    if (g_frame_size > sizeof(raw))
        return ESP_ERR_INVALID_SIZE;

    UINT br;
    if (f_read(&fp, raw, g_frame_size, &br) != FR_OK || br != g_frame_size)
        return ESP_ERR_NOT_FOUND;

    uint8_t *p = raw;
    uint32_t sum = 0;

    /* start_time */
    uint32_t ts = p[0] | (p[1]<<8) | (p[2]<<16) | (p[3]<<24);
    sum += p[0] + p[1] + p[2] + p[3];
    p += 4;
    out->timestamp = ts;

    /* fade */
    out->fade = (*p != 0);
    sum += *p;
    p++;

    /* OF */
    for (uint8_t i = 0; i < g_layout.of_num; i++) {
        out->data.pca9955b[i].g = p[0];
        out->data.pca9955b[i].r = p[1];
        out->data.pca9955b[i].b = p[2];
        sum += p[0] + p[1] + p[2];
        p += 3;
    }

    /* LED */
    for (uint8_t ch = 0; ch < g_layout.strip_num; ch++) {
        for (uint8_t i = 0; i < g_layout.led_num[ch]; i++) {
            out->data.ws2812b[ch][i].g = p[0];
            out->data.ws2812b[ch][i].r = p[1];
            out->data.ws2812b[ch][i].b = p[2];
            sum += p[0] + p[1] + p[2];
            p += 3;
        }
    }

    /* checksum */
    uint32_t chk = p[0] | (p[1]<<8) | (p[2]<<16) | (p[3]<<24);
    if (chk != sum) {
        ESP_LOGE(TAG, "checksum mismatch %u != %u", sum, chk);
        return ESP_ERR_INVALID_CRC;
    }

    return ESP_OK;
}