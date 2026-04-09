# Shell 基础库 (Shell Library)

本目录包含 AgentOS 部署和运维工具链使用的 **Shell 脚本基础库**。

## 📦 库文件说明

### common.sh - 通用工具函数库
**功能**: 提供全局变量定义、路径管理、颜色输出等基础设施

**主要功能:**
```bash
# 全局路径变量
AGENTOS_SCRIPTS_DIR   # scripts 目录绝对路径
AGENTOS_PROJECT_ROOT   # 项目根目录
AGENTOS_CONFIG_DIR     # 配置目录
AGENTOS_LOG_DIR        # 日志目录

# 路径解析函数
agentos_resolve_path()     # 规范化路径
agentos_ensure_dir()       # 确保目录存在（自动创建）
```

**使用示例:**
```bash
source "${LIB_DIR}/common.sh"
echo "项目根目录: ${AGENTOS_PROJECT_ROOT}"
```

---

### log.sh - 统一日志系统
**功能**: 提供结构化的日志记录、错误处理、进度显示等功能

**日志级别:**
| 级别 | 用途 | 颜色 |
|------|------|------|
| `LOG_LEVEL_DEBUG` | 调试信息 | 灰色 |
| `LOG_LEVEL_INFO` | 一般信息 | 绿色 |
| `LOG_LEVEL_WARN` | 警告消息 | 黄色 |
| `LOG_LEVEL_ERROR` | 错误信息 | 红色 |

**核心API:**
```bash
source "${LIB_DIR}/log.sh"

# 日志记录
agentos_log_info "服务启动成功"
agentos_log_warn "内存使用率过高"
agentos_log_error "连接失败"

# 进度显示
agentos_show_progress 5 10 "正在安装依赖..."

# 错误处理
agentos_handle_error $? "部署失败"
agentos_get_error_stats
```

---

### error.sh - 错误码定义体系
**功能**: 定义统一的错误码常量和错误描述映射

**错误码分类:**
| 范围 | 类别 | 示例 |
|------|------|------|
| 0-9 | 成功/通用 | `0=成功`, `1=通用错误` |
| 10-19 | 参数错误 | `10=缺少参数`, `11=参数无效` |
| 20-29 | 环境错误 | `20=Docker未运行`, `21=权限不足` |
| 30-39 | 配置错误 | `30=配置文件缺失`, `31=格式错误` |
| 40-49 | 网络错误 | `40=连接超时`, `41=DNS解析失败` |

**使用示例:**
```bash
source "${LIB_DIR}/error.sh"

# 返回特定错误码
exit ${AGENTOS_ERR_DOCKER_NOT_RUNNING}

# 获取错误描述
description=$(agentos_get_error_description ${AGENTOS_ERR_DOCKER_NOT_RUNNING})
echo "错误: ${description}"
```

---

### platform.sh - 跨平台检测与适配
**功能**: 检测操作系统类型、发行版、架构等平台信息

**支持的平台:**
- ✅ Linux (Ubuntu/Debian/CentOS/Fedora/Arch/Alpine)
- ✅ macOS (Intel + Apple Silicon)
- ✅ Windows (原生 / WSL / Git Bash / MSYS2)

**核心API:**
```bash
source "${LIB_DIR}/platform.sh"

# 平台检测
platform=$(agentos_platform_detect)      # linux/macos/windows
arch=$(agentos_arch_detect)              # x86_64/arm64/aarch64

# 详细信息
distro=$(agentos_linux_distro)           # Ubuntu/Debian/CentOS...
pm=$(agentos_package_manager_detect)    # apt/yum/dnf/brew...

# 系统资源
cores=$(agentos_cpu_count)               # CPU核心数
mem=$(agentos_total_memory)              # 总内存(KB)
```

---

## 🔗 依赖关系图

```
install.sh / install.ps1
    │
    ├── source lib/common.sh     ← 基础设施（必须最先加载）
    ├── source lib/log.sh        ← 日志系统（依赖common.sh）
    ├── source lib/error.sh      ← 错误码（独立）
    └── source lib/platform.sh   ← 平台检测（独立）
    
deploy/*.sh, ci/*.sh, dev/*.sh
    │
    └── 同样引用上述4个库文件
```

## 📖 使用规范

### 加载顺序要求
```bash
# ✅ 正确顺序（common必须在最前）
source "${SCRIPT_DIR}/lib/common.sh"
source "${SCRIPT_DIR}/lib/lib/log.sh"
source "${SCRIPT_DIR}/lib/error.sh"
source "${SCRIPT_DIR}/lib/platform.sh"

# ❌ 错误顺序（会导致变量未定义）
source "${SCRIPT_DIR}/lib/log.sh"  # 缺少 AGENTOS_SCRIPTS_DIR
source "${SCRIPT_DIR}/lib/common.sh"
```

### 编码规范
- **字符编码**: UTF-8 无BOM（已通过.editorconfig强制）
- **行尾格式**: LF（Unix风格）
- **缩进**: 4空格（不使用Tab）
- **注释语言**: 中文（用户面向）+ 英文（API文档）

### 函数命名约定
```bash
# 公共API（可被外部调用）
agentos_xxx_yyy()

# 内部函数（仅限库内部使用）
_agentos_internal_xxx()

# 变量命名
AGENTOS_CONSTANT_VAR    # 全局常量
local_var              # 局部变量
```

## 🛡️ 测试方法

### 单元测试
```bash
cd scripts/tests/shell
./test_framework.sh          # 运行完整测试套件
./test_common_utils.sh       # 仅测试common.sh
```

### 手动验证
```bash
# 测试加载是否成功
for f in common log error platform; do
    bash -n "lib/${f}.sh" && echo "$f ✓" || echo "$f ✗"
done

# 测试乱码检测
grep -r $'\xef\xbf\xbd' lib/ && echo "❌ 发现乱码" || echo "✅ 无乱码"
```

## 📊 版本历史

| 版本 | 日期 | 更新内容 |
|------|------|----------|
| v2.1.0 | 2026-04-08 | 修复23处中文注释乱码，完善文档 |
| v2.0.0 | 2026-04-08 | V2.0重构，从根目录迁移至scripts/lib |
| v1.x.x | 2026-03 | 初始版本 |

## 🤝 贡献指南

修改此目录下的文件时：
1. 保持 UTF-8 编码
2. 不引入新的硬编码路径
3. 新增公共API需更新本文档
4. 通过所有现有测试用例

## 📚 相关文档

- [主README](../README.md) - scripts模块总览
- [重构报告](../REFACTORING_REPORT_V2.1.md) - 本次整理详情
- [AgentOS架构原则](../../agentos/manuals/ARCHITECTURAL_PRINCIPLES.md) - 设计规范
