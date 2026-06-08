
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(SOURCES
    platform/platform.c
    utils/platform/src/platform_adapter.c
    utils/token/src/counter.c
    utils/token/src/budget.c
    utils/token/src/token_standard.c
    utils/cost/src/estimator.c
    utils/cost/src/controller.c
    utils/observability/src/logger.c
    utils/observability/src/metrics.c
    utils/observability/src/trace.c
    utils/error/src/handler.c
    utils/io/src/file_utils.c
    utils/security/src/input_validator.c
    utils/resource/src/resource_guard.c
    utils/resource/src/resource_quota.c
    utils/logging/src/logging.c
    utils/logging/src/logging_compat.c
    utils/logging/src/atomic_logging.c
    utils/logging/src/service_logging.c
    utils/logging/src/logging_common.c
    utils/memory/src/memory.c
    utils/memory/src/memory_pool.c
    utils/memory/src/memory_debug.c
    utils/memory/src/memory_common.c
    utils/string/src/string_common.c
    utils/string/src/string.c
    utils/sync/src/sync.c
    utils/sync/src/sync_platform.c
    utils/sync/src/sync_internal.c
    utils/sync/src/sync_spinlock.c
    utils/sync/src/sync_mutex.c
    utils/sync/src/sync_rwlock.c
    utils/sync/src/sync_semaphore.c
    utils/sync/src/sync_condition.c
    utils/sync/src/sync_event.c
    utils/sync/src/sync_barrier.c
    utils/sync/src/sync_recursive_mutex.c
    utils/sync/src/sync_common.c
    utils/cache/src/cache_common.c
    utils/compat/src/compat.c
    utils/compat/src/compat2.c
    utils/cognition/src/cognition_common.c
    utils/execution/src/execution_common.c
    utils/strategy/src/strategy_common.c
    utils/network/src/network_common.c
    utils/uuid/src/uuid_generator.c
    utils/config_unified/src/config_service.c
    utils/config_unified/src/core_config.c
    utils/config_unified/src/config_source.c
    utils/config_unified/src/config_compat.c
)
if(NOT WIN32)
    list(APPEND SOURCES utils/ipc/src/ipc_common.c)
endif()

add_library(agentos_common STATIC ${SOURCES})

# 验证源文件存在性
message(STATUS "[COMMONS] Verifying source files...")
set(_missing_count 0)
foreach(_src ${SOURCES})
    if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${_src}")
        message(WARNING "[COMMONS]   [MISSING] ${_src}")
        math(EXPR _missing_count "${_missing_count} + 1")
    endif()
endforeach()
if(_missing_count EQUAL 0)
    message(STATUS "[COMMONS]   All source files verified OK")
else()
    message(WARNING "[COMMONS]   ${_missing_count} source file(s) MISSING!")
endif()

# 验证 include 目录存在性
message(STATUS "[COMMONS] Verifying include directories...")
set(_inc_dirs
    platform/include
    utils/types/include
    utils/logging/include
    utils/error/include
    utils/sync/include
    utils/execution/include
    utils/include
    utils/memory
    utils/string
    utils/compat/include
    utils/token/include
    utils/cost/include
    utils/observability/include
    utils/io/include
    utils/security/include
    utils/resource/include
    utils/memory/include
    utils/string/include
    utils/cache/include
    utils/platform/include
    utils/config_unified/include
    utils/cognition/include
    utils/strategy/include
    utils/network/include
    utils/ipc/include
    utils/uuid/include
)
set(_inc_missing 0)
foreach(_inc ${_inc_dirs})
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${_inc}")
        message(STATUS "[COMMONS]   [OK] ${_inc}")
    else()
        message(WARNING "[COMMONS]   [MISSING] ${_inc}")
        math(EXPR _inc_missing "${_inc_missing} + 1")
    endif()
endforeach()
if(_inc_missing GREATER 0)
    message(WARNING "[COMMONS]   ${_inc_missing} include directory(ies) MISSING!")
endif()

set_source_files_properties(
    utils/memory/src/memory.c
    utils/memory/src/memory_pool.c
    utils/memory/src/memory_debug.c
    utils/memory/src/memory_common.c
    utils/logging/src/logging.c
    utils/logging/src/logging_compat.c
    utils/logging/src/atomic_logging.c
    utils/logging/src/service_logging.c
    utils/logging/src/logging_common.c
    PROPERTIES COMPILE_DEFINITIONS "AGENTOS_COMPLIANCE_IMPL"
)

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
    ${CMAKE_CURRENT_SOURCE_DIR}/utils/uuid/include
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
install(FILES utils/security/src/input_validator.h DESTINATION include/agentos/utils/security)
install(FILES utils/resource/src/resource_guard.h DESTINATION include/agentos/utils/resource)
install(FILES utils/resource/src/resource_quota.h DESTINATION include/agentos/utils/resource)
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

option(BUILD_TESTS "Build unit tests" ON)
if(BUILD_TESTS)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests")
        add_subdirectory(tests)
    endif()
endif()
