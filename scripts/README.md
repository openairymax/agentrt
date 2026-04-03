# AgentOS Scripts - 开发与运维工具集

**版本**: v1.0.0.6  
**最后更新**: 2026-03-29  

---

## 📋 概述

`scripts/` 模块是 AgentOS 的开发、构建、部署和运维工具集，提供全生命周期的自动化脚本支持。遵循 AgentOS 架构设计原则，实现**反馈闭环**、**安全内生**、**工程美学**和**跨平台一致性**。

### 核心能力

- 🏗️ **构建自动化** - CMake 构建、Docker 镜像打包、多环境配置
- 🧪 **测试框架** - Shell/Python 双语言测试、CI/CD 集成、覆盖率统计
- 🚀 **部署工具** - 多环境部署、蓝绿发布、一键回滚
- 🔍 **运维监控** - 健康检查、性能基准、日志管理、安全扫描
- 🔌 **插件系统** - 动态插件加载、事件总线、可扩展架构
- 🌍 **跨平台支持** - Windows/macOS/Linux/WSL 全平台兼容

---

## 📁 模块结构

```
scripts/
├── lib/                      # Shell 核心库
│   ├── common.sh            # 通用工具函数（路径、字符串、数组）
│   ├── log.sh               # 统一日志系统（颜色、级别、TraceID）
│   ├── error.sh             # 错误码定义（通用/构建/安装/Docker）
│   └── platform.sh          # 跨平台检测（系统识别、架构检测）
│
├── core/                     # Python 核心模块
│   ├── __init__.py          # 模块入口（版本：1.0.0.6）
│   ├── plugin.py            # 插件系统（动态加载、元数据管理）
│   ├── events.py            # 事件总线（同步/异步、优先级、追踪）
│   ├── security.py          # 安全管理（输入验证、权限控制）
│   ├── telemetry.py         # 遥测收集（指标、追踪、日志）
│   ├── config.py            # 配置管理（多环境、热加载）
│   └── cli.py               # 命令行接口（参数解析、帮助生成）
│
├── build/                    # 构建脚本
│   ├── build.sh             # 核心构建（CMake 配置、并行构建）
│   └── ...
│
├── dev/                      # 开发工具
│   ├── cicd.sh              # CI/CD 入口（代码检查、测试、安全扫描）
│   ├── rollback.sh          # 版本回滚（历史记录、蓝绿切换）
│   └── buildlog.sh          # 构建日志管理（收集、分析、报告）
│   ├── dev/                   # 开发辅助
│   │   ├── config/           # 开发配置（clang-format, vcpkg.json 等）
│   │   ├── quickstart.sh     # 快速入门（一键启动）
│   │   └── validate.sh       # 环境验证
│
├── ops/                      # 运维工具
│   ├── doctor.py            # 健康检查（系统/依赖/网络/Docker）
│   ├── benchmark.py         # 性能基准（IPC/内存/上下文切换）
│   └── validate_contracts.py # 契约验证
│
├── tests/                    # 测试框架
│   ├── shell/               # Shell 测试
│   │   ├── test_framework.sh    # 测试框架（断言函数）
│   │   └── test_common_utils.sh # 工具测试
│   ├── python/              # Python 测试
│   │   ├── conftest.py          # pytest 配置
│   │   └── test_core.py         # 核心模块测试
│   └── README.md            # 测试指南
│
└── init/                     # 初始化配置
    ├── init_config.py       # 配置初始化（环境检测、模板渲染）
    └── README.md            # 初始化指南
```

---

## 🚀 快速开始

### 1. 环境准备

```bash
# 克隆仓库
git clone https://github.com/SpharxTeam/AgentOS.git
cd AgentOS

# 运行初始化向导
cd scripts/init
python init_config.py
```

### 2. 构建项目

```bash
# 开发构建
cd scripts/build
bash build.sh dev

# 生产构建
bash build.sh release

# 查看构建选项
bash build.sh --help
```

### 3. 运行测试

