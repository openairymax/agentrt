cmake_minimum_required(VERSION 3.20)
project(agentos-commons VERSION 0.0.5 LANGUAGES C)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(SOURCES
    platform/platform.c
    utils/platform/platform_adapter.c
    utils/token/counter.c
    utils/token/budget.c
    utils/token/token_standard.c
    utils/cost/estimator.c
    utils/cost/controller.c
    utils/observability/logger.c
    utils/observability/metrics.c
    utils/observability/trace.c
    utils/error/handler.c
    utils/io/file_utils.c
    utils/security/input_validator.c
    utils/resource/resource_guard.c
    utils/logging/src/logging.c
    utils/logging/src/logging_compat.c
    utils/logging/logging_common.c
    utils/memory/src/memory.c
    utils/memory/memory_common.c
    utils/string/string_common.c
    utils/sync/src/sync.c
    utils/sync/src/sync_platform.c
    utils/sync/sync_common.c
    utils/cache/cache_common.c
    utils/compat/src/compat.c
    utils/cognition/cognition_common.c
    utils/execution/execution_common.c
    utils/strategy/strategy_common.c
    utils/network/network_common.c
)
if(NOT WIN32)
    list(APPEND SOURCES utils/ipc/ipc_common.c)
endif()

add_library(agentos_common STATIC ${SOURCES})

if(MSVC)
    target_compile_options(agentos_common PRIVATE /std:c11 /utf-8 /W3 /wd4819 /wd4005 /D_ENABLE_ATOMIC_ALIGNMENT_FIX)
endif()

target_include_directories(agentos_common PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/platform/include
    ${CMAKE_CURRENT_SOURCE_DIR}/utils/types/include
    ${CMAKE_CURRENT_SOURCE_DIR}/utils/logging/include
    ${CMAKE_CURRENT_SOURCE_DIR}/utils/error/include
    ${CMAKE_CURRENT_SOURCE_DIR}/utils/sync/include
    ${CMAKE_CURRENT_SOURCE_DIR}/utils/execution/include
    ${CMAKE_CURRENT_SOURCE_DIR}/utils/include
    ${CMAKE_CURRENT_SOURCE_DIR}/utils/memory
    ${CMAKE_CURRENT_SOURCE_DIR}/utils/string
    ${CMAKE_CURRENT_SOURCE_DIR}/../atoms/corekern/include
    ${CMAKE_CURRENT_SOURCE_DIR}/utils/compat/include
    ${CMAKE_CURRENT_SOURCE_DIR}/utils/token/include
    ${CMAKE_CURRENT_SOURCE_DIR}/utils/cost/include
    ${CMAKE_CURRENT_SOURCE_DIR}/utils/observability/include
    ${CMAKE_CURRENT_SOURCE_DIR}/utils/io/include
    ${CMAKE_CURRENT_SOURCE_DIR}/utils/security/include
    ${CMAKE_CURRENT_SOURCE_DIR}/utils/resource/include
    ${CMAKE_CURRENT_SOURCE_DIR}/utils/memory/include
    ${CMAKE_CURRENT_SOURCE_DIR}/utils/string/include
    ${CMAKE_CURRENT_SOURCE_DIR}/utils/cache/include
    ${CMAKE_CURRENT_SOURCE_DIR}/utils/platform/include
    ${CMAKE_CURRENT_SOURCE_DIR}/utils/config_unified/include
    ${CMAKE_CURRENT_SOURCE_DIR}/utils/cognition/include
    ${CMAKE_CURRENT_SOURCE_DIR}/utils/strategy/include
    ${CMAKE_CURRENT_SOURCE_DIR}/utils/network/include
    ${CMAKE_CURRENT_SOURCE_DIR}/utils/ipc/include
)

target_compile_definitions(agentos_common PRIVATE _CRT_SECURE_NO_WARNINGS)
if(NOT AGENTOS_HAS_CJSON)
    target_compile_definitions(agentos_common PRIVATE AGENTOS_NO_CJSON)
endif()

target_link_libraries(agentos_common
    PUBLIC Threads::Threads
    PUBLIC agentos_compile_defs
)

if(AGENTOS_HAS_YAML AND YAML_FOUND)
    target_link_libraries(agentos_common PRIVATE ${YAML_LIBRARIES})
    target_include_directories(agentos_common PRIVATE ${YAML_INCLUDE_DIRS})
endif()
if(AGENTOS_HAS_CJSON)
    if(cJSON_FOUND)
        target_link_libraries(agentos_common PRIVATE cjson)
        if(cJSON_INCLUDE_DIRS)
            target_include_directories(agentos_common PRIVATE ${cJSON_INCLUDE_DIRS})
        endif()
    elseif(CJSON_FOUND)
        target_link_libraries(agentos_common PRIVATE ${CJSON_LIBRARIES})
        if(CJSON_INCLUDE_DIRS)
            target_include_directories(agentos_common PRIVATE ${CJSON_INCLUDE_DIRS})
        endif()
    endif()
endif()

install(TARGETS agentos_common DESTINATION lib)
install(DIRECTORY platform/include/ DESTINATION include/agentos/platform)
install(DIRECTORY utils/token/include/ DESTINATION include/agentos/utils/token)
install(DIRECTORY utils/cost/include/ DESTINATION include/agentos/utils/cost)
install(DIRECTORY utils/observability/include/ DESTINATION include/agentos/utils/observability)
install(DIRECTORY utils/logging/include/ DESTINATION include/agentos/utils/logging)
install(DIRECTORY utils/error/include/ DESTINATION include/agentos/utils/error)
install(DIRECTORY utils/config_unified/include/ DESTINATION include/agentos/utils/config_unified)
install(DIRECTORY utils/io/include/ DESTINATION include/agentos/utils/io)
install(DIRECTORY utils/types/include/ DESTINATION include/agentos/utils/types)
install(FILES utils/security/input_validator.h DESTINATION include/agentos/utils/security)
install(FILES utils/resource/resource_guard.h DESTINATION include/agentos/utils/resource)
install(DIRECTORY utils/memory/include/ DESTINATION include/agentos/utils/memory)
install(DIRECTORY utils/string/include/ DESTINATION include/agentos/utils/string)
install(DIRECTORY utils/sync/include/ DESTINATION include/agentos/utils/sync)
install(DIRECTORY utils/cache/include/ DESTINATION include/agentos/utils/cache)
install(DIRECTORY utils/compat/include/ DESTINATION include/agentos/utils/compat)
install(DIRECTORY utils/network/include/ DESTINATION include/agentos/utils/network)
install(DIRECTORY utils/ipc/include/ DESTINATION include/agentos/utils/ipc)
install(DIRECTORY utils/platform/include/ DESTINATION include/agentos/utils/platform)
install(DIRECTORY utils/cognition/include/ DESTINATION include/agentos/utils/cognition)
install(DIRECTORY utils/execution/include/ DESTINATION include/agentos/utils/execution)
install(DIRECTORY utils/strategy/include/ DESTINATION include/agentos/utils/strategy)

option(BUILD_TESTS "Build unit tests" OFF)
if(BUILD_TESTS)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests")
        add_subdirectory(tests)
    endif()
endif()
