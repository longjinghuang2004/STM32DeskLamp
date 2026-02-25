idf_component_register(
    SRCS "src/manager/mgr_wifi.c"
         # "src/service_core.c" # 暂时注释，还没写
    INCLUDE_DIRS "include"
    REQUIRES esp_wifi esp_event nvs_flash lwip esp_netif # 依赖网络栈
)