```bash
# Python 测试
cd scripts/tests/python
pytest -v

# Shell 测试
cd scripts/tests/shell
for test in test_*.sh; do bash "$test"; done
```

### 4. Docker 部署

```bash
# 快速入门（一键完成）
cd scripts/deploy/docker
bash quickstart.sh

# 手动构建镜像
bash build.sh all release

# 启动服务
docker-compose up -d
```

---

## 📚 核心功能详解

### 🔧 lib/ - Shell 核心库

#### common.sh - 通用工具

提供路径管理、依赖加载、字符串处理等基础函数：

```bash
# 加载库文件
source "$AGENTOS_SCRIPTS_DIR/lib/common.sh"
agentos_load_libs  # 自动加载 log.sh, error.sh, platform.sh

# 路径常量
AGENTOS_SCRIPT_DIR        # 当前脚本目录
AGENTOS_SCRIPTS_DIR       # scripts 模块目录
AGENTOS_PROJECT_ROOT      # 项目根目录
```

#### log.sh - 统一日志

```bash
# 日志级别
LOG_LEVEL_DEBUG=0
LOG_LEVEL_INFO=1
LOG_LEVEL_WARN=2
LOG_LEVEL_ERROR=3

# 日志函数
log_info "这是一条信息"
log_error "发生错误"
log_debug "调试信息（仅 DEBUG 级别显示）"

# 带 TraceID 的日志
export AGENTOS_TRACE_ID="build-12345"
log_info "构建开始"  # 自动附加 TraceID
```

#### error.sh - 错误码体系

```bash
# 通用错误码
AGENTOS_ERR_GENERAL=1000
AGENTOS_ERR_INVALID_ARGS=1001

# 构建错误码
AGENTOS_ERR_BUILD_FAILED=2001
AGENTOS_ERR_CMAKE_NOT_FOUND=2002

# Docker 错误码
AGENTOS_ERR_DOCKER_NOT_FOUND=4001
AGENTOS_ERR_DOCKER_NOT_RUNNING=4002
```

#### platform.sh - 跨平台检测

```bash
# 平台识别
detect_platform  # 返回：linux/macos/windows/wsl

# 架构检测
detect_arch  # 返回：x86_64/aarch64/x86_32

# 平台特定逻辑
case "$(detect_platform)" in
    linux)
        # Linux 特定操作
        ;;
    windows)
        # Windows 特定操作
        ;;
esac
```

---

### 🐍 core/ - Python 核心模块

#### plugin.py - 插件系统

支持动态插件发现、加载和执行：

```python
from scripts.core import PluginRegistry, Plugin, PluginMetadata

# 获取全局注册表
registry = get_registry()

# 发现插件
plugins = registry.discover_plugins("./plugins")

# 加载插件
plugin = registry.load_plugin_from_module("my_plugin.py", "MyPlugin")

# 执行插件
result = registry.execute_plugin("my_plugin", {"param": "value"})

# 注册钩子
registry.register_hook("pre_execute", lambda p, ctx: print(f"执行前：{p.name}"))
```

#### events.py - 事件总线

统一的事件处理架构，支持同步/异步、优先级、分布式追踪：

```python
from scripts.core import EventBus, Event, EventType, EventPriority

# 获取全局事件总线
bus = get_event_bus()

# 创建事件处理器
class BuildHandler(EventHandler):
    def __init__(self):
        super().__init__("build_monitor", [EventType.BUILD_COMPLETED])
    
    def handle(self, event: Event) -> bool:
        print(f"构建完成：{event.data.get('version')}")
        return True

# 订阅事件
bus.subscribe(BuildHandler())

# 发布事件
event = Event(
    type=EventType.BUILD_COMPLETED,
    source="build.sh",
    data={"version": "1.0.0.6", "status": "success"},
    priority=EventPriority.HIGH
)
bus.publish(event)

# 查询事件历史
history = bus.get_history(EventType.BUILD_COMPLETED, limit=10)
```

---

### 🏗️ dev/ - 开发工具

