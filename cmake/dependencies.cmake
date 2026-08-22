# =============================================================================
# dependencies.cmake — Airymax 统一依赖查找模块
# 版本：1.0.0
# 创建：2026-07-06
# 归属：agentrt 管理仓直属（v0.1.2 起自伞仓迁入，IRON-9 [IND] 独立层）
#
# 设计目标：
#   集中管理所有可选/必需的系统依赖查找逻辑，统一通过 pkg-config 或
#   find_package 查找，并设置对应的 AIRY_HAS_* 编译宏。避免在子模块
#   CMakeLists.txt 中重复 find_package / pkg_check_modules 调用。
#
#   查找的依赖：
#     必需: Threads
#     可选: PkgConfig, SQLite3, cJSON, YAML, OpenSSL, CURL,
#           libmicrohttpd, libwebsockets, libevent, FAISS
#
# 使用方式（在 CMakeLists.txt 中）:
#   list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
#   include(dependencies)
#   airy_find_required_deps()
#   airy_find_optional_deps()
# =============================================================================

include_guard(GLOBAL)

# =============================================================================
# airy_find_required_deps — 查找必需依赖
# 当前仅 Threads（Windows 使用内置线程支持）
# =============================================================================
function(airy_find_required_deps)
    if(WIN32)
        message(STATUS "Using Windows built-in thread support")
    else()
        find_package(Threads REQUIRED)
        message(STATUS "Threads library found: ${CMAKE_THREAD_LIBS_INIT}")
    endif()
endfunction()

