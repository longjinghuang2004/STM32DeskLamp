#include "agents/agent_baidu_asr.h"
#include "dev_audio.h"
#include "esp_log.h"
#include "esp_http_client.h"
#include "cJSON.h"
#include "esp_mac.h"
#include "esp_heap_caps.h" 

#include "esp_crt_bundle.h"

static const char *TAG = "BaiduASR";

static char *s_access_token = NULL;
static bool s_is_recording = false;

// --- 缓冲区定义 ---
static char *s_token_resp_buf = NULL;
static int s_token_resp_len = 0;

static char *s_asr_resp_buf = NULL;
static int s_asr_resp_len = 0;

// ============================================================================
// 1. Token 获取相关 (已验证通过)
// ============================================================================

esp_err_t _token_http_event_handler(esp_http_client_event_t *evt) {
    if (evt->event_id == HTTP_EVENT_ON_DATA) {
        if (s_token_resp_buf == NULL) {
            s_token_resp_buf = malloc(evt->data_len + 1);
            memcpy(s_token_resp_buf, evt->data, evt->data_len);
            s_token_resp_buf[evt->data_len] = 0;
            s_token_resp_len = evt->data_len;
        } else {
            s_token_resp_buf = realloc(s_token_resp_buf, s_token_resp_len + evt->data_len + 1);
            memcpy(s_token_resp_buf + s_token_resp_len, evt->data, evt->data_len);
            s_token_resp_len += evt->data_len;
            s_token_resp_buf[s_token_resp_len] = 0;
        }
    }
    return ESP_OK;
}

static void _get_token(void) {
    if (s_access_token) return;

    ESP_LOGI(TAG, "Getting Access Token...");
    
    if (s_token_resp_buf) { free(s_token_resp_buf); s_token_resp_buf = NULL; s_token_resp_len = 0; }

    char url[512];
    snprintf(url, sizeof(url), "%s?grant_type=client_credentials&client_id=%s&client_secret=%s",
             BAIDU_TOKEN_URL, BAIDU_API_KEY, BAIDU_SECRET_KEY);

    esp_http_client_config_t config = {
        .url = url,
        .method = HTTP_METHOD_POST,
        .timeout_ms = 5000,
        .crt_bundle_attach = esp_crt_bundle_attach, // Token 接口是 HTTPS，必须校验
        .event_handler = _token_http_event_handler,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    
    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        int status = esp_http_client_get_status_code(client);
        if (status == 200 && s_token_resp_buf) {
            cJSON *json = cJSON_Parse(s_token_resp_buf);
            if (json) {
                cJSON *token_item = cJSON_GetObjectItem(json, "access_token");
                if (token_item && token_item->valuestring) {
                    if (s_access_token) free(s_access_token);
                    s_access_token = strdup(token_item->valuestring);
                    ESP_LOGI(TAG, "Token Got: %s...", s_access_token);
                }
                cJSON_Delete(json);
            }
        } else {
            ESP_LOGE(TAG, "Token HTTP Error: %d", status);
        }
    } else {
        ESP_LOGE(TAG, "Token Request Failed: %s", esp_err_to_name(err));
    }
    
    if (s_token_resp_buf) { free(s_token_resp_buf); s_token_resp_buf = NULL; }
    esp_http_client_cleanup(client);
}

// ============================================================================
// 2. ASR 识别相关 (新增 Event Handler)
// ============================================================================

esp_err_t _asr_http_event_handler(esp_http_client_event_t *evt) {
    if (evt->event_id == HTTP_EVENT_ON_DATA) {
        // 动态拼接接收到的数据
        if (s_asr_resp_buf == NULL) {
            s_asr_resp_buf = malloc(evt->data_len + 1);
            memcpy(s_asr_resp_buf, evt->data, evt->data_len);
            s_asr_resp_buf[evt->data_len] = 0;
            s_asr_resp_len = evt->data_len;
        } else {
            s_asr_resp_buf = realloc(s_asr_resp_buf, s_asr_resp_len + evt->data_len + 1);
            memcpy(s_asr_resp_buf + s_asr_resp_len, evt->data, evt->data_len);
            s_asr_resp_len += evt->data_len;
            s_asr_resp_buf[s_asr_resp_len] = 0;
        }
    }
    return ESP_OK;
}

void Agent_ASR_Init(void) {
    _get_token();
}

void Agent_ASR_Stop(void) {
    s_is_recording = false;
}