#### cicd.sh - CI/CD 入口

统一触发 CI/CD 各阶段任务：

```bash
# 运行完整 CI 流程
bash cicd.sh ci

# 只运行测试
bash cicd.sh test

# 安全扫描
bash cicd.sh security

# 构建
bash cicd.sh build --version v1.0.0 --type release

# 部署到 staging 环境
bash cicd.sh deploy staging

# 回滚生产环境
bash cicd.sh rollback production --version v1.0.0
```

**CI 流程包含**：
1. 代码质量检查（Shell/Python 语法）
2. ShellCheck 静态分析
3. Python Lint（Ruff/Flake8）
4. 单元测试（Shell/Python）
5. 安全扫描（Bandit/TruffleHog）

#### rollback.sh - 版本回滚

提供版本回滚和部署历史管理：

```bash
# 查看部署历史
bash rollback.sh history

# 回滚到指定版本
bash rollback.sh rollback --version v1.0.0 --environment production

# 清理旧版本（保留最近 5 个）
bash rollback.sh cleanup --keep 5

# 验证当前版本
bash rollback.sh verify --version v1.0.0
```

#### buildlog.sh - 构建日志管理

统一收集、分析和报告构建日志：

```bash
# 收集构建日志
bash buildlog.sh collect --build-id 12345

# 分析错误日志
bash buildlog.sh analyze --level error --file build.log

# 生成 HTML 报告
bash buildlog.sh report --output build-report.html

# 归档日志
bash buildlog.sh archive --build-id 12345

# 清理过期日志（30 天前）
bash buildlog.sh cleanup --days 30

# 实时查看日志
bash buildlog.sh tail --file build.log

# 搜索日志
bash buildlog.sh search --pattern "ERROR.*timeout"
```

---

### 🚀 deploy/ - 部署配置

#### docker/build.sh - Docker 镜像构建

```bash
# 构建内核镜像（生产版）
bash build.sh kernel release

# 构建服务镜像（开发版）
bash build.sh service dev

# 构建所有镜像
bash build.sh all release

# 清理旧镜像
bash build.sh --cleanup
```

**镜像类型**：
- `spharx/agentos-kernel` - 微内核镜像（最小化运行时）
- `spharx/agentos-services` - 服务层镜像（包含所有服务）

#### docker/quickstart.sh - 快速入门

一键完成环境检查、镜像构建和服务启动：

```bash
bash quickstart.sh
```

**自动完成**：
1. ✅ Docker 环境检查
2. ✅ Docker Compose 检查
3. ✅ 端口占用检查（8080-8084）
4. ✅ 环境变量配置（.env 文件）
5. ✅ 镜像构建（生产版/开发版可选）
6. ✅ 服务启动
7. ✅ 健康检查
8. ✅ 显示访问信息

**服务访问**：
- LLM 服务：http://localhost:8080
- 工具服务：http://localhost:8081
- 市场服务：http://localhost:8082
- 调度服务：http://localhost:8083
- 监控服务：http://localhost:8084
- Jaeger UI：http://localhost:16686
- Prometheus：http://localhost:8888/metrics

---

### 🔍 ops/ - 运维工具

#### doctor.py - 系统健康检查

全面检查 AgentOS 运行环境：

```bash
# 运行完整检查
python doctor.py

# 详细输出
python doctor.py --verbose

# 输出 JSON 格式
python doctor.py --output json

# 只检查特定项
python doctor.py --check docker,network,disk

# 自动修复可修复的问题
python doctor.py --fix
```

**检查项目**：
- ✅ 系统信息（OS、架构）
- ✅ Python 版本（最低 3.8）
- ✅ 依赖命令（bash, cmake, make, gcc）
- ✅ 网络连接（DNS、互联网）
- ✅ 磁盘空间（最低 1GB）
- ✅ 配置文件（agentos.conf 等）
- ✅ Docker 环境（可选）
- ✅ 安全扫描（敏感文件）

#### benchmark.py - 性能基准测试

测试 AgentOS 核心组件性能：

