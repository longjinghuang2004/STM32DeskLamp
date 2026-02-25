#pragma once
#include <stdint.h>
#include <stddef.h>
#include "esp_err.h"

// 定义配置结构体
typedef struct {
    int bck_io_num;     // BCLK 引脚
    int ws_io_num;      // LRCK 引脚
    int data_in_num;    // DIN/SD 引脚
    int sample_rate;    // 采样率 (e.g. 16000)
} Audio_Config_t;

// 初始化 I2S (传入配置参数)
esp_err_t Dev_Audio_Init(const Audio_Config_t *cfg);

// 读取原始音频数据
esp_err_t Dev_Audio_Read(void *buffer, size_t len, size_t *bytes_read);