# =============================================================================
# airy_find_optional_deps — 查找可选依赖
# 每个依赖设置对应的 AIRY_HAS_* 宏，未找到时仅警告不终止
# =============================================================================
function(airy_find_optional_deps)
    # PkgConfig 是其他 pkg_check_modules 的前置
    find_package(PkgConfig QUIET)
    if(NOT PkgConfig_FOUND)
        message(WARNING "PkgConfig not found, cannot check for optional libraries via pkg-config")
    endif()

    # ---- SQLite3 ----
    find_package(SQLite3 QUIET)
    if(NOT SQLite3_FOUND AND PkgConfig_FOUND)
        pkg_check_modules(SQLITE3 QUIET sqlite3)
    endif()
    if(SQLite3_FOUND OR SQLITE3_FOUND)
        add_compile_definitions(AIRY_HAS_SQLITE3=1)
        message(STATUS "SQLite3 found: ${SQLite3_VERSION}")
    else()
        message(WARNING "SQLite3 not found, storage features may be limited")
    endif()

    # ---- cJSON ----
    find_package(cJSON QUIET)
    if(NOT cJSON_FOUND AND PkgConfig_FOUND)
        pkg_check_modules(CJSON QUIET libcjson)
    endif()
    if(cJSON_FOUND OR CJSON_FOUND)
        add_compile_definitions(AIRY_HAS_CJSON=1)
        message(STATUS "cJSON found: ${cJSON_VERSION}")
    else()
        message(WARNING "cJSON not found, JSON features may be limited")
    endif()

    # ---- libyaml ----
    if(PkgConfig_FOUND)
        pkg_check_modules(YAML QUIET yaml-0.1)
        if(NOT YAML_FOUND)
            pkg_check_modules(YAML QUIET libyaml)
        endif()
    endif()
    if(YAML_FOUND)
        add_compile_definitions(AIRY_HAS_YAML=1)
        message(STATUS "libyaml found: ${YAML_VERSION}")
    else()
        message(WARNING "libyaml not found, YAML features may be limited")
    endif()

    # ---- OpenSSL ----
    find_package(OpenSSL QUIET)
    if(OpenSSL_FOUND)
        add_compile_definitions(AIRY_HAS_OPENSSL=1)
        message(STATUS "OpenSSL found: ${OPENSSL_VERSION}")
    else()
        message(WARNING "OpenSSL not found, TLS features may be limited")
    endif()

    # ---- libcurl ----
    find_package(CURL QUIET)
    if(NOT CURL_FOUND AND PkgConfig_FOUND)
        pkg_check_modules(LIBCURL QUIET libcurl)
    endif()
    if(CURL_FOUND OR LIBCURL_FOUND)
        add_compile_definitions(AIRY_HAS_CURL=1)
        message(STATUS "libcurl found: ${CURL_VERSION}${LIBCURL_VERSION}")
    else()
        message(WARNING "libcurl not found, network features may be limited")
    endif()

    # ---- libmicrohttpd ----
    find_package(libmicrohttpd QUIET)
    if(NOT libmicrohttpd_FOUND AND PkgConfig_FOUND)
        pkg_check_modules(MICROHTTPD QUIET libmicrohttpd)
    endif()
    if(libmicrohttpd_FOUND OR MICROHTTPD_FOUND)
        add_compile_definitions(AIRY_HAS_MICROHTTPD=1)
        message(STATUS "libmicrohttpd found")
    else()
        message(WARNING "libmicrohttpd not found, embedded HTTP server features disabled")
    endif()

    # ---- libwebsockets ----
    find_package(libwebsockets QUIET)
    if(NOT libwebsockets_FOUND AND PkgConfig_FOUND)
        pkg_check_modules(LIBWEBSOCKETS QUIET libwebsockets)
    endif()
    if(libwebsockets_FOUND OR LIBWEBSOCKETS_FOUND)
        add_compile_definitions(AIRY_HAS_LIBWEBSOCKETS=1)
        message(STATUS "libwebsockets found")
    else()
        message(WARNING "libwebsockets not found, WebSocket features disabled")
    endif()

    # ---- libevent ----
    find_package(libevent CONFIG QUIET)
    if(NOT libevent_FOUND AND PkgConfig_FOUND)
        pkg_check_modules(LIBEVENT QUIET libevent)
    endif()
    if(libevent_FOUND OR LIBEVENT_FOUND)
        add_compile_definitions(AIRY_HAS_LIBEVENT=1)
        message(STATUS "libevent found")
    else()
        message(WARNING "libevent not found, event loop features may use internal implementation")
    endif()

    # ---- FAISS（向量检索，主要用于 MemoryRovol）----
    if(PkgConfig_FOUND)
        pkg_check_modules(FAISS QUIET faiss)
        if(FAISS_FOUND)
            add_compile_definitions(AIRY_HAS_FAISS=1)
            message(STATUS "FAISS found: ${FAISS_VERSION}")
        else()
            message(WARNING "FAISS not found, some vector search features may be limited")
        endif()
    endif()
endfunction()

# =============================================================================
# airy_find_all_deps — 一键查找所有依赖
# 推荐在顶级 CMakeLists.txt 中调用
# =============================================================================
function(airy_find_all_deps)
    airy_find_required_deps()
    airy_find_optional_deps()
endfunction()

# =============================================================================
# airy_print_deps_summary — 打印依赖查找结果摘要
# =============================================================================
function(airy_print_deps_summary)
    message(STATUS "=========================================")
    message(STATUS "  Airymax Dependencies Summary")
    message(STATUS "=========================================")
    message(STATUS "Threads:         ${CMAKE_THREAD_LIBS_INIT}")
    message(STATUS "PkgConfig:       ${PkgConfig_FOUND}")
    message(STATUS "SQLite3:         ${SQLite3_FOUND}")
    message(STATUS "cJSON:           ${cJSON_FOUND}")
    message(STATUS "libyaml:         ${YAML_FOUND}")
    message(STATUS "OpenSSL:         ${OpenSSL_FOUND}")
    message(STATUS "libcurl:         ${CURL_FOUND}")
    message(STATUS "libmicrohttpd:   ${libmicrohttpd_FOUND}")
    message(STATUS "libwebsockets:   ${libwebsockets_FOUND}")
    message(STATUS "libevent:        ${libevent_FOUND}")
    message(STATUS "FAISS:           ${FAISS_FOUND}")
    message(STATUS "=========================================")
endfunction()