```bash
# 运行完整基准测试
python benchmark.py

# 自定义迭代次数
python benchmark.py --iterations 10000

# 只运行 IPC 测试
python benchmark.py --suite ipc

# 输出 JSON 结果
python benchmark.py --format json --output results.json

# 输出 CSV 结果
python benchmark.py --format csv --output results.csv
```

**测试套件**：
- IPC 延迟测试
- 内存分配性能（1KB/10KB）
- 上下文切换（深度 10/50）
- 任务调度（100/1000 任务）
- 字符串操作
- JSON 解析

---

### 🧪 tests/ - 测试框架

#### Shell 测试框架

提供丰富的断言函数：

```bash
#!/usr/bin/env bash
source "../lib/test_framework.sh"

# 断言函数
assert_true "$result" "结果应为真"
assert_false "$result" "结果应为假"
assert_equal "$actual" "$expected" "值应相等"
assert_contains "$string" "$substring" "字符串应包含子串"
assert_file_exists "/path/to/file" "文件应存在"
assert_dir_exists "/path/to/dir" "目录应存在"
assert_command_exists "docker" "命令应存在"
assert_not_empty "$array" "数组不应为空"

# 运行测试
test_example() {
    local result=$(my_function)
    assert_equal "$result" "expected" "函数返回值正确"
}

# 执行测试套件
run_tests
```

#### Python 测试框架

使用 pytest，支持 fixtures、parametrize、async：

```python
# conftest.py
import pytest

@pytest.fixture
def sample_data():
    return {"key": "value"}

@pytest.fixture
def plugin_registry():
    return get_registry()

# test_core.py
def test_plugin_loading(plugin_registry):
    plugin = plugin_registry.load_plugin("test_plugin")
    assert plugin is not None
    assert plugin.state == PluginState.LOADED

@pytest.mark.asyncio
async def test_async_event_bus():
    bus = get_event_bus()
    bus.start_async_processing()
    # ... 测试异步逻辑
```

**运行测试**：
```bash
# 运行所有测试
pytest scripts/tests/python/ -v

# 带覆盖率
pytest scripts/tests/python/ --cov=scripts/core --cov-report=html

# 只运行特定测试
pytest scripts/tests/python/test_core.py::TestPluginRegistry -v
```

---

### ⚙️ init/ - 初始化配置

#### init_config.py - 配置初始化

交互式配置向导，生成所有必需配置文件：

```bash
# 交互式初始化
python init_config.py

# 非交互式（使用默认配置）
python init_config.py --non-interactive

# 指定配置模板
python init_config.py --template production

# 强制覆盖现有配置
python init_config.py --force
```

**配置流程**：
1. 环境检测（OS、Python、CMake）
2. 选择配置模板（development/production/testing）
3. 数据库配置（SQLite/PostgreSQL/MySQL）
4. LLM 服务配置（OpenAI/Anthropic/DeepSeek/本地）
5. 安全配置（JWT Secret、Session Key、Encryption Key）
6. 目录结构创建
7. 配置文件生成

**生成的配置文件**：
- `.env` - 环境变量
- `manager/kernel/settings.yaml` - 内核配置
- `manager/security/policy.yaml` - 安全策略
- `manager/logging/manager.yaml` - 日志配置

---

## 🛡️ 安全特性

### 安全扫描集成

```bash
# Bandit Python 代码扫描
bandit -r scripts/core/ -f json -o security-report.json

# ShellCheck Shell 脚本扫描
shellcheck scripts/**/*.sh -S warning

# TruffleHog 秘钥扫描
trufflehog filesystem scripts/ --json > secrets-report.json

# Safety 依赖漏洞扫描
safety check --json
```

### 安全最佳实践

1. **敏感信息保护**
   ```bash
   # .gitignore 配置
   echo ".env" >> .gitignore
   echo "*.key" >> .gitignore
   echo "*.pem" >> .gitignore
   ```

2. **配置文件权限**
   ```bash
   chmod 600 .env
   chmod 600 manager/security/*.yaml
   ```

