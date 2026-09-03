<!-- SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0 -->
<!-- Copyright (c) 2025-2026 SPHARX Ltd. All Rights Reserved. -->

# `.github/` — AgentRT 仓库自动化与模板

> GitHub Actions 工作流、Issue / PR 模板及社区健康文件，服务于
> [AgentRT (AirymaxAgentRT)](https://atomgit.com/openairymax/agentrt) 管理仓库。

---

## 定位

本目录承载 **agentrt 管理仓库的发布平面（workflow 宿主）**。agentrt 是 Airymax 平台的
**AI Agent 运行时平台工程**——为 AI Agent 团队提供 OS 级运行时基础设施，定位类似于
JVM 之于语言、containerd 之于容器。

本管理仓库聚合 **7 个叶子仓库**（atoms / commons / cupolas / daemons / gateway /
heapstore / protocols）作为 git 子模块，对外提供微内核原语、认知循环、内存分层、
安全穹顶、IPC 协议、网关服务和常驻守护进程等 OS 级机制。

**拓扑约定（2026-09 架构裁决）**：`airymaxhub` 伞仓为纯容器（superproject），
**不承载任何流水线**；CI/CD（构建 / 测试 / codegen / SSoT / 镜像同步 / 六平台发布）
全部下沉到本 agentrt 仓，因为 agentrt 的版本发布语义（VERSION / CHANGELOG /
7 叶子聚合）都以本仓为唯一宿主。atomgit 为 SSoT 主托管，GitHub / Gitee 仅为镜像
与执行面（按需经各自 API/MCP 同步，不做本地循环触发）。

## 目录内容

```
.github/
├── README.md          # 本文件
├── scripts/
│   ├── init-submodules.sh   # CI 依赖布局：7 叶子子模块 + release 期 sibling 数据
│   └── sync-mirror.sh       # agentrt + 7 叶子 → GitHub / Gitee 镜像同步
└── workflows/
    ├── build-test.yml       # 构建 / 测试 / 覆盖率门禁（Linux / macOS / Windows）
    ├── codegen-check.yml    # syscall.xml SSoT 漂移校验（codegen 产物一致性）
    ├── release.yml          # 六平台发布（cosign + GPG 签名，atomgit 官方 + GitHub 镜像）
    ├── ssot-validate.yml    # 用户态 SSoT 技术点权威源门禁（TP-012~016，dispatch 按需）
    └── sync-mirror.yml      # atomgit(SSoT) → GitHub / Gitee 镜像同步触发器
```

## 工作流

| workflow | 触发 | 说明 |
|----------|------|------|
| `build-test.yml` | `push main` / `pull_request` / `workflow_dispatch` | Debug 全量构建 + `ctest` + `lcov` 覆盖率门禁 55%（Linux container 20.04 / macOS Homebrew / Windows vcpkg） |
| `codegen-check.yml` | `push main` / `pull_request` / `workflow_dispatch` | `syscall_gen.py --check` 校验 `syscall.xml` 与生成产物漂移 |
| `release.yml` | `tag v*` 推送 / `workflow_dispatch`（输入 version） | 六平台（linux-x64 / macos-arm64 / windows-x64 / riscv64 / arm64 / 32bit matrix）制品 → cosign + GPG 签名 → `publish-release.sh` 上传 atomgit 官方制品仓 + GitHub Release 镜像 |
| `ssot-validate.yml` | `workflow_dispatch`（按需） | 伞级 SSoT 权威源门禁：以 `airymaxhub`（GitHub 镜像）为数据源克隆到临时区后运行 `validate-ssot.py`，校验 docs 登记的技术点物理宿主路径存在 |
| `sync-mirror.yml` | `push main` / `workflow_dispatch` | `sync-mirror.sh`：agentrt + 7 叶子从 atomgit(SSoT) 同步至 GitHub / Gitee（缺仓自动创建，atoms 私有，错误隔离汇总） |

## 布局与子模块

- agentrt 工作树根即 agentrt 源码；`git submodule update --init --recursive` 在根上
  一次性拉齐 7 个叶子（SHA 由 agentrt 树 gitlink 钉定）。私有叶子 `atoms` 经
  `GH_TOKEN`（org PAT，`~/.netrc`）认证。
- release 链额外克隆 sibling 数据到**历史相对路径**（`tools/` 与
  `agent-workload/{sdk,ecosystem}`，来自 GitHub 镜像期 clone），使打包 / 发布脚本的
  路径引用与伞仓时代一致、无需逐处改写：
  - `tools/`：`lib-builddeps.sh` / `ops/templates/*`（config 模板）/ `ops/bin/agentrt-bootstrap.sh` / `ci/release/publish-release.sh` 等；
  - `agent-workload/sdk/tui`：Rust TUI（`agentrt-tui`）构建源；
  - `agent-workload/ecosystem`：Python 运行时（agents / manager 配置 / markets maths-toolkit）。
- 构建源一律 `cmake -S .`（agentrt 即宿主根），不再有 `agent-workload/agentrt` 前缀。

## 脚本

### init-submodules.sh — CI 依赖布局

```bash
bash .github/scripts/init-submodules.sh               # 仅 7 叶子（build-test / codegen）
bash .github/scripts/init-submodules.sh agent-workload tools   # 叶子 + sibling 数据（release）
```

参数保持伞仓时代调用形式（`agent-workload` / `tools`），脚本内部重映射；私有叶子与
sibling clone 凭据统一走 `GH_TOKEN`。

### sync-mirror.sh — agentrt + 叶子双镜像同步

同步模型：源 = atomgit(SSoT)，`clone --mirror` 抓全量 refs 后仅 push **heads + tags**
（force 与 SSoT 一致，不透传 atomgit 平台内部 ref）；子模块树以各仓 `HEAD:.gitmodules`
BFS 解析；缺仓自动创建（atoms 私有）；每仓错误隔离、末尾汇总。

## Secrets

| Secret | 用途 | 使用方 |
|--------|------|--------|
| `GH_TOKEN` | GitHub org PAT（建仓 / 推送 / 私有子仓认证；PAT 推送的 tag 可触发 `release.yml`，`GITHUB_TOKEN` 推送不会） | 全部 workflow |
| `GT_TOKEN` | Gitee 令牌（org 建仓 + 推送） | `sync-mirror.yml` |
| `ATOMGIT_TOKEN` | atomgit 令牌（私有子仓 mirror clone / 官方制品上传） | `sync-mirror.yml`、`release.yml` |
| `GPG_PRIVATE_KEY` / `GPG_PASSPHRASE` | 发布 GPG 签名（manifest `*.asc`） | `release.yml` |
| `COSIGN_PRIVATE_KEY` / `COSIGN_PASSWORD` | cosign 容器签名（tarball `*.sig`） | `release.yml` |

签名密钥初始化见 `tools/scripts/ci/release/init-signing-keys.sh`（release 期从
`tools/` sibling 取得）。

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
