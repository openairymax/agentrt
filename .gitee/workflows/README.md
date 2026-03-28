# Gitee GoCI 工作流目录

**版本**: v1.0.0.6  
**最后更新**: 2026-03-26  
**许可证**: Apache License 2.0

---

## 📁 目录说明

Gitee GoCI 会自动发现并执行本目录中的工作流文件。

## 📋 工作流文件清单

| 文件 | 说明 |
|------|------|
| `agentos-ci.yml` | Gitee GoCI 完整 CI/CD 流水线配置 |
| `GITEE_CI_ENVIRONMENT.md` | Gitee 环境配置说明与变量指南 |
| `GITEE_DEPLOYMENT_STRATEGY.md` | Gitee 部署策略文档 (蓝绿/金丝雀/滚动) |
| `GITEE_CI_USAGE_GUIDE.md` | Gitee CI/CD 使用指南与快速开始 |

## 🔄 流水线阶段

| 阶段 | 说明 | 触发条件 |
|------|------|----------|
| Code Quality | 代码质量检查 (静态分析、格式化) | push/PR |
| Build | 跨平台构建 (Linux/macOS/Windows) | push/PR |
| Test | 单元/集成/契约/性能测试 | push/PR |
| Security | 安全扫描 (漏洞、秘钥、许可证) | push/定时 |
| Deploy Staging | 预发布环境部署 | develop 分支 |
| Deploy Production | 生产环境部署 | master/tag (手动) |

## 🎯 部署策略

| 策略 | 说明 | 适用场景 |
|------|------|----------|
| 蓝绿部署 | 双环境切换，无停机 | 生产环境重大更新 |
| 金丝雀部署 | 渐进式流量切换 | 重大功能发布 |
| 滚动更新 | 逐步替换实例 | 常规版本更新 |

## 🔗 相关文档

- [Gitee CI/CD 使用指南](GITEE_CI_USAGE_GUIDE.md)
- [Gitee 环境配置说明](GITEE_CI_ENVIRONMENT.md)
- [Gitee 部署策略](GITEE_DEPLOYMENT_STRATEGY.md)
- [CI/CD 全面检查报告](../../.本地总结/CI_CD全面检查报告.md)
- [Gitee 官方文档](https://gitee.com/help/articles/building-in-ci)

## ⚙️ 必需的流水线变量

## 📞 联系方式

- **维护者**: AgentOS 架构委员会
- **技术支持**: lidecheng@spharx.cn
- **问题反馈**: https://github.com/SpharxTeam/AgentOS/issues
- **官方仓库**: https://gitee.com/spharx/agentos

---

© 2026 SPHARX Ltd. All Rights Reserved.

*"Gitee Go 加速中国开源。"*
| `DINGTALK_TOKEN` | 钉钉机器人 Webhook | **是** |
| `WECOM_WEBHOOK` | 企业微信机器人 Webhook | **是** |
| `POSTGRES_PASSWORD` | PostgreSQL 数据库密码 | **是** |
| `GRAFANA_PASSWORD` | Grafana 管理员密码 | **是** |

---

*© 2026 SPHARX Ltd. 保留所有权利*
