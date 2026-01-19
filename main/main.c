#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include "readframe.h"
#include "channel_info.h"
#include "table_frame.h"

static const char *TAG = "TEST";

static void frame_test_task(void *arg)
{
    table_frame_t frame;
    int frame_count = 0;

    while (1) {
        esp_err_t err = read_frame(&frame);
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "read_frame finished (%d)", err);
            break;
        }

        /* ===== 印基本資訊 ===== */
        ESP_LOGI(TAG,
            "[%04d] ts=%llu fade=%d "
            "OF0=(%u,%u,%u) LED0_0=(%u,%u,%u)",
            frame_count,
            (unsigned long long)frame.timestamp,
            frame.fade,
            frame.data.pca9955b[0].g,
            frame.data.pca9955b[0].r,
            frame.data.pca9955b[0].b,
            frame.data.ws2812b[0][0].g,
            frame.data.ws2812b[0][0].r,
            frame.data.ws2812b[0][0].b
        );

        /* ===== 簡單一致性檢查 ===== */
        if (frame_count > 0 && frame.timestamp == 0) {
            ESP_LOGE(TAG, "timestamp error at frame %d", frame_count);
            break;
        }

        frame_count++;
        vTaskDelay(pdMS_TO_TICKS(30));  // 模擬 30fps player
    }

    ESP_LOGI(TAG, "Frame test done, total=%d", frame_count);
    vTaskDelete(NULL);
}

void app_main(void)
{
    ESP_LOGI(TAG, "=== readframe system test start ===");

    /* 初始化整個 frame system */
    esp_err_t err = frame_system_init("/sd/control.dat",
                                      "/sd/frame.dat");
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "frame_system_init failed (%d)", err);
        return;
    }

    /* 測試 channel info */
    ch_info_t ch = get_channel_info();

    ESP_LOGI(TAG, "Channel info:");
    for (int i = 0; i < WS2812B_NUM; i++) {
        ESP_LOGI(TAG, "  RMT[%d] = %u LEDs", i, ch.rmt_strips[i]);
    }
    for (int i = 0; i < PCA9955B_CH_NUM; i++) {
        ESP_LOGI(TAG, "  I2C[%d] = %u LEDs", i, ch.i2c_leds[i]);
    }

    /* 啟動 frame 讀取測試 task */
    xTaskCreate(
        frame_test_task,
        "frame_test",
        4096,
        NULL,
        5,
        NULL
    );
}
