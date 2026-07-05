# AgentRT 内置内存子系统（免费开源底座）
# 提供 L1+L2 基础功能，无 MemoryRovol / FAISS / SQLite 依赖

agentrt_print_info("[MEMORY] Configuring builtin memory module (backend: ${AGENTRT_MEMORY_BACKEND})")

# 验证源文件
foreach(_src src/builtin_storage.c src/builtin_index.c src/builtin_retrieval.c src/builtin_provider.c src/memory_provider.h)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${_src}")
        agentrt_print_ok("[MEMORY]   [OK] ${_src}")
    else()
        agentrt_print_warn("[MEMORY]   [MISSING] ${_src}")
    endif()
endforeach()

add_library(agentrt_memory STATIC
    src/builtin_storage.c
    src/builtin_index.c
    src/builtin_retrieval.c
    src/builtin_provider.c
)

target_include_directories(agentrt_memory PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../corekern/include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../commons/utils/error/include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../commons/utils/compat/include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../commons/platform/include>
)

if(MSVC)
    target_compile_options(agentrt_memory PRIVATE /W4)
    target_compile_definitions(agentrt_memory PRIVATE _CRT_SECURE_NO_WARNINGS _CRT_NONSTDC_NO_DEPRECATE)
else()
    target_compile_options(agentrt_memory PRIVATE -Wall -Wextra -Wpedantic)
endif()

install(TARGETS agentrt_memory DESTINATION lib)
install(FILES src/memory_provider.h DESTINATION include/agentrt/memory)

target_link_libraries(agentrt_memory PUBLIC agentrt_common)
