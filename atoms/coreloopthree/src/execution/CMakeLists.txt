# Copyright (c) 2026 SPHARX. All Rights Reserved. "From data intelligence emerges."
# 行动层构建文件

cmake_minimum_required(VERSION 3.20)
project(agentos-execution)

# 收集源文件
file(GLOB EXECUTION_SOURCES
    "engine.c"
    "registry.c"
    "compensation.c"
    "trace.c"
    "units/*.c"
)

# 创建静态库
add_library(agentos_execution STATIC ${EXECUTION_SOURCES})

# 包含头文件路径
target_include_directories(agentos_execution PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../core/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../..  # 为了包含 registry.h 等（可选）
)

# 链接依赖
target_link_libraries(agentos_execution
    agentos_core          # 基础内核库（提供同步原语、时间等）
)

# 编译选项
target_compile_options(agentos_execution PRIVATE -Wall -Wextra -Werror -O2)

# 安装
install(TARGETS agentos_execution DESTINATION lib)
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/../../include/execution.h DESTINATION include/agentos)
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/../../include/registry.h DESTINATION include/agentos)   # 如果存在