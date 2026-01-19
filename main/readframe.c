#include "readframe.h"

#include "control_reader.h"
#include "frame_reader.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

#include "esp_log.h"
#include <string.h>

static const char *TAG = "readframe";

/* ================= runtime state ================= */

static control_info_t g_ctrl;

static table_frame_t frame_buf;     /* single internal buffer */

static SemaphoreHandle_t sem_free;  /* buffer write */
static SemaphoreHandle_t sem_ready; /* buffer read */

static TaskHandle_t sd_task = NULL;

static bool inited  = false;
static bool running = false;

/* ---- SD task command ---- */
typedef enum {
    CMD_NONE = 0,
    CMD_RESET,
    CMD_SEEK,
} sd_cmd_t;

static sd_cmd_t cmd = CMD_NONE;
static uint32_t seek_idx = 0;

/* ================= helpers ================= */
#include "channel_info.h"

ch_info_t get_channel_info(void)
{
    ch_info_t info;
    memset(&info, 0, sizeof(info));

    /* WS2812B (RMT) */
    uint8_t strip_n = g_ctrl.strip_num;
    if (strip_n > WS2812B_NUM)
        strip_n = WS2812B_NUM;

    for (uint8_t i = 0; i < strip_n; i++) {
        uint16_t n = g_ctrl.led_num[i];
        if (n > WS2812B_MAX_LED)
            n = WS2812B_MAX_LED;
        info.rmt_strips[i] = n;
    }

    /* PCA9955B (I2C) */
    uint8_t of_n = g_ctrl.of_num;
    if (of_n > PCA9955B_CH_NUM)
        of_n = PCA9955B_CH_NUM;

    for (uint8_t i = 0; i < of_n; i++) {
        info.i2c_leds[i] = 1;  // 每個 channel = 1 pixel
    }

    return info;
}


static uint32_t find_idx_by_ts(uint64_t ts)
{
    if (g_ctrl.frame_num == 0) return 0;

    uint32_t l = 0, r = g_ctrl.frame_num - 1, ans = 0;
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

/* ================= SD reader task ================= */

static void sd_reader_task(void *arg)
{
    while (running) {

        /* 等 buffer 空出來 */
        if (xSemaphoreTake(sem_free, portMAX_DELAY) != pdTRUE)
            continue;

        /* ---- handle command ---- */
        if (cmd == CMD_RESET) {
            frame_reader_seek(0);
            cmd = CMD_NONE;
        } else if (cmd == CMD_SEEK) {
            uint32_t offset = seek_idx * frame_reader_frame_size();
            frame_reader_seek(offset);
            cmd = CMD_NONE;
        }

        /* ---- read one frame ---- */
        esp_err_t err = frame_reader_read(&frame_buf);

        if (err == ESP_ERR_NOT_FOUND) {
            ESP_LOGI(TAG, "EOF reached");
            running = false;
            xSemaphoreGive(sem_ready);
            break;
        }

        if (err != ESP_OK) {
            ESP_LOGE(TAG, "frame_reader_read failed: %s",
                     esp_err_to_name(err));
            running = false;
            xSemaphoreGive(sem_ready);
            break;
        }

        /* 通知 player：buffer ready */
        xSemaphoreGive(sem_ready);
    }

    ESP_LOGI(TAG, "sd_reader_task exit");
    vTaskDelete(NULL);
}

/* ================= public API ================= */

esp_err_t frame_system_init(const char *control_path,
                            const char *frame_path)
{
    if (inited) return ESP_ERR_INVALID_STATE;

    esp_err_t err;

    memset(&g_ctrl, 0, sizeof(g_ctrl));

    /* 1. load control.dat */
    err = control_reader_load(control_path, &g_ctrl);
    if (err != ESP_OK)
        return err;

    /* 2. init frame reader */
    frame_layout_t layout = {
        .of_num    = g_ctrl.of_num,
        .strip_num = g_ctrl.strip_num,
        .led_num   = g_ctrl.led_num,
    };

    err = frame_reader_init(frame_path, &layout);
    if (err != ESP_OK) {
        control_reader_free(&g_ctrl);
        return err;
    }

    /* 3. semaphore */
    sem_free  = xSemaphoreCreateBinary();
    sem_ready = xSemaphoreCreateBinary();
    if (!sem_free || !sem_ready) {
        frame_reader_deinit();
        control_reader_free(&g_ctrl);
        return ESP_ERR_NO_MEM;
    }

    xSemaphoreGive(sem_free); /* buffer initially free */

    running = true;
    cmd = CMD_NONE;

    /* 4. create SD task */
    xTaskCreate(
        sd_reader_task,
        "sd_reader",
        4096,
        NULL,
        5,
        &sd_task
    );

    inited = true;
    ESP_LOGI(TAG, "frame system initialized (single buffer)");
    return ESP_OK;
}

/* ---- normal sequential read ---- */
esp_err_t read_frame(table_frame_t *playerbuffer)
{
    if (!inited) return ESP_ERR_INVALID_STATE;
    if (!playerbuffer) return ESP_ERR_INVALID_ARG;

    if (xSemaphoreTake(sem_ready, portMAX_DELAY) != pdTRUE)
        return ESP_FAIL;

    if (!running)
        return ESP_ERR_NOT_FOUND;

    memcpy(playerbuffer, &frame_buf, sizeof(table_frame_t));

    xSemaphoreGive(sem_free);
    return ESP_OK;
}

/* ---- read by timestamp ---- */
esp_err_t read_frame_ts(table_frame_t *playerbuffer, uint64_t ts)
{
    if (!inited) return ESP_ERR_INVALID_STATE;
    if (!playerbuffer) return ESP_ERR_INVALID_ARG;

    uint32_t idx = find_idx_by_ts(ts);
    if (idx >= g_ctrl.frame_num)
        return ESP_ERR_NOT_FOUND;

    /* 告訴 SD task 要 seek */
    seek_idx = idx;
    cmd = CMD_SEEK;

    /* 釋放 buffer 讓 SD task 執行 seek + read */
    xSemaphoreGive(sem_free);

    /* 等新 frame ready */
    if (xSemaphoreTake(sem_ready, portMAX_DELAY) != pdTRUE)
        return ESP_FAIL;

    if (!running)
        return ESP_ERR_NOT_FOUND;

    memcpy(playerbuffer, &frame_buf, sizeof(table_frame_t));
    xSemaphoreGive(sem_free);
    return ESP_OK;
}

/* ---- reset to frame 0 ---- */
esp_err_t frame_reset(void)
{
    if (!inited) return ESP_ERR_INVALID_STATE;

    cmd = CMD_RESET;
    xSemaphoreGive(sem_free);
    return ESP_OK;
}

void frame_system_deinit(void)
{
    if (!inited) return;

    running = false;

    if (sd_task) {
        xSemaphoreGive(sem_free);
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    if (sem_free)  vSemaphoreDelete(sem_free);
    if (sem_ready) vSemaphoreDelete(sem_ready);

    frame_reader_deinit();
    control_reader_free(&g_ctrl);

    sem_free = sem_ready = NULL;
    sd_task = NULL;
    inited = false;

    ESP_LOGI(TAG, "frame system deinit");
}
