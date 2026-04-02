# AgentOS Git 配置目录

本目录包含 AgentOS 项目的 Git 相关配置、钩子和脚本。

## 目录结构

- `hooks/` - Git 钩子脚本
- `config/` - Git 配置文件示例
- `scripts/` - Git 相关实用脚本
- `templates/` - Git 模板文件

## 用途说明

### 1. Git 钩子 (hooks)

Git 钩子是在 Git 执行特定操作时自动运行的脚本。我们提供了一些标准钩子示例：

- `pre-commit` - 提交前检查代码格式和运行测试
- `commit-msg` - 检查提交消息格式
- `pre-push` - 推送前运行完整测试套件

### 2. Git 配置 (config)

提供了项目特定的 Git 配置示例，包括：
- 别名配置
- 差异工具配置
- 合并策略配置
- 多平台路径转换

### 3. 实用脚本 (scripts)

提供了一些与 Git 工作流相关的实用脚本：
- 批量重命名分支
- 清理已合并的分支
- 生成变更日志
- 统计贡献者

### 4. 模板文件 (templates)

Git 模板文件用于初始化新仓库时的默认配置。

## 使用方法

### 安装 Git 钩子

```bash
# 将钩子复制到 .git/hooks 目录
cp .gitcode/hooks/* .git/hooks/
chmod +x .git/hooks/*
```

### 应用 Git 配置

```bash
# 将配置合并到全局 Git 配置
git config --global include.path "$(pwd)/.gitcode/config/gitconfig"
```

### 使用脚本

```bash
# 运行实用脚本
./.gitcode/scripts/cleanup-branches.sh
```

## 平台兼容性

这些配置和脚本考虑了跨平台兼容性：
- **Windows**: 使用 Git Bash 或 WSL 运行脚本
- **Linux/macOS**: 原生支持

## 相关链接

- [Git 官方文档](https://git-scm.com/doc)
- [Git 钩子文档](https://git-scm.com/docs/githooks)
- [AgentOS 贡献指南](../CONTRIBUTING.md)

## 许可证

Copyright (c) 2026 SPHARX. All Rights Reserved.