#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "manager/mgr_wifi.h"
#include "dev_audio.h"
#include "app_config.h"
#include "agents/agent_baidu_asr.h" // 引入新组件

static const char *TAG = "MAIN";

#define MY_WIFI_SSID      "CMCC-2079"
#define MY_WIFI_PASS      "88888888"

void asr_test_task(void *pvParameters) {
    // 1. 等待网络
    while (Mgr_Wifi_GetStatus() != WIFI_STATUS_CONNECTED) {
        vTaskDelay(pdMS_TO_TICKS(500));
    }
    ESP_LOGI(TAG, "WiFi Connected. Init ASR...");

    // 2. 初始化 ASR (获取 Token)
    Agent_ASR_Init();

    while (1) {
        ESP_LOGI(TAG, ">>> Press ENTER to start recording (Simulated by 5s delay) <<<");
        vTaskDelay(pdMS_TO_TICKS(3000)); // 倒计时

        ESP_LOGI(TAG, ">>> Recording 5 seconds... Speak Now! <<<");
        
        // 3. 启动识别 (设置最大 60秒，但我们会在 5秒后手动停止)
        // 注意：Agent_ASR_Run_Session 是阻塞的，直到 Agent_ASR_Stop 被调用或超时
        // 为了测试，我们这里直接让它录满 5 秒 (通过修改 Run_Session 内部逻辑或传入 5000)
        // 这里我们传入 5000ms 进行测试
        char *text = Agent_ASR_Run_Session(5000);

        if (text) {
            ESP_LOGW(TAG, "### ASR Result: [%s] ###", text);
            free(text);
        } else {
            ESP_LOGE(TAG, "ASR Failed.");
        }

        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}

void app_main(void) {
    ESP_LOGI(TAG, "System Start...");

    // 初始化
    Mgr_Wifi_Init();
    
    Audio_Config_t audio_cfg = {
        .bck_io_num = AUDIO_I2S_BCK_PIN,
        .ws_io_num = AUDIO_I2S_WS_PIN,
        .data_in_num = AUDIO_I2S_DATA_IN_PIN,
        .sample_rate = AUDIO_SAMPLE_RATE
    };
    Dev_Audio_Init(&audio_cfg);

    Mgr_Wifi_Connect(MY_WIFI_SSID, MY_WIFI_PASS);

    // 创建 ASR 测试任务
    xTaskCreate(asr_test_task, "asr_test", 8192, NULL, 5, NULL);
}
