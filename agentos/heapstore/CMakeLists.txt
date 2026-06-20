message(STATUS "[HEAPSTORE] Configuring heapstore module (SQLite3: ${AGENTOS_HAS_SQLITE3})")

set(heapstore_SOURCES
    src/heapstore_core.c
    src/heapstore_registry.c
    src/heapstore_trace.c
    src/heapstore_log.c
    src/heapstore_ipc.c
    src/heapstore_memory.c
    src/heapstore_batch.c
    src/heapstore_token.c
    src/heapstore_integration.c
    src/heapstore_migration.c
    src/utils.c
    kernel/services/trace_store_service.c
    kernel/services/log_store_service.c
)

set(heapstore_HEADERS
    include/heapstore.h
    include/heapstore_registry.h
    include/heapstore_trace.h
    include/heapstore_log.h
    include/heapstore_ipc.h
    include/heapstore_memory.h
    include/heapstore_batch.h
    include/heapstore_migration.h
    include/heapstore_token.h
    include/heapstore_integration.h
    include/utils.h
    src/private.h
)

add_library(agentos_heapstore STATIC ${heapstore_SOURCES} ${heapstore_HEADERS})

target_include_directories(agentos_heapstore PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../commons/platform/include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../commons/utils/include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../commons/utils/sync/include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../commons/utils/compat/include>
    $<INSTALL_INTERFACE:include>
)

target_compile_definitions(agentos_heapstore PRIVATE
    agentos_heapstore_VERSION_MAJOR=${PROJECT_VERSION_MAJOR}
    agentos_heapstore_VERSION_MINOR=${PROJECT_VERSION_MINOR}
    agentos_heapstore_VERSION_PATCH=${PROJECT_VERSION_PATCH}
)

target_link_libraries(agentos_heapstore
    PRIVATE Threads::Threads
    PUBLIC agentos_compile_defs
)

if(TARGET agentos_common)
    target_link_libraries(agentos_heapstore PUBLIC agentos_common)
endif()

if(AGENTOS_HAS_SQLITE3)
    if(SQLite3_FOUND)
        target_include_directories(agentos_heapstore PRIVATE ${SQLite3_INCLUDE_DIRS})
        target_link_libraries(agentos_heapstore PRIVATE ${SQLite3_LIBRARIES})
    elseif(SQLITE3_FOUND)
        target_include_directories(agentos_heapstore PRIVATE ${SQLITE3_INCLUDE_DIRS})
        target_link_libraries(agentos_heapstore PRIVATE ${SQLITE3_LIBRARIES})
    endif()
    message(STATUS "heapstore: SQLite3 registry backend enabled")
else()
    message(STATUS "heapstore: using in-memory registry backend (no SQLite3)")
endif()

install(TARGETS agentos_heapstore
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
)

install(FILES ${heapstore_HEADERS}
    DESTINATION include/agentos/heapstore
)

if(BUILD_TESTS)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests")
        add_subdirectory(tests)
    endif()
endif()

message(STATUS "AgentRT heapstore v${PROJECT_VERSION}")
