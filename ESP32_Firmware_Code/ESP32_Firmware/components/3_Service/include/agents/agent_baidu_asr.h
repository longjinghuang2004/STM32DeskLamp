#pragma once
#include <stdbool.h>
#include "esp_err.h"

// --- 配置宏 (请修改为你自己的) ---
#define BAIDU_API_KEY       "w3Jwped6RKLCjRecVZN2qedh"
#define BAIDU_SECRET_KEY    "IeOjLgwKyyr9ixv3ldD9OVJafazxaRq0"

// 百度 ASR 接口地址 (HTTP 速度更快，且音频数据不敏感)
#define BAIDU_ASR_URL       "http://vop.baidu.com/server_api"
#define BAIDU_TOKEN_URL     "https://aip.baidubce.com/oauth/2.0/token"

/**
 * @brief 初始化 ASR 代理 (获取 Token)
 */
void Agent_ASR_Init(void);

/**
 * @brief 开始一次语音识别会话
 * @param max_duration_ms 最大录音时长 (ms)，例如 60000
 * @return char* 识别到的文字 (需要调用者 free)，失败返回 NULL
 */
char* Agent_ASR_Run_Session(int max_duration_ms);

/**
 * @brief 强制停止当前的录音会话 (用于按键松开或 VAD 截断)
 */
void Agent_ASR_Stop(void);