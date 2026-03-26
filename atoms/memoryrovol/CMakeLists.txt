# Copyright (c) 2026 SPHARX. All Rights Reserved. "From data intelligence emerges."
# MemoryRovol 模块顶层构建文件

cmake_minimum_required(VERSION 3.20)
project(agentos-memoryrovol VERSION 1.0.0.5 LANGUAGES C)

# 选项
option(BUILD_TESTS "Build tests" OFF)
option(ENABLE_TRACING "Enable OpenTelemetry tracing" OFF)
option(USE_SENTENCE_TRANSFORMERS "Enable Sentence Transformers support" OFF)

# 查找必需依赖
find_package(PkgConfig REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(Threads REQUIRED)

# 查找 FAISS
pkg_check_modules(FAISS REQUIRED faiss)

# 查找 libcurl
pkg_check_modules(LIBCURL REQUIRED libcurl)

# 查找 cJSON
pkg_check_modules(CJSON REQUIRED libcjson)

# 查找 libyaml
pkg_check_modules(YAML REQUIRED yaml-0.1)

# 查找 SQLite3
pkg_check_modules(SQLITE3 REQUIRED sqlite3)

# 可选：Sentence Transformers
if(USE_SENTENCE_TRANSFORMERS)
    pkg_check_modules(SENTENCE_TRANSFORMERS sentence_transformers)
    if(SENTENCE_TRANSFORMERS_FOUND)
        add_definitions(-DHAVE_SENTENCE_TRANSFORMERS)
    else()
        message(WARNING "Sentence Transformers not found, disabling support")
    endif()
endif()

# 包含子目录
add_subdirectory(src)

# 安装头文件
install(DIRECTORY include/ DESTINATION include/agentos/memoryrovol)

# 安装库
install(TARGETS agentos_memoryrovol DESTINATION lib)

# 安全编译选项（基于AgentOS C编码风格指南附录A）
if(MSVC)
    target_compile_options(agentos_memoryrovol PRIVATE /W4 /WX)
    target_compile_definitions(agentos_memoryrovol PRIVATE _CRT_SECURE_NO_WARNINGS)
    # MSVC安全选项
    target_compile_options(agentos_memoryrovol PRIVATE /GS)  # 缓冲区安全检查
    target_compile_options(agentos_memoryrovol PRIVATE /sdl) # 启用额外安全检查
    target_compile_options(agentos_memoryrovol PRIVATE /guard:cf) # 控制流防护
else()
    target_compile_options(agentos_memoryrovol PRIVATE -Wall -Wextra -Wpedantic -Werror)
    # GCC/Clang安全选项
    target_compile_options(agentos_memoryrovol PRIVATE -fstack-protector-strong)
    target_compile_options(agentos_memoryrovol PRIVATE -D_FORTIFY_SOURCE=2)
    target_compile_options(agentos_memoryrovol PRIVATE -fPIE)
    target_compile_options(agentos_memoryrovol PRIVATE -Wformat -Wformat-security)
    target_compile_options(agentos_memoryrovol PRIVATE -fno-omit-frame-pointer)
endif()

# 链接器安全选项
if(UNIX AND NOT APPLE)
    target_link_options(agentos_memoryrovol PRIVATE -Wl,-z,relro -Wl,-z,now -pie)
elseif(APPLE)
    target_link_options(agentos_memoryrovol PRIVATE -Wl,-dead_strip -pie)
endif()

# 单元测试
if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()