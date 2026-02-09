#include "USART_DMA.h"
#include <stdarg.h>
#include <string.h>
#include <stdio.h>

// --- 发送相关变量 ---
static uint8_t  s_TxBuf[USART_DMA_TX_BUF_SIZE];
static volatile uint16_t s_TxHead = 0; // 写入位置 (CPU)
static volatile uint16_t s_TxTail = 0; // 读取位置 (DMA)
static volatile uint8_t  s_DmaTxBusy = 0;
static volatile uint16_t s_LastSendLen = 0;

// --- 接收相关变量 ---
static uint8_t  s_RxBuffer[USART_DMA_RX_BUF_SIZE]; // DMA 自动写入的循环缓冲区
static volatile uint16_t s_RxReadIndex = 0;        // 软件读取位置

// --- 内部函数声明 ---
static void _CheckAndStartTxDMA(void);

// --- 初始化 ---
void USART_DMA_Init(void)
{
    // 1. 开启时钟
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_USART1 | RCC_APB2Periph_GPIOA, ENABLE);
    RCC_AHBPeriphClockCmd(RCC_AHBPeriph_DMA1, ENABLE);

    // 2. GPIO 配置
    GPIO_InitTypeDef GPIO_InitStructure;
    // PA9 TX
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_9;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    // PA10 RX
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_10;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    // 3. USART 配置
    USART_InitTypeDef USART_InitStructure;
    USART_InitStructure.USART_BaudRate = USART_DMA_BAUDRATE;
    USART_InitStructure.USART_WordLength = USART_WordLength_8b;
    USART_InitStructure.USART_StopBits = USART_StopBits_1;
    USART_InitStructure.USART_Parity = USART_Parity_No;
    USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    USART_InitStructure.USART_Mode = USART_Mode_Tx | USART_Mode_Rx;
    USART_Init(USART1, &USART_InitStructure);

    // 4. DMA TX 配置 (DMA1_Channel4)
    DMA_DeInit(DMA1_Channel4);
    DMA_InitTypeDef DMA_InitStructure;
    DMA_InitStructure.DMA_PeripheralBaseAddr = (uint32_t)&USART1->DR;
    DMA_InitStructure.DMA_MemoryBaseAddr = (uint32_t)s_TxBuf;
    DMA_InitStructure.DMA_DIR = DMA_DIR_PeripheralDST;
    DMA_InitStructure.DMA_BufferSize = 0; // 初始为0
    DMA_InitStructure.DMA_PeripheralInc = DMA_PeripheralInc_Disable;
    DMA_InitStructure.DMA_MemoryInc = DMA_MemoryInc_Enable;
    DMA_InitStructure.DMA_PeripheralDataSize = DMA_PeripheralDataSize_Byte;
    DMA_InitStructure.DMA_MemoryDataSize = DMA_MemoryDataSize_Byte;
    DMA_InitStructure.DMA_Mode = DMA_Mode_Normal; // 发送用普通模式
    DMA_InitStructure.DMA_Priority = DMA_Priority_Medium;
    DMA_InitStructure.DMA_M2M = DMA_M2M_Disable;
    DMA_Init(DMA1_Channel4, &DMA_InitStructure);

    DMA_ITConfig(DMA1_Channel4, DMA_IT_TC, ENABLE); // 开启发送完成中断

    // 5. DMA RX 配置 (DMA1_Channel5)
    DMA_DeInit(DMA1_Channel5);
    DMA_InitStructure.DMA_PeripheralBaseAddr = (uint32_t)&USART1->DR;
    DMA_InitStructure.DMA_MemoryBaseAddr = (uint32_t)s_RxBuffer;
    DMA_InitStructure.DMA_DIR = DMA_DIR_PeripheralSRC;
    DMA_InitStructure.DMA_BufferSize = USART_DMA_RX_BUF_SIZE;
    // 关键：接收使用循环模式
    DMA_InitStructure.DMA_Mode = DMA_Mode_Circular; 
    DMA_Init(DMA1_Channel5, &DMA_InitStructure);

    // 6. 开启 DMA 通道和 USART DMA 请求
    DMA_Cmd(DMA1_Channel5, ENABLE); // 立即开启接收 DMA
    USART_DMACmd(USART1, USART_DMAReq_Tx | USART_DMAReq_Rx, ENABLE);

    // 7. 中断优先级配置
    NVIC_InitTypeDef NVIC_InitStructure;
    
    // DMA TX 中断
    NVIC_InitStructure.NVIC_IRQChannel = DMA1_Channel4_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);

    // USART1 全局中断 (仅用于处理错误，不处理接收)
    NVIC_InitStructure.NVIC_IRQChannel = USART1_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 1;
    NVIC_Init(&NVIC_InitStructure);

    // 8. 使能串口
    USART_Cmd(USART1, ENABLE);
    
    // 初始化状态
    s_TxHead = 0;
    s_TxTail = 0;
    s_DmaTxBusy = 0;
    s_RxReadIndex = 0;
}

