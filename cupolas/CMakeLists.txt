cmake_minimum_required(VERSION 3.10)

project(cupolas C)

set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 11)

if(MSVC)
    set(CMAKE_C_FLAGS "/W4 /WX- /D_CRT_SECURE_NO_DEPRECATE /D_SCL_SECURE_NO_WARNINGS")
    set(CMAKE_C_FLAGS_DEBUG "/Zi /Od /RTC1")
    set(CMAKE_C_FLAGS_RELEASE "/O2 /GL /DNDEBUG")
    add_definitions(-DCUPOLAS_PLATFORM_WINDOWS=1)
    add_definitions(-D_CRT_SECURE_NO_WARNINGS)
    add_definitions(-DNOMINMAX)
    add_definitions(-D_WIN32_WINNT=0x0601)
else()
    set(CMAKE_C_FLAGS "-Wall -Wextra -Wpedantic -Wno-unused-parameter -fPIC")
    set(CMAKE_C_FLAGS_DEBUG "-g -O0 -DDEBUG")
    set(CMAKE_C_FLAGS_RELEASE "-O3 -DNDEBUG")
    add_definitions(-DCUPOLAS_PLATFORM_POSIX=1)
    if(APPLE)
        add_definitions(-DCUPOLAS_PLATFORM_MACOS=1)
        add_definitions(-DCUPOLAS_PLATFORM_64BIT=1)
    elseif(CMAKE_SIZEOF_VOID_P EQUAL 8)
        add_definitions(-DCUPOLAS_PLATFORM_LINUX=1)
        add_definitions(-DCUPOLAS_PLATFORM_64BIT=1)
    endif()

    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -pie")
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -pie")
endif()

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

find_package(OpenSSL REQUIRED)
if(OpenSSL_FOUND)
    message(STATUS "Found OpenSSL: ${OPENSSL_VERSION}")
else()
    message(FATAL_ERROR "OpenSSL is required for security modules")
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
    src/platform/platform.c
    src/cupolas_metrics.c
    src/cupolas_config.c
    src/cupolas_monitoring.c
    src/permission/permission_engine.c
    src/permission/permission_rule.c
    src/permission/permission_cache.c
    src/audit/audit_logger.c
    src/audit/audit_queue.c
    src/audit/audit_rotator.c
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
)

set(CUPOLAS_HEADERS
    include/cupolas.h
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
)

# commons 模块路径
set(COMMON_MODULE_DIR ${CMAKE_SOURCE_DIR}/../commons)

# commons 模块的 include 目录
set(COMMON_INCLUDE_DIRS
    ${COMMON_MODULE_DIR}/platform/include
    ${COMMON_MODULE_DIR}/utils/core/include
    ${COMMON_MODULE_DIR}/utils/error/include
    ${COMMON_MODULE_DIR}/utils/observability/include
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
    ${OPENSSL_INCLUDE_DIR}
)

# 链接 commons 模块
target_link_libraries(${PROJECT_NAME} PRIVATE agentos_common ${OPENSSL_LIBRARIES})

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
    target_link_libraries(test_cupolas_core ${PROJECT_NAME})

    add_executable(test_cupolas_integration tests/integration/test_cupolas_integration.c)
    target_link_libraries(test_cupolas_integration ${PROJECT_NAME})

    if(MSVC)
        add_custom_command(TARGET test_cupolas_core POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE> $<TARGET_FILE_DIR>)
    endif()

    add_test(NAME cupolas_core_tests COMMAND test_cupolas_core)
    add_test(NAME cupolas_integration_tests COMMAND test_cupolas_integration)
endif()

configure_file(config.h.in ${CMAKE_CURRENT_BINARY_DIR}/config.h @ONLY)

install(TARGETS ${PROJECT_NAME}
    EXPORT cupolas-targets
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib/static
)

install(EXPORT cupolas-targets
    FILE cupolas-targets.cmake
    DESTINATION lib/cmake
)

install(FILES ${CUPOLAS_HEADERS}
    DESTINATION include/cupolas
)

install(FILES include/cupolas.h
    DESTINATION include
)
