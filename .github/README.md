<!-- SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0 -->
<!-- Copyright (c) 2025-2026 SPHARX Ltd. All Rights Reserved. -->

# `.github/` — AgentRT 仓库自动化与模板

> GitHub Actions 工作流、Issue / PR 模板及社区健康文件，服务于
> [AgentRT (AirymaxAgentRT)](https://atomgit.com/openairymax/agentrt) 管理仓库。

---

## 定位

本目录承载 agentrt 管理仓库的 GitHub 级自动化配置。agentrt 是 Airymax 平台的
**AI Agent 运行时平台工程**——为 AI Agent 团队提供 OS 级运行时基础设施，定位类似于
JVM 之于语言、containerd 之于容器。

本管理仓库聚合 **7 个叶子仓库**（atoms / commons / cupolas / daemons / gateway /
heapstore / protocols）作为 git 子模块，对外提供微内核原语、认知循环、内存分层、
安全穹顶、IPC 协议、网关服务和常驻守护进程等 OS 级机制。

## 目录内容

```
.github/
├── README.md          # 本文件
└── workflows/         # GitHub Actions 工作流（待配置）
```

## 相关链接

| 资源 | 链接 |
|------|------|
| **主 README** | [agentrt/README.md](../README.md) |
| **中文 README** | [agentrt/README_zh.md](../README_zh.md) |
| **伞仓** | [airymaxhub](https://atomgit.com/openairymax/airymaxhub) |
| **构建系统** | [agentrt/cmake/](../cmake/) |

## 许可证

双许可证：**AGPL v3 + Apache 2.0**（SPDX: `AGPL-3.0-or-later OR Apache-2.0`）。
详见仓库根目录 [LICENSE](../LICENSE) 与 [NOTICE](../NOTICE)。

Copyright (c) 2025-2026 SPHARX Ltd. All Rights Reserved.
