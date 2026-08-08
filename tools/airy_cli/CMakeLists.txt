# airy_cli — AgentRT 交互式产品入口
# 依赖 coreloopthree（GCCP / 工作大厅 / DAG 适配层）

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

if(NOT TARGET airy_coreloopthree)
    message(FATAL_ERROR "airy_cli requires airy_coreloopthree target")
endif()

add_executable(airy_cli
    main.c
)

# 链接模式：coreloopthree 与 llm/tool service 存在交叉引用（静态库 + 全局
# -flto），用 --start-group/--end-group 让链接器循环扫描解析，避免手动排序。
if(TARGET airy_llm_service AND TARGET airy_tool_service)
    target_link_libraries(airy_cli PRIVATE
        "-Wl,--start-group"
        airy_coreloopthree
        airy_llm_service
        airy_tool_service
        airy_cognition
        airy_execution
        airy_core
        airy_memory
        airy_atoms
        airy_common
        airy_taskflow
        svc_common
        "-Wl,--end-group"
        Threads::Threads
        uuid
        m
    )
else()
    target_link_libraries(airy_cli PRIVATE
        airy_coreloopthree
        airy_atoms
        airy_cognition
        airy_execution
        airy_core
        airy_memory
        airy_taskflow
        svc_common
    )
endif()

# 继承 coreloopthree 的 cJSON 能力
if(AIRY_HAS_CJSON)
    target_compile_definitions(airy_cli PRIVATE AIRY_HAS_CJSON=1)
    if(TARGET cJSON OR TARGET cjson)
        target_link_libraries(airy_cli PRIVATE $<IF:$<TARGET_EXISTS:cJSON>,cJSON,cjson>)
    endif()
    if(AIRY_CJSON_INCLUDE_DIRS)
        target_include_directories(airy_cli PRIVATE ${AIRY_CJSON_INCLUDE_DIRS})
    endif()
    if(AIRY_CJSON_LIBRARIES)
        target_link_libraries(airy_cli PRIVATE ${AIRY_CJSON_LIBRARIES})
    endif()
endif()

# include 路径（与 coreloopthree 同源）
target_include_directories(airy_cli PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../atoms/coreloopthree/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../atoms/taskflow/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../commons/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../commons/utils/memory/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../commons/utils/string/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../commons/utils/logging/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../commons/utils/error/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../commons/utils/ipc/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../commons/utils/sd/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../commons/platform/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../daemons/llm_d/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../daemons/tool_d/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../daemons/common/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../cupolas/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../atoms/memory
    ${CMAKE_CURRENT_SOURCE_DIR}/../atoms/memory/src
)

if(WIN32)
    target_link_libraries(airy_cli PRIVATE ws2_32)
elseif(UNIX)
    target_link_libraries(airy_cli PRIVATE uuid)
endif()

install(TARGETS airy_cli RUNTIME DESTINATION bin)
