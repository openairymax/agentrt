cmake_minimum_required(VERSION 3.15)
project(agentos_core C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(COREKERN_SOURCES
    src/main.c
    src/error.c
    src/ipc/binder.c
    src/ipc/channel.c
    src/ipc/buffer.c
    src/mem/alloc.c
    src/mem/pool.c
    src/mem/guard.c
    src/task/scheduler.c
    src/task/scheduler_core.c
    src/task/scheduler_platform.c
    src/task/sync.c
    src/task/thread.c
    src/time/clock.c
    src/time/timer.c
    src/time/event.c
)

# 平台特定调度器文件
if(WIN32)
    list(APPEND COREKERN_SOURCES src/task/scheduler_windows.c)
else()
    list(APPEND COREKERN_SOURCES src/task/scheduler_posix.c)
endif()

add_library(agentos_core STATIC ${COREKERN_SOURCES})

# 使用统一的头文件路径管理
target_link_libraries(agentos_core PUBLIC agentos_atoms agentos_common)

target_include_directories(agentos_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# 安全编译选项（基于AgentOS C编码风格指南附录A）
if(MSVC)
    target_compile_options(agentos_core PRIVATE /W4 /std:c11 /wd4819 /wd4117 /wd4273)
    target_compile_definitions(agentos_core PRIVATE _CRT_SECURE_NO_WARNINGS _CRT_SECURE_NO_DEPRECATE)
    # MSVC安全选项
    target_compile_options(agentos_core PRIVATE /GS)  # 缓冲区安全检查
    target_compile_options(agentos_core PRIVATE /sdl) # 启用额外安全检查
    target_compile_options(agentos_core PRIVATE /guard:cf) # 控制流防护
else()
    target_compile_options(agentos_core PRIVATE -Wall -Wextra -Wpedantic)
    # GCC/Clang安全选项
    target_compile_options(agentos_core PRIVATE -fstack-protector-strong)
    target_compile_options(agentos_core PRIVATE -D_FORTIFY_SOURCE=2)
    target_compile_options(agentos_core PRIVATE -fPIE)
    target_compile_options(agentos_core PRIVATE -Wformat -Wformat-security)
    target_compile_options(agentos_core PRIVATE -fno-omit-frame-pointer)
endif()

# 链接器安全选项
if(UNIX AND NOT APPLE)
    target_link_libraries(agentos_core PUBLIC pthread rt)
    target_link_options(agentos_core PRIVATE -Wl,-z,relro -Wl,-z,now -pie)
elseif(APPLE)
    target_link_libraries(agentos_core PUBLIC pthread)
    target_link_options(agentos_core PRIVATE -Wl,-dead_strip -pie)
endif()

# 发布构建的优化选项
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)  # 链接时优化

# 单元测试支持
if(BUILD_TESTS)
    enable_testing()
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests")
        add_subdirectory(tests)
    endif()
endif()
