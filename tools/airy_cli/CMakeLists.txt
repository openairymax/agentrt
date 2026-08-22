# airy_cli — AgentRT 交互式产品入口
# 依赖 coreloopthree（GCCP / 工作大厅 / DAG 适配层）

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

if(NOT TARGET airy_coreloopthree)
    message(FATAL_ERROR "airy_cli requires airy_coreloopthree target")
endif()

# 源/头分离布局（2026-08-19）：include/ 公共头，src/ 实现
add_executable(airy_cli
    src/main.c
    src/daemon_cmds.c
    src/cli_chat.c
    src/cli_classify.c
    src/cli_think.c
    src/cli_dag.c
    src/cli_display.c
    src/cli_cmds.c
    src/cli_render.c
    src/cli_term.c
    src/cli_tui.c
    # 阶段 4: TUI 面板数据源（任务看板 / 事件流）
    src/cli_panel.c
    # 阶段 4（item 4）: 认知阶段并行子 agent 审查（事实/风险）
    src/cli_review.c
    # 改进6（P3）: 执行复核管线 t2/t1-f LLM 语义委托（门禁 -> t2 -> t1-f）
    src/cli_exec_review.c
)

# 链接模式：coreloopthree 与 llm/tool service 存在交叉引用（静态库 + 全局
# -flto），用 --start-group/--end-group 让链接器循环扫描解析，避免手动排序。
# --start-group/--end-group 是 GNU ld 专有选项：macOS ld64 与 MSVC 不识别，
# 这两平台直接链接即可（LTO 符号可见性由顶层 -ffat-lto-objects 保证）。
# m（libm）仅在非 MSVC 平台存在；uuid 按下方 WIN32/UNIX 平台分支处理。
if(TARGET airy_llm_service AND TARGET airy_tool_service)
    if(UNIX AND NOT APPLE)
        target_link_libraries(airy_cli PRIVATE
            "-Wl,--start-group"
            airy_coreloopthree
            airy_llm_service
            airy_tool_service
            airy_cognition
            airy_core
            airy_memory
            airy_atoms
            airy_common
            airy_taskflow
            svc_common
            "-Wl,--end-group"
        )
    else()
        target_link_libraries(airy_cli PRIVATE
            airy_coreloopthree
            airy_llm_service
            airy_tool_service
            airy_cognition
            airy_core
            airy_memory
            airy_atoms
            airy_common
            airy_taskflow
            svc_common
        )
    endif()
    target_link_libraries(airy_cli PRIVATE Threads::Threads)
    if(NOT MSVC)
        target_link_libraries(airy_cli PRIVATE m)
    endif()
else()
    target_link_libraries(airy_cli PRIVATE
        airy_coreloopthree
        airy_atoms
        airy_cognition
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

# include 路径（本工具 include/ + coreloopthree 同源；相对路径自本目录
# tools/airy_cli 起算，源码树为 agentrt/tools/airy_cli → ../../）
set(CLI_INC_DIRS
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../atoms/coreloopthree/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../atoms/taskflow/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../commons/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../commons/utils/memory/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../commons/utils/string/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../commons/utils/logging/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../commons/utils/error/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../commons/utils/sync/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../commons/utils/ipc/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../commons/utils/sd/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../commons/platform/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../commons/utils/ime/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../daemons/llm_d/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../daemons/tool_d/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../daemons/common/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../cupolas/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../atoms/memory
    ${CMAKE_CURRENT_SOURCE_DIR}/../../atoms/memory/src
)
target_include_directories(airy_cli PRIVATE ${CLI_INC_DIRS})

if(WIN32)
    target_link_libraries(airy_cli PRIVATE ws2_32)
endif()

install(TARGETS airy_cli RUNTIME DESTINATION bin)

# 2.5.x 意图分辨启发式单测（纯函数，独立于 LLM/daemon）
if(BUILD_TESTS)
    add_executable(test_cli_classify tests/test_cli_classify.c src/cli_classify.c)
    target_include_directories(test_cli_classify PRIVATE ${CLI_INC_DIRS})
    target_compile_definitions(test_cli_classify PRIVATE AIRY_HAS_CJSON=1)
    # 借 coreloopthree 的 interface include（types.h 等传递依赖）——
    # cli_classify.c 无外部符号引用，静态库按需抽取，无重链接负担。
    target_link_libraries(test_cli_classify PRIVATE airy_coreloopthree)
    add_test(NAME cli_classify_heuristic COMMAND test_cli_classify)
endif()
