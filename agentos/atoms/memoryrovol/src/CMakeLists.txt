# src/CMakeLists.txt
# MemoryRovol 模块源文件列表
# 支持 OSS (L1+L2) 和 PRO (L1+L2+L3+L4+forgetting) 双模式构建

# === L1 原始卷源文件（OSS + PRO 共享） ===
set(L1_SOURCES
    layer1_raw/compression.c
    layer1_raw/metadata.c
    layer1_raw/serialization.c
    layer1_raw/storage.c
    layer1_raw/layer1_raw_io.c
    layer1_raw/layer1_raw_queue.c
    layer1_raw/async_storage_utils.c
    layer1_raw/async_storage_engine.c
    layer1_raw/async_storage_io.c
    layer1_raw/async_storage_queue.c
    layer1_raw/async_storage_engine_refactored.c
    layer1_raw/async_storage.c
    layer1_raw/advanced_storage_utils.c
    layer1_raw/advanced_storage_cache.c
    layer1_raw/advanced_storage_async.c
    layer1_raw/advanced_storage.c
)

# === L2 特征层源文件（OSS + PRO 共享） ===
set(L2_SOURCES
    layer2_feature/bm25.c
    layer2_feature/embedder.c
    layer2_feature/hybrid.c
    layer2_feature/index.c
    layer2_feature/similarity.c
)

# === L3 结构层源文件（仅 PRO） ===
set(L3_SOURCES
    layer3_structure/binder.c
    layer3_structure/graph.c
    layer3_structure/relation.c
    layer3_structure/sequence.c
    layer3_structure/unbinder.c
)

# === L4 模式层源文件（仅 PRO） ===
set(L4_SOURCES
    layer4_pattern/clustering.c
    layer4_pattern/miner.c
    layer4_pattern/persistence.c
    layer4_pattern/rules.c
    layer4_pattern/validator.c
    layer4_pattern/layer4_pattern.c
)

# === 遗忘引擎源文件（仅 PRO） ===
set(FORGETTING_SOURCES
    forgetting/archive.c
    forgetting/decay.c
    forgetting/prune.c
    forgetting/revive.c
)

# === 检索层源文件（OSS + PRO 共享） ===
set(RETRIEVAL_SOURCES
    retrieval/cache.c
    retrieval/energy.c
)

# === 检索层高级源文件（仅 PRO） ===
set(RETRIEVAL_PRO_SOURCES
    retrieval/attractor.c
    retrieval/mount.c
    retrieval/rerank.c
)

# === 核心源文件（OSS + PRO 共享） ===
set(CORE_SOURCES
    config.c
    license.c
    garbage_collector.c
    lru_cache.c
    memory_optimizer.c
    memory_pool.c
    memoryrovol.c
    memoryrovol_provider.c
    vector_store.c
    monitoring.c
    monitoring_metrics.c
    frag_monitor.c
)

# === 商业源文件（仅 PRO） ===
set(PRO_SOURCES
    llm_client.c
)

# === 根据构建模式组合源文件 ===
if(MEMORYROVOL_OSS)
    set(MR_SOURCES
        ${CORE_SOURCES}
        ${L1_SOURCES}
        ${L2_SOURCES}
        ${RETRIEVAL_SOURCES}
    )
elseif(MEMORYROVOL_PRO_MODE)
    add_library(agentos_memoryrovol STATIC IMPORTED)
    set_target_properties(agentos_memoryrovol PROPERTIES
        IMPORTED_LOCATION "${MEMORYROVOL_PRO_LIB}"
    )
    return()
else()
    set(MR_SOURCES
        ${CORE_SOURCES}
        ${L1_SOURCES}
        ${L2_SOURCES}
        ${L3_SOURCES}
        ${L4_SOURCES}
        ${FORGETTING_SOURCES}
        ${RETRIEVAL_SOURCES}
        ${RETRIEVAL_PRO_SOURCES}
        ${PRO_SOURCES}
    )
endif()

add_library(agentos_memoryrovol STATIC ${MR_SOURCES})

