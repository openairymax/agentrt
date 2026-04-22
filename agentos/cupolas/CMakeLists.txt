cmake_minimum_required(VERSION 3.10)

project(cupolas C)

set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 11)

# 使用父项目的全局编译选项，避免重复设置
# 父项目已设置全局安全编译选项

option(BUILD_TESTS "Build tests" OFF)
option(BUILD_WITH_SANITIZERS "Enable sanitizers (ASAN/MSAN/TSAN)" OFF)
option(BUILD_WITH_LOGGING "Enable detailed logging" ON)

if(BUILD_WITH_LOGGING)
    add_definitions(-DCUPOLAS_ENABLE_LOGGING=1)
endif()

find_package(PkgConfig QUIET)

if(PKG_CONFIG_FOUND)
    pkg_check_modules(YAML QUIET libyaml)
    pkg_check_modules(CJSON QUIET libcjson)
endif()

# OpenSSL查找（与根项目保持一致）
find_package(OpenSSL QUIET)
if(OpenSSL_FOUND)
    message(STATUS "Found OpenSSL: ${OpenSSL_VERSION}")
    add_definitions(-DCUPOLAS_HAS_OPENSSL=1)
else()
    message(WARNING "OpenSSL not found, some security features may be limited")
    # 在没有OpenSSL的情况下，可以编译但某些功能受限
endif()

if(YAML_FOUND OR YAML_LIBRARIES)
    set(HAVE_YAML 1)
    message(STATUS "Found libyaml: ${YAML_LIBRARIES}")
else()
    set(HAVE_YAML 0)
    message(STATUS "libyaml not found, YAML manager support disabled")
endif()

if(CJSON_FOUND OR CJSON_LIBRARIES)
    set(HAVE_CJSON 1)
    message(STATUS "Found libcjson: ${CJSON_LIBRARIES}")
else()
    set(HAVE_CJSON 0)
    message(STATUS "libcjson not found, JSON support disabled")
endif()

set(CUPOLAS_SOURCES
    src/cupolas.c
    src/platform/platform.c
    src/utils/cupolas_utils.c
    src/cupolas_metrics.c
    src/cupolas_config.c
    src/cupolas_monitoring.c
    src/permission/permission_engine.c
    src/permission/permission_rule.c
    src/permission/permission_cache.c
    src/audit/audit_logger.c
    src/audit/audit_queue.c
    src/audit/audit_rotator.c
    src/audit/audit_overflow.c
    src/sanitizer/sanitizer_core.c
    src/sanitizer/sanitizer_rules.c
    src/sanitizer/sanitizer_cache.c
    src/workbench/workbench.c
    src/workbench/workbench_process_core.c
    src/workbench/workbench_container.c
    src/workbench/workbench_limits.c
    src/security/cupolas_signature.c
    src/security/cupolas_vault.c
    src/security/cupolas_entitlements.c
    src/security/cupolas_runtime_protection.c
    src/security/cupolas_network_security.c
    src/security/cupolas_error.c
    src/security/network/tls_security.c
    src/security/network/http_security.c
    src/security/network/dns_security.c
    src/security/network/network_filter.c
    src/security/network/network_utils.c
    src/guards/guard_core.c
    src/guards/guard_integration.c
    src/circuit_breaker.c
)

set(CUPOLAS_HEADERS
    include/cupolas.h
    src/utils/cupolas_utils.h
    src/cupolas_metrics.h
    src/cupolas_config.h
    src/cupolas_monitoring.h
    src/platform/platform.h
    src/permission/permission.h
    src/permission/permission_engine.h
    src/permission/permission_rule.h
    src/permission/permission_cache.h
    src/audit/audit.h
    src/audit/audit_queue.h
    src/audit/audit_rotator.h
    src/audit/audit_overflow.h
    src/sanitizer/sanitizer.h
    src/sanitizer/sanitizer_rules.h
    src/sanitizer/sanitizer_cache.h
    src/workbench/workbench.h
    src/workbench/workbench_process.h
    src/workbench/workbench_container.h
    src/workbench/workbench_limits.h
    src/security/cupolas_signature.h
    src/security/cupolas_vault.h
    src/security/cupolas_entitlements.h
    src/security/cupolas_runtime_protection.h
    src/security/cupolas_network_security.h
    src/security/cupolas_error.h
    src/security/network/tls_security.h
    src/security/network/http_security.h
    src/security/network/dns_security.h
    src/security/network/network_filter.h
    src/security/network/network_utils.h
    src/guards/guard_core.h
    src/guards/guard_integration.h
    src/circuit_breaker.h
)

# commons 模块路径
set(COMMON_MODULE_DIR ${CMAKE_SOURCE_DIR}/agentos/commons)

# commons 模块的 include 目录
set(COMMON_INCLUDE_DIRS
    ${COMMON_MODULE_DIR}/platform/include
    ${COMMON_MODULE_DIR}/utils/core/include
    ${COMMON_MODULE_DIR}/utils/error/include
    ${COMMON_MODULE_DIR}/utils/observability/include
    ${COMMON_MODULE_DIR}/utils/types/include
)

