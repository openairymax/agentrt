# Copyright (c) 2026 SPHARX. All Rights Reserved. "From data intelligence emerges."
# MemoryRovol 模块顶层构建文件

cmake_minimum_required(VERSION 3.20)
project(agentos-memoryrovol VERSION 1.0.0.5 LANGUAGES C)

# 选项
option(BUILD_TESTS "Build tests" OFF)
option(ENABLE_TRACING "Enable OpenTelemetry tracing" OFF)
option(USE_SENTENCE_TRANSFORMERS "Enable Sentence Transformers support" OFF)

# 查找依赖
find_package(PkgConfig QUIET)
find_package(OpenSSL QUIET)
find_package(Threads REQUIRED)

# 查找 FAISS
if(PkgConfig_FOUND)
    pkg_check_modules(FAISS QUIET faiss)
    if(FAISS_FOUND)
        message(STATUS "FAISS found: ${FAISS_VERSION}")
        add_definitions(-DHAVE_FAISS)
    else()
        message(WARNING "FAISS not found, some vector search features may be limited")
    endif()
else()
    message(WARNING "PkgConfig not found, cannot check for FAISS")
endif()

# 查找 libcurl
if(PkgConfig_FOUND)
    pkg_check_modules(LIBCURL QUIET libcurl)
    if(LIBCURL_FOUND)
        message(STATUS "libcurl found: ${LIBCURL_VERSION}")
        add_definitions(-DHAVE_LIBCURL)
    else()
        message(WARNING "libcurl not found, network features may be limited")
    endif()
else()
    message(WARNING "PkgConfig not found, cannot check for libcurl")
endif()

# 查找 cJSON
if(PkgConfig_FOUND)
    pkg_check_modules(CJSON QUIET libcjson)
    if(CJSON_FOUND)
        message(STATUS "cJSON found: ${CJSON_VERSION}")
        add_definitions(-DHAVE_CJSON)
    else()
        message(WARNING "cJSON not found, JSON features may be limited")
    endif()
else()
    message(WARNING "PkgConfig not found, cannot check for cJSON")
endif()

# 查找 libyaml
if(PkgConfig_FOUND)
    pkg_check_modules(YAML QUIET yaml-0.1)
    if(YAML_FOUND)
        message(STATUS "libyaml found: ${YAML_VERSION}")
        add_definitions(-DHAVE_YAML)
    else()
        message(WARNING "libyaml not found, YAML features may be limited")
    endif()
else()
    message(WARNING "PkgConfig not found, cannot check for libyaml")
endif()

# 查找 SQLite3
if(PkgConfig_FOUND)
    pkg_check_modules(SQLITE3 QUIET sqlite3)
    if(SQLITE3_FOUND)
        message(STATUS "SQLite3 found: ${SQLITE3_VERSION}")
        add_definitions(-DHAVE_SQLITE3)
    else()
        message(WARNING "SQLite3 not found, storage features may be limited")
    endif()
else()
    message(WARNING "PkgConfig not found, cannot check for SQLite3")
endif()

# 可选：Sentence Transformers
if(USE_SENTENCE_TRANSFORMERS AND PkgConfig_FOUND)
    pkg_check_modules(SENTENCE_TRANSFORMERS sentence_transformers)
    if(SENTENCE_TRANSFORMERS_FOUND)
        add_definitions(-DHAVE_SENTENCE_TRANSFORMERS)
    else()
        message(WARNING "Sentence Transformers not found, disabling support")
    endif()
endif()

# 包含子目录
add_subdirectory(src)

# 安全编译选项（基于AgentOS C编码风格指南附录A）
if(TARGET agentos_memoryrovol)
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

    # 安装头文件
    install(DIRECTORY include/ DESTINATION include/agentos/memoryrovol)

    # 安装库
    install(TARGETS agentos_memoryrovol DESTINATION lib)
endif()

# 单元测试
if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()