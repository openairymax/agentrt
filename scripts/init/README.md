# AgentOS 初始化配置脚本

**版本**: v1.0.0.6  
**最后更新**: 2026-03-21  

---

## 📋 概述

`scripts/init/` 目录包含 AgentOS 项目的初始化配置工具，用于快速搭建开发环境和生成必要的配置文件。

---

## 📁 文件清单

| 文件 | 说明 | 类型 | 状态 |
|------|------|------|------|
| `init_config.py` | 配置文件初始化脚本 | Python | ✅ 生产就绪 |

---

## 🔧 init_config.py - 配置初始化

### 功能特性

- **环境检测**: 自动识别操作系统和运行环境
- **模板渲染**: 基于模板生成配置文件
- **交互式配置**: 引导用户完成配置项设置
- **依赖检查**: 验证必需的软件包和工具
- **目录创建**: 建立项目所需的目录结构

### 快速开始

```bash
cd scripts/init

# 运行初始化向导
python init_config.py

# 非交互式安装（使用默认配置）
python init_config.py --non-interactive

# 指定配置模板
python init_config.py --template production
```

### 配置流程

初始化工具会引导您完成以下配置步骤：

#### 1. 基础环境配置

```
=== AgentOS 配置向导 ===

检测到您的系统: Linux (Ubuntu 22.04)
Python 版本：3.9.7 ✓
CMake 版本：3.24.1 ✓

请选择配置模板:
[1] development  - 开发环境（默认）
[2] production   - 生产环境
[3] testing      - 测试环境

输入选项 [1-3]: 1
✓ 已选择 development 模板
```

#### 2. 数据库配置

```
请配置数据库连接:

数据库类型:
[1] SQLite (默认，适合开发)
[2] PostgreSQL (生产推荐)
[3] MySQL

输入选项 [1-3]: 1

SQLite 数据库路径 [/home/user/.agentos/data.db]:
✓ 数据库配置完成
```

#### 3. LLM 服务配置

```
请配置 LLM 服务:

主要模型提供商:
[1] OpenAI
[2] Anthropic
[3] DeepSeek
[4] 本地模型

输入选项 [1-4]: 1

OpenAI API Key: sk-********
API Base URL [https://api.openai.com/v1]:
✓ LLM 配置完成
```

#### 4. 安全配置

```
生成安全密钥...

✓ JWT Secret: 已生成并保存
✓ Session Key: 已生成并保存
✓ Encryption Key: 已生成并保存

是否启用双因素认证？[y/N]: N
```

#### 5. 目录结构创建

```
创建项目目录结构...

✓ agentos/heapstore/kernel/
✓ agentos/heapstore/logs/
✓ agentos/heapstore/services/
✓ agentos/heapstore/models/
✓ agentos/manager/cache/

目录结构创建完成！
```

#### 6. 配置文件生成

```
生成配置文件...

✓ .env                    # 环境变量
✓ agentos/manager/kernel/settings.yaml    # 内核配置
✓ agentos/manager/security/policy.yaml    # 安全策略
✓ agentos/manager/logging/manager.yaml     # 日志配置

配置文件生成完成！
```

---

## 📊 生成的配置文件

### .env - 环境变量

```bash
# AgentOS 环境变量配置
# 由 init_config.py 自动生成

# 基础配置
AGENTOS_ENV=development
AGENTOS_DEBUG=true
AGENTOS_LOG_LEVEL=INFO

# 数据库配置
DATABASE_URL=sqlite:///home/user/.agentos/data.db
DATABASE_POOL_SIZE=10

# LLM 配置
LLM_PROVIDER=openai
OPENAI_API_KEY=sk-xxx
OPENAI_API_BASE=https://api.openai.com/v1

# 安全配置
JWT_SECRET=your-secret-key-here
SESSION_KEY=session-key-here
ENCRYPTION_KEY=encryption-key-here

# 服务端口
HTTP_PORT=8080
GRPC_PORT=9090
```

### agentos/manager/kernel/settings.yaml - 内核配置

```yaml
# AgentOS 内核配置

kernel:
  name: agentos-kernel
  version: 1.0.0.6
  
# IPC 配置
ipc:
  binder:
    max_connections: 1024
    timeout_ms: 5000
    
# 内存管理
memory:
  pool_size_mb: 512
  enable_gc: true
  
# 任务调度
scheduler:
  strategy: weighted_round_robin
  time_slice_ms: 10
  
# 时间服务
time:
  timezone: UTC
  ntp_server: pool.ntp.org
```

### agentos/manager/security/policy.yaml - 安全策略

```yaml
# AgentOS 安全策略

security:
  # 权限控制
  permission:
    default_policy: deny
    enable_cache: true
    cache_ttl_seconds: 300
    
  # 输入净化
  sanitizer:
    enabled: true
    rules_file: sanitizer/rules.json
    
  # 审计日志
  audit:
    enabled: true
    log_file: agentos/heapstore/logs/audit.log
    rotation:
      max_size_mb: 100
      max_files: 10
      
  # 虚拟工位
  workbench:
    isolation: process
    resource_limits:
      cpu_percent: 50
      memory_mb: 1024
```

