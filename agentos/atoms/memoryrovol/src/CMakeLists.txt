# src/CMakeLists.txt
# MemoryRovol 模块源文件列表
# 注意：部分高级功能文件（mount.c/rerank.c等）因依赖问题暂时排除
add_library(agentos_memoryrovol STATIC
    garbage_collector.c
    forgetting/archive.c
    forgetting/decay.c
    forgetting/prune.c
    forgetting/revive.c
    layer1_raw/compression.c
    layer1_raw/metadata.c
    layer1_raw/serialization.c
    layer1_raw/storage.c
    layer2_feature/bm25.c
    layer2_feature/embedder.c
    layer2_feature/hybrid.c
    layer2_feature/index.c
    layer2_feature/similarity.c
    layer3_structure/binder.c
    layer3_structure/graph.c
    layer3_structure/relation.c
    layer3_structure/sequence.c
    layer3_structure/unbinder.c
    layer4_pattern/clustering.c
    layer4_pattern/miner.c
    layer4_pattern/persistence.c
    layer4_pattern/rules.c
    layer4_pattern/validator.c
    retrieval/attractor.c
    retrieval/cache.c
    retrieval/energy.c
    retrieval/mount.c
    retrieval/rerank.c
    lru_cache.c
    memory_optimizer.c
    memory_pool.c
    memoryrovol.c
    vector_store.c
    llm_client.c
)

# 使用统一的头文件路径管理
target_link_libraries(agentos_memoryrovol PUBLIC agentos_atoms)

target_include_directories(agentos_memoryrovol PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../include>
    $<INSTALL_INTERFACE:include/agentos/memoryrovol>
)

target_link_libraries(agentos_memoryrovol PRIVATE
    agentos_core
    ${FAISS_LIBRARIES}
    ${LIBCURL_LIBRARIES}
    ${CJSON_LIBRARIES}
    ${YAML_LIBRARIES}
    ${SQLITE3_LIBRARIES}
    $<$<NOT:$<BOOL:${WIN32}>>:Threads::Threads>
    $<$<BOOL:${OpenSSL_FOUND}>:OpenSSL::SSL>
    $<$<BOOL:${OpenSSL_FOUND}>:OpenSSL::Crypto>
)

# 如果使用 Sentence Transformers，添加链接
if(USE_SENTENCE_TRANSFORMERS AND SENTENCE_TRANSFORMERS_FOUND)
    target_link_libraries(agentos_memoryrovol ${SENTENCE_TRANSFORMERS_LIBRARIES})
endif()