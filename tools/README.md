# tools — 仓库内置工具与质量门禁

`agentrt/tools/`

---

## 概述

AgentRT 仓库内置的工具集，包含交互式 CLI 产品入口与契约代码
生成器。本目录下所有工具均为仓库内开发维护的一等公民，不引入
第三方工具链依赖。

> 依赖图构建期门禁工具 `airy_depgraph` 已于 0.1.19（M5-3）迁出至
> 顶层 `tools` 仓（v0.1.4 由 devtools 更名），由 agentrt 构建期经
> 跨仓 `add_subdirectory` 纳入；见 agentrt 根 `CMakeLists.txt`。

## 目录结构

```
tools/
├── airy_cli/          # 交互式 CLI 产品入口（Chat/Task 双模式 + TUI）
└── codegen/           # 契约代码生成器（syscall.xml 唯一真值源 → 生成头文件）
```

## 工具一览

| 工具 | 语言 | 说明 |
|------|------|------|
| `airy_cli` | C11 | AgentRT 交互式产品入口：对话/任务双模式、TUI 终端界面、编排管线（`/orch`）、统一网关客户端；任务全量经 gateway → daemon 派发 |
| `codegen/syscall_gen.py` | Python 3 | 用户态 syscall 层契约代码生成器：解析 `atoms/syscall/include/syscall.xml` 唯一真值源，生成 `syscall_ids.h` 与 `syscall_table_gen.h`；支持 `--gen`（重新生成写回）与 `--check`（与仓库产物 diff，不一致返回非零退出码，用于构建/CI 防漂移）；仅依赖 Python 标准库，为脚本工具，不经 CMake 构建，按需直接运行 |

## 构建方式

`airy_cli` 随主工程 CMake 一并构建；`codegen/` 为 Python 脚本，
无需构建。构建须以 out-of-source 方式进行（禁止源码区编译）：

```bash
# 在仓库根目录执行
cmake -S . -B /tmp/airy-build
cmake --build /tmp/airy-build --target airy_cli --parallel $(nproc)
```

相关构建开关及默认值：

| 开关 | 默认值 | 说明 |
|------|--------|------|
| `AIRY_BUILD_ALL` | `ON` | 全模块构建 |
| `BUILD_TESTS` | `ON` | 构建测试目标 |
| `BUILD_CLI` | 非 Windows `ON` / Windows `OFF` | 构建交互式 CLI 产品；Windows 上默认不构建，如需尝试须显式 `-DBUILD_CLI=ON` |

## 与 CMake 链接白名单门禁的协作

`airy_depgraph`（顶层 `tools` 仓）除独立运行外，还被
`cmake/airy_linkgate.cmake` 的链接白名单门禁调用：构建配置期从各
CMake 目标收集实际链接清单，安装校验阶段执行
`airy_depgraph --links <白名单> --actual <实际清单>`，任一越权链接
即以退出码 2 使构建失败（fail-closed）。白名单由仓库根
`link-whitelist.txt` 单点维护。详见 `cmake/README.md`。

## 约束

- `airy_cli` 版本号单一来源为仓库根 `VERSION` 文件（构建期读取注入
  `AIRY_CLI_VERSION` 宏，文件缺失时回退 `cli_internal.h` 缺省值）
- `codegen/syscall_gen.py` 生成的产物文件名保持稳定，修改 XML 契约后
  须执行 `--gen` 写回并提交产物，CI 以 `--check` 校验防漂移

---

© 2025-2026 SPHARX Ltd. All Rights Reserved.