// --- 发送逻辑 (保持不变) ---

static void _CheckAndStartTxDMA(void)
{
    if (s_DmaTxBusy) return;
    if (s_TxHead == s_TxTail) return;

    uint16_t sendLen;
    uint16_t head = s_TxHead;
    uint16_t tail = s_TxTail;

    if (head > tail) sendLen = head - tail;
    else sendLen = USART_DMA_TX_BUF_SIZE - tail;

    s_LastSendLen = sendLen;
    s_DmaTxBusy = 1;

    DMA_Cmd(DMA1_Channel4, DISABLE);
    DMA1_Channel4->CMAR = (uint32_t)&s_TxBuf[tail];
    DMA1_Channel4->CNDTR = sendLen;
    DMA_Cmd(DMA1_Channel4, ENABLE);
}

int USART_DMA_Send(uint8_t *data, uint16_t len)
{
    if (len == 0 || len > USART_DMA_TX_BUF_SIZE) return 0;

    uint16_t head = s_TxHead;
    uint16_t tail = s_TxTail;
    uint16_t used = (head >= tail) ? (head - tail) : (USART_DMA_TX_BUF_SIZE + head - tail);
    uint16_t free = USART_DMA_TX_BUF_SIZE - 1 - used;

    if (len > free) return 0;

    uint16_t chunk1 = USART_DMA_TX_BUF_SIZE - head;
    if (len <= chunk1)
    {
        memcpy(&s_TxBuf[head], data, len);
        s_TxHead = (head + len) % USART_DMA_TX_BUF_SIZE;
    }
    else
    {
        memcpy(&s_TxBuf[head], data, chunk1);
        memcpy(&s_TxBuf[0], data + chunk1, len - chunk1);
        s_TxHead = len - chunk1;
    }

    __disable_irq();
    _CheckAndStartTxDMA();
    __enable_irq();

    return 1;
}

int USART_DMA_Printf(const char *fmt, ...)
{
    char buf[128];
    va_list args;
    va_start(args, fmt);
    int len = vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    
    if (len > 0) return USART_DMA_Send((uint8_t*)buf, len);
    return 0;
}

uint8_t USART_DMA_GetUsage(void)
{
    uint16_t head = s_TxHead;
    uint16_t tail = s_TxTail;
    uint16_t used = (head >= tail) ? (head - tail) : (USART_DMA_TX_BUF_SIZE + head - tail);
    return (uint8_t)((uint32_t)used * 100 / USART_DMA_TX_BUF_SIZE);
}

int fputc(int ch, FILE *f)
{
    uint8_t c = (uint8_t)ch;
    USART_DMA_Send(&c, 1);
    return ch;
}

// --- 接收逻辑 (新增) ---

uint16_t USART_DMA_ReadRxBuffer(uint8_t *output_buf, uint16_t max_len)
{
    uint16_t bytes_read = 0;
    
    // 计算 DMA 当前写到了哪里 (Head)
    // CNDTR 是递减的，所以：Head = 总大小 - 剩余大小
    uint16_t write_index = USART_DMA_RX_BUF_SIZE - DMA_GetCurrDataCounter(DMA1_Channel5);
    
    // 如果读指针 != 写指针，说明有新数据
    while (s_RxReadIndex != write_index && bytes_read < max_len)
    {
        output_buf[bytes_read++] = s_RxBuffer[s_RxReadIndex];
        
        s_RxReadIndex++;
        if (s_RxReadIndex >= USART_DMA_RX_BUF_SIZE)
        {
            s_RxReadIndex = 0;
        }
    }
    
    return bytes_read;
}

// --- 中断处理 ---

// TX DMA 完成中断
void DMA1_Channel4_IRQHandler(void)
{
    if (DMA_GetITStatus(DMA1_IT_TC4))
    {
        DMA_ClearITPendingBit(DMA1_IT_TC4);
        s_TxTail = (s_TxTail + s_LastSendLen) % USART_DMA_TX_BUF_SIZE;
        s_DmaTxBusy = 0;
        _CheckAndStartTxDMA();
    }
}

// USART1 错误处理中断 (防止 ORE 导致死机)
void USART1_IRQHandler(void)
{
    volatile uint8_t clear_temp;
    
    if (USART_GetFlagStatus(USART1, USART_FLAG_ORE) != RESET ||
        USART_GetFlagStatus(USART1, USART_FLAG_NE) != RESET ||
        USART_GetFlagStatus(USART1, USART_FLAG_FE) != RESET ||
        USART_GetFlagStatus(USART1, USART_FLAG_PE) != RESET)
    {
        // 读取 SR 和 DR 清除错误
        clear_temp = USART1->SR;
        clear_temp = USART1->DR;
        (void)clear_temp;
    }
}
