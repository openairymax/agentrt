<!-- SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0 -->
<!-- Copyright (c) 2025-2026 SPHARX Ltd. All Rights Reserved. -->

# AgentRT `.github/` — 自动化与发布工程

> 本目录承载 AgentRT 仓库的自动化工程：跨平台构建与测试、发布流水线、发布前
> 真机核验与公开托管平台同步。
>
> 想**安装或使用** AgentRT，请看仓库根 [README](../README.md) /
> [简体中文](../README_zh.md)；
> 想**参与开发**，请看 [CONTRIBUTING](../CONTRIBUTING.md)；
> 发现**安全漏洞**，请按 [SECURITY](../SECURITY.md) 的私密上报流程处理，
> 不要直接开公开 issue。

## AgentRT 是什么

AgentRT（AirymaxAgentRT）是面向 AI Agent 团队的 OS 级运行时平台——定位类似于
JVM 之于语言、containerd 之于容器。它把多智能体认知循环、记忆演进、安全隔离
与协议互操作收敛为一套统一运行时，定义 AI Agent 团队如何运行。

这个仓库以 git 子模块聚合 7 个叶子源码仓库（atoms / commons / cupolas /
daemons / gateway / heapstore / protocols），对外交付微内核原语、认知循环、
内存分层、安全穹顶、IPC 协议、网关服务与常驻守护进程等 OS 级机制。本
`.github/` 目录就是支撑这套工程持续交付的自动化层。

## 自动化在做什么

AgentRT 的每一次提交与每一个版本发布都由这里的流水线驱动：

- **每次 push / pull request**：在 Linux、macOS、Windows 上自动构建并运行门禁
  测试，保证合入即绿；
- **每个版本 tag**：自动触发完整发布流水线——跨平台并行构建 → 自包含打包 →
  在无任何开发依赖的干净环境安装并启动核验 → 签名后对外发布；
- **macOS 正式发布出口**：额外在干净 macOS 宿主机上做一次真机核验，确认安装
  包不依赖构建机的私有环境也能完整启动 daemon 群。

门禁与发布共用同一套构建与打包逻辑，避免“两条线各写一份而漂移”。

## 目录结构

```
.github/
├── workflows/   # 流水线编排：触发条件 × 任务拓扑
├── actions/     # 可复用的复合动作（门禁与发布共用）
├── scripts/     # 流水线引用的长逻辑脚本
└── docker/      # 交叉编译工具链镜像（钉版，qemu 腿使用）
```

### 自动化一览

| 工作流 | 何时运行 | 做什么 |
|---|---|---|
| **Build & Test** | push / pull_request | Linux、macOS、Windows 构建与门禁测试 |
| **Codegen Checks** | push / pull_request | 校验 syscall 定义与生成代码不漂移 |
| **Toolchain Images** | 手动触发 | 构建并推送交叉编译工具链镜像 |
| **Release** | `v*` tag 推送 / 手动触发 | 跨平台构建 → 打包 → 干净环境核验 → 签名发布 |
| **G4b macOS Clean-Host** | `g4b-*` tag 推送 / 手动触发 | 干净 macOS 真机安装与完整启动核验 |
| **Mirror Sync** | push / 手动触发 | 源码与 tag 在公开托管平台间同步 |

发布是“fail-closed”的：任一平台构建失败、或干净环境无法安装/启动，都会阻断
发布，不会产出带病制品。

## 一个版本是怎么发布的

1. 维护者在源码仓库打出版本 tag（如 `v0.1.13`）；
2. Release 流水线并行构建全平台产物，打包为**自包含**安装包；
3. 在干净环境（Linux 容器 / macOS 宿主机）按普通用户流程离线安装、启动完整
   daemon 群并做端到端冒烟，覆盖升级与回滚路径；
4. 全部通过后产物签名并发布，安装器/更新器即可从官方渠道获取。

想从源码构建而不是用安装包？克隆后先执行
`git submodule update --init --recursive`（叶子仓库由 gitlink 钉定），构建入口
是仓库根的 `cmake`。完整步骤见 [CONTRIBUTING](../CONTRIBUTING.md)。

## 参与贡献

欢迎以 pull request 提交修复与改进。每个 PR 会自动运行上述门禁，全部通过才可
合入；请保持提交说明简短清晰（一个 PR 一个逻辑变更）。需要跨平台验证的任务
由流水线完成，无需贡献者自备 macOS / Windows 机器。

- 从哪改起：见各叶子仓库与 [CONTRIBUTING](../CONTRIBUTING.md)；
- 安全问题：严格走 [SECURITY](../SECURITY.md) 私密上报流程。

## 许可证

双许可证：**AGPL v3 或 Apache 2.0**（SPDX: `AGPL-3.0-or-later OR Apache-2.0`）。
详见仓库根 [LICENSE](../LICENSE) 与 [NOTICE](../NOTICE)。

Copyright (c) 2025-2026 SPHARX Ltd. All Rights Reserved.
