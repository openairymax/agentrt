# AgentOS 安全策略

## 支持的版本

本项目目前支持以下版本的安全更新：

| 版本 | 状态 | 支持截止日期 |
|------|------|-------------|
| v1.0.x | ✅ 当前支持中 | 2026-12-31 |
| v0.9.x | ⚠️ 仅安全修复 | 2026-06-30 |
| < 0.9 | ❌ 不再支持 | - |

## 报告漏洞

我们非常重视 AgentOS 的安全性。如果您发现安全漏洞，请**不要**公开 Issue，而是按照以下步骤报告：

### 1. 通过 Gitee 私信报告（推荐）

发送邮件至：security@spharx.com
主题格式：[SECURITY] AgentOS 安全漏洞报告 - [简短描述]

### 2. 通过 GitHub 私信报告

如果您使用的是 GitHub 镜像，可以通过：
- https://github.com/SpharxTeam/AgentOS/security/advisories/new
- 或发送至上述邮箱

### 3. 报告内容应包含

请尽可能详细地描述漏洞：

- **漏洞类型**：例如 XSS、SQL 注入、缓冲区溢出、权限绕过等
- **受影响版本**：确认受影响的版本范围
- **复现步骤**：详细的复现步骤和示例代码
- **潜在影响**：该漏洞可能造成的危害
- **建议修复方案**：如果有的话

### 4. 响应时间表

| 时间 | 行动 |
|------|------|
| 24 小时内 | 确认收到并初步评估 |
| 48 小时内 | 提供初步响应和状态更新 |
| 7 天内 | 完成修复或提供临时缓解方案 |
| 14 天内 | 发布安全补丁（如适用） |

## 安全最佳实践

### 对于用户

1. **保持更新**：始终使用最新版本的 AgentOS
2. **最小权限原则**：仅授予必要的权限
3. **网络安全**：确保网络环境安全，使用 HTTPS
4. **定期审计**：定期检查日志和安全配置
5. **备份**：定期备份重要数据

### 对于开发者

1. **代码审查**：所有代码变更必须经过审查
2. **依赖管理**：定期更新依赖项，修复已知漏洞
3. **安全测试**：在发布前进行安全测试
4. **输入验证**：严格验证所有外部输入
5. **错误处理**：避免泄露敏感信息

## 已知安全问题

查看当前已知的安全问题及其状态：

- [Gitee Issues (标签: security)](https://gitee.com/spharx/agentos/issues?labels=security)
- [GitHub Security Advisories](https://github.com/SpharxTeam/AgentOS/security/advisories)

## 安全相关资源

- [OWASP Top 10](https://owasp.org/www-project-top-ten/)
- [CWE Top 25](https://cwe.mitre.org/top25/archive/2023/2023_cwe_top25.html)
- [AgentOS 架构文档](../manuals/architecture/)
- [AgentOS 安全文档](../manuals/security/)

## 致谢

感谢所有为 AgentOS 安全做出贡献的研究者和开发者！您的努力让我们的生态系统更加安全。

---

**最后更新**: 2026-04-02
