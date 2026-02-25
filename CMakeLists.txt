cmake_minimum_required(VERSION 3.20)
project(StayAwake)

add_definitions(
        -DUNICODE
        -D_UNICODE
)

include_directories(src)

add_executable(StayAwake_ANALYSIS
        src/main.c
        src/config.c
        src/tray.c
        src/power.c
)

target_compile_options(StayAwake_ANALYSIS PRIVATE /std:clatest)

target_link_libraries(StayAwake_ANALYSIS
        user32
        shell32
        kernel32
        gdi32
        ole32
        advapi32
)