# OSS 模式添加编译定义
if(MEMORYROVOL_OSS)
    target_compile_definitions(agentos_memoryrovol PUBLIC MEMORYROVOL_OSS)
endif()

target_link_libraries(agentos_memoryrovol PUBLIC agentos_atoms)

target_include_directories(agentos_memoryrovol PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<BUILD_INTERFACE:${AGENTOS_INCLUDE_DIR}>
    $<BUILD_INTERFACE:${AGENTOS_INCLUDE_DIR}/commons>
    $<BUILD_INTERFACE:${AGENTOS_INCLUDE_DIR}/commons/utils>
    $<BUILD_INTERFACE:${AGENTOS_INCLUDE_DIR}/commons/utils/memory/include>
    $<BUILD_INTERFACE:${AGENTOS_INCLUDE_DIR}/commons/utils/logging/include>
    $<BUILD_INTERFACE:${AGENTOS_INCLUDE_DIR}/commons/utils/string/include>
    $<BUILD_INTERFACE:${AGENTOS_INCLUDE_DIR}/commons/utils/sync/include>
    $<BUILD_INTERFACE:${AGENTOS_INCLUDE_DIR}/commons/utils/types/include>
    $<BUILD_INTERFACE:${AGENTOS_INCLUDE_DIR}/commons/utils/error/include>
    $<BUILD_INTERFACE:${AGENTOS_INCLUDE_DIR}/commons/utils/io/include>
    $<BUILD_INTERFACE:${AGENTOS_INCLUDE_DIR}/commons/utils/security/include>
    $<BUILD_INTERFACE:${AGENTOS_INCLUDE_DIR}/commons/utils/compat/include>
    $<BUILD_INTERFACE:${AGENTOS_INCLUDE_DIR}/commons/utils/execution/include>
    $<BUILD_INTERFACE:${AGENTOS_INCLUDE_DIR}/commons/utils/cognition/include>
    $<BUILD_INTERFACE:${AGENTOS_INCLUDE_DIR}/commons/utils/cache/include>
    $<BUILD_INTERFACE:${AGENTOS_INCLUDE_DIR}/commons/utils/resource/include>
    $<BUILD_INTERFACE:${AGENTOS_INCLUDE_DIR}/commons/utils/ipc/include>
    $<BUILD_INTERFACE:${AGENTOS_INCLUDE_DIR}/commons/utils/network/include>
    $<BUILD_INTERFACE:${AGENTOS_INCLUDE_DIR}/commons/utils/strategy/include>
    $<BUILD_INTERFACE:${AGENTOS_INCLUDE_DIR}/commons/platform/include>
    $<BUILD_INTERFACE:${AGENTOS_ATOMS_DIR}>
    $<BUILD_INTERFACE:${AGENTOS_ATOMS_DIR}/memory>
    $<BUILD_INTERFACE:${AGENTOS_ATOMS_DIR}/taskflow/include>
    $<BUILD_INTERFACE:${AGENTOS_ATOMS_DIR}/corekern/include>
    $<INSTALL_INTERFACE:include/agentos/memoryrovol>
)

target_link_libraries(agentos_memoryrovol PRIVATE
    $<TARGET_NAME_IF_EXISTS:agentos_core>
    ${FAISS_LIBRARIES}
    ${LIBCURL_LIBRARIES}
    ${CJSON_LIBRARIES}
    ${YAML_LIBRARIES}
    ${SQLITE3_LIBRARIES}
    $<$<NOT:$<BOOL:${WIN32}>>:Threads::Threads>
    $<$<BOOL:${OpenSSL_FOUND}>:OpenSSL::SSL>
    $<$<BOOL:${OpenSSL_FOUND}>:OpenSSL::Crypto>
)

if(OpenSSL_FOUND)
    target_compile_definitions(agentos_memoryrovol PRIVATE AGENTOS_USE_OPENSSL)
endif()

if(USE_SENTENCE_TRANSFORMERS AND SENTENCE_TRANSFORMERS_FOUND)
    target_link_libraries(agentos_memoryrovol ${SENTENCE_TRANSFORMERS_LIBRARIES})
endif()
