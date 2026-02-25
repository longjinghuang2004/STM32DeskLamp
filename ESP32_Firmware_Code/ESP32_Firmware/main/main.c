#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_http_client.h"
#include "esp_crt_bundle.h"

// 引入模块头文件
#include "manager/mgr_wifi.h"
#include "dev_audio.h"
#include "app_config.h"

static const char *TAG = "MAIN";

// 硬编码配置 (后续可改为 BLE 配网)
#define MY_WIFI_SSID      "CMCC-2079"
#define MY_WIFI_PASS      "88888888"

// --- HTTPS 测试任务 (保持不变，用于验证封装是否成功) ---
esp_err_t _http_event_handler(esp_http_client_event_t *evt) {
    if (evt->event_id == HTTP_EVENT_ON_DATA && !esp_http_client_is_chunked_response(evt->client)) {
        printf("%.*s", evt->data_len, (char*)evt->data);
    }
    return ESP_OK;
}

void https_test_task(void *pvParameters) {
    ESP_LOGI(TAG, "Waiting for Time Sync...");
    
    // 1. 阻塞等待时间同步 (使用封装好的接口)
    while (!Mgr_Wifi_IsTimeSynced()) {
        vTaskDelay(pdMS_TO_TICKS(500));
    }
    ESP_LOGI(TAG, "Time Synced! Starting HTTPS...");

    // 2. 发起请求
    esp_http_client_config_t config = {
        .url = "https://httpbin.org/get",
        .event_handler = _http_event_handler,
        .crt_bundle_attach = esp_crt_bundle_attach,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    esp_err_t err = esp_http_client_perform(client);

    if (err == ESP_OK) {
        ESP_LOGI(TAG, "HTTPS Status = %d", esp_http_client_get_status_code(client));
    } else {
        ESP_LOGE(TAG, "HTTPS Failed: %s", esp_err_to_name(err));
    }
    esp_http_client_cleanup(client);
    vTaskDelete(NULL);
}

void app_main(void) {
    ESP_LOGI(TAG, "System Start...");

    // 1. 初始化 Wi-Fi 管理器
    Mgr_Wifi_Init();

    // 2. 初始化音频驱动 (注入配置)
    Audio_Config_t audio_cfg = {
        .bck_io_num = AUDIO_I2S_BCK_PIN,
        .ws_io_num = AUDIO_I2S_WS_PIN,
        .data_in_num = AUDIO_I2S_DATA_IN_PIN,
        .sample_rate = AUDIO_SAMPLE_RATE
    };
    Dev_Audio_Init(&audio_cfg);

    // 3. 启动连接
    Mgr_Wifi_Connect(MY_WIFI_SSID, MY_WIFI_PASS);

    // 4. 创建业务任务 (这里暂时是测试任务)
    xTaskCreate(https_test_task, "https_test", 8192, NULL, 5, NULL);
}
