#ifndef APP_CONFIG_H
#define APP_CONFIG_H

// --- Audio System (I2S0) ---
// INMP441 Microphone
#define AUDIO_I2S_BCK_PIN       42
#define AUDIO_I2S_WS_PIN        41  // LRCK
#define AUDIO_I2S_DATA_IN_PIN   40  // SD / DIN
// MAX98357A Amplifier (暂未连接，但先定义)
#define AUDIO_I2S_DATA_OUT_PIN  39

// Audio Format
#define AUDIO_SAMPLE_RATE       16000 // 16kHz (AI 语音标准)
#define AUDIO_BIT_WIDTH         32    // INMP441 输出 24bit，使用 32bit 槽位对齐

#endif // APP_CONFIG_H