char* Agent_ASR_Run_Session(int max_duration_ms) {
    if (!s_access_token) _get_token();
    if (!s_access_token) return NULL;

    // 清理旧的响应缓存
    if (s_asr_resp_buf) { free(s_asr_resp_buf); s_asr_resp_buf = NULL; s_asr_resp_len = 0; }

    ESP_LOGI(TAG, "Start ASR Session (PSRAM Mode)...");
    s_is_recording = true;

    // 1. 分配 PSRAM 缓冲区 (存储 16-bit 数据)
    // 16kHz * 2bytes * time
    size_t buffer_size = (16000 * 2 * max_duration_ms) / 1000;
    uint8_t *audio_buffer = (uint8_t *)heap_caps_malloc(buffer_size, MALLOC_CAP_SPIRAM);
    if (!audio_buffer) {
        ESP_LOGE(TAG, "PSRAM Malloc Failed!");
        return NULL;
    }

    // 2. 录音循环
    size_t total_bytes_recorded = 0;
    
    // 临时缓冲区：用于接收 I2S 的 32-bit 原始数据
    // 每次读 1024 个采样点 (1024 * 4 bytes = 4096 bytes)
    size_t samples_per_chunk = 1024;
    int32_t *raw_i2s_buffer = malloc(samples_per_chunk * sizeof(int32_t));
    
    size_t bytes_read = 0;
    
    while (s_is_recording && total_bytes_recorded < buffer_size) {
        // 2.1 读取 32-bit 原始数据
        Dev_Audio_Read(raw_i2s_buffer, samples_per_chunk * sizeof(int32_t), &bytes_read);
        
        int samples_read = bytes_read / sizeof(int32_t);
        int16_t *dest_ptr = (int16_t *)(audio_buffer + total_bytes_recorded);

        // 2.2 格式转换 (32-bit -> 16-bit) & 软件增益
        for (int i = 0; i < samples_read; i++) {
            int32_t val = raw_i2s_buffer[i];
            
            // INMP441 的数据在高 24 位。
            // 标准转换是 val >> 16 (取高 16 位)。
            // 为了放大音量，我们只右移 14 位 (相当于乘以 4)，并做限幅保护。
            
            val = val >> 14; 

            // 限幅 (Clipping) 防止溢出
            if (val > 32767) val = 32767;
            if (val < -32768) val = -32768;

            dest_ptr[i] = (int16_t)val;
        }

        // 更新已录制的字节数 (注意是 16-bit 的字节数)
        total_bytes_recorded += samples_read * sizeof(int16_t);
        
        // 防止溢出保护
        if (total_bytes_recorded > buffer_size) total_bytes_recorded = buffer_size;

        vTaskDelay(1);
    }
    
    free(raw_i2s_buffer); // 释放临时 buffer
    ESP_LOGI(TAG, "Recording finished. Bytes: %d. Uploading...", total_bytes_recorded);

    // 3. 准备上传 (以下代码保持不变)
    char *result_text = NULL;
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    char cuid[18];
    snprintf(cuid, sizeof(cuid), "%02X%02X%02X%02X%02X%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

    char url[512];
    snprintf(url, sizeof(url), "%s?cuid=%s&token=%s&dev_pid=1537", BAIDU_ASR_URL, cuid, s_access_token);

    esp_http_client_config_t config = {
        .url = url,
        .method = HTTP_METHOD_POST,
        .timeout_ms = 10000,
        .event_handler = _asr_http_event_handler,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);

    esp_http_client_set_header(client, "Content-Type", "audio/pcm;rate=16000");
    esp_http_client_set_post_field(client, (const char *)audio_buffer, total_bytes_recorded);

    esp_err_t err = esp_http_client_perform(client);
    
    if (err == ESP_OK) {
        int status = esp_http_client_get_status_code(client);
        ESP_LOGI(TAG, "ASR HTTP Status: %d", status);

        if (status == 200 && s_asr_resp_buf) {
            ESP_LOGI(TAG, "ASR Raw Response: %s", s_asr_resp_buf);

            cJSON *json = cJSON_Parse(s_asr_resp_buf);
            if (json) {
                cJSON *err_no = cJSON_GetObjectItem(json, "err_no");
                if (err_no && err_no->valueint == 0) {
                    cJSON *result = cJSON_GetObjectItem(json, "result");
                    if (result && cJSON_GetArraySize(result) > 0) {
                        cJSON *text = cJSON_GetArrayItem(result, 0);
                        if (text && text->valuestring) {
                            result_text = strdup(text->valuestring);
                        }
                    }
                } else {
                    ESP_LOGE(TAG, "ASR API Error: %d, Msg: %s", 
                             err_no ? err_no->valueint : -1,
                             cJSON_GetStringValue(cJSON_GetObjectItem(json, "err_msg")));
                }
                cJSON_Delete(json);
            }
        } else {
            ESP_LOGE(TAG, "ASR HTTP Error or Empty Response");
        }
    } else {
        ESP_LOGE(TAG, "ASR Request Failed: %s", esp_err_to_name(err));
    }

    esp_http_client_cleanup(client);
    heap_caps_free(audio_buffer);
    if (s_asr_resp_buf) { free(s_asr_resp_buf); s_asr_resp_buf = NULL; }
    
    return result_text;
}

