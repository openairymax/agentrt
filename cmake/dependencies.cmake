# =============================================================================
# dependencies.cmake — Airymax 统一依赖查找模块
# 版本：1.1.0
# 创建：2026-07-06
# 归属：agentrt 管理仓直属（v0.1.2 起自伞仓迁入，IRON-9 [IND] 独立层）
#
# 设计目标：
#   集中管理所有可选/必需的系统依赖查找逻辑，统一通过 pkg-config 或
#   find_package 查找。查找结果以 AIRY_HAS_* CACHE BOOL 单点记录（检测），
#   宏下发收口到 airy_apply_dep_defs()（策略）——机制与策略分离。
#   避免在子模块 CMakeLists.txt 中重复 find_package / pkg_check_modules 调用。
#
#   查找的依赖：
#     必需: Threads
#     可选: PkgConfig, SQLite3, cJSON, YAML, OpenSSL, CURL,
#           libmicrohttpd, libwebsockets, libevent, FAISS
#
# 1.1.0（§254c，产品仓独立构建承接）：
#   - AIRY_HAS_* 由函数内 add_compile_definitions 改为 CACHE BOOL FORCE，
#     跨作用域可见且每次 configure 重算；宏下发统一收口 airy_apply_dep_defs
#     （与 agentrt 根 airy_compile_defs 双路下发形态等价对齐）。
#   - pkg_check_modules 系宏，函数内调用产生的 normal 变量不外泄；
#     YAML_* / CJSON_* 等目录作用域消费变量在函数尾部 PARENT_SCOPE 提升。
#   - cJSON 补 find_path/find_library 兜底、YAML 补 keg-only 绝对路径提升
#     （镜像 agentrt 根检测形态，消除独立构建与嵌入构建的行为差异）。
#
# 使用方式（在 CMakeLists.txt 中）:
#   list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
#   include(dependencies)
#   airy_find_required_deps()
#   airy_find_optional_deps()
#   airy_apply_dep_defs()
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
        set(AIRY_HAS_SQLITE3 TRUE CACHE BOOL "SQLite3 available" FORCE)
        message(STATUS "SQLite3 found: ${SQLite3_VERSION}")
    else()
        set(AIRY_HAS_SQLITE3 FALSE CACHE BOOL "SQLite3 available" FORCE)
        message(WARNING "SQLite3 not found, storage features may be limited")
    endif()

    # ---- cJSON ----
    find_package(cJSON QUIET)
    if(NOT cJSON_FOUND AND PkgConfig_FOUND)
        pkg_check_modules(CJSON QUIET libcjson)
    endif()
    # 部分发行版（如 Ubuntu 20.04）libcjson-dev 不安装 .pc 文件，pkg-config
    # 与 find_package 均失效，回退 find_path/find_library 直查（镜像根形态）。
    if(NOT cJSON_FOUND AND NOT CJSON_FOUND)
        find_path(CJSON_INCLUDE_DIR cjson/cJSON.h)
        find_library(CJSON_LIBRARY NAMES cjson)
        if(CJSON_INCLUDE_DIR AND CJSON_LIBRARY)
            set(CJSON_INCLUDE_DIRS ${CJSON_INCLUDE_DIR})
            set(CJSON_LIBRARIES ${CJSON_LIBRARY})
            set(CJSON_FOUND TRUE)
        endif()
    endif()
    if(cJSON_FOUND OR CJSON_FOUND)
        set(AIRY_HAS_CJSON TRUE CACHE BOOL "cJSON available" FORCE)
        if(cJSON_VERSION)
            message(STATUS "cJSON found: ${cJSON_VERSION}")
        elseif(CJSON_VERSION)
            message(STATUS "cJSON found: ${CJSON_VERSION}")
        else()
            message(STATUS "cJSON found")
        endif()
    else()
        set(AIRY_HAS_CJSON FALSE CACHE BOOL "cJSON available" FORCE)
        message(WARNING "cJSON not found, JSON features may be limited")
    endif()

    # ---- libyaml ----
    find_package(YAML QUIET)
    if(NOT YAML_FOUND AND PkgConfig_FOUND)
        pkg_check_modules(YAML QUIET yaml-0.1)
    endif()
    if(NOT YAML_FOUND AND PkgConfig_FOUND)
        pkg_check_modules(YAML QUIET libyaml)
    endif()
    if(YAML_FOUND)
        # libyaml keg-only 安装（Homebrew）只把目录给 pkg-config，-L 到不了
        # 链接行，下游裸 ${YAML_LIBRARIES}（-lyaml）链接失败；提升为绝对
        # 路径后所有消费点无需 -L（镜像根形态）。
        if(NOT IS_ABSOLUTE "${YAML_LIBRARIES}")
            find_library(YAML_ABSOLUTE_LIBRARY
                         NAMES yaml yaml-0.1
                         HINTS ${YAML_LIBRARY_DIRS}
                         PATH_SUFFIXES lib)
            if(YAML_ABSOLUTE_LIBRARY)
                set(YAML_LIBRARIES "${YAML_ABSOLUTE_LIBRARY}")
            endif()
        endif()
        set(AIRY_HAS_YAML TRUE CACHE BOOL "libyaml available" FORCE)
        message(STATUS "libyaml found: ${YAML_LIBRARIES}")
    else()
        set(AIRY_HAS_YAML FALSE CACHE BOOL "libyaml available" FORCE)
        message(WARNING "libyaml not found, YAML features may be limited")
    endif()

    # ---- OpenSSL ----
    find_package(OpenSSL QUIET)
    if(OpenSSL_FOUND)
        set(AIRY_HAS_OPENSSL TRUE CACHE BOOL "OpenSSL available" FORCE)
        message(STATUS "OpenSSL found: ${OPENSSL_VERSION}")
    else()
        set(AIRY_HAS_OPENSSL FALSE CACHE BOOL "OpenSSL available" FORCE)
        message(WARNING "OpenSSL not found, TLS features may be limited")
    endif()

    # ---- libcurl ----
    find_package(CURL QUIET)
    if(NOT CURL_FOUND AND PkgConfig_FOUND)
        pkg_check_modules(LIBCURL QUIET libcurl)
    endif()
    if(CURL_FOUND OR LIBCURL_FOUND)
        set(AIRY_HAS_CURL TRUE CACHE BOOL "libcurl available" FORCE)
        message(STATUS "libcurl found: ${CURL_VERSION}${LIBCURL_VERSION}")
    else()
        set(AIRY_HAS_CURL FALSE CACHE BOOL "libcurl available" FORCE)
        message(WARNING "libcurl not found, network features may be limited")
    endif()

    # ---- libmicrohttpd ----
    find_package(libmicrohttpd QUIET)
    if(NOT libmicrohttpd_FOUND AND PkgConfig_FOUND)
        pkg_check_modules(MICROHTTPD QUIET libmicrohttpd)
    endif()
    if(libmicrohttpd_FOUND OR MICROHTTPD_FOUND)
        set(AIRY_HAS_MICROHTTPD TRUE CACHE BOOL "libmicrohttpd available" FORCE)
        message(STATUS "libmicrohttpd found")
    else()
        set(AIRY_HAS_MICROHTTPD FALSE CACHE BOOL "libmicrohttpd available" FORCE)
        message(WARNING "libmicrohttpd not found, embedded HTTP server features disabled")
    endif()

    # ---- libwebsockets ----
    find_package(libwebsockets QUIET)
    if(NOT libwebsockets_FOUND AND PkgConfig_FOUND)
        pkg_check_modules(LIBWEBSOCKETS QUIET libwebsockets)
    endif()
    if(libwebsockets_FOUND OR LIBWEBSOCKETS_FOUND)
        set(AIRY_HAS_LIBWEBSOCKETS TRUE CACHE BOOL "libwebsockets available" FORCE)
        message(STATUS "libwebsockets found")
    else()
        set(AIRY_HAS_LIBWEBSOCKETS FALSE CACHE BOOL "libwebsockets available" FORCE)
        message(WARNING "libwebsockets not found, WebSocket features disabled")
    endif()

    # ---- libevent ----
    find_package(libevent CONFIG QUIET)
    if(NOT libevent_FOUND AND PkgConfig_FOUND)
        pkg_check_modules(LIBEVENT QUIET libevent)
    endif()
    if(libevent_FOUND OR LIBEVENT_FOUND)
        set(AIRY_HAS_LIBEVENT TRUE CACHE BOOL "libevent available" FORCE)
        message(STATUS "libevent found")
    else()
        set(AIRY_HAS_LIBEVENT FALSE CACHE BOOL "libevent available" FORCE)
        message(WARNING "libevent not found, event loop features may use internal implementation")
    endif()

    # ---- FAISS（向量检索，主要用于 MemoryRovol）----
    # 注：无 AIRY_HAS_FAISS 宏消费者，不入 airy_apply_dep_defs 下发清单。
    if(PkgConfig_FOUND)
        pkg_check_modules(FAISS QUIET faiss)
        if(FAISS_FOUND)
            set(AIRY_HAS_FAISS TRUE CACHE BOOL "FAISS available" FORCE)
            message(STATUS "FAISS found: ${FAISS_VERSION}")
        else()
            set(AIRY_HAS_FAISS FALSE CACHE BOOL "FAISS available" FORCE)
            message(WARNING "FAISS not found, some vector search features may be limited")
        endif()
    endif()

    # ---- 提升调用者作用域 ----
    # pkg_check_modules 系宏，函数内产生的 normal 变量不外泄；目录作用域
    # 消费者（产品壳层 / commons 条件）直读 ${YAML_*} ${CJSON_*}，须显式提升。
    set(YAML_FOUND "${YAML_FOUND}" PARENT_SCOPE)
    set(YAML_LIBRARIES "${YAML_LIBRARIES}" PARENT_SCOPE)
    set(YAML_INCLUDE_DIRS "${YAML_INCLUDE_DIRS}" PARENT_SCOPE)
    set(YAML_LIBRARY_DIRS "${YAML_LIBRARY_DIRS}" PARENT_SCOPE)
    set(CJSON_FOUND "${CJSON_FOUND}" PARENT_SCOPE)
    set(CJSON_LIBRARIES "${CJSON_LIBRARIES}" PARENT_SCOPE)
    set(CJSON_INCLUDE_DIRS "${CJSON_INCLUDE_DIRS}" PARENT_SCOPE)
    # FindOpenSSL 的 OPENSSL_LIBRARIES/OPENSSL_INCLUDE_DIR 为 normal 变量；
    # embedded 下根顶层 find_package 天然可见，standalone 下函数作用域须提升，
    # 产品壳层（cupolas body）直读该消费面，两树供给形态一致。
    set(OPENSSL_LIBRARIES "${OPENSSL_LIBRARIES}" PARENT_SCOPE)
    set(OPENSSL_INCLUDE_DIR "${OPENSSL_INCLUDE_DIR}" PARENT_SCOPE)
    set(OPENSSL_CRYPTO_LIBRARY "${OPENSSL_CRYPTO_LIBRARY}" PARENT_SCOPE)
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
# airy_apply_dep_defs — AIRY_HAS_* 宏统一下发（检测/下发收口的下发侧）
# 幂等创建 airy_compile_defs INTERFACE 目标；对每个已检出依赖双路下发
# （INTERFACE 传递 + add_compile_definitions 全局裸名），保证未链接 defs
# 的目标与 MSVC /FI 强制预包含目标的宏视图一致（对齐 agentrt 根的
# airy_compile_defs 映射块形态；宏清单一致，不含零消费的 FAISS）。
# 须在顶层目录调用：add_compile_definitions 修改调用者目录属性，
# 顶层调用即覆盖全部子目录。
# =============================================================================
function(airy_apply_dep_defs)
    if(NOT TARGET airy_compile_defs)
        add_library(airy_compile_defs INTERFACE)
    endif()
    foreach(_dep SQLITE3 CJSON YAML OPENSSL CURL MICROHTTPD LIBWEBSOCKETS LIBEVENT)
        if(AIRY_HAS_${_dep})
            target_compile_definitions(airy_compile_defs INTERFACE AIRY_HAS_${_dep})
            add_compile_definitions(AIRY_HAS_${_dep})
        endif()
    endforeach()
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
