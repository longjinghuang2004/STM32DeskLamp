/* App/Protocol/Protocol.h */
#ifndef __PROTOCOL_H
#define __PROTOCOL_H

#include <stdint.h>

/** @brief QoS 水位线阈值 (百分比) */
#define PROTOCOL_QOS_THRESHOLD  70

/* --- 回调函数类型定义 --- */
typedef void (*Proto_ModeCallback_t)(uint8_t mode);
typedef void (*Proto_LightCallback_t)(uint16_t warm, uint16_t cold);

/* --- 基础接口 --- */
void Protocol_Init(void);

/**
 * @brief 协议处理主循环
 * @note  需在 main loop 中频繁调用。内部会自动从 DMA 缓冲区拉取数据并解析。
 */
void Protocol_Process(void);

/* --- 回调注册 --- */
void Protocol_SetModeCallback(Proto_ModeCallback_t cb);
void Protocol_SetLightCallback(Proto_LightCallback_t cb);

/* --- 发送接口 (高优先级) --- */
void Protocol_Report_Encoder(int16_t diff);
void Protocol_Report_Key(const char* name, const char* action);
void Protocol_Report_Gesture(uint8_t gesture);

/* --- 发送接口 (低优先级 - QoS) --- */
void Protocol_Report_State(uint16_t warm, uint16_t cold);
void Protocol_Report_Env(int8_t temp, uint8_t humi, uint16_t lux);
void Protocol_Report_Heartbeat(uint32_t uptime);

#endif
