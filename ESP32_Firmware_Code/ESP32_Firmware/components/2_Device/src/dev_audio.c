#include "dev_audio.h"
#include "driver/i2s_std.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"


static const char *TAG = "Dev_Audio";
static i2s_chan_handle_t rx_handle = NULL;

esp_err_t Dev_Audio_Init(const Audio_Config_t *cfg) {
    ESP_LOGI(TAG, "Initializing I2S for INMP441...");

    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_AUTO, I2S_ROLE_MASTER);
    ESP_ERROR_CHECK(i2s_new_channel(&chan_cfg, NULL, &rx_handle));

    i2s_std_config_t std_cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(cfg->sample_rate), // 使用参数
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_32BIT, I2S_SLOT_MODE_MONO),
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,
            .bclk = cfg->bck_io_num,    // 使用参数
            .ws = cfg->ws_io_num,       // 使用参数
            .dout = I2S_GPIO_UNUSED,
            .din = cfg->data_in_num,    // 使用参数
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv = false,
            },
        },
    };
    
    std_cfg.slot_cfg.slot_mask = I2S_STD_SLOT_LEFT;

    ESP_ERROR_CHECK(i2s_channel_init_std_mode(rx_handle, &std_cfg));
    ESP_ERROR_CHECK(i2s_channel_enable(rx_handle));
    
    ESP_LOGI(TAG, "I2S Initialized.");
    return ESP_OK;
}

esp_err_t Dev_Audio_Read(void *buffer, size_t len, size_t *bytes_read) {
    if (!rx_handle) return ESP_FAIL;
    return i2s_channel_read(rx_handle, buffer, len, bytes_read, portMAX_DELAY);
}

