# components/5_Utils/CMakeLists.txt

idf_component_register(
    SRCS "src/ring_buffer.c"  # 目前只创建了这个文件，先只写这个
    # "src/event_bus.c"       # 暂时注释掉，等创建了文件再打开
    # "src/cJSON.c"           # 暂时注释掉
    INCLUDE_DIRS "include"
)
