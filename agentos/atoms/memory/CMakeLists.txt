# AgentRT 内置内存子系统（免费开源底座）
# 提供 L1+L2 基础功能，无 MemoryRovol / FAISS / SQLite 依赖

message(STATUS "[MEMORY] Configuring builtin memory module (backend: ${AGENTOS_MEMORY_BACKEND})")

# 验证源文件
foreach(_src src/builtin_storage.c src/builtin_index.c src/builtin_retrieval.c src/builtin_provider.c src/memory_provider.h)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${_src}")
        message(STATUS "[MEMORY]   [OK] ${_src}")
    else()
        message(WARNING "[MEMORY]   [MISSING] ${_src}")
    endif()
endforeach()

add_library(agentos_memory STATIC
    src/builtin_storage.c
    src/builtin_index.c
    src/builtin_retrieval.c
    src/builtin_provider.c
)

target_include_directories(agentos_memory PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../corekern/include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../commons/utils/error/include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../commons/utils/compat/include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../commons/platform/include>
)

if(MSVC)
    target_compile_options(agentos_memory PRIVATE /W4)
    target_compile_definitions(agentos_memory PRIVATE _CRT_SECURE_NO_WARNINGS _CRT_NONSTDC_NO_DEPRECATE)
else()
    target_compile_options(agentos_memory PRIVATE -Wall -Wextra -Wpedantic)
endif()

install(TARGETS agentos_memory DESTINATION lib)
install(FILES src/memory_provider.h DESTINATION include/agentos/memory)

target_link_libraries(agentos_memory PUBLIC agentos_common)
