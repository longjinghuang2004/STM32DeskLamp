#ifndef __USART_DMA_H
#define __USART_DMA_H

#include "stm32f10x.h"
#include <stdio.h>

// --- 配置 ---
#define USART_DMA_BAUDRATE      115200
#define USART_DMA_TX_BUF_SIZE   512    // 发送缓冲区 (DMA TX)
#define USART_DMA_RX_BUF_SIZE   512    // 接收缓冲区 (DMA RX Circular)

// --- 接口 ---

/**
  * @brief  初始化 USART1 + DMA (TX:Ch4, RX:Ch5)
  */
void USART_DMA_Init(void);

/**
  * @brief  非阻塞格式化发送 (类似 printf)
  * @param  fmt: 格式化字符串
  * @return 1=成功写入缓冲区, 0=缓冲区满(丢弃)
  */
int USART_DMA_Printf(const char *fmt, ...);

/**
  * @brief  发送原始数据块
  */
int USART_DMA_Send(uint8_t *data, uint16_t len);

/**
  * @brief  获取发送缓冲区占用率 (0-100)
  */
uint8_t USART_DMA_GetUsage(void);

/**
  * @brief  [新增] 从 DMA 接收缓冲区读取数据
  * @param  output_buf: 用户提供的接收缓冲区
  * @param  max_len:    用户缓冲区最大容量
  * @return uint16_t:   实际读取到的字节数
  */
uint16_t USART_DMA_ReadRxBuffer(uint8_t *output_buf, uint16_t max_len);

#endif
