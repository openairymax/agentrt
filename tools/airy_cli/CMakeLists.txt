# airy_cli — AgentRT 交互式产品入口
# 依赖 coreloopthree（GCCP / 工作大厅 / DAG 适配层）

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

if(NOT TARGET airy_coreloopthree)
    message(FATAL_ERROR "airy_cli requires airy_coreloopthree target")
endif()

# 版本号 SSoT（2.6.2 Unify Design）：单一来源 agentrt/VERSION（如 0.1.5）。
# 构建期读取并注入 AIRY_CLI_VERSION 宏；文件缺失时回退 cli_internal.h 缺省值。
set(AIRY_CLI_VERSION_FILE "${CMAKE_CURRENT_SOURCE_DIR}/../../VERSION")
set(AIRY_CLI_VERSION "0.1.5a")
if(EXISTS "${AIRY_CLI_VERSION_FILE}")
    file(STRINGS "${AIRY_CLI_VERSION_FILE}" AIRY_CLI_VERSION_RAW LIMIT_COUNT 1)
    string(STRIP "${AIRY_CLI_VERSION_RAW}" AIRY_CLI_VERSION)
endif()
message(STATUS "airy_cli version (SSoT): ${AIRY_CLI_VERSION}")

# 源/头分离布局（2026-08-19）：include/ 公共头，src/ 实现
# 2026-08-27 目录原子化：src/ 按功能域组织
#   core/ 入口与编排管线 · cmd/ 命令面 · chat/ 对话会话 · tui/ 终端界面 · render/ 渲染与展示
add_executable(airy_cli
    src/core/main.c
    # main.c 域拆分（2026-08-27：1930 行 → 3 个职责模块）
    src/core/airy_cli_pipeline.c
    src/core/airy_cli_exec.c
    # main.c 二轮拆分（2026-08-27：893 行 → 3 个职责模块）
    src/core/airy_cli_cmdline.c
    src/core/airy_cli_taskflow.c
    # daemon_cmds.c 域拆分（2026-08-27：1480 行 → 3 个命名空间模块）
    src/cmd/airy_cli_cmd_system.c
    src/cmd/airy_cli_cmd_cognition.c
    src/cmd/airy_cli_cmd_capability.c
    src/chat/cli_chat.c
    # cli_chat.c 域拆分（2026-08-27：2040 行 → 6 个职责模块）
    src/chat/cli_chat_usage.c
    src/chat/cli_chat_memory.c
    src/chat/cli_chat_gccp.c
    src/chat/cli_chat_history.c
    src/chat/cli_chat_tools.c
    # cli_chat.c 二轮拆分（2026-08-27：802 行 → 4 个职责模块）
    src/chat/cli_chat_classify.c
    src/chat/cli_chat_stream.c
    src/chat/cli_chat_finalize.c
    src/cmd/cli_classify.c
    src/render/cli_think.c
    src/core/cli_dag.c
    # cli_display.c 域拆分（2026-08-27：963 行 → 3 个展示职责模块）
    src/render/cli_display.c
    src/render/cli_live_board.c
    src/render/cli_banner.c
    src/cmd/cli_cmds.c
    # cli_render.c 域拆分（2026-08-27：1695 行 → 3 个渲染职责模块）
    src/render/cli_render.c
    src/render/airy_cli_markdown.c
    src/render/airy_cli_output.c
    src/core/cli_term.c
    # TUI 引擎（2026-08-27 域拆分：cli_tui.c 3988 行 → 7 文件）
    src/tui/cli_tui.c
    src/tui/tui_keys.c
    src/tui/tui_input.c
    src/tui/tui_ime.c
    src/tui/tui_history.c
    src/tui/tui_render.c
    src/tui/tui_panel_dispatch.c
    # cli_tui.c 二轮拆分（2026-08-27：1102 行 → 3 个职责模块）
    src/tui/tui_readline.c
    src/tui/tui_readline_nav.c
    # tui_input.c 二轮拆分（2026-08-27：835 行 → 3 个职责模块）
    src/tui/tui_complete.c
    src/tui/tui_line_readline.c
    # 阶段 4: TUI 面板数据源（任务看板 / 事件流）
    src/tui/cli_panel.c
    # 阶段 4（item 4）: 认知阶段并行子 agent 审查（事实/风险）
    src/cmd/cli_review.c
    # 改进6（P3）: 执行复核管线 t2/t1-f LLM 语义委托（门禁 -> t2 -> t1-f）
    src/cmd/cli_exec_review.c
    # S-5 编排管线：/orch 命令（orchestrator 七阶段流程编排）
    src/core/cli_orch.c
    # 架构约束（2026-08-25）：统一网关客户端（所有客户端必须经 gateway 派发）
    src/cmd/cli_gw.c
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
    # 2026-08-27 目录原子化：src/ 子目录供跨域裸名 include 解析
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core
    ${CMAKE_CURRENT_SOURCE_DIR}/src/cmd
    ${CMAKE_CURRENT_SOURCE_DIR}/src/chat
    ${CMAKE_CURRENT_SOURCE_DIR}/src/tui
    ${CMAKE_CURRENT_SOURCE_DIR}/src/render
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
target_compile_definitions(airy_cli PRIVATE AIRY_CLI_VERSION="${AIRY_CLI_VERSION}")

if(WIN32)
    target_link_libraries(airy_cli PRIVATE ws2_32)
endif()

install(TARGETS airy_cli RUNTIME DESTINATION bin)

# 2.5.x 意图分辨启发式单测（纯函数，独立于 LLM/daemon）
if(BUILD_TESTS)
    add_executable(test_cli_classify tests/test_cli_classify.c src/cmd/cli_classify.c)
    target_include_directories(test_cli_classify PRIVATE ${CLI_INC_DIRS})
    target_compile_definitions(test_cli_classify PRIVATE AIRY_HAS_CJSON=1)
    # 借 coreloopthree 的 interface include（types.h 等传递依赖）——
    # cli_classify.c 无外部符号引用，静态库按需抽取，无重链接负担。
    target_link_libraries(test_cli_classify PRIVATE airy_coreloopthree)
    add_test(NAME cli_classify_heuristic COMMAND test_cli_classify)
endif()
