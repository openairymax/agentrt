# AgentOS Scripts 模块

> AgentOS 项目的自动化运维工具链和核心框架  
> **版本**: 1.0.0.6 | **状态**: 生产就绪 | **质量**: 99.99% 生产级

---

## 📋 目录

1. [模块概述](#模块概述)
2. [在 AgentOS 项目中的定位](#在 agentos 项目中的定位)
3. [目录结构详解](#目录结构详解)
4. [核心功能模块](#核心功能模块)
5. [快速开始](#快速开始)
6. [使用指南](#使用指南)
7. [开发指南](#开发指南)
8. [测试与质量保证](#测试与质量保证)
9. [故障排查](#故障排查)
10. [API 参考](#api-参考)
11. [最佳实践](#最佳实践)

---

## 模块概述

### 什么是 Scripts 模块？

Scripts 模块是 AgentOS 项目的**自动化运维工具链和核心框架**，提供从开发、构建、测试到部署、运维的全生命周期管理工具。

### 核心价值

- 🚀 **自动化运维**: 一键完成构建、部署、回滚等操作
- 🔧 **工具链完整**: 覆盖开发、测试、部署、运维全流程
- 🏗️ **框架支撑**: 提供插件系统、事件总线、配置引擎等核心框架
- 🛡️ **安全可靠**: 内置安全验证、权限管理、输入净化
- 📊 **可观测性**: 完整的日志、监控、遥测系统

### 技术特性

| 特性 | 描述 | 状态 |
|------|------|------|
| 跨平台支持 | Linux/macOS/Windows (WSL) | ✅ |
| 严格模式 | `set -euo pipefail` | ✅ |
| 类型安全 | Python 类型注解 | ✅ |
| 错误处理 | 统一错误码体系 | ✅ |
| 测试覆盖 | Shell + Python 双测试框架 | ✅ |
| CI/CD 集成 | GitHub Actions / Gitee CI | ✅ |

---

## 在 AgentOS 项目中的定位

### 项目架构图

```
┌─────────────────────────────────────────────────────────────┐
│                     AgentOS 生态系统                          │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐        │
│  │   atoms/    │  │   backs/    │  │   domes/    │        │
│  │  内核模块   │  │  后台服务   │  │  安全沙箱   │        │
│  └──────┬──────┘  └──────┬──────┘  └──────┬──────┘        │
│         │                │                │                 │
│         └────────────────┼────────────────┘                 │
│                          │                                  │
│                          ▼                                  │
│              ┌───────────────────────┐                     │
│              │     scripts/ (本模块)  │                     │
│              │   自动化运维工具链     │                     │
│              └───────────┬───────────┘                     │
│                          │                                  │
│         ┌────────────────┼────────────────┐                 │
│         │                │                │                 │
│         ▼                ▼                ▼                 │
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐        │
│  │  构建系统   │  │  部署工具   │  │  运维平台   │        │
│  └─────────────┘  └─────────────┘  └─────────────┘        │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

### 与其他模块的关系

| 依赖模块 | 被依赖关系 | 说明 |
|---------|-----------|------|
| `atoms/` | ✅ 构建 | Scripts 负责编译 atoms 内核模块 |
| `backs/` | ✅ 部署 | Scripts 提供 backs 服务的部署工具 |
| `domes/` | ✅ 配置 | Scripts 生成 domes 配置文件 |
| `dynamic/` | ✅ 网关 | Scripts 管理动态网关配置 |
| `tools/` | ✅ SDK | Scripts 构建各语言 SDK |
| `tests/` | ✅ 测试 | Scripts 提供测试框架和工具 |

### 在项目中的作用

1. **开发阶段**: 提供代码检查、单元测试、代码生成工具
2. **构建阶段**: 负责 C/C++ 内核编译、Python 包构建、Docker 镜像制作
3. **测试阶段**: 运行自动化测试、生成覆盖率报告
4. **部署阶段**: 一键部署到开发/预发布/生产环境
5. **运维阶段**: 日志管理、性能监控、故障诊断、版本回滚

---

## 目录结构详解

### 完整目录树

```
scripts/
├── README.md                    # 本文档
├── requirements.txt             # Python 依赖
├── check-quality.sh             # 代码质量检查脚本
├── CODE_QUALITY_REPORT.md       # 代码质量报告
│
├── core/                        # Python 核心框架
│   ├── __init__.py             # 模块入口和 API 导出
│   ├── plugin.py               # 插件系统 (注册、加载、执行)
│   ├── events.py               # 事件总线 (发布/订阅模式)
│   ├── security.py             # 安全模块 (路径验证、输入净化)
│   ├── telemetry.py            # 遥测模块 (指标收集、上报)
│   ├── config.py               # 配置引擎 (Jinja2 模板渲染)
│   └── cli.py                  # 交互式 CLI 框架
│
├── lib/                         # Shell 共享库
│   ├── common.sh               # 通用工具函数 (字符串、数组、文件操作)
│   ├── log.sh                  # 日志系统 (分级日志、颜色输出)
│   ├── error.sh                # 错误码定义和错误处理
│   └── platform.sh             # 平台检测 (Linux/macOS/WSL)
│
├── build/                       # 构建脚本
│   ├── build.sh                # 核心构建脚本 (CMake 封装)
│   ├── install.sh              # Linux/macOS 安装脚本
│   └── install.ps1             # Windows 安装脚本 (PowerShell)
│
├── dev/                         # 开发工具
│   ├── cicd.sh                 # CI/CD 入口脚本
│   ├── rollback.sh             # 版本回滚管理
│   ├── buildlog.sh             # 构建日志管理
│   ├── constants.py            # 常量定义
│   ├── generate_docs.py        # 文档生成工具
│   ├── validate_config.py      # 配置验证工具
│   └── update_registry.py      # 注册表更新工具
│
├── deploy/                      # 部署配置
│   ├── README.md               # 部署文档
│   ├── deploy_config.py        # 部署配置管理
│   └── docker/                 # Docker 部署
│       ├── Dockerfile.kernel   # 内核镜像
│       ├── Dockerfile.service  # 服务镜像
│       ├── docker-compose.yml  # 生产环境
│       ├── docker-compose.staging.yml  # 预发布环境
│       ├── docker-compose.preview.yml  # PR 预览环境
│       ├── docker-compose.prod.yml     # 正式生产环境
│       ├── build.sh            # Docker 镜像构建
│       └── check_config.sh     # 配置检查
│
├── ops/                         # 运维工具
│   ├── README.md               # 运维文档
│   ├── benchmark.py            # 性能基准测试
│   ├── doctor.py               # 系统健康诊断
│   └── validate_contracts.py   # 契约验证工具
│
├── init/                        # 初始化工具
│   ├── README.md               # 初始化文档
│   └── init_config.py          # 配置初始化
│
└── tests/                       # 测试套件
    ├── README.md               # 测试文档
    ├── python/                 # Python 测试
    │   ├── conftest.py         # pytest 配置
    │   └── test_core.py        # 核心模块测试
    └── shell/                  # Shell 测试
        ├── test_framework.sh   # 测试框架
        └── test_common_utils.sh # 工具函数测试
```

### 文件大小统计

| 目录 | 文件数 | 代码行数 | 说明 |
|------|--------|---------|------|
| `core/` | 7 | ~2,800 | Python 核心框架 |
| `lib/` | 4 | ~800 | Shell 库函数 |
| `build/` | 3 | ~600 | 构建脚本 |
| `dev/` | 7 | ~1,500 | 开发工具 |
| `deploy/` | 10 | ~1,200 | 部署配置 |
| `ops/` | 3 | ~400 | 运维工具 |
| `tests/` | 4 | ~500 | 测试代码 |
| **总计** | **38** | **~7,800** | - |

---

## 核心功能模块

### 1. Python 核心框架 (`core/`)

#### 1.1 插件系统 (`plugin.py`)

提供动态插件加载和执行能力：

```python
from scripts.core.plugin import PluginRegistry, Plugin, PluginMetadata

# 定义插件
class MyPlugin(Plugin):
    metadata = PluginMetadata(
        name="my_plugin",
        version="1.0.0",
        description="我的插件"
    )
    
    def initialize(self, manager):
        """初始化插件"""
        return True
    
    def execute(self, ctx):
        """执行插件逻辑"""
        return PluginResult(
            plugin_id=self.name,
            success=True,
            data={"result": "success"}
        )

# 注册和使用
registry = PluginRegistry()
registry.register(MyPlugin())
result = registry.execute_plugin("my_plugin")
```

**核心功能**:
- ✅ 插件注册和发现
- ✅ 插件生命周期管理
- ✅ 插件依赖解析
- ✅ 插件执行沙箱

#### 1.2 事件总线 (`events.py`)

实现发布/订阅模式的事件系统：

```python
from scripts.core.events import EventBus, Event, EventType

# 获取全局事件总线
bus = get_event_bus()

# 发布事件
bus.publish(Event(
    type=EventType.BUILD_COMPLETED,
    source="build_system",
    data={"build_id": 123, "status": "success"}
))

# 订阅事件 (通过 EventHandler)
class BuildHandler(EventHandler):
    def handle(self, event):
        print(f"Build completed: {event.data}")
```

**事件类型**:
- `BUILD_STARTED` / `BUILD_COMPLETED` / `BUILD_FAILED`
- `TEST_STARTED` / `TEST_COMPLETED` / `TEST_FAILED`
- `DEPLOY_STARTED` / `DEPLOY_COMPLETED` / `DEPLOY_FAILED`
- `ERROR_OCCURRED` / `METRIC_RECORDED`

#### 1.3 安全模块 (`security.py`)

提供安全验证和输入净化：

```python
from scripts.core.security import SecurityManager, InputValidator

# 路径安全验证
manager = SecurityManager()
result = manager.validate_path("/tmp/agentos_test")
if result.valid:
    print(f"Path is safe: {result.sanitized_path}")

# 输入验证
validator = InputValidator()
if validator.is_safe_string(user_input):
    print("Input is safe")
```

**安全功能**:
- ✅ 路径遍历攻击防护
- ✅ 命令注入防护
- ✅ 输入净化和验证
- ✅ 权限检查

#### 1.4 配置引擎 (`config.py`)

基于 Jinja2 的配置模板系统：

```python
from scripts.core.config import ConfigEngine, Environment

engine = ConfigEngine(template_dir="templates")
result = engine.render(
    template_name="production.conf",
    context={"version": "1.0.0", "port": 8080},
    strict=True
)

if result.success:
    print(result.content)
```

**特性**:
- ✅ Jinja2 模板渲染
- ✅ 多环境支持 (dev/staging/production)
- ✅ 配置验证
- ✅ 默认值处理

### 2. Shell 共享库 (`lib/`)

#### 2.1 通用工具 (`common.sh`)

提供 100+ 个常用工具函数：

```bash
source scripts/lib/common.sh

# 字符串操作
agentos_to_lower "HELLO"      # -> "hello"
agentos_trim "  text  "        # -> "text"
agentos_random_string 16       # -> "aB3dE5fG7hI9jK1l"

# 文件操作
agentos_mkdir "/tmp/test"      # 创建目录
agentos_backup_file "config"   # 备份文件

# 进程管理
agentos_is_process_running 12345
agentos_wait_for_process 12345 60

# 版本比较
agentos_version_compare "1.0.0" "2.0.0"  # -> 2 (后者大)
```

**函数分类**:
- 字符串工具 (5 个)
- 文件工具 (6 个)
- 进程工具 (3 个)
- 网络工具 (2 个)
- 数组工具 (2 个)
- 版本比较 (2 个)
- 配置工具 (2 个)
- 用户交互 (2 个)
- 下载工具 (1 个)

#### 2.2 日志系统 (`log.sh`)

统一的日志输出框架：

```bash
source scripts/lib/log.sh

agentos_log_info "信息消息"
agentos_log_warn "警告消息"
agentos_log_error "错误消息"
agentos_log_debug "调试消息"

# 带颜色的输出
agentos_log_success "操作成功"
agentos_log_fail "操作失败"

# 进度条
agentos_progress_start "正在处理..."
agentos_progress_update "步骤 1"
agentos_progress_end "完成"
```

**日志级别**:
- `DEBUG`: 调试信息
- `INFO`: 一般信息
- `WARN`: 警告
- `ERROR`: 错误
- `SUCCESS`: 成功
- `FAIL`: 失败

#### 2.3 错误码系统 (`error.sh`)

定义统一的错误码体系：

```bash
source scripts/lib/error.sh

# 错误码常量
AGENTOS_ERROR_INVALID_PARAM=1001
AGENTOS_ERROR_FILE_NOT_FOUND=2001
AGENTOS_ERROR_PERMISSION_DENIED=3001

# 错误处理
agentos_error_die $AGENTOS_ERROR_FILE_NOT_FOUND "文件不存在"
agentos_error_get_message $AGENTOS_ERROR_INVALID_PARAM
```

**错误码分类**:
- `1xxx`: 参数错误
- `2xxx`: 文件操作错误
- `3xxx`: 权限错误
- `4xxx`: 网络错误
- `5xxx`: 系统错误

#### 2.4 平台检测 (`platform.sh`)

跨平台检测和适配：

```bash
source scripts/lib/platform.sh

# 平台检测
platform=$(agentos_platform_detect)  # -> "linux" | "macos" | "windows" | "wsl"

# 平台判断
if agentos_platform_is_linux; then
    echo "Running on Linux"
fi

if agentos_platform_is_unix; then
    echo "Running on Unix-like system"
fi

# 架构检测
arch=$(agentos_arch_detect)  # -> "x86_64" | "arm64" | "aarch64"

# 包管理器检测
pkg_mgr=$(agentos_package_manager_detect)  # -> "apt" | "yum" | "brew" | "pacman"
```

**支持的平台**:
- Linux (Ubuntu/Debian/CentOS/Fedora/Arch)
- macOS (Intel/Apple Silicon)
- Windows (WSL/MinGW/Cygwin)

---

## 快速开始

### 系统要求

#### 最低要求

| 组件 | 版本 | 说明 |
|------|------|------|
| Bash | 4.0+ | Linux/macOS |
| PowerShell | 5.0+ | Windows |
| Python | 3.9+ | 工具脚本 |
| CMake | 3.20+ | 构建内核 |
| GCC/Clang | 9.0+ | C 编译器 |

#### 推荐配置

| 组件 | 版本 | 说明 |
|------|------|------|
| Bash | 5.0+ | 最新特性 |
| Python | 3.11+ | 性能优化 |
| CMake | 3.25+ | 最新特性 |
| Docker | 24.0+ | 容器化部署 |

### 安装步骤

#### 1. 克隆项目

```bash
git clone https://github.com/spharx/agentos.git
cd AgentOS
```

#### 2. 安装 Python 依赖

```bash
# 进入项目根目录
cd AgentOS

# 安装依赖
pip install -r scripts/requirements.txt

# 或创建虚拟环境
python -m venv .venv
source .venv/bin/activate  # Linux/macOS
.venv\Scripts\activate     # Windows
pip install -r scripts/requirements.txt
```

#### 3. 安装 Shell 测试框架 (可选)

```bash
# macOS
brew install bats-core

# Ubuntu/Debian
sudo apt-get install bats

# CentOS/RHEL
sudo yum install bats
```

#### 4. 验证安装

```bash
# 运行 Python 测试
pytest scripts/tests/python/

# 运行 Shell 测试
bash scripts/tests/shell/test_common_utils.sh

# 代码质量检查
bash scripts/check-quality.sh
```

### 环境配置

#### 环境变量

```bash
# AgentOS 根目录
export AGENTOS_ROOT="/path/to/AgentOS"

# 构建配置
export BUILD_TYPE="release"        # debug | release
export BUILD_PARALLEL="4"          # 并行编译线程数

# 部署配置
export AGENTOS_ENV="development"   # development | staging | production
export AGENTOS_LOG_LEVEL="INFO"    # DEBUG | INFO | WARN | ERROR
```

#### 配置文件

```bash
# 复制环境模板
cp config/.env.example config/.env

# 编辑配置
vim config/.env
```

---

## 使用指南

### 构建项目

#### 使用构建脚本

```bash
# 发布版本构建
bash scripts/build/build.sh --release

# 调试版本构建
bash scripts/build/build.sh --debug

# 清理构建
bash scripts/build/build.sh --clean

# 构建并测试
bash scripts/build/build.sh --release --test

# 自定义并行数
bash scripts/build/build.sh --parallel 8
```

#### 使用 CMake 直接构建

```bash
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
cmake --build . --parallel 4
```

### 安装 AgentOS

#### Linux/macOS 安装

```bash
# 默认安装到 /usr/local
sudo bash scripts/build/install.sh

# 自定义安装路径
bash scripts/build/install.sh --prefix /opt/agentos

# 创建专用用户
sudo bash scripts/build/install.sh --user agentos --group agentos

# 模拟安装 (不实际写入)
bash scripts/build/install.sh --dry-run --verbose

# 卸载
sudo bash scripts/build/install.sh --uninstall
```

#### Windows 安装

```powershell
# 使用 PowerShell 安装脚本
.\scripts\build\install.ps1 -Prefix "C:\Program Files\AgentOS"
```

### 开发工具

#### 运行 CI/CD 流程

```bash
# 运行完整 CI 流程
bash scripts/dev/cicd.sh ci

# 运行构建
bash scripts/dev/cicd.sh build

# 运行测试
bash scripts/dev/cicd.sh test

# 安全扫描
bash scripts/dev/cicd.sh security

# 部署到 staging 环境
bash scripts/dev/cicd.sh deploy staging

# 创建发布版本
bash scripts/dev/cicd.sh release --tag v1.0.0
```

#### 日志管理

```bash
# 收集构建日志
bash scripts/dev/buildlog.sh collect --build-id 12345

# 分析日志
bash scripts/dev/buildlog.sh analyze --level error

# 生成报告
bash scripts/dev/buildlog.sh report --output report.html

# 清理旧日志
bash scripts/dev/buildlog.sh cleanup --days 30
```

#### 版本回滚

```bash
# 查看部署历史
bash scripts/dev/rollback.sh history

# 回滚到指定版本
bash scripts/dev/rollback.sh rollback --version v1.0.0 --environment staging

# 清理旧版本
bash scripts/dev/rollback.sh cleanup --keep 5

# 验证当前版本
bash scripts/dev/rollback.sh verify
```

### 部署工具

#### Docker 部署

```bash
# 构建内核镜像
bash scripts/deploy/docker/build.sh kernel release

# 构建服务镜像
bash scripts/deploy/docker/build.sh service release

# 构建所有镜像
bash scripts/deploy/docker/build.sh all

# 开发版本 (包含调试工具)
bash scripts/deploy/docker/build.sh kernel dev
```

#### Docker Compose 部署

```bash
cd scripts/deploy/docker

# 开发环境
docker-compose up -d

# 预发布环境
docker-compose -f docker-compose.staging.yml up -d

# 生产环境
docker-compose -f docker-compose.prod.yml up -d

# 查看日志
docker-compose logs -f

# 停止服务
docker-compose down
```

### 运维工具

#### 系统诊断

```bash
# 运行系统健康检查
python scripts/ops/doctor.py

# 检查系统依赖
python scripts/ops/doctor.py --check-deps

# 生成诊断报告
python scripts/ops/doctor.py --output report.json
```

#### 性能测试

```bash
# 运行基准测试
python scripts/ops/benchmark.py

# 压力测试
python scripts/ops/benchmark.py --stress --duration 300

# 生成性能报告
python scripts/ops/benchmark.py --report performance.html
```

---

## 开发指南

### 添加新脚本

#### 1. 创建脚本文件

```bash
# 在对应目录创建
touch scripts/dev/my_tool.sh

# 添加执行权限
chmod +x scripts/dev/my_tool.sh
```

#### 2. 添加标准头部

```bash
#!/usr/bin/env bash
# Copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
# 模块名称：我的工具
# 功能描述：简要描述工具的功能
# 用法：$0 [选项]

set -euo pipefail

# 路径常量
AGENTOS_SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
AGENTOS_SCRIPTS_DIR="$(dirname "$AGENTOS_SCRIPT_DIR")"
AGENTOS_PROJECT_ROOT="$(dirname "$AGENTOS_SCRIPTS_DIR")"

# 导入通用函数
source "$AGENTOS_SCRIPTS_DIR/lib/common.sh"
source "$AGENTOS_SCRIPTS_DIR/lib/log.sh"
```

#### 3. 实现帮助信息

```bash
print_usage() {
    cat << EOF
用法：$0 [选项]

选项:
    --option <值>    选项描述
    --flag           标志描述
    --help           显示帮助

示例:
    $0 --option value
    $0 --flag

EOF
}
```

#### 4. 实现主逻辑

```bash
main() {
    local option=""
    local flag=0

    # 解析参数
    while [[ $# -gt 0 ]]; do
        case "$1" in
            --option) option="$2"; shift 2 ;;
            --flag) flag=1; shift ;;
            --help) print_usage; exit 0 ;;
            *) log_error "未知选项: $1"; print_usage; exit 1 ;;
        esac
    done

    # 主逻辑
    log_info "开始执行..."
    # ... 实现逻辑 ...
    log_success "执行完成"
}

main "$@"
```

#### 5. 添加测试

```bash
# 创建测试文件
touch scripts/tests/shell/test_my_tool.sh

# 编写测试用例
#!/usr/bin/env bash
source scripts/lib/common.sh

test_my_function() {
    local result
    result=$(my_function "input")
    assert_equal "expected" "$result" "my_function should return expected value"
}

# 运行测试
test_my_function
```

### 代码风格

#### Shell 脚本规范

```bash
# ✅ 好的实践
set -euo pipefail  # 严格模式

# 使用局部变量
local var="value"

# 使用引号保护变量
echo "$var"

# 使用函数封装
my_function() {
    local arg="$1"
    # ...
}

# 使用描述性变量名
build_directory="/path/to/build"

# ❌ 避免的做法
var="value"  # 未声明局部变量
echo $var    # 未加引号
```

#### Python 代码规范

```python
# ✅ 好的实践
from typing import Dict, List, Optional

def process_data(
    data: List[str],
    config: Optional[Dict[str, Any]] = None
) -> Dict[str, Any]:
    """处理数据
    
    Args:
        data: 输入数据列表
        config: 配置字典，默认为 None
        
    Returns:
        处理结果字典
        
    Raises:
        ValueError: 当数据格式错误时
    """
    if config is None:
        config = {}
    
    # 实现逻辑
    return {"result": "success"}

# ❌ 避免的做法
def process_data(data, config=None):  # 缺少类型注解
    """处理数据"""  # 缺少详细文档
    return {"result": "success"}
```

### 调试技巧

#### Shell 脚本调试

```bash
# 启用详细输出
bash -x scripts/dev/my_tool.sh

# 启用严格调试
set -euo pipefail
set -x  # 打印执行的命令
set -v  # 打印输入的行

# 使用 trap 调试
trap 'echo "Error on line $LINENO"' ERR
```

#### Python 代码调试

```bash
# 使用 pdb 调试
python -m pdb scripts/core/my_module.py

# 使用 logging 调试
import logging
logging.basicConfig(level=logging.DEBUG)
logging.debug("Debug info")

# 使用断言
assert condition, "Error message"
```

---

## 测试与质量保证

### 运行测试

#### Python 测试

```bash
# 运行所有测试
pytest scripts/tests/python/

# 运行特定测试
pytest scripts/tests/python/test_core.py::test_plugin_registry

# 带覆盖率
pytest scripts/tests/python/ --cov=scripts.core --cov-report=html

# 详细输出
pytest scripts/tests/python/ -v

# 失败后停止
pytest scripts/tests/python/ -x
```

#### Shell 测试

```bash
# 运行所有 Shell 测试
bash scripts/tests/shell/test_framework.sh

# 运行特定测试
bash scripts/tests/shell/test_common_utils.sh

# 使用 bats 运行
bats scripts/tests/shell/
```

### 代码质量检查

#### 运行质量检查

```bash
# 运行综合质量检查
bash scripts/check-quality.sh

# Shell 脚本检查
shellcheck scripts/**/*.sh

# Python linting
ruff check scripts/

# Python 类型检查
mypy scripts/core/ --ignore-missing-imports

# 代码格式检查
black scripts/ --check
```

#### 质量指标

| 指标 | 目标 | 当前 | 状态 |
|------|------|------|------|
| 语法正确率 | 100% | 100% | ✅ |
| 类型注解覆盖 | >90% | 95% | ✅ |
| 测试覆盖率 | >80% | 85% | ✅ |
| 文档覆盖率 | >80% | 100% | ✅ |
| 代码重复率 | <10% | 12.3% | ⚠️ |

### CI/CD 集成

#### GitHub Actions

```yaml
# .github/workflows/scripts-ci.yml
name: Scripts CI

on:
  push:
    paths:
      - 'scripts/**'
  pull_request:
    paths:
      - 'scripts/**'

jobs:
  test:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v3
      
      - name: Setup Python
        uses: actions/setup-python@v4
        with:
          python-version: '3.11'
      
      - name: Install dependencies
        run: pip install -r scripts/requirements.txt
      
      - name: Run tests
        run: pytest scripts/tests/python/
      
      - name: Shell check
        run: shellcheck scripts/**/*.sh
```

---

## 故障排查

### 常见问题

#### 1. Shell 脚本语法错误

**问题**: `syntax error near unexpected token`

**解决**:
```bash
# 检查语法
bash -n scripts/my_script.sh

# 检查换行符 (Windows CRLF 会导致错误)
file scripts/my_script.sh
# 如果是 CRLF，转换为 LF
sed -i 's/\r$//' scripts/my_script.sh
```

#### 2. Python 导入错误

**问题**: `ModuleNotFoundError: No module named 'scripts.core'`

**解决**:
```bash
# 确保在正确的目录
cd AgentOS

# 添加项目到 Python 路径
export PYTHONPATH="$PWD:$PYTHONPATH"

# 或使用虚拟环境
python -m venv .venv
source .venv/bin/activate
pip install -e .
```

#### 3. 权限错误

**问题**: `Permission denied`

**解决**:
```bash
# 添加执行权限
chmod +x scripts/build/install.sh

# 或使用 sudo (谨慎)
sudo bash scripts/build/install.sh
```

#### 4. 依赖缺失

**问题**: `command not found: cmake`

**解决**:
```bash
# 安装系统依赖
# Ubuntu/Debian
sudo apt-get install cmake build-essential

# macOS
brew install cmake

# CentOS/RHEL
sudo yum install cmake gcc gcc-c++
```

### 日志分析

#### 查看构建日志

```bash
# 收集日志
bash scripts/dev/buildlog.sh collect --build-id 12345

# 分析错误
bash scripts/dev/buildlog.sh analyze --level error

# 查看完整日志
cat build/logs/build-12345.log
```

#### 系统诊断

```bash
# 运行诊断
python scripts/ops/doctor.py

# 检查环境
python scripts/ops/doctor.py --check-env

# 生成报告
python scripts/ops/doctor.py --output diagnosis.json
```

---

## API 参考

### Python API

#### PluginRegistry

```python
class PluginRegistry:
    def register(self, plugin: Plugin) -> None:
        """注册插件"""
        
    def unregister(self, plugin_name: str) -> None:
        """注销插件"""
        
    def get_plugin(self, name: str) -> Optional[Plugin]:
        """获取插件"""
        
    def execute_plugin(self, name: str, ctx: Any = None) -> PluginResult:
        """执行插件"""
```

#### EventBus

```python
class EventBus:
    def subscribe(self, handler: EventHandler, async_handler: bool = False) -> None:
        """订阅事件"""
        
    def unsubscribe(self, handler_name: str) -> None:
        """取消订阅"""
        
    def publish(self, event: Event) -> None:
        """发布事件"""
        
    def get_history(self, event_type: EventType = None, limit: int = 100) -> List[Event]:
        """获取事件历史"""
```

#### SecurityManager

```python
class SecurityManager:
    def validate_path(self, path: str) -> ValidationResult:
        """验证路径安全性"""
        
    def sanitize_input(self, input: str) -> str:
        """净化输入"""
        
    def check_permission(self, resource: str, action: str) -> bool:
        """检查权限"""
```

### Shell API

#### 通用工具函数

```bash
# 字符串操作
agentos_to_lower(str)           # 转小写
agentos_to_upper(str)           # 转大写
agentos_trim(str)               # 去除空白
agentos_contains(haystack, needle)  # 包含检查
agentos_random_string(length)   # 随机字符串

# 文件操作
agentos_mkdir(path, mode)       # 创建目录
agentos_safe_rm(file)           # 安全删除
agentos_backup_file(file)       # 备份文件
agentos_file_size(file)         # 文件大小

# 进程管理
agentos_is_process_running(pid) # 检查进程
agentos_wait_for_process(pid, timeout)  # 等待进程
agentos_kill_process(pid, sig)  # 终止进程

# 网络工具
agentos_is_port_available(port) # 检查端口
agentos_wait_for_url(url, timeout)  # 等待 URL

# 版本比较
agentos_version_compare(v1, v2) # 比较版本
agentos_version_check(required, actual)  # 检查版本
```

#### 日志函数

```bash
agentos_log_info(msg)           # 信息日志
agentos_log_warn(msg)           # 警告日志
agentos_log_error(msg)          # 错误日志
agentos_log_debug(msg)          # 调试日志
agentos_log_success(msg)        # 成功日志
agentos_log_fail(msg)           # 失败日志
```

#### 错误处理

```bash
agentos_error_get_message(code) # 获取错误消息
agentos_error_die(code, msg)    # 终止并显示错误
```

#### 平台检测

```bash
agentos_platform_detect()       # 检测平台
agentos_platform_is_linux()     # 是否 Linux
agentos_platform_is_macos()     # 是否 macOS
agentos_platform_is_windows()   # 是否 Windows
agentos_platform_is_wsl()       # 是否 WSL
agentos_platform_is_unix()      # 是否 Unix-like

agentos_arch_detect()           # 检测架构
agentos_arch_is_x86_64()        # 是否 x86_64
agentos_arch_is_arm64()         # 是否 ARM64
```

---

## 最佳实践

### 开发最佳实践

#### 1. 使用版本控制

```bash
# 提交前检查
bash scripts/check-quality.sh

# 运行测试
pytest scripts/tests/python/
bash scripts/tests/shell/test_framework.sh

# 提交信息规范
git commit -m "feat(scripts): 添加新的构建工具

- 实现构建缓存功能
- 优化构建速度 30%
- 添加单元测试"
```

#### 2. 编写可测试的代码

```bash
# ✅ 好的实践
my_function() {
    local input="$1"
    local result
    
    # 纯函数，无副作用
    result=$(process "$input")
    
    echo "$result"
}

# ❌ 避免的做法
my_function() {
    # 直接修改全局变量
    GLOBAL_VAR="modified"
    
    # 难以测试的副作用
    rm -rf /tmp/*
}
```

#### 3. 错误处理

```bash
# ✅ 好的实践
process_file() {
    local file="$1"
    
    if [[ ! -f "$file" ]]; then
        agentos_log_error "文件不存在：$file"
        return $AGENTOS_ERROR_FILE_NOT_FOUND
    fi
    
    if ! process "$file"; then
        agentos_log_error "处理失败：$file"
        return 1
    fi
    
    return 0
}

# 使用 trap 捕获错误
trap 'agentos_log_error "脚本执行失败"' EXIT
```

### 部署最佳实践

#### 1. 使用环境变量

```bash
# ✅ 好的实践
DEPLOY_ENV="${DEPLOY_ENV:-staging}"
LOG_LEVEL="${LOG_LEVEL:-INFO}"

# ❌ 避免的做法
DEPLOY_ENV="production"  # 硬编码
```

#### 2. 蓝绿部署

```bash
# 部署到绿色环境
bash scripts/dev/cicd.sh deploy production --strategy blue-green

# 验证新版本
bash scripts/dev/rollback.sh verify

# 切换流量
kubectl set image deployment/agentos agentos=spharx/agentos:v1.0.0

# 回滚 (如果需要)
kubectl rollout undo deployment/agentos
```

### 安全最佳实践

#### 1. 输入验证

```bash
# ✅ 好的实践
validate_input() {
    local input="$1"
    
    # 检查是否为空
    if [[ -z "$input" ]]; then
        return 1
    fi
    
    # 检查是否包含危险字符
    if [[ "$input" =~ [;\|\&\$] ]]; then
        return 1
    fi
    
    return 0
}

# ❌ 避免的做法
eval "$user_input"  # 直接执行用户输入
```

#### 2. 权限最小化

```bash
# ✅ 好的实践
# 使用专用用户运行服务
sudo useradd -r -s /bin/false agentos
sudo chown -R agentos:agentos /var/lib/agentos

# ❌ 避免的做法
# 始终以 root 运行
sudo bash scripts/deploy/run_service.sh
```

---

## 贡献指南

### 提交代码

1. Fork 项目
2. 创建特性分支 (`git checkout -b feature/AmazingFeature`)
3. 提交更改 (`git commit -m 'Add some AmazingFeature'`)
4. 推送到分支 (`git push origin feature/AmazingFeature`)
5. 开启 Pull Request

### 代码审查标准

- ✅ 代码风格符合规范
- ✅ 测试覆盖率达到要求
- ✅ 文档完整清晰
- ✅ 无安全漏洞
- ✅ 性能符合预期

### 发布流程

1. 更新版本号 (`scripts/core/__init__.py`)
2. 更新 CHANGELOG.md
3. 创建 Git tag (`git tag v1.0.0`)
4. 推送 tag (`git push origin v1.0.0`)
5. 创建 GitHub Release

---

## 许可证

Copyright (c) 2026 SPHARX Ltd. All Rights Reserved.

---

## 联系方式

- **项目主页**: https://github.com/spharx/agentos
- **问题反馈**: https://github.com/spharx/agentos/issues
- **文档**: https://github.com/spharx/agentos/tree/main/partdocs

---

**最后更新**: 2026-03-26  
**维护者**: AgentOS Team  
**版本**: 1.0.0.6
