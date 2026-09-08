<!-- SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0 -->
<!-- Copyright (c) 2025-2026 SPHARX Ltd. All Rights Reserved. -->

# `.github/` — AgentRT 仓库自动化

> 本目录承载 [AgentRT](https://atomgit.com/openairymax/agentrt) 的 CI/CD 工作流、
> 可复用 Action、构建镜像、维护脚本与社区模板。

## 项目背景

AgentRT（AirymaxAgentRT）是面向 AI Agent 团队的 OS 级运行时平台——定位类似于 JVM
之于语言、containerd 之于容器。本仓库是平台的**管理仓库**，以 git 子模块聚合 7 个
叶子仓库（atoms / commons / cupolas / daemons / gateway / heapstore / protocols），
对外提供微内核原语、认知循环、内存分层、安全穹顶、IPC 协议、网关服务和常驻守护
进程等 OS 级机制。

## 仓库拓扑

- **atomgit 为主托管（SSoT）**：源码真相与官方制品发布地；GitHub 与 Gitee 为镜像
  与工作流执行面，由 `Mirror Sync` 单向自动同步。
- 所有流水线宿主在本仓库：构建、测试、codegen 校验、镜像同步与跨平台发布。
- 版本发布是跨平台原子事件：一个版本 tag 触发全平台构建，产物一次性聚合发布。

## 工作流一览

| 工作流（显示名） | 触发 | 职责 |
|---|---|---|
| `build-test.yml`（Build & Test） | `push` / `pull_request` 到 `main` | Linux / macOS / Windows 三平台构建与测试门禁（ctest + 覆盖率阈值） |
| `codegen-check.yml`（Codegen Checks） | `push` / `pull_request` 到 `main` | 校验 syscall 定义（SSoT XML）与生成代码不漂移 |
| `build-toolchain-images.yml`（Toolchain Images） | 工具链镜像变更 | 构建并推送交叉编译工具链镜像（GHCR，digest 钉版） |
| `release.yml`（Release） | 版本 tag（`v*`）推送或手动派发 | 跨平台发布链（见下节） |
| `sync-mirror.yml`（Mirror Sync） | `push` / tag | SSoT → GitHub / Gitee 镜像同步（agentrt + 7 叶子） |
| `g4b-macos-clean-host.yml`（G4b macOS Clean-Host） | 核验 tag 或手动派发 | 在干净 macOS 宿主上离线安装发布制品并完整启动 daemon 群，正式版本发布前的真机核验 |

临时性移植探针工作流（如 Windows x86-32 编译面调查）随结论拆除，不在长期清单内；
各文件头部注释记录了其目的与载体纪律。

## 发布链（release.yml）

一次版本 tag 触发的完整发布流程：

1. **平台构建腿**：linux-x86-64 / linux-arm-64 / linux-arm-32 / linux-x86-32 /
   macos-arm-64 / macos-x86-64 / windows-x86-64 七条产品腿并行构建 + 打包
   （linux-riscv-64 为编译 canary，不出制品）。qemu 交叉腿运行于固化工具链容器，
   产物自包含（打包前收集运行依赖库）。
2. **聚合**：所有 gate 腿产物汇成单一 release 事件。
3. **洁净房 e2e 门禁**：在无任何开发依赖的干净容器内执行——离线安装发布制品 →
   启动完整 daemon 群 → 网关探测 → CLI 冒烟，并覆盖旧版升级与回滚路径。任一断言
   失败即阻塞发布。
4. **签名与 publish**（需维护者审批放行）：产物清单完整性预检（缺件即中止）、
   GPG 签名 + cosign 容器签名、全量资产与版本清单一次性发布。

安装与升级方式见仓库根目录 [README](../README.md)。

## 目录结构

```
.github/
├── actions/     # 可复用复合 Action（子模块检出、容器腿构建、打包模板）
├── docker/      # 交叉编译工具链镜像（arm64 / arm32 / i386）
├── scripts/     # 长逻辑脚本：依赖布局、镜像同步、发布环境配置、失败日志归档等
└── workflows/   # 工作流编排（触发 × job 拓扑）
```

编排层只描述"何时跑什么"，可复用的构建/打包步骤收敛在 `actions/`（避免同一逻辑
在门禁线与发布线各写一份而漂移），交叉腿环境收敛在 `docker/`（避免每轮从源码
重编依赖）。

## 本地开发

```bash
git clone --recursive git@atomgit.com:openairymax/agentrt.git
# 或已有克隆：
git submodule update --init --recursive
```

7 个叶子的精确提交由本仓库树内 gitlink 钉定，构建以 agentrt 根为宿主
（`cmake -S .`）。CI 与本地同布局。

## Secrets 概览

fork 或自部署需要配置的最小集合（本表只列用途，不含任何值）：

| Secret | 用途 |
|---|---|
| `GH_TOKEN` | GitHub 组织令牌：镜像推送、私有子仓检出 |
| `GT_TOKEN` | Gitee 令牌：镜像同步 |
| `ATOMGIT_TOKEN` | atomgit 令牌：SSoT 拉取与官方制品上传 |
| `GPG_PRIVATE_KEY` / `GPG_PASSPHRASE` | 发布物 GPG 签名 |
| `COSIGN_PRIVATE_KEY` / `COSIGN_PASSWORD` | 容器镜像 cosign 签名 |

## 相关链接

| 资源 | 链接 |
|------|------|
| **主 README** | [agentrt/README.md](../README.md) |
| **中文 README** | [agentrt/README_zh.md](../README_zh.md) |
| **伞仓（纯容器）** | [airymaxhub](https://atomgit.com/openairymax/airymaxhub) |
| **构建系统** | [agentrt/cmake/](../cmake/) |

## 许可证

双许可证：**AGPL v3 + Apache 2.0**（SPDX: `AGPL-3.0-or-later OR Apache-2.0`）。
详见仓库根目录 [LICENSE](../LICENSE) 与 [NOTICE](../NOTICE)。

Copyright (c) 2025-2026 SPHARX Ltd. All Rights Reserved.
