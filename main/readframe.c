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

        if (!readframe_from_sd(&frame_buf)) {
            ESP_LOGW(TAG, "EOF, SD reader stopped");
            vTaskDelay(portMAX_DELAY);
        }

        xSemaphoreGive(frame_sem);
    }
}

void frame_system_init(void)
{
    frame_sem = xSemaphoreCreateBinary();
    configASSERT(frame_sem);

    /* 初始狀態：允許 SD 先讀一幀 */
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
}

table_frame_t *read_frame(void)
{
    /* 等 SD task 讀好 */
    xSemaphoreTake(frame_sem, portMAX_DELAY);

    /* 直接回傳 buffer */
    table_frame_t *p = &frame_buf;

    /* 允許 SD task 讀下一幀 */
    xSemaphoreGive(frame_sem);

    return p;
}
