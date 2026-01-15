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
