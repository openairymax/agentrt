
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

set(CMAKE_C_VISIBILITY_PRESET hidden)
set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_VISIBILITY_INLINES_HIDDEN 1)

# 使用根级依赖检测结果 (BAN-12: 单一依赖检测点)
if(AGENTRT_HAS_CJSON)
    set(AGENTOS_CJSON_FOUND ON CACHE INTERNAL "cJSON available for coreloopthree")
    if(cJSON_FOUND)
        set(AGENTOS_CJSON_INCLUDE_DIRS "${cJSON_INCLUDE_DIRS}" CACHE INTERNAL "")
        set(AGENTOS_CJSON_LIBRARIES "${cJSON_LIBRARIES}" CACHE INTERNAL "")
    elseif(CJSON_FOUND)
        set(AGENTOS_CJSON_INCLUDE_DIRS "${CJSON_INCLUDE_DIRS}" CACHE INTERNAL "")
        set(AGENTOS_CJSON_LIBRARIES "${CJSON_LIBRARIES}" CACHE INTERNAL "")
    endif()
    message(STATUS "coreloopthree: cJSON available")
else()
    message(STATUS "coreloopthree: cJSON not found, some features disabled")
endif()

add_subdirectory(src/cognition)
add_subdirectory(src/execution)

if(BUILD_TESTS)
    option(BUILD_CORELOOPTHREE_TESTS "Build CoreLoopThree test suite" ON)
    if(BUILD_CORELOOPTHREE_TESTS)
        enable_testing()
        add_subdirectory(tests)
    endif()
endif()

add_library(agentos_coreloopthree STATIC
    src/loop.c
    src/config_loader.c
    src/yaml_loader.c
    src/prompt_loader.c
    src/orch_adapter.c
    src/checkpoint_adapter.c
    src/manager_adapter.c
    src/memory/memory_service.c
    src/memory/memoryrovol_bridge.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../daemon/common/src/checkpoint.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../daemon/common/src/orchestrator.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../cupolas/src/yaml_minimal.c
)

target_link_libraries(agentos_coreloopthree PUBLIC agentos_atoms)

# C-L12: MemoryRovol 构建模式
# 三种模式：
#   AGENTOS_MEMORY_BUILTIN    — 仅内置免费提供商
#   AGENTOS_MEMORY_MEMORYROVOL — 仅 MemoryRovol 商业提供商
#   AGENTOS_MEMORY_HYBRID     — 混合模式（默认，根据配置切换）
if(NOT DEFINED AGENTOS_MEMORY_MODE)
    set(AGENTOS_MEMORY_MODE "HYBRID" CACHE STRING "Memory provider mode: BUILTIN, MEMORYROVOL, HYBRID")
endif()
message(STATUS "coreloopthree: AGENTOS_MEMORY_MODE=${AGENTOS_MEMORY_MODE}")

if(AGENTOS_MEMORY_MODE STREQUAL "BUILTIN")
    target_compile_definitions(agentos_coreloopthree PRIVATE
        AGENTOS_MEMORY_BUILTIN=1
    )
    message(STATUS "coreloopthree: Memory mode = BUILTIN (free provider only)")
elseif(AGENTOS_MEMORY_MODE STREQUAL "MEMORYROVOL")
    target_compile_definitions(agentos_coreloopthree PRIVATE
        AGENTOS_MEMORY_MEMORYROVOL=1
    )
    if(TARGET agentos_memoryrovol)
        target_link_libraries(agentos_coreloopthree PRIVATE agentos_memoryrovol)
        message(STATUS "coreloopthree: Memory mode = MEMORYROVOL (commercial provider)")
    else()
        message(WARNING "coreloopthree: MEMORYROVOL mode but agentos_memoryrovol target not found, falling back to BUILTIN")
        target_compile_definitions(agentos_coreloopthree PRIVATE AGENTOS_MEMORY_BUILTIN=1)
    endif()
else()
    # 默认 HYBRID 模式
    target_compile_definitions(agentos_coreloopthree PRIVATE
        AGENTOS_MEMORY_HYBRID=1
    )
    if(TARGET agentos_memoryrovol)
        target_link_libraries(agentos_coreloopthree PRIVATE agentos_memoryrovol)
        message(STATUS "coreloopthree: Memory mode = HYBRID (builtin + MemoryRovol)")
    else()
        message(STATUS "coreloopthree: Memory mode = HYBRID (MemoryRovol not found, builtin fallback available)")
    endif()
endif()

target_include_directories(agentos_coreloopthree PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../daemon/llm_d/include>
    $<INSTALL_INTERFACE:include/agentos/coreloopthree>
)

if(MSVC)
    target_compile_options(agentos_coreloopthree PRIVATE /W4 /WX)
    target_compile_definitions(agentos_coreloopthree PRIVATE _CRT_SECURE_NO_WARNINGS)
    target_compile_options(agentos_coreloopthree PRIVATE /GS)
    target_compile_options(agentos_coreloopthree PRIVATE /sdl)
    target_compile_options(agentos_coreloopthree PRIVATE /guard:cf)
else()
    target_compile_options(agentos_coreloopthree PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(agentos_coreloopthree PRIVATE -fstack-protector-strong)
    target_compile_options(agentos_coreloopthree PRIVATE -D_FORTIFY_SOURCE=2)
    target_compile_options(agentos_coreloopthree PRIVATE -fPIE)
    target_compile_options(agentos_coreloopthree PRIVATE -Wformat -Wformat-security)
    target_compile_options(agentos_coreloopthree PRIVATE -fno-omit-frame-pointer)
endif()

if(UNIX AND NOT APPLE)
    target_link_options(agentos_coreloopthree PRIVATE -Wl,-z,relro -Wl,-z,now -pie)
elseif(APPLE)
    target_link_options(agentos_coreloopthree PRIVATE -Wl,-dead_strip -pie)
endif()

target_include_directories(agentos_coreloopthree PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src/cognition
    ${CMAKE_CURRENT_SOURCE_DIR}/src/cognition/critique
    ${CMAKE_CURRENT_SOURCE_DIR}/src/cognition/foundation
    ${CMAKE_CURRENT_SOURCE_DIR}/../memory
    ${CMAKE_CURRENT_SOURCE_DIR}/../memory/src
    ${CMAKE_CURRENT_SOURCE_DIR}/../../daemon/common/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../daemon/llm_d/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../daemon/tool_d/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../commons/utils/quality
    ${CMAKE_CURRENT_SOURCE_DIR}/../../cupolas/src
)

target_link_libraries(agentos_coreloopthree PUBLIC
    agentos_cognition
    agentos_execution
    agentos_core
    agentos_memory
    svc_common
)

if(AGENTOS_CJSON_FOUND)
    target_compile_definitions(agentos_coreloopthree PRIVATE AGENTOS_HAS_CJSON=1)
    target_include_directories(agentos_coreloopthree PRIVATE ${AGENTOS_CJSON_INCLUDE_DIRS})
    target_link_libraries(agentos_coreloopthree PRIVATE ${AGENTOS_CJSON_LIBRARIES})
endif()

if(WIN32)
    target_link_libraries(agentos_coreloopthree PRIVATE ws2_32)
elseif(UNIX)
    target_link_libraries(agentos_coreloopthree PRIVATE uuid)
endif()

install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/
    DESTINATION include/agentos
    FILES_MATCHING PATTERN "*.h"
)

install(TARGETS agentos_coreloopthree
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
)