add_library(cupolas STATIC ${CUPOLAS_SOURCES} ${CUPOLAS_HEADERS})

target_include_directories(${PROJECT_NAME} PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
    $<INSTALL_INTERFACE:include>
    $<INSTALL_INTERFACE:include/cupolas>
)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${COMMON_INCLUDE_DIRS}
)

# 条件包含OpenSSL目录
if(OpenSSL_FOUND)
    target_include_directories(${PROJECT_NAME} PRIVATE ${OPENSSL_INCLUDE_DIR})
endif()

# 链接 commons 模块
target_link_libraries(${PROJECT_NAME} PRIVATE agentos_common)

# 条件链接OpenSSL库
if(OpenSSL_FOUND)
    target_link_libraries(${PROJECT_NAME} PRIVATE ${OPENSSL_LIBRARIES})
endif()

if(YAML_FOUND)
    target_include_directories(${PROJECT_NAME} PRIVATE ${YAML_INCLUDE_DIRS})
endif()

if(CJSON_FOUND)
    target_include_directories(${PROJECT_NAME} PRIVATE ${CJSON_INCLUDE_DIRS})
endif()

if(MSVC)
    target_link_libraries(${PROJECT_NAME} PRIVATE ws2_32 advapi32)

    target_compile_options(${PROJECT_NAME} PRIVATE
        /W4
        $<$<CONFIG:Release>:/Guard:cf /guard:ehcont>
    )
else()
    find_package(Threads REQUIRED)
    target_link_libraries(${PROJECT_NAME} PRIVATE ${CMAKE_THREAD_LIBS_LIBRARY} m)

    target_compile_options(${PROJECT_NAME} PRIVATE
        -Wall -Wextra -Wpedantic
        -Wformat=2 -Wformat-security
        -fstack-protector-strong
        -fstack-clash-protection
        -D_FORTIFY_SOURCE=2
        -fvisibility=hidden
    )

    target_link_options(${PROJECT_NAME} PRIVATE
        -Wl,-z,relro
        -Wl,-z,now
        -Wl,-z,noexecstack
    )

    if(BUILD_WITH_SANITIZERS)
        target_compile_options(${PROJECT_NAME} PRIVATE
            -fsanitize=address,undefined,leak
            -fno-omit-frame-pointer
        )
        target_link_options(${PROJECT_NAME} PRIVATE
            -fsanitize=address,undefined,leak
        )
    endif()
endif()

if(YAML_FOUND)
    target_link_libraries(${PROJECT_NAME} PRIVATE ${YAML_LIBRARIES})
endif()

if(CJSON_FOUND)
    target_link_libraries(${PROJECT_NAME} PRIVATE ${CJSON_LIBRARIES})
endif()

if(BUILD_TESTS)
    enable_testing()

    add_executable(test_cupolas_core tests/unit/test_cupolas_core.c)
    target_include_directories(test_cupolas_core PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${COMMON_INCLUDE_DIRS}
    )
    target_link_libraries(test_cupolas_core ${PROJECT_NAME})

    add_executable(test_cupolas_integration tests/integration/test_cupolas_integration.c)
    target_include_directories(test_cupolas_integration PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${COMMON_INCLUDE_DIRS}
    )
    target_link_libraries(test_cupolas_integration ${PROJECT_NAME})

    add_executable(test_cupolas_signature tests/unit/test_cupolas_signature.c)
    target_include_directories(test_cupolas_signature PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${COMMON_INCLUDE_DIRS}
    )
    target_link_libraries(test_cupolas_signature ${PROJECT_NAME})

    add_executable(test_cupolas_vault tests/unit/test_cupolas_vault.c)
    target_include_directories(test_cupolas_vault PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${COMMON_INCLUDE_DIRS}
    )
    target_link_libraries(test_cupolas_vault ${PROJECT_NAME})

    add_executable(test_stress_concurrent tests/stress/test_stress_concurrent.c)
    target_include_directories(test_stress_concurrent PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${COMMON_INCLUDE_DIRS}
    )
    target_link_libraries(test_stress_concurrent ${PROJECT_NAME})

    if(MSVC)
        add_custom_command(TARGET test_cupolas_core POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE> $<TARGET_FILE_DIR>)
    endif()

    add_test(NAME cupolas_core_tests COMMAND test_cupolas_core)
    add_test(NAME cupolas_integration_tests COMMAND test_cupolas_integration)
    add_test(NAME cupolas_signature_tests COMMAND test_cupolas_signature)
    add_test(NAME cupolas_vault_tests COMMAND test_cupolas_vault)
    add_test(NAME cupolas_stress_tests COMMAND test_stress_concurrent)
endif()

install(TARGETS ${PROJECT_NAME}
    EXPORT cupolas-targets
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib/static
)

# install(EXPORT cupolas-targets
#     FILE cupolas-targets.cmake
#     DESTINATION lib/cmake
# )

install(FILES ${CUPOLAS_HEADERS}
    DESTINATION include/cupolas
)

install(FILES include/cupolas.h
    DESTINATION include
)
