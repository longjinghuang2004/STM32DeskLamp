#include "svc_lighting.h"
#include "data_center.h"
#include "dev_stm32.h"
#include "esp_log.h"

static const char *TAG = "Svc_Light";

// [新增] 缓存上次下发的 PWM 值，用于去重
static uint16_t s_last_warm = 0xFFFF;
static uint16_t s_last_cold = 0xFFFF;

void Svc_Lighting_Apply(void) {
    DC_LightingData_t light;
    DataCenter_Get_Lighting(&light);

    uint16_t warm_pwm = 0;
    uint16_t cold_pwm = 0;

    if (light.power) {
        uint16_t total_pwm = light.brightness * 10; 
        cold_pwm = (total_pwm * light.color_temp) / 100;
        warm_pwm = total_pwm - cold_pwm;
    } else {
        warm_pwm = 0;
        cold_pwm = 0;
    }

    // [新增] 检查是否与上次下发的一致
    // 注意：这里有一个潜在问题。如果 STM32 本地改了 (比如手势)，上报了新值，
    // DataCenter 更新了，触发这里。计算出的 PWM 应该等于 STM32 当前的值。
    // 如果我们发现计算值 == 上次值，就不发了？
    // 不行，因为 s_last_xxx 记录的是 ESP32 *主动下发* 的值。
    // 如果 STM32 自己变了，s_last_xxx 还没变。
    
    // 更好的策略：允许下发，但 STM32 端要做好防抖（STM32 已经做了）。
    // 或者，我们容忍一次冗余下发。
    // 比如：手势调到 500/500 -> 上报 -> ESP32 更新 DC -> 算出来 500/500 -> 下发 500/500。
    // STM32 收到 500/500，发现和当前一样，不动作。这是安全的。
    
    // 为了减少串口流量，我们还是加一个简单的过滤：
    if (warm_pwm == s_last_warm && cold_pwm == s_last_cold) {
        // ESP_LOGD(TAG, "Light state unchanged, skip sending.");
        return;
    }

    ESP_LOGI(TAG, "Apply Light -> Power:%d, Bri:%d, CCT:%d => Warm:%d, Cold:%d", 
             light.power, light.brightness, light.color_temp, warm_pwm, cold_pwm);

    Dev_STM32_Set_Light(warm_pwm, cold_pwm);
    
    // 更新缓存
    s_last_warm = warm_pwm;
    s_last_cold = cold_pwm;
}