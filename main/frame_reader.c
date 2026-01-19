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

    uint32_t sum = 0;

    /* start_time (uint32 little-endian) */
    uint8_t start_time_raw[4];
    esp_err_t err = read_exact(start_time_raw, 4, &sum);
    if (err != ESP_OK) return err;

    uint32_t start_time =
        (uint32_t)start_time_raw[0]
      | ((uint32_t)start_time_raw[1] << 8)
      | ((uint32_t)start_time_raw[2] << 16)
      | ((uint32_t)start_time_raw[3] << 24);

    out->timestamp = (uint64_t)start_time;

    /* fade */
    uint8_t fade_u8 = 0;
    err = read_exact(&fade_u8, 1, &sum);
    if (err != ESP_OK) return err;
    out->fade = (fade_u8 != 0);

    /* OF GRB data: OF[i] = (G,R,B) */
    for (uint8_t i = 0; i < g_layout.of_num; i++) {
        uint8_t grb[3];
        err = read_exact(grb, 3, &sum);
        if (err != ESP_OK) return err;

        out->data.pca9955b[i].g = grb[0];
        out->data.pca9955b[i].r = grb[1];
        out->data.pca9955b[i].b = grb[2];
    }

    /* LED GRB data: ordered by strip, then LED index */
    for (uint8_t ch = 0; ch < g_layout.strip_num; ch++) {
        uint8_t n = g_layout.led_num[ch];
        if (n > WS2812B_MAX_LED) n = WS2812B_MAX_LED;

        for (uint8_t i = 0; i < n; i++) {
            uint8_t grb[3];
            err = read_exact(grb, 3, &sum);
            if (err != ESP_OK) return err;

            out->data.ws2812b[ch][i].g = grb[0];
            out->data.ws2812b[ch][i].r = grb[1];
            out->data.ws2812b[ch][i].b = grb[2];
        }
    }

    /* checksum (uint32 little-endian) : not included in sum */
    uint8_t checksum_raw[4];
    err = read_exact(checksum_raw, 4, NULL);
    if (err != ESP_OK) return err;

    uint32_t checksum_file =
        (uint32_t)checksum_raw[0]
      | ((uint32_t)checksum_raw[1] << 8)
      | ((uint32_t)checksum_raw[2] << 16)
      | ((uint32_t)checksum_raw[3] << 24);

    if (sum != checksum_file) {
        ESP_LOGE(TAG, "checksum mismatch: calc=%u file=%u (ts=%u)",
                 (unsigned)sum, (unsigned)checksum_file, (unsigned)start_time);
        return ESP_ERR_INVALID_CRC;
    }

    return ESP_OK;
}
