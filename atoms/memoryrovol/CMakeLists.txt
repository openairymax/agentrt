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

# 单元测试
if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()