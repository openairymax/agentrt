# Copyright (c) 2026 SPHARX. All Rights Reserved. "From data intelligence emerges."
# MemoryRovol 测试构建文件

# 单元测试
add_executable(test_memoryrovol_core unit/test_memoryrovol_core.c)
target_include_directories(test_memoryrovol_core PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../core/include
    ${FAISS_INCLUDE_DIRS}
    ${CJSON_INCLUDE_DIRS}
    ${YAML_INCLUDE_DIRS}
    ${SQLITE3_INCLUDE_DIRS}
)
target_link_libraries(test_memoryrovol_core
    agentos_memoryrovol
    agentos_core
    ${FAISS_LIBRARIES}
    ${CJSON_LIBRARIES}
    ${YAML_LIBRARIES}
    ${SQLITE3_LIBRARIES}
    OpenSSL::SSL
    OpenSSL::Crypto
    Threads::Threads
)
add_test(NAME test_memoryrovol_core COMMAND test_memoryrovol_core)

# 集成测试
add_executable(test_memoryrovol_integration integration/test_memoryrovol_integration.c)
target_include_directories(test_memoryrovol_integration PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../core/include
    ${FAISS_INCLUDE_DIRS}
    ${CJSON_INCLUDE_DIRS}
    ${YAML_INCLUDE_DIRS}
    ${SQLITE3_INCLUDE_DIRS}
)
target_link_libraries(test_memoryrovol_integration
    agentos_memoryrovol
    agentos_core
    ${FAISS_LIBRARIES}
    ${CJSON_LIBRARIES}
    ${YAML_LIBRARIES}
    ${SQLITE3_LIBRARIES}
    OpenSSL::SSL
    OpenSSL::Crypto
    Threads::Threads
)
add_test(NAME test_memoryrovol_integration COMMAND test_memoryrovol_integration)
