# main/CMakeLists.txt

idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "."
    # REQUIRES 2_Device 5_Utils # 通常不需要写，系统会自动链接，如果报错再加
)

