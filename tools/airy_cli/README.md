# airy_cli — AgentRT 交互式产品入口

`agentrt/tools/airy_cli/`

**版本**: 单一来源为仓库根 `VERSION` 文件（构建期注入 `AIRY_CLI_VERSION` 宏）

---

## 概述

airy_cli 是 AgentRT 的交互式 CLI 产品入口（C11 实现），提供对话/任务
双模式终端体验与 TUI 终端界面。自 0.1.9 M1-1c 起，CLI 不再进程内持有
本地 work_hall/cog，任务全量经 gateway → daemon 派发，自身作为统一
网关客户端（`cli_gw`）接入。

## 目录结构

```
airy_cli/
├── include/                        # 公共头文件
│   ├── cli_internal.h              # 内部契约（含版本缺省值兜底）
│   ├── cli_tui.h / cli_render.h    # TUI 引擎与渲染接口
│   ├── airy_cli_pipeline.h         # 编排管线
│   ├── airy_cli_exec.h             # 执行接口
│   ├── cli_gw.h                    # 统一网关客户端
│   ├── cli_review.h                # 认知阶段并行子 agent 审查
│   ├── daemon_cmds.h               # daemon 命令面
│   ├── airy_cli_cmd_internal.h     # 命令内部契约
│   └── cli_term.h                  # 终端能力（尺寸回退链等）
├── src/                            # 实现（按功能域组织）
│   ├── core/                       # 入口与编排管线（main 域拆分/cmdline/taskflow/orch/dag/term）
│   ├── cmd/                        # 命令面（system/cognition/capability/gw/classify/review）
│   ├── chat/                       # 对话会话（usage/memory/gccp/history/tools/classify/stream/finalize）
│   ├── tui/                        # TUI 引擎（keys/input/ime/history/render/panel/readline/complete）
│   └── render/                     # 渲染与展示（think/display/live_board/banner/markdown/output）
├── tests/                          # 单元测试（意图分辨启发式 test_cli_classify）
└── CMakeLists.txt
```

## 核心能力

| 能力 | 模块 | 说明 |
|------|------|------|
| 对话模式 | `chat` | 面向对话的交互会话，经 llm/tool service 客户端派发；含意图分辨启发式（纯函数，独立单测）、流式输出、历史与记忆 |
| 任务模式 | `core` | 面向复杂任务的执行模式，大任务自动进入规划-执行闭环 |
| 编排管线 | `core/cli_orch` | `/orch` 命令接入 orchestrator 七阶段流程编排 |
| TUI | `tui` | 全屏终端界面：多视图切换、任务看板、事件流面板、readline/补全/IME/历史 |
| 渲染 | `render` | Markdown 渲染、实时看板、banner、思考过程展示、输出格式化 |
| 统一网关 | `cmd/cli_gw` | 架构约束：所有客户端必须经 gateway 派发（架构约束 2026-08-25） |
| 命令面 | `cmd` | system / cognition / capability 三类命令命名空间 |
| 并行审查 | `cmd/cli_review` | 认知阶段并行子 agent 审查（事实/风险） |

## 构建与测试

```bash
# out-of-source 构建（禁止源码区编译）
cmake -S agentrt -B /tmp/airy-build -DBUILD_TESTS=ON -DAIRY_BUILD_ALL=ON
cmake --build /tmp/airy-build --target airy_cli --parallel $(nproc)

# 意图分辨启发式单测
ctest --test-dir /tmp/airy-build -R "cli_classify_heuristic" --output-on-failure
```

## 依赖

| 依赖 | 来源 | 用途 |
|------|------|------|
| CoreLoopThree | atoms/coreloopthree | loop 引擎底座 / 计划类型 / 通信适配（必需） |
| llm_service / tool_service | daemons/llm_d, daemons/tool_d | chat 链路客户端（llm_response_* / tool_approval_*） |
| CoreKern | atoms/corekern | `airy_rt.h` 运行时基础（`airy_init()` 五原子统一入口） |
| commons | agentrt/commons | 统一类型、IPC、平台兼容（compat）、IME 等公共能力 |

> TaskFlow 与 cupolas 已自本表移除（0.1.9 C2e 链接收敛后对 taskflow 无符号
> 引用；cupolas 仅经 RPC 方法名调用，无编译期依赖；2026-09-12 复核）。

---

© 2025-2026 SPHARX Ltd. All Rights Reserved.
