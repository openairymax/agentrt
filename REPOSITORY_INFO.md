# AgentOS 项目仓库说明

**版本**: 1.0.0.5  
**更新日期**: 2026-03-20  
**维护者**: SPHARX Team

---

## 📑 双仓库策略

AgentOS 项目采用 **Gitee + GitHub** 双仓库策略，以更好地服务全球开发者。

### 官方主仓库（中国）

**🏛️ Gitee - 官方主仓库**  
📍 地址：https://gitee.com/spharx/agentos  
🌏 适用地区：中国大陆及亚洲地区  
⚡ 访问速度：国内访问最快  
📦 内容：完整源代码、Issues、Discussions  
👥 维护团队：SPHARX 官方团队  

**优势**:
- ✅ 国内访问速度快
- ✅ 中文社区活跃
- ✅ 官方首选支持渠道
- ✅ 符合国内开发习惯

### 镜像仓库（国际）

**🌐 GitHub - 国际镜像仓库**  
📍 地址：https://github.com/SpharxTeam/AgentOS  
🌏 适用地区：中国大陆以外地区  
⚡ 访问速度：国际访问优化  
📦 内容：与 Gitee 同步的完整镜像  
👥 维护团队：SPHARX 国际团队  

**优势**:
- ✅ 国际访问速度快
- ✅ 便于国际合作
- ✅ 集成 GitHub 生态工具
- ✅ 支持 GitHub Actions CI/CD

---

## 🎯 如何选择仓库

### 推荐选择指南

| 您的情况 | 推荐仓库 | 理由 |
|---------|---------|------|
| 在中国大陆 | Gitee (首选) | 访问速度快，无网络限制 |
| 在港澳台 | GitHub | 国际网络环境优化 |
| 在海外 | GitHub | 本地访问优化 |
| 参与国际合作 | GitHub | 便于与国际开发者协作 |
| 企业用户（国内） | Gitee | 合规性更好 |
| 开源贡献者 | 任意 | 两者完全同步 |

### 访问速度对比

```
中国大陆用户:
├── Gitee:    ~50ms   ✅ 推荐
└── GitHub:   ~300ms  ⚠️ 可能不稳定

海外用户:
├── Gitee:    ~200ms  ⚠️ 较慢
└── GitHub:   ~30ms   ✅ 推荐
```

---

## 🔗 重要链接

### Gitee 官方仓库

- **主页**: https://gitee.com/spharx/agentos
- **Issues**: https://gitee.com/spharx/agentos/issues
- **Discussions**: https://gitee.com/spharx/agentos/gists
- **Releases**: https://gitee.com/spharx/agentos/releases

### GitHub 镜像仓库

- **主页**: https://github.com/SpharxTeam/AgentOS
- **Issues**: https://github.com/SpharxTeam/AgentOS/issues
- **Discussions**: https://github.com/SpharxTeam/AgentOS/discussions
- **Releases**: https://github.com/SpharxTeam/AgentOS/releases

---

## 📝 使用建议

### Issue 提交

**推荐做法**:
1. **优先选择 Gitee**（如果您在中国大陆）
2. 在两个仓库中搜索是否已有类似问题
3. 选择一个平台提交即可（无需重复提交）
4. 注明您的网络环境和所在地区

**Issue 模板**:
```markdown
### 问题描述
[清晰描述问题]

### 环境信息
- 操作系统：[如 Ubuntu 22.04]
- Docker 版本：[如 24.0.7]
- 所在地区：[如 北京]
- 使用仓库：[Gitee/GitHub]

### 复现步骤
[详细步骤]

### 期望行为
[期望结果]

### 实际行为
[实际结果]

### 日志信息
[相关日志]
```

### Discussions 讨论

**适合话题**:
- ✅ 一般性问题
- ✅ 使用经验分享
- ✅ 最佳实践
- ✅ 项目案例展示
- ✅ 功能建议

**分区说明**:
- 💬 **General**: 综合讨论
- ❓ **Q&A**: 问答交流
- 💡 **Ideas**: 功能建议
- 📢 **Announcements**: 官方公告

---

## 🔄 同步机制

### 自动同步

两个仓库保持实时同步：

```
Gitee (主仓库)
    ↓ [自动镜像]
GitHub (镜像仓库)
```

**同步频率**: 
- 代码提交：实时同步（< 5 分钟）
- Issues/Discussions：不同步（独立社区）

### 版本发布

所有正式版本会同时在两个平台发布：

```bash
# 查看最新版本
git tag -l  # 两个仓库标签完全一致

# 当前版本：v1.0.0.5
# 下一版本：v1.0.0.6 (计划中)
```

---

## 🌍 全球社区分布

### 开发者地域分布

```
Gitee 社区:
├── 中国大陆：85%
├── 港澳台：10%
└── 其他：5%

GitHub 社区:
├── 北美：35%
├── 欧洲：30%
├── 亚洲（除中国）：20%
└── 其他：15%
```

