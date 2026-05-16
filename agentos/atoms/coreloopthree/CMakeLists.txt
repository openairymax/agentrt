
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

set(CMAKE_C_VISIBILITY_PRESET hidden)
set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_VISIBILITY_INLINES_HIDDEN 1)

# 统一查找 cJSON 依赖
find_package(cJSON QUIET)
if(cJSON_FOUND)
    set(AGENTOS_CJSON_FOUND ON CACHE INTERNAL "cJSON available for coreloopthree")
    set(AGENTOS_CJSON_INCLUDE_DIRS "${cJSON_INCLUDE_DIRS}" CACHE INTERNAL "")
    set(AGENTOS_CJSON_LIBRARIES "${cJSON_LIBRARIES}" CACHE INTERNAL "")
    message(STATUS "coreloopthree: cJSON found via find_package")
else()
    if(NOT PkgConfig_FOUND)
        find_package(PkgConfig QUIET)
    endif()
    if(PkgConfig_FOUND)
        pkg_check_modules(CJSON QUIET libcjson)
        if(CJSON_FOUND)
            set(AGENTOS_CJSON_FOUND ON CACHE INTERNAL "cJSON available for coreloopthree")
            set(AGENTOS_CJSON_INCLUDE_DIRS "${CJSON_INCLUDE_DIRS}" CACHE INTERNAL "")
            set(AGENTOS_CJSON_LIBRARIES "${CJSON_LIBRARIES}" CACHE INTERNAL "")
            message(STATUS "coreloopthree: cJSON found via pkg-config")
        endif()
    endif()
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
    src/memory/memory_service.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../daemon/common/src/checkpoint.c
)

target_link_libraries(agentos_coreloopthree PUBLIC agentos_atoms)

target_include_directories(agentos_coreloopthree PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
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
    ${CMAKE_CURRENT_SOURCE_DIR}/../memory
    ${CMAKE_CURRENT_SOURCE_DIR}/../../daemon/common/include
)

target_link_libraries(agentos_coreloopthree PUBLIC
    agentos_cognition
    agentos_execution
    agentos_core
    agentos_memory
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
