/**
  ******************************************************************************
  * @file    LightCtrl.c
  * @brief   灯光控制业务逻辑 (V6.3 Force Report Support)
  ******************************************************************************
  */
#include "LightCtrl.h"
#include "LED.h"
#include "Protocol.h"
#include "SystemModel.h" // 引用全局模型
#include "SystemSupport.h"

// --- 内部变量 ---
static uint8_t s_IsDirty = 0;
static uint32_t s_LastChangeTime = 0;

// 缓存当前的 PWM 值，用于上报
static uint16_t s_CurrWarm = 0;
static uint16_t s_CurrCold = 0;

// --- 辅助函数 ---
static int16_t _Clamp(int16_t val, int16_t min, int16_t max) {
    if (val < min) return min;
    if (val > max) return max;
    return val;
}

// 将模型数据应用到硬件
static void _ApplyModelToHardware(void) {
    uint16_t warm, cold;
    float bri_factor = g_SystemModel.Light.Brightness / 1000.0f;
    float cct_factor = g_SystemModel.Light.ColorTemp / 1000.0f;

    warm = (uint16_t)((1.0f - cct_factor) * 1000 * bri_factor);
    cold = (uint16_t)(cct_factor * 1000 * bri_factor);

    // 驱动硬件
    LED_SetDualColor(warm, cold);
    
    // 更新缓存用于上报
    s_CurrWarm = warm;
    s_CurrCold = cold;
}

void LightCtrl_Init(void) {
    LED_Init();
    _ApplyModelToHardware();
}

void LightCtrl_AdjustBrightness(int16_t delta) {
    g_SystemModel.Light.Brightness += delta;
    g_SystemModel.Light.Brightness = _Clamp(g_SystemModel.Light.Brightness, 0, 1000);
    
    _ApplyModelToHardware();
    
    s_IsDirty = 1;
    s_LastChangeTime = System_GetTick();
}

void LightCtrl_AdjustColorTemp(int16_t delta) {
    g_SystemModel.Light.ColorTemp += delta;
    g_SystemModel.Light.ColorTemp = _Clamp(g_SystemModel.Light.ColorTemp, 0, 1000);
    
    _ApplyModelToHardware();
    
    s_IsDirty = 1;
    s_LastChangeTime = System_GetTick();
}

// 远程控制接口
void LightCtrl_SetRawPWM(uint16_t warm, uint16_t cold) {
    // 1. 直接驱动硬件
    LED_SetDualColor(warm, cold);
    s_CurrWarm = warm;
    s_CurrCold = cold;
    
    // 2. 反向更新模型 (近似值)，确保 UI 显示正确
    uint32_t total = warm + cold;
    if (total > 1000) total = 1000;
    
    g_SystemModel.Light.Brightness = (int16_t)total;
    
    if (total > 0) {
        g_SystemModel.Light.ColorTemp = (int16_t)((cold * 1000) / total);
    }
    
    // 远程设置通常不需要回传 State，避免死循环
    s_IsDirty = 0; 
}

uint16_t LightCtrl_GetBrightness(void) { return g_SystemModel.Light.Brightness; }
uint16_t LightCtrl_GetColorTemp(void) { return g_SystemModel.Light.ColorTemp; }

void LightCtrl_Task(void) {
    // 节流上报：上报 Warm/Cold 值
    if (s_IsDirty && (System_GetTick() - s_LastChangeTime > 200)) {
        Protocol_Report_State(s_CurrWarm, s_CurrCold);
        s_IsDirty = 0;
    }
}

// [新增] 强制上报当前灯光状态
void LightCtrl_ForceReport(void) {
    Protocol_Report_State(s_CurrWarm, s_CurrCold);
    s_IsDirty = 0; // 上报后清除脏标记，避免重复上报
}