### agentos/manager/logging/manager.yaml - 日志配置

```yaml
# AgentOS 日志配置

logging:
  version: 1
  
  # 根日志记录器
  root:
    level: INFO
    handlers: [console, file]
    
  # 处理器配置
  handlers:
    console:
      class: logging.StreamHandler
      level: INFO
      formatter: colored
      
    file:
      class: logging.handlers.RotatingFileHandler
      filename: agentos/heapstore/logs/agentos.log
      level: DEBUG
      maxBytes: 10485760  # 10MB
      backupCount: 5
      formatter: detailed
      
  # 格式化器
  formatters:
    colored:
      format: '%(asctime)s - %(name)s - %(levelname)s - %(message)s'
      
    detailed:
      format: '%(asctime)s - %(name)s - %(levelname)s - %(message)s [%(filename)s:%(lineno)d]'
      
  # 特定模块日志级别
  loggers:
    agentos.core: DEBUG
    agentos.services: INFO
    agentos.llm_d: DEBUG
```

---

## 🔍 故障排查

### 常见问题

#### 1. 权限不足

**错误**: `Permission denied: /etc/agentos`

**解决**:
```bash
# 使用 sudo 运行（不推荐）
sudo python init_config.py

# 或修改目录权限
sudo chown -R $USER:$USER /etc/agentos
```

#### 2. Python 依赖缺失

**错误**: `ModuleNotFoundError: No module named 'pyyaml'`

**解决**:
```bash
# 安装依赖
pip install pyyaml cryptography

# 或使用 requirements.txt
pip install -r requirements.txt
```

#### 3. 配置文件冲突

**警告**: `manager file already exists: .env`

**解决**:
```bash
# 备份旧配置
cp .env .env.backup.$(date +%Y%m%d)

# 重新运行初始化
python init_config.py

# 或强制覆盖
python init_config.py --force
```

#### 4. 环境变量未生效

**问题**: 配置已更新但程序仍使用旧值

**解决**:
```bash
# 重新加载环境变量
source .env

# 或重启终端/IDE
# 或重启服务
docker-compose restart
```

---

## 🛡️ 安全建议

### 敏感信息保护

1. **不要提交 .env 到版本控制**:
```bash
# 确认 .gitignore 包含
echo ".env" >> .gitignore
echo "*.key" >> .gitignore
```

2. **使用密钥管理服务**:
```yaml
# 生产环境配置
security:
  secrets_manager: aws_secrets_manager
  vault_url: https://vault.example.com
```

3. **定期轮换密钥**:
```bash
# 每季度轮换一次
./rotate_keys.sh --quarterly
```

### 配置文件权限

```bash
# 设置配置文件权限
chmod 600 .env
chmod 600 agentos/manager/security/*.yaml
chown $USER:$USER .env agentos/manager/

# 验证权限
ls -la .env agentos/manager/
```

---

## 📝 最佳实践

### 多环境配置管理

```bash
# 为不同环境创建独立配置
python init_config.py --template development --output .env.dev
python init_config.py --template production --output .env.prod
python init_config.py --template testing --output .env.test

# 使用 dotenv 加载对应环境
export AGENTOS_ENV=development
source .env.$AGENTOS_ENV
```

### 配置版本控制

```bash
# 跟踪配置变更
git add agentos/manager/
git commit -m "chore: 初始化配置文件"

# 使用 Git hooks 验证配置
cat > .git/hooks/pre-commit << 'EOF'
#!/bin/bash
if git diff --cached --name-only | grep -q "\.env$"; then
    echo "错误：禁止提交 .env 文件！"
    exit 1
fi
EOF
chmod +x .git/hooks/pre-commit
```

### 自动化初始化

```yaml
# .github/workflows/init.yml
name: Initialize Environment

on: [push, pull_request]

jobs:
  init:
    runs-on: ubuntu-latest
    
    steps:
    - uses: actions/checkout@v3
    
    - name: Setup Python
      uses: actions/setup-python@v4
      with:
        python-version: '3.9'
    
    - name: Initialize manager
      run: |
        cd scripts/init
        python init_config.py --non-interactive --template testing
    
    - name: Verify manager
      run: |
        python -c "import yaml; yaml.safe_load(open('agentos/manager/kernel/settings.yaml'))"
```

---

## 📞 相关文档

- [主 README](../README.md) - 脚本总览
- [构建指南](../build/README.md) - 编译和安装
- [快速入门](../../paper/guides/getting_started.md) - 新手教程

---

## 🤝 贡献

欢迎提交改进建议和新功能！

**Issue 追踪**: https://github.com/SpharxTeam/AgentOS/issues  
**讨论区**: https://github.com/SpharxTeam/AgentOS/discussions

---

© 2026 SPHARX Ltd. All Rights Reserved.

*From data intelligence emerges.*  
*始于数据，终于智能。*


