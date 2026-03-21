# Copyright (c) 2026 SPHARX. All Rights Reserved. "From data intelligence emerges."

cmake_minimum_required(VERSION 3.20)
project(agentos-core-src)

add_library(agentos_core STATIC
    error.c
    main.c
    ipc/binder.c
    ipc/channel.c
    ipc/buffer.c
    mem/alloc.c
    mem/pool.c
    mem/guard.c
    task/scheduler.c
    task/thread.c
    task/sync.c
    time/clock.c
    time/timer.c
    time/event.c
)

target_include_directories(agentos_core PUBLIC include)

# 依赖 pthread 用于 mutex（如果使用 pthread 实现）
find_package(Threads REQUIRED)
target_link_libraries(agentos_core Threads::Threads)

# 设置编译选项
target_compile_options(agentos_core PRIVATE -Wall -Wextra -Werror -O2)

# 符号导出管理
set(CMAKE_C_VISIBILITY_PRESET hidden)
set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_VISIBILITY_INLINES_HIDDEN 1)

install(TARGETS agentos_core DESTINATION lib)
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/include/agentos.h DESTINATION include/agentos)
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/include/error.h DESTINATION include/agentos)
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/include/export.h DESTINATION include/agentos)
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/include/ipc.h DESTINATION include/agentos)
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/include/mem.h DESTINATION include/agentos)
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/include/task.h DESTINATION include/agentos)
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/include/time.h DESTINATION include/agentos)