# Copyright (c) 2026 SPHARX. All Rights Reserved. "From data intelligence emerges."
# 认知层构建文件

cmake_minimum_required(VERSION 3.20)
project(agentos-cognition)

# 收集所有源文件
file(GLOB COGNITION_SOURCES
    "engine.c"
    "planner/*.c"
    "coordinator/*.c"
    "dispatcher/*.c"
)

# 创建静态库
add_library(agentos_cognition STATIC ${COGNITION_SOURCES})

# 包含头文件路径
target_include_directories(agentos_cognition PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../core/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../memoryrovol/include  # 如果需要记忆引擎头文件
)

# 链接依赖（如果其他模块已定义）
target_link_libraries(agentos_cognition
    agentos_core          # 基础内核库（提供同步原语）
    # 其他依赖如 llm_client 将在外部链接
)

# 编译选项
target_compile_options(agentos_cognition PRIVATE -Wall -Wextra -Werror -O2)

# 安装
install(TARGETS agentos_cognition DESTINATION lib)
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/../../include/cognition.h DESTINATION include/agentos)