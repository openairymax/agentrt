# AgentOS 内置内存子系统（免费开源底座）
# 提供 L1+L2 基础功能，无 MemoryRovol / FAISS / SQLite 依赖

add_library(agentos_memory STATIC
    builtin_storage.c
    builtin_index.c
    builtin_retrieval.c
    builtin_provider.c
)

target_include_directories(agentos_memory PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../include>
    # R-09-124: migrated from root global include_directories
    ${CMAKE_CURRENT_SOURCE_DIR}/../../commons
    ${CMAKE_CURRENT_SOURCE_DIR}/../../atoms/corekern/include
)

if(MSVC)
    target_compile_options(agentos_memory PRIVATE /W4)
    target_compile_definitions(agentos_memory PRIVATE _CRT_SECURE_NO_WARNINGS)
else()
    target_compile_options(agentos_memory PRIVATE -Wall -Wextra -Wpedantic)
endif()

install(TARGETS agentos_memory DESTINATION lib)
install(FILES memory_provider.h DESTINATION include/agentos/memory)