### 活跃度对比

| 指标 | Gitee | GitHub |
|------|-------|--------|
| Stars | 主要 | 镜像 |
| Forks | 主要 | 镜像 |
| Issues | 活跃 | 较少 |
| PRs | 较多 | 中等 |
| 讨论 | 中文为主 | 英文为主 |

---

## 💡 最佳实践

### 对于贡献者

1. **Fork 仓库**
   - 中国大陆开发者 → Fork Gitee
   - 海外开发者 → Fork GitHub

2. **提交 PR**
   - 基于您 Fork 的仓库提交
   - 两个平台都接受 PR
   - 审核团队会合并到两个仓库

3. **参与讨论**
   - 根据所在地选择平台
   - 鼓励跨平台交流
   - 重要讨论可邮件列表备份

### 对于使用者

1. **克隆代码**
   ```bash
   # 中国大陆（推荐 Gitee）
   git clone https://gitee.com/spharx/agentos.git
   
   # 海外（推荐 GitHub）
   git clone https://github.com/SpharxTeam/AgentOS.git
   ```

2. **获取帮助**
   - 查看文档：两个仓库文档完全一致
   - 提交 Issue：选择访问更快的平台
   - 参与讨论：根据语言偏好选择

3. **关注更新**
   - Watch 仓库接收通知
   - 订阅 Release 邮件
   - 关注官方社交媒体

---

## 📞 联系方式

### 官方渠道

- **官方网站**: https://spharx.cn
- **技术文档**: https://docs.spharx.cn/agentos

### 社区渠道

- **Gitee**: https://gitee.com/spharx/agentos
- **GitHub**: https://github.com/SpharxTeam/AgentOS
- **邮件列表**: agentos-community@spharx.cn

### 联系邮箱

- **技术支持**: lidecheng@spharx.cn
- **安全问题**: wangliren@spharx.cn
- **商务合作**: zhouzhixian@spharx.cn

---

## ❓ 常见问题

### Q1: 我应该 Star 哪个仓库？

**A**: 建议两个都 Star，支持项目发展！
- Gitee: 支持国内社区
- GitHub: 提升国际影响力

### Q2: Issue 应该提交到哪里？

**A**: 根据您的位置选择：
- 中国大陆 → Gitee Issues（响应更快）
- 海外 → GitHub Issues（沟通更方便）

### Q3: 两个仓库的代码会不同步吗？

**A**: 不会。我们有自动化镜像机制，确保代码完全一致。

### Q4: 可以在两个平台都提交 Issue 吗？

**A**: 不建议。选择一个平台即可，避免重复工作。

### Q5: 如果 Gitee 和 GitHub 都有我的 Issue，会怎样？

**A**: 审核团队会在两个平台都关闭重复的 Issue，只保留一个进行处理。

---

## 📊 统计数据

### 仓库统计（截至 2026-03-20）

| 指标 | Gitee | GitHub | 总计 |
|------|-------|--------|------|
| Stars | 1,200+ | 800+ | 2,000+ |
| Forks | 300+ | 200+ | 500+ |
| Watchers | 150+ | 100+ | 250+ |
| Issues (Open) | 50+ | 20+ | 70+ |
| Releases | 15 | 15 | 15 |

### 增长趋势

```
月度新增 Stars:
├── Gitee: +80/month
└── GitHub: +50/month

月度新增 Forks:
├── Gitee: +20/month
└── GitHub: +15/month
```

---

## 🎓 教育资源

### 新手入门

1. **阅读文档**
   - [快速入门](partdocs/guides/getting_started.md)
   - [架构说明](partdocs/architecture/)
   - [部署指南](scripts/docker/DEPLOYMENT_GUIDE.md)

2. **选择仓库**
   - 根据位置选择 Gitee 或 GitHub
   - Clone 代码到本地
   - 按照文档开始使用

3. **参与社区**
   - 加入 Discussions
   - 关注 Issues
   - 分享经验

### 进阶贡献

1. **代码贡献**
   - Fork 合适的仓库
   - 创建功能分支
   - 提交 Pull Request

2. **文档贡献**
   - 修正错别字
   - 补充示例
   - 翻译文档

3. **社区贡献**
   - 回答问题
   - 分享案例
   - 组织活动

---

## 🔒 安全声明

### 仓库安全

- ✅ 两个仓库均启用双因素认证
- ✅ 代码签名验证
- ✅ 依赖安全检查
- ✅ 定期安全审计

### 报告安全问题

请通过邮件报告敏感安全问题：
- 📧 **安全联系**: wangliren@spharx.cn
- 🔐 不要公开披露

---

<div align="center">


*"From data intelligence emerges"*  
*"始于数据，终于智能"*

© 2026 SPHARX Ltd. All Rights Reserved.

</div>