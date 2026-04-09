# AgentOS 文档中心

**版本**: 1.0.0  
**最后更新**: 2026-04-05  
**维护者**: lirenwang 

---

## 📚 文档导航

欢迎使用AgentOS智能体操作系统文档中心。本文档采用分层组织结构，从入门到精通，满足不同层次的需求。

### 🎯 快速入口

| 角色 | 推荐阅读路径 | 预计时间 |
|------|------------|---------|
| **初学者** | 快速开始 → 安装指南 → 配置基础 | 30分钟 |
| **开发者** | API参考 → 编码规范 → 贡献指南 | 2小时 |
| **运维工程师** | 部署指南 → 监控运维 → 故障排查 | 1.5小时 |
| **架构师** | 系统概览 → 微内核架构 → 设计原则 | 3小时 |

---

## 📖 文档分类

### 1️⃣ 入门指南 (Guides)

面向新用户的引导式教程，帮助快速上手：

- [**快速开始**](guides/getting-started.md) — 5分钟体验AgentOS核心功能
- [**安装指南**](guides/installation.md) — 详细的环境搭建步骤
- [**配置指南**](guides/configuration.md) — 完整的配置选项说明
- [**部署指南**](guides/deployment.md) — 生产环境部署最佳实践

---

### 2️⃣ 架构设计 (Architecture)

深入理解AgentOS的设计哲学和技术实现：

- [**系统概览**](architecture/overview.md) — 整体架构与模块关系
- [**微内核架构**](architecture/microkernel.md) — K-1~K-4 原则的实现
- [**核心循环三层**](architecture/coreloopthree.md) — Cognition→Planning→Action
- [**记忆卷载系统**](architecture/memoryrovol.md) — L1→L2→L3→L4 四层记忆
- [**安全穹顶**](architecture/cupolas.md) — Cupolas四层安全防护
- [**设计原则**](../agentos/manuals/ARCHITECTURAL_PRINCIPLES.md) — 五维正交设计原则完整版

---

### 3️⃣ API 参考 (API Reference)

完整的接口文档，包含请求示例和响应格式：

- [**内核API**](api/kernel-api.md) — IPC/Mem/Task/Time 四大原子机制
- [**守护进程API**](api/daemon-api.md) — llm_d, market_d, monit_d等服务接口
- [**Python SDK**](api/python-sdk.md) — Python语言绑定API
- [**Go SDK**](api/go-sdk.md) — Go语言绑定API
- [**错误码手册**](api/error-codes.md) — 完整的错误码定义和处理建议

---

### 4️⃣ 开发者指南 (Development)

参与AgentOS开发的必备知识：

- [**贡献指南**](../.gitcode/CONTRIBUTING.md) — 提交PR的完整流程
- [**编码规范**](development/coding-standards.md) — C/Python/Go/Rust代码风格
- [**测试指南**](development/testing.md) — 单元测试、集成测试、E2E测试
- [**调试技巧**](development/debugging.md) — 性能分析、内存检查、死锁检测
- [**架构决策记录**](development/adr/) — ADR索引（重要技术决策的历史记录）

---

### 5️⃣ 运维手册 (Operations)

生产环境的运维保障：

- [**Docker部署**](../docker/README.md) — 容器化部署完整方案
- [**Kubernetes部署**](operations/kubernetes-deployment.md) — K8s集群编排
- [**监控运维**](operations/monitoring.md) — Prometheus+Grafana监控栈
- [**备份恢复**](operations/backup-recovery.md) — 数据备份与灾难恢复
- [**性能调优**](operations/performance-tuning.md) — 生产级性能优化
- [**安全加固**](operations/security-hardening.md) — 安全配置清单

---

### 6️⃣ 故障排查 (Troubleshooting)

常见问题及解决方案：

- [**常见问题FAQ**](troubleshooting/common-issues.md) — Top 20高频问题
- [**错误诊断**](troubleshooting/diagnosis.md) — 日志分析与问题定位
- [**已知问题**](troubleshooting/known-issues.md) — 已知Bug及临时解决方案

---

### 7️⃣ 参考资料 (References)

补充材料和外部链接：

- [**术语表**](../agentos/specifications/TERMINOLOGY.md) — 统一术语定义
- [**变更日志**](changelog.md) — 版本更新历史
- [**路线图**](roadmap.md) — 未来版本规划
- [**许可证**](../LICENSE) — Apache-2.0 许可证全文

---

## 🔍 文档使用技巧

### 搜索功能

使用 `Ctrl+F` 或 `Cmd+F` 在当前页面搜索关键词。

跨页面搜索可以使用以下命令：

```bash
# 在所有Markdown文件中搜索"IPC"
grep -r "IPC" docs/ --include="*.md"
```

### 文档反馈

发现文档错误或有改进建议？

1. 在对应文档页面点击右上角的 **编辑此页** 按钮
2. 直接修改并提交PR
3. 或者在 [GitCode Issues](https://gitcode.com/spharx/agentos/issues) 反馈

### 版本选择

本文档始终与最新稳定版代码同步。如需查看历史版本文档：

```bash
# 切换到指定版本的文档
git checkout v1.0.0 -- docs/
```

---

## 📊 文档统计

| 类别 | 文档数量 | 总字数 | 最后更新 |
|------|---------|--------|---------|
| 入门指南 | 4篇 | ~15,000字 | 2026-04-05 |
| 架构设计 | 6篇 | ~40,000字 | 2026-04-05 |
| API参考 | 5篇 | ~25,000字 | 2026-04-05 |
| 开发者指南 | 5篇 | ~30,000字 | 2026-04-05 |
| 运维手册 | 6篇 | ~35,000字 | 2026-04-05 |
| 故障排查 | 3篇 | ~12,000字 | 2026-04-05 |
| 参考资料 | 4篇 | ~8,000字 | 2026-04-05 |

**总计**: 33篇文档，约165,000字

---

## 🎯 文档质量标准

AgentOS文档遵循**完美主义原则 (A-4)**：

✅ **完整性**：每个公共API都有文档  
✅ **准确性**：示例代码可运行，配置参数经过验证  
✅ **及时性**：代码变更后24小时内同步更新文档  
✅ **易读性**：使用清晰的语言，避免过度技术化  
✅ **可操作性**：每个指南都提供分步操作说明  

---

## 📞 联系方式

- **问题**: wangliren@spharx.cn

---

**© 2026 SPHARX Ltd. All Rights Reserved.**
