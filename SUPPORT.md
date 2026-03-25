# 支持与支持渠道 SUPPORT

如果您在使用 AgentOS 过程中遇到问题或需要帮助，请参考以下支持渠道。

## 📋 目录

- [文档资源](#文档资源)
- [社区支持](#社区支持)
- [商业支持](#商业支持)
- [联系方式](#联系方式)

---

## 文档资源

在寻求帮助之前，建议您先查阅以下文档：

### 官方文档

- **[README.md](README.md)** - 项目概述和快速开始
- **[快速入门](partdocs/guides/getting_started.md)** - 新手指南
- **[架构文档](partdocs/architecture/)** - 深入理解系统设计
- **[API 文档](partdocs/api/)** - 接口参考
- **[开发指南](partdocs/guides/)** - 最佳实践

<!-- From data intelligence emerges. by spharx -->
### 技术文档

- **[CoreLoopThree 架构](partdocs/architecture/coreloopthree.md)** - 三层一体核心运行时
- **[MemoryRovol 架构](partdocs/architecture/memoryrovol.md)** - 记忆卷载系统
- **[IPC 机制](partdocs/architecture/ipc.md)** - 进程间通信
- **[微内核设计](partdocs/architecture/microkernel.md)** - 内核架构
- **[系统调用](partdocs/architecture/syscall.md)** - 系统调用接口

### 外部资源

- **[Workshop 文档](../Workshop/README.md)** - 数据采集工厂
- **[Deepness 文档](../Deepness/README.md)** - 深度加工系统
- **[Benchmark 指标](../Benchmark/metrics/README.md)** - 性能评测

---

## 社区支持

### GitHub Issues（国际镜像）

**适用场景**:
- 报告 Bug
- 提出功能建议
- 询问具体问题

**使用方法**:
1. 访问 [GitHub Issues](https://github.com/SpharxTeam/AgentOS/issues) (国际镜像)
   或 [Gitee Issues](https://gitee.com/spharx/agentos/issues) (国内官方，访问更快)
2. 搜索是否已有类似问题
3. 创建新 Issue 并选择合适的模板
4. 提供详细信息（环境、复现步骤、错误日志等）

**响应时间**: 48 小时内

### GitHub Discussions（国际镜像）

**适用场景**:
- 一般性问题讨论
- 分享使用经验
- 展示项目案例
- 社区交流

**使用方法**:
1. 访问 [GitHub Discussions](https://github.com/SpharxTeam/AgentOS/discussions)
2. 选择合适的分类（Q&A、Show and tell、General 等）
3. 发布讨论帖

**响应时间**: 视社区活跃度而定

**注意**: 如果您在中国大陆，访问 Gitee 会更快；如果您在其他地区，建议使用 GitHub。

### 邮件列表

**适用场景**:
- 安全问题报告
- 商务合作
- 其他不适合公开讨论的话题

**邮箱地址**:
- 技术支持：lidecheng@spharx.cn
- 安全问题：wangliren@spharx.cn
- 行为准则：wangliren@spharx.cn

---

## 商业支持

我们提供专业的商业支持服务，适用于企业用户和生产环境部署。

### 商业支持内容

#### 1. 技术支持服务

- **优先响应**: 工作日 4 小时内响应
- **专属客服**: 一对一技术支持
- **远程协助**: 问题排查和解决
- **定期巡检**: 系统健康检查

#### 2. 定制化开发

- **功能定制**: 根据需求开发特定功能
- **硬件适配**: 专用硬件集成和优化
- **性能调优**: 针对业务场景优化性能
- **系统集成**: 与现有系统集成

#### 3. 培训服务

- **在线培训**: 远程技术培训
- **现场培训**: 上门技术指导
- **文档定制**: 定制化技术文档
- **认证考试**: 技术人员认证

#### 4. 授权许可

- **商业闭源授权**: 闭源使用和销售许可
- **企业版授权**: 多用户企业版
- **OEM 授权**: 嵌入式设备授权
- **云服务授权**: SaaS 服务授权

### 服务级别

| 服务级别 | 响应时间 | 服务方式 | 适用场景 |
|---------|---------|---------|---------|
| **基础版** | 工作日 24h | 邮件/远程 | 中小企业 |
| **专业版** | 工作日 4h | 专属客服 + 远程 | 生产环境 |
| **企业版** | 7×24h 4h | 专属团队 + 现场 | 关键业务 |

### 联系我们

**商务咨询**:
- 邮箱：zhouzhixian@spharx.cn
- 官网：https://spharx.cn
- 工作时间：全天 9:00-18:00 (北京时间)

---

## 常见问题 FAQ

### Q1: 安装时遇到依赖问题怎么办？

**A**: 请按以下步骤操作：

1. 检查 Python 版本是否为 3.9+
   ```bash
   python3 --version
   ```

2. 升级 pip 到最新版本
   ```bash
   python3 -m pip install --upgrade pip
   ```

3. 使用虚拟环境
   ```bash
   python3 -m venv venv
   source venv/bin/activate
   ```

4. 运行环境验证脚本
   ```bash
   ./validate.sh
   ```

### Q2: 如何报告安全漏洞？

**A**: 请遵循 [SECURITY.md](SECURITY.md) 中的流程：

- 发送详细信息至 wangliren@spharx.cn
- 不要公开披露漏洞细节
- 给我们合理的时间修复

### Q3: 可以用于商业用途吗？

**A**: 可以，但需要注意：

- **开源用途**: 免费使用，但需遵守 Apache 2.0 协议
- **商业用途**: 需要申请商业授权
- 详见 [LICENSE](LICENSE) 文件

### Q4: 支持哪些操作系统？

**A**: AgentOS 支持：

- ✅ Linux (Ubuntu 22.04+, CentOS 8+)
- ✅ macOS (13.0+)
- ✅ Windows 11 (WSL2)

### Q5: 如何升级到新版本？

**A**: 升级步骤：

1. 查看 [CHANGELOG.md](CHANGELOG.md) 了解变更
2. 备份当前版本和数据
3. 拉取最新代码
   ```bash
   git pull origin main
   ```
4. 更新依赖
   ```bash
   poetry install  # 或 pip install -e .
   ```
5. 运行迁移脚本（如有）
   ```bash
   python scripts/migrate.py
   ```

### Q6: 性能不达标怎么办？

**A**: 可以尝试：

1. 查看 [性能基准](partdocs/specifications/performance.md)
2. 运行性能诊断工具
   ```bash
   make benchmark
   ```
3. 调整配置参数（详见内核调优指南）
4. 联系技术支持获取优化建议

---

## 反馈与建议

我们非常重视您的反馈，无论是对项目的批评还是建议，都能帮助我们变得更好。

### 提交反馈

- 在 GitHub Issue 中描述您的问题
- 发送邮件至技术支持邮箱
- 在 Discussions 中发起讨论

### 参与改进

- 提交 Pull Request 改进代码
- 改进和完善文档
- 分享使用经验和最佳实践

---

## 社区守则

参与社区讨论时，请遵守：

1. **尊重他人**: 保持礼貌和专业
2. **建设性**: 提供有建设性的意见
3. **保密**: 不泄露敏感信息
4. **合法合规**: 遵守相关法律法规

详见 [CODE_OF_CONDUCT.md](CODE_OF_CONDUCT.md)

---

<div align="center">

**感谢您选择 AgentOS！**

如有任何问题，请随时联系我们。

**SPHARX 极光感知科技**

**技术支持**: lidecheng@spharx.cn  
**安全问题**: wangliren@spharx.cn  
**商务合作**: zhouzhixian@spharx.cn  
**官方网站**: https://spharx.cn

</div>

---

© 2026 SPHARX Ltd. All Rights Reserved.
