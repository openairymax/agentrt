cmake_minimum_required(VERSION 3.15)
project(agentos_core C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(COREKERN_SOURCES
    src/main.c
    src/error.c
    src/ipc/binder.c
    src/ipc/channel.c
    src/ipc/buffer.c
    src/mem/alloc.c
    src/mem/pool.c
    src/mem/guard.c
    src/task/scheduler.c
    src/task/sync.c
    src/task/thread.c
    src/time/clock.c
    src/time/timer.c
    src/time/event.c
)

add_library(agentos_core STATIC ${COREKERN_SOURCES})

target_include_directories(agentos_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

if(MSVC)
    target_compile_options(agentos_core PRIVATE /W4 /WX)
    target_compile_definitions(agentos_core PRIVATE _CRT_SECURE_NO_WARNINGS)
else()
    target_compile_options(agentos_core PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

if(UNIX AND NOT APPLE)
    target_link_libraries(agentos_core PUBLIC pthread rt)
elseif(APPLE)
    target_link_libraries(agentos_core PUBLIC pthread)
endif()
