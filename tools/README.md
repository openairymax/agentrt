# tools — 仓库内置工具与质量门禁

`agentrt/tools/`

---

## 概述

AgentRT 仓库内置的工具集，包含交互式 CLI 产品入口、依赖图构建期
门禁与契约代码生成器。本目录下所有工具均为仓库内开发维护的一等
公民，不引入第三方工具链依赖。

## 目录结构

```
tools/
├── airy_cli/          # 交互式 CLI 产品入口（Chat/Task 双模式 + TUI）
├── airy_depgraph/     # 依赖图校验工具（commons 子域 DAG 门禁 + include 漂移检测 + 链接白名单校验）
└── codegen/           # 契约代码生成器（syscall.xml 唯一真值源 → 生成头文件）
```

## 工具一览

| 工具 | 语言 | 说明 |
|------|------|------|
| `airy_cli` | C11 | AgentRT 交互式产品入口：对话/任务双模式、TUI 终端界面、编排管线（`/orch`）、统一网关客户端；任务全量经 gateway → daemon 派发 |
| `airy_depgraph` | C11 | 构建期质量门禁：解析 commons 子域依赖声明 → DAG 拓扑排序 + 环检测 + include 漂移检测 + 链接白名单校验；fail-closed（退出码 0=通过 / 1=存在环 / 2=manifest 解析错误或漂移违规） |
| `codegen/syscall_gen.py` | Python 3 | 用户态 syscall 层契约代码生成器：解析 `atoms/syscall/include/syscall.xml` 唯一真值源，生成 `syscall_ids.h` 与 `syscall_table_gen.h`；支持 `--gen`（重新生成写回）与 `--check`（与仓库产物 diff，不一致返回非零退出码，用于构建/CI 防漂移）；仅依赖 Python 标准库，为脚本工具，不经 CMake 构建，按需直接运行 |

## 构建方式

`airy_cli` 与 `airy_depgraph` 随主工程 CMake 一并构建；`codegen/` 为
Python 脚本，无需构建。构建须以 out-of-source 方式进行（禁止源码区
编译）：

```bash
# 在仓库根目录执行
cmake -S . -B /tmp/airy-build
cmake --build /tmp/airy-build --target airy_cli airy_depgraph --parallel $(nproc)
```

相关构建开关及默认值：

| 开关 | 默认值 | 说明 |
|------|--------|------|
| `AIRY_BUILD_ALL` | `ON` | 全模块构建 |
| `BUILD_TESTS` | `ON` | 构建测试目标 |
| `BUILD_CLI` | 非 Windows `ON` / Windows `OFF` | 构建交互式 CLI 产品；Windows 上默认不构建，如需尝试须显式 `-DBUILD_CLI=ON` |

> `airy_depgraph` 的构建还要求 `BUILD_COMMONS` 与 `BUILD_ATOMS`
> 开启（二者默认 `ON`）。

## 与 CMake 链接白名单门禁的协作

`airy_depgraph` 除独立运行外，还被 `cmake/airy_linkgate.cmake` 的链接
白名单门禁调用：构建配置期从各 CMake 目标收集实际链接清单，安装校验
阶段执行 `airy_depgraph --links <白名单> --actual <实际清单>`，任一
越权链接即以退出码 2 使构建失败（fail-closed）。白名单由仓库根
`link-whitelist.txt` 单点维护。详见 `cmake/README.md`。

## 约束

- `airy_cli` 版本号单一来源为仓库根 `VERSION` 文件（构建期读取注入
  `AIRY_CLI_VERSION` 宏，文件缺失时回退 `cli_internal.h` 缺省值）
- `codegen/syscall_gen.py` 生成的产物文件名保持稳定，修改 XML 契约后
  须执行 `--gen` 写回并提交产物，CI 以 `--check` 校验防漂移
- `airy_depgraph` 为 fail-closed 门禁：任何环、漂移或白名单违规都会
  使构建失败，白名单调整须与 manifest 一并评审

---

© 2025-2026 SPHARX Ltd. All Rights Reserved.
