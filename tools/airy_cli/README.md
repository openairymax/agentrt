# airy_cli — AgentRT 交互式产品入口

`agentrt/tools/airy_cli/`

**版本**: 单一来源为仓库根 `VERSION` 文件（构建期注入 `AIRY_CLI_VERSION` 宏；文件缺失时回退 `cli_internal.h` 缺省值）

---

## 概述

airy_cli 是 AgentRT 的交互式 CLI 产品入口（C11 实现），提供对话/任务
双模式终端体验与 TUI 终端界面。CLI 不进程内持有本地 work_hall/cog，
任务全量经 gateway → daemon 派发，自身作为统一网关客户端（`cli_gw`）
接入。

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
│   ├── chat/                       # 对话会话（usage/memory/gccp/history/tools/classify/finalize）
│   ├── tui/                        # TUI 引擎（keys/input/ime/history/render/panel/readline/complete）
│   └── render/                     # 渲染与展示（think/display/live_board/banner/markdown/output）
├── tests/                          # 单元测试（意图分辨启发式 test_cli_classify）
└── CMakeLists.txt
```

## 核心能力

| 能力 | 模块 | 说明 |
|------|------|------|
| 对话模式 | `chat` | 面向对话的交互会话，经 llm/tool service 客户端派发；含意图分辨启发式（纯函数，独立单测）、历史与记忆 |
| 任务模式 | `core` | 面向复杂任务的执行模式，大任务自动进入规划-执行闭环 |
| 编排管线 | `core/cli_orch` | `/orch` 命令接入 orchestrator 七阶段流程编排 |
| TUI | `tui` | 全屏终端界面：多视图切换、任务看板、事件流面板、readline/补全/IME/历史 |
| 渲染 | `render` | Markdown 渲染、实时看板、banner、思考过程展示、输出格式化 |
| 统一网关 | `cmd/cli_gw` | 所有客户端流量统一经 gateway 派发，CLI 自身亦为网关客户端接入 |
| 命令面 | `cmd` | system / cognition / capability 三类命令命名空间 |
| 并行审查 | `cmd/cli_review` | 认知阶段并行子 agent 审查（事实/风险） |

## 构建与测试

```bash
# out-of-source 构建（禁止源码区编译），在仓库根目录执行
cmake -S . -B /tmp/airy-build
cmake --build /tmp/airy-build --target airy_cli --parallel $(nproc)

# 意图分辨启发式单测（需 BUILD_TESTS=ON，默认即为 ON）
ctest --test-dir /tmp/airy-build -R "cli_classify_heuristic" --output-on-failure
```

> Windows 平台默认不构建 CLI（`BUILD_CLI` 于 WIN32 默认 `OFF`）；
> 如需尝试可显式配置 `-DBUILD_CLI=ON`（Windows 下额外链接 `ws2_32`）。

## 依赖

| 依赖 | 来源 | 用途 |
|------|------|------|
| CoreLoopThree | atoms/coreloopthree | loop 引擎底座 / 计划类型 / 通信适配（必需） |
| llm_service / tool_service | daemons/llm_d, daemons/tool_d | chat 链路客户端（llm_response_* / tool_approval_*） |
| commons | agentrt/commons | 统一类型、IPC、平台兼容（compat）、IME 等公共能力 |

> 0.1.16 B2（CLI 进程内 corekern 收回）：CLI 为 gateway 纯客户端，不再链接
> atoms/corekern（`airy_atoms`）微核心聚合，亦不引用 corekern 总伞头
> `airy_rt.h`——内核机制只被 daemon 服务面访问（架构铁律第 2 句）。错误码
> 契约经 commons `airy_types.h` 提供。

> TaskFlow 与 cupolas 不构成编译期依赖：CLI 对 taskflow 无符号引用；
> cupolas 仅经 RPC 方法名调用。

---

© 2025-2026 SPHARX Ltd. All Rights Reserved.
