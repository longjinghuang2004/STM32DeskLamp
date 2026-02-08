#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_timer.h"

// 引入组件
#include "KeyManager.h"
#include "bsp_led.h"

static const char *TAG = "MAIN";

// --- 引脚定义 ---
#define PIN_TEST_BUTTON     21  // 杜邦线接的按钮
#define PIN_WS2812          48  // 板载 RGB 灯 (v1.0)，v1.1时将引脚改为38

// --- 按键对象 ---
static Key_t s_TestKey;

// --- 定时器回调 (每 20ms 运行一次) ---
// 相当于 STM32 的 SysTick 或 TIM 中断
static void key_scan_timer_callback(void* arg)
{
    // 核心状态机 (非阻塞)
    KeyManager_Tick();
}

void app_main(void)
{
    ESP_LOGI(TAG, "System Start...");

    // 1. 初始化 LED
    BSP_LED_Init(PIN_WS2812);
    BSP_LED_SetColor(0, 20, 0); // 亮个绿灯表示启动成功
    vTaskDelay(pdMS_TO_TICKS(500));
    BSP_LED_SetColor(0, 0, 0);

    // 2. 初始化按键管理器
    KeyManager_Init();

    // 3. 注册按键 (GPIO 21, 低电平有效)
    // 注意：Key_Init 内部会自动调用 Key_HAL_Init_Pin 配置 GPIO
    Key_Init(&s_TestKey, 0, PIN_TEST_BUTTON, 0); 
    KeyManager_Register(&s_TestKey);

    // 4. 启动高精度定时器 (替代 STM32 的 SysTick)
    const esp_timer_create_args_t timer_args = {
        .callback = &key_scan_timer_callback,
        .name = "key_scan"
    };
    esp_timer_handle_t timer_handle;
    ESP_ERROR_CHECK(esp_timer_create(&timer_args, &timer_handle));
    // 周期性启动，单位微秒 (20ms = 20000us)
    ESP_ERROR_CHECK(esp_timer_start_periodic(timer_handle, 20000));

    ESP_LOGI(TAG, "Key Scanner Started. Press Button on GPIO 21...");

    // 5. 主循环 (消费事件)
    KeyEvent_t evt;
    while (1) {
        // 轮询获取事件 (KeyManager 内部有缓冲)
        if (KeyManager_GetEvent(&evt)) {
            
            ESP_LOGI(TAG, "Key Event: ID=%d, Type=%d", (int)evt.Mask, (int)evt.Type);

            switch (evt.Type) {
                case KEY_EVT_CLICK:
                    ESP_LOGI(TAG, ">> Single Click: Toggle Red Light");
                    BSP_LED_Toggle();
                    break;

                case KEY_EVT_DOUBLE_CLICK:
                    ESP_LOGI(TAG, ">> Double Click: Blue Light");
                    BSP_LED_SetColor(0, 0, 50);
                    break;

                case KEY_EVT_HOLD_START:
                    ESP_LOGI(TAG, ">> Hold Start: White Light");
                    BSP_LED_SetColor(20, 20, 20);
                    break;
                
                case KEY_EVT_HOLD_END:
                    ESP_LOGI(TAG, ">> Hold End: Off");
                    BSP_LED_SetColor(0, 0, 0);
                    break;

                default:
                    break;
            }
        }

        // 这里的延时决定了主循环处理事件的响应速度
        // 10ms 足够快，且能让出 CPU 给 Wi-Fi 等任务
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
