#include "readframe.h"
#include "frame_reader.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_log.h"

static const char *TAG = "readframe";

static table_frame_t frame_buf;
static SemaphoreHandle_t frame_sem;

static void sd_reader_task(void *arg)
{
    while (1) {
        xSemaphoreTake(frame_sem, portMAX_DELAY);
        bool ok = readframe_from_sd(&frame_buf);
        xSemaphoreGive(frame_sem);
        if (!ok) {
            ESP_LOGW(TAG, "SD reader EOF or error");
            vTaskDelay(portMAX_DELAY);
        }
    }
}

bool frame_init(const char *sd_path)
{
    if (!frame_reader_init(sd_path)) {
        ESP_LOGE(TAG, "frame_reader_open failed");
        return false;
    }
    frame_sem = xSemaphoreCreateBinary();
    if (!frame_sem) {
        ESP_LOGE(TAG, "Semaphore create failed");
        return false;
    }

    xSemaphoreGive(frame_sem);

    xTaskCreate(
        sd_reader_task,
        "sd_reader",
        4096,
        NULL,
        5,
        NULL
    );

    ESP_LOGI(TAG, "Frame system (single buffer) initialized");
    return false;
}

bool read_frame(table_frame_t *playerbufferptr)
{
    if (!playerbufferptr) return false;

    /* 等 SD task 讀好 internal buffer */
    xSemaphoreTake(frame_sem, portMAX_DELAY);

    /* 把 internal buffer copy 給 player */
    memcpy(playerbufferptr, &frame_buf, sizeof(table_frame_t));

    /* 允許 SD task 讀下一幀 */
    xSemaphoreGive(frame_sem);

    return true;
}
