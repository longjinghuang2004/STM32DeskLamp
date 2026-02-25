#pragma once
#include <stdbool.h>
#include "esp_err.h"

// Wi-Fi 连接状态枚举
typedef enum {
    WIFI_STATUS_IDLE = 0,
    WIFI_STATUS_CONNECTING,
    WIFI_STATUS_CONNECTED,
    WIFI_STATUS_DISCONNECTED,
    WIFI_STATUS_ERROR
} Wifi_Status_t;

/**
 * @brief 初始化 Wi-Fi 和 SNTP 服务
 */
void Mgr_Wifi_Init(void);

/**
 * @brief 连接指定热点
 * @param ssid Wi-Fi 名称
 * @param password Wi-Fi 密码
 */
void Mgr_Wifi_Connect(const char *ssid, const char *password);

/**
 * @brief 获取当前连接状态
 */
Wifi_Status_t Mgr_Wifi_GetStatus(void);

/**
 * @brief 检查时间是否已同步 (HTTPS 请求前必须检查)
 */
bool Mgr_Wifi_IsTimeSynced(void);
