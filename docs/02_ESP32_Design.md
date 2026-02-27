# ESP32-S3 固件架构与组件设计

ESP32 端代码采用了基于 ESP-IDF 的现代化组件化设计，强调模块解耦与数据一致性。

## 1. 目录结构与组件划分

代码位于 `components/` 目录下，按职责严格分层：
*   `1_DataRepo/`: 数据中心。定义了全局状态结构体，并提供线程安全的读写接口。
*   `2_Device/`: 硬件驱动层。封装了 I2S 麦克风 (`dev_audio.c`) 和 UART 通信 (`dev_stm32.c`)。
*   `3_Service/`: 业务逻辑层。包含 Wi-Fi 管理、大模型代理 (`agent_lampmind.c`)、ASR 代理 (`agent_baidu_asr.c`) 以及核心状态机 (`service_core.c`)。
*   `5_Utils/`: 通用工具层。实现了基于 FreeRTOS Queue 的事件总线 (`event_bus.c`) 和环形缓冲区。

## 2. 核心机制

### 2.1 DataCenter (单一真理源)
为了避免多任务并发修改数据导致的状态撕裂，所有业务数据（灯光、环境、定时器）均存储在 `DataCenter` 中。
*   **互斥锁保护**: 内部使用 `xSemaphoreCreateMutex()` 保证读写安全。
*   **数据驱动**: 当调用 `DataCenter_Set_Lighting()` 且数据发生实质变化时，模块会自动向 EventBus 抛出 `EVT_DATA_LIGHT_CHANGED` 事件，订阅该事件的模块（如 STM32 串口发送任务）会自动执行下发逻辑。

### 2.2 EventBus (事件总线)
系统摒弃了传统的函数直接调用，采用发布-订阅模式。
*   例如：物理按键按下 -> `dev_button` 发送 `EVT_KEY_CLICK` -> `service_core` 接收事件并决定是否打断当前录音。

## 3. 系统状态机 (System State Machine)

`service_core.c` 维护了系统的核心交互生命周期：

```mermaid
stateDiagram-v2
    [*] --> IDLE
    IDLE --> LISTENING : 唤醒词 / 按键 Click
    LISTENING --> PROCESSING : VAD 静音超时 / 录音达上限
    LISTENING --> IDLE : 按键 Click (主动取消)
    PROCESSING --> SPEAKING : LLM 返回结果
    SPEAKING --> IDLE : TTS 播放完毕
    
    state IDLE {
        %% 待机状态，处理环境数据更新
    }
    state LISTENING {
        %% 开启 I2S 录音，进行 VAD 检测
    }
    state PROCESSING {
        %% HTTP 请求百度 ASR 及 LampMind LLM
    }
