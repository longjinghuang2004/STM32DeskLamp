/* App/Protocol/Protocol.c */
#include "Protocol.h"
#include "USART_DMA.h"
#include "cJSON.h"
#include <string.h>
#include <stdio.h>

// --- 应用层接收缓冲区 ---
// 用于暂存从 DMA 搬运过来的数据流，直到凑齐一个完整的 JSON 包
#define APP_RX_BUF_SIZE 512
static char s_AppRxBuf[APP_RX_BUF_SIZE];
static uint16_t s_AppRxLen = 0;

// --- 回调函数 ---
static Proto_ModeCallback_t s_ModeCb = NULL;
static Proto_LightCallback_t s_LightCb = NULL;

// --- 内部辅助：检查 QoS 水位线 ---
static int _CheckQoS(void)
{
    if (USART_DMA_GetUsage() > PROTOCOL_QOS_THRESHOLD) return 0;
    return 1;
}

// --- 内部辅助：解析 JSON 指令 ---
static void _ParseJsonCmd(char* json_str)
{
    cJSON *root = cJSON_Parse(json_str);
    if (root)
    {
        cJSON *cmd = cJSON_GetObjectItem(root, "cmd");
        if (cJSON_IsString(cmd))
        {
            // 1. 模式切换指令
            if (strcmp(cmd->valuestring, "mode") == 0)
            {
                cJSON *val = cJSON_GetObjectItem(root, "val");
                if (cJSON_IsNumber(val) && s_ModeCb)
                {
                    s_ModeCb((uint8_t)val->valueint);
                }
            }
            // 2. 灯光控制指令
            else if (strcmp(cmd->valuestring, "light") == 0)
            {
                cJSON *warm = cJSON_GetObjectItem(root, "warm");
                cJSON *cold = cJSON_GetObjectItem(root, "cold");
                
                if (cJSON_IsNumber(warm) && cJSON_IsNumber(cold) && s_LightCb)
                {
                    s_LightCb((uint16_t)warm->valueint, (uint16_t)cold->valueint);
                }
            }
        }
        cJSON_Delete(root);
    }
    else
    {
        USART_DMA_Printf("[Proto] JSON Parse Error: %s\r\n", json_str);
    }
}

void Protocol_Init(void)
{
    s_AppRxLen = 0;
    memset(s_AppRxBuf, 0, APP_RX_BUF_SIZE);
}

void Protocol_Process(void)
{
    // 1. 从 DMA 驱动层拉取新数据
    uint8_t temp_buf[128];
    uint16_t len = USART_DMA_ReadRxBuffer(temp_buf, sizeof(temp_buf));

    if (len > 0)
    {
        // 防止缓冲区溢出
        if (s_AppRxLen + len < APP_RX_BUF_SIZE)
        {
            memcpy(&s_AppRxBuf[s_AppRxLen], temp_buf, len);
            s_AppRxLen += len;
            s_AppRxBuf[s_AppRxLen] = '\0'; // 确保字符串结束符
        }
        else
        {
            // 溢出保护：清空缓冲区
            s_AppRxLen = 0;
            USART_DMA_Printf("[Proto] Buffer Overflow! Reset.\r\n");
        }
    }

    // 2. 检查是否包含完整的数据帧 (以 \n 结尾)
    if (s_AppRxLen > 0)
    {
        char* newline_ptr = strchr(s_AppRxBuf, '\n');
        
        while (newline_ptr != NULL)
        {
            // 计算当前帧长度 (包含 \n)
            int frame_len = (newline_ptr - s_AppRxBuf) + 1;
            
            // 提取帧内容 (临时修改 \n 为 \0 以便字符串处理)
            *newline_ptr = '\0';
            
            // 如果有 \r 也去掉
            if (frame_len > 1 && s_AppRxBuf[frame_len - 2] == '\r')
            {
                s_AppRxBuf[frame_len - 2] = '\0';
            }

            // 解析 JSON
            if (strlen(s_AppRxBuf) > 0)
            {
                _ParseJsonCmd(s_AppRxBuf);
            }

            // 3. 移除已处理的数据 (滑动窗口)
            int remaining = s_AppRxLen - frame_len;
            if (remaining > 0)
            {
                memmove(s_AppRxBuf, &s_AppRxBuf[frame_len], remaining);
                s_AppRxLen = remaining;
                s_AppRxBuf[s_AppRxLen] = '\0';
                
                // 继续查找下一个 \n (处理粘包)
                newline_ptr = strchr(s_AppRxBuf, '\n');
            }
            else
            {
                // 全部处理完毕
                s_AppRxLen = 0;
                newline_ptr = NULL;
            }
        }
    }
}

void Protocol_SetModeCallback(Proto_ModeCallback_t cb) { s_ModeCb = cb; }
void Protocol_SetLightCallback(Proto_LightCallback_t cb) { s_LightCb = cb; }

/* --- 发送接口实现 (保持不变) --- */

void Protocol_Report_Encoder(int16_t diff)
{
    USART_DMA_Printf("{\"ev\":\"enc\",\"diff\":%d}\r\n", diff);
}

void Protocol_Report_Key(const char* name, const char* action)
{
    USART_DMA_Printf("{\"ev\":\"key\",\"id\":\"%s\",\"act\":\"%s\"}\r\n", name, action);
}

void Protocol_Report_Gesture(uint8_t gesture)
{
    USART_DMA_Printf("{\"ev\":\"gest\",\"val\":%d}\r\n", gesture);
}

void Protocol_Report_State(uint16_t warm, uint16_t cold)
{
    if (_CheckQoS())
    {
        USART_DMA_Printf("{\"ev\":\"state\",\"warm\":%d,\"cold\":%d}\r\n", warm, cold);
    }
}

void Protocol_Report_Env(int8_t temp, uint8_t humi, uint16_t lux)
{
    if (_CheckQoS())
    {
        USART_DMA_Printf("{\"ev\":\"env\",\"t\":%d,\"h\":%d,\"l\":%d}\r\n", temp, humi, lux);
    }
}

void Protocol_Report_Heartbeat(uint32_t uptime)
{
    if (_CheckQoS())
    {
        USART_DMA_Printf("{\"ev\":\"hb\",\"up\":%d}\r\n", uptime);
    }
}