3. **定期轮换密钥**
   ```bash
   # 每季度轮换
   ./rotate_keys.sh --quarterly
   ```

---

## 📊 CI/CD 集成

### GitHub Actions 工作流

`.github/workflows/scripts-ci.yml` 包含：

```yaml
name: Scripts CI/CD

on: [push, pull_request]

jobs:
  ci:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v3
      
      - name: Setup Python
        uses: actions/setup-python@v4
        with:
          python-version: '3.9'
      
      - name: Code Quality
        run: bash scripts/dev/cicd.sh ci
      
      - name: Security Scan
        run: bash scripts/dev/cicd.sh security
      
      - name: Build
        run: bash scripts/dev/cicd.sh build
      
      - name: Test
        run: bash scripts/dev/cicd.sh test
```

### 部署流程

```bash
# 开发环境
bash cicd.sh deploy dev

# 预发布环境
bash cicd.sh deploy staging

# PR 预览环境
bash cicd.sh deploy preview

# 生产环境（蓝绿部署）
bash cicd.sh deploy production
```

---

## 🔍 故障排查

### 常见问题

#### 1. Shell 脚本权限问题

**错误**: `Permission denied`

**解决**:
```bash
chmod +x scripts/**/*.sh
```

#### 2. Python 依赖缺失

**错误**: `ModuleNotFoundError`

**解决**:
```bash
pip install -r requirements.txt
```

#### 3. Docker 构建失败

**错误**: `Docker not found` 或 `Docker not running`

**解决**:
```bash
# 检查 Docker
docker --version
docker info

# 启动 Docker（Linux）
sudo systemctl start docker

# 启动 Docker Desktop（macOS/Windows）
```

#### 4. 跨平台路径问题

**错误**: 路径分隔符不正确

**解决**:
```bash
# 使用 platform.sh 自动检测
source scripts/lib/platform.sh
case "$(detect_platform)" in
    windows) PATH_SEP="\\" ;;
    *) PATH_SEP="/" ;;
esac
```

---

## 📝 最佳实践

### 1. 脚本开发规范

```bash
#!/usr/bin/env bash
# 所有 Shell 脚本应包含：
# - Shebang 行
# - Copyright 声明
# - set -euo pipefail（严格模式）
# - 帮助信息函数
# - 错误处理

set -euo pipefail

# 加载库
source "$(dirname "$0")/lib/common.sh"

# 主函数
main() {
    log_info "脚本开始"
    # ... 逻辑
    log_success "脚本完成"
}

main "$@"
```

### 2. Python 代码规范

```python
#!/usr/bin/env python3
# Copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
"""模块文档字符串"""

import ...

def main():
    """主函数文档"""
    # ... 逻辑
    pass

if __name__ == "__main__":
    sys.exit(main())
```

### 3. 日志记录

```bash
# 使用统一日志系统
source scripts/lib/log.sh

log_info "信息级别"
log_warn "警告级别"
log_error "错误级别"
log_debug "调试级别（仅 DEBUG 模式显示）"
```

### 4. 错误处理

```bash
# 使用统一错误码
source scripts/lib/error.sh

if ! command -v docker &> /dev/null; then
    agentos_exit "$AGENTOS_ERR_DOCKER_NOT_FOUND" "Docker 未安装"
fi
```

---

## 📞 相关文档

- [架构设计原则](../manuals/architecture/ARCHITECTURAL_PRINCIPLES.md)
- [快速入门](../paper/guides/getting_started.md)
- [API 文档](../apis/README.md)
- [运维手册](../manuals/ops/README.md)

---

## 🤝 贡献

欢迎提交改进建议和新功能！

**Issue 追踪**: https://github.com/SpharxTeam/AgentOS/issues  
**讨论区**: https://github.com/SpharxTeam/AgentOS/discussions

---

© 2026 SPHARX Ltd. All Rights Reserved.

*From data intelligence emerges.*  
*始于数据，终于智能。*
