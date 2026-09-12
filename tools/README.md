# tools — 内部工具与质量门禁

`agentrt/tools/`

---

## 概述

AgentRT 的内部工具集，包含交互式 CLI 产品入口、依赖图构建期门禁与
契约代码生成器。本目录下所有工具均为仓库内开发维护的一等公民，
不引入第三方工具链依赖。

## 目录结构

```
tools/
├── airy_cli/          # 交互式 CLI 产品入口（Chat/Task 双模式 + TUI）
├── airy_depgraph/     # 依赖图校验工具（commons 子域 DAG 门禁 + include 漂移检测）
└── codegen/           # 契约代码生成器（syscall.xml SSoT → 生成头文件）
```

## 工具一览

| 工具 | 语言 | 说明 |
|------|------|------|
| `airy_cli` | C11 | AgentRT 交互式产品入口：对话/任务双模式、TUI 终端界面、编排管线（`/orch`）、统一网关客户端；任务全量经 gateway → daemon 派发 |
| `airy_depgraph` | C11 | 构建期质量门禁：解析 commons 子域依赖声明 → DAG 拓扑排序 + 环检测 + include 漂移检测 + 链接白名单校验；fail-closed（退出码 0=通过 / 1=存在环 / 2=manifest 解析错误或漂移违规） |
| `codegen/syscall_gen.py` | Python 3 | 用户态 syscall 层契约代码生成器（P2-3 SSoT）：解析 `atoms/syscall/include/syscall.xml` 唯一真值源，生成 `syscall_ids.h` 与 `syscall_table_gen.h`；支持 `--gen`（重新生成写回）与 `--check`（与仓库产物 diff，不一致返回非零退出码，用于构建/CI 防漂移）；仅依赖 Python 标准库 |

## 构建方式

各工具随主工程 CMake 一并构建（`AIRY_BUILD_ALL=ON` 时纳入），亦可在
out-of-source 构建目录中单独定位目标：

```bash
# out-of-source 构建（禁止源码区编译）
cmake -S agentrt -B /tmp/airy-build -DBUILD_TESTS=ON -DAIRY_BUILD_ALL=ON
cmake --build /tmp/airy-build --target airy_cli airy_depgraph --parallel $(nproc)
```

## 约束

- `airy_cli` 版本号单一来源为仓库根 `VERSION` 文件（构建期读取注入
  `AIRY_CLI_VERSION` 宏，文件缺失时回退 `cli_internal.h` 缺省值）
- `codegen/syscall_gen.py` 生成的产物文件名保持稳定，修改 XML 契约后
  须执行 `--gen` 写回并提交产物，CI 以 `--check` 校验防漂移
- `airy_depgraph` 为 fail-closed 门禁：任何环、漂移或白名单违规都会
  使构建失败，白名单调整须与 manifest 一并评审

---

© 2025-2026 SPHARX Ltd. All Rights Reserved.
