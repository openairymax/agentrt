# AgentOS 开发辅助脚本

**版本**: v1.0.0.7  
**最后更新**: 2026-04-04  

---

## 📋 概述

`scripts/development/` 目录包含 AgentOS 开发过程中的辅助工具，提供文档生成、注册表更新、示例运行等功能。

### 配置管理架构

本项目采用**配置分离管理**原则：

```
AgentOS/
├── agentos/manager/                    # ✅ 运行时业务配置中心
│   ├── kernel/settings.yaml    # 内核配置
│   ├── model/model.yaml        # 模型配置
│   ├── security/policy.yaml    # 安全配置
│   └── ...                     # 所有运行时配置
│
├── scripts/development/config/         # ✅ 开发工具统一配置
│   ├── .clang-format           # C/C++ 代码格式化
│   ├── .clang-tidy             # C/C++ 静态分析
│   ├── .clangd                 # clangd 语言服务器
│   ├── .editorconfig           # 编辑器配置
│   ├── .lizardrc               # 圈复杂度检查
│   ├── .jscpd.json             # 代码重复检测
│   └── .pre-commit-config.yaml # git pre-commit 钩子
│
└── agentos/cupolas/                    # ✅ 模块特定工具
    ├── Doxyfile                # API 文档生成配置
    └── generate_api_docs.bat   # Windows 一键生成脚本
```

**重要说明**：
- ❌ **模块根目录不再保留开发工具配置**（如 `agentos/atoms/.clang-format`、`agentos/gateway/.clang-tidy`）
- ✅ **所有开发工具配置统一到 `scripts/development/config/`**，避免重复和冲突
- ✅ **Manager 模块只管理运行时业务配置**（YAML/JSON 格式）
- ✅ **模块特定工具保留在模块内**（如 cupolas 的 Doxygen 配置）

---

## 📁 文件清单

### 开发脚本

| 文件 | 说明 | 类型 | 状态 |
|------|------|------|------|
| `quickstart.sh` | 一键快速启动脚本 | Bash | ✅ 生产就绪 |
| `validate.sh` | 环境验证脚本 | Bash | ✅ 生产就绪 |

### 开发配置

| 文件 | 说明 | 类型 | 状态 |
|------|------|------|------|
| `config/.clang-format` | C/C++ 代码格式化配置 | Config | ✅ 生产就绪 |
| `config/.clang-tidy` | C/C++ 静态分析配置 | Config | ✅ 生产就绪 |
| `config/.clangd` | Clangd 语言服务器配置 | Config | ✅ 生产就绪 |
| `config/.editorconfig` | 编辑器统一配置 | Config | ✅ 生产就绪 |
| `config/.lizardrc` | 圈复杂度检查配置 | Config | ✅ 生产就绪 |
| `config/.jscpd.json` | 重复代码检测配置 | Config | ✅ 生产就绪 |
| `config/.pre-commit-config.yaml` | Git 钩子配置 | Config | ✅ 生产就绪 |
| `config/vcpkg.json` | C++ 依赖配置 (Windows) | Config | ✅ 生产就绪 |

> 📌 **注意**: 
> - 这些配置文件原位于项目根目录或模块根目录，已统一迁移到 `scripts/development/config/` 目录
> - **模块根目录（如 agentos/atoms/、agentos/gateway/）不再保留 .clang-format 和 .clang-tidy**
> - 使用工具时，工具会自动从 `scripts/development/config/` 读取配置

---

## 🔧 开发工具配置使用指南

### 统一配置说明

所有开发工具配置统一位于 `scripts/development/config/` 目录，各工具会自动从该目录读取配置。

#### 1. clang-format - 代码格式化

**用途**: 统一 C/C++ 代码格式，保持代码风格一致

**使用方法**:
```bash
# 格式化单个文件
clang-format -i --style=file:scripts/development/config/.clang-format src/main.c

# 格式化整个目录
find agentos/atoms/ -name "*.c" -o -name "*.h" | xargs clang-format -i --style=file:scripts/development/config/.clang-format

# 检查格式（不修改）
clang-format --dry-run --Werror --style=file:scripts/development/config/.clang-format src/main.c
```

**VS Code 集成**:
```json
// .vscode/settings.json
{
  "C_Cpp.formatting": "clangFormat",
  "C_Cpp.clang_format_fallbackStyle": "file:scripts/development/config/.clang-format"
}
```

**配置说明**:
```yaml
# scripts/development/config/.clang-format
Language: Cpp
BasedOnStyle: LLVM
IndentWidth: 4              # 缩进 4 空格
ColumnLimit: 120            # 每行最大 120 字符
BreakBeforeBraces: Attach   # 大括号风格
PointerAlignment: Right     # 指针右对齐
```

#### 2. clang-tidy - 静态分析

**用途**: 检查代码质量问题、安全隐患、现代化建议

**使用方法**:
```bash
# 分析单个文件
clang-tidy src/main.c -- -I include -std=c11

# 分析整个项目（使用 compile_commands.json）
clang-tidy -p build src/main.c

# 自动修复问题
clang-tidy -fix src/main.c -- -I include -std=c11
```

**配置说明**:
```yaml
# scripts/development/config/.clang-tidy
Checks: >
  -*,
  bugprone-*,
  cert-*,
  cppcoreguidelines-*,
  modernize-*,
  performance-*,
  readability-*
CheckOptions:
  - key: readability-identifier-naming.NamespaceCase
    value: lower_case
  - key: readability-function-size.LineThreshold
    value: 100
```

#### 3. clangd - 语言服务器

**用途**: 提供代码补全、跳转、诊断等 IDE 功能

**使用方法**:
```bash
# 生成 compile_commands.json（clangd 必需）
cmake -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -B build

# 复制 compile_commands.json 到项目根目录
cp build/compile_commands.json .

# clangd 会自动读取 scripts/development/config/.clangd 配置
```

**VS Code 集成**:
```json
// .vscode/settings.json
{
  "clangd.path": "clangd",
  "clangd.arguments": [
    "--config=scripts/development/config/.clangd"
  ]
}
```

#### 4. EditorConfig - 编辑器统一配置

**用途**: 统一不同编辑器的代码风格（缩进、编码、换行等）

**使用方法**:
```bash
# VS Code 安装 EditorConfig 插件后自动生效

# 检查配置
editorconfig-checker
```

**配置说明**:
```ini
# scripts/development/config/.editorconfig
root = true

[*]
charset = utf-8
end_of_line = lf
insert_final_newline = true
trim_trailing_whitespace = true
indent_style = space
indent_size = 4

[*.md]
trim_trailing_whitespace = false
```

#### 5. lizard - 圈复杂度检查

**用途**: 检测函数复杂度，确保代码可维护性

**使用方法**:
```bash
# 检查单个文件
lizard src/main.c

# 检查整个项目
lizard agentos/atoms/ agentos/gateway/ agentos/commons/

# 生成 XML 报告
lizard --xml > lizard-report.xml

# 使用项目配置
lizard -c scripts/development/config/.lizardrc agentos/atoms/
```

**配置说明**:
```json
# scripts/development/config/.lizardrc
{
  "CCN": 15,              // 最大圈复杂度
  "length": 50,           // 最大函数行数
  "arguments": 5          // 最大参数数量
}
```

#### 6. jscpd - 代码重复检测

**用途**: 检测代码重复率，避免复制粘贴代码

**使用方法**:
```bash
# 检查重复代码
jscpd --config scripts/development/config/.jscpd.json

# 指定目录
jscpd "agentos/atoms/**/*.c" "agentos/gateway/**/*.c" --config scripts/development/config/.jscpd.json

# 生成 HTML 报告
jscpd --report html --config scripts/development/config/.jscpd.json
```

**配置说明**:
```json
# scripts/development/config/.jscpd.json
{
  "threshold": 15,        // 重复率阈值（%）
  "reporters": ["html", "console"],
  "ignore": [
    "**/tests/**",
    "**/build/**"
  ]
}
```

#### 7. pre-commit - Git 钩子

**用途**: 在 commit 前自动执行代码检查

**使用方法**:
```bash
# 安装 pre-commit 钩子
pre-commit install --config scripts/development/config/.pre-commit-config.yaml

# 手动运行所有检查
pre-commit run --all-files --config scripts/development/config/.pre-commit-config.yaml

# 检查特定文件
pre-commit run --files src/main.c --config scripts/development/config/.pre-commit-config.yaml
```

**配置说明**:
```yaml
# scripts/development/config/.pre-commit-config.yaml
repos:
  - repo: local
    hooks:
      - id: clang-format
        name: clang-format
        entry: clang-format -i
        language: system
        files: \.(c|h|cpp|hpp)$
      
      - id: clang-tidy
        name: clang-tidy
        entry: clang-tidy
        language: system
        files: \.(c|h|cpp|hpp)$
      
      - id: lizard
        name: lizard
        entry: lizard
        language: system
        files: \.(c|h|cpp|hpp)$
```

---

## 🔧 工具详解

### 1. generate_docs.py - 文档生成器

**用途**: 自动生成 AgentOS API 文档和技术文档

**功能特性**:
- Doxygen 文档提取
- Markdown 格式转换
- API 参考手册生成
- 架构图自动绘制
- 代码示例整合

**使用方法**:

```bash
cd scripts/development

# 生成完整文档
python generate_docs.py --all

# 只生成 API 文档
python generate_docs.py --api

# 只生成架构文档
python generate_docs.py --architecture

# 指定输出目录
python generate_docs.py --output ../paper/generated
```

**生成内容**:

```
paper/generated/
├── api/                    # API 参考手册
│   ├── corekern/          # 微内核 API
│   ├── coreloopthree/     # 运行时 API
│   └── memoryrovol/       # 记忆系统 API
├── architecture/          # 架构文档
│   ├── overview.md        # 总体架构
│   └── diagrams/          # 架构图
└── examples/              # 代码示例
    ├── cpp/              # C++ 示例
    ├── python/           # Python 示例
    └── go/               # Go 示例
```

**配置选项**:

```yaml
# docs_config.yaml
generator:
  format: markdown
  theme: default
  language: zh-CN
  
doxygen:
  extract_all: true
  extract_private: false
  warnings_as_errors: true
  
output:
  directory: ../paper/generated
  clean_before: true
  include_examples: true
```

---

### 2. update_registry.py - 注册表更新工具

**用途**: 维护和更新 AgentOS 组件注册信息

**功能特性**:
- Agent 注册表更新
- Skill 注册表同步
- 服务发现配置
- 版本兼容性检查

**使用方法**:

```bash
cd scripts/development

# 更新所有注册表
python update_registry.py --all

# 只更新 Agent 注册表
python update_registry.py --agents

# 只更新 Skill 注册表
python update_registry.py --skills

# 验证注册表完整性
python update_registry.py --verify
```

**注册表结构**:

```yaml
# agentos/manager/agent/registry.yaml
agents:
  - id: agent_architect_001
    name: 架构师智能体
    version: 1.0.0
    skills:
      - skill_design_pattern
      - skill_code_review
    status: active
    
  - id: agent_developer_002
    name: 开发者智能体
    version: 1.0.0
    skills:
      - skill_coding
      - skill_testing
    status: active

# agentos/manager/skill/registry.yaml
skills:
  - id: skill_design_pattern
    name: 设计模式
    category: development
    version: 1.0.0
    
  - id: skill_coding
    name: 代码编写
    category: development
    version: 1.0.0
```

---

### 3. run_example.sh - 示例运行脚本

**用途**: 快速运行 AgentOS 示例程序

**功能特性**:
- 一键运行示例
- 自动环境检查
- 依赖项验证
- 结果展示

**使用方法**:

```bash
cd scripts/development

# 运行所有示例
./run_example.sh --all

# 运行特定示例
./run_example.sh hello_world
./run_example.sh task_management
./run_example.sh memory_operations

# 列出可用示例
./run_example.sh --list
```

**可用示例**:

```bash
# 基础示例
hello_world           # Hello World 示例
task_submit          # 任务提交示例
memory_write         # 记忆写入示例

# 进阶示例
agent_collaboration  # 多 Agent 协作
skill_composition    # 技能组合
pattern_recognition  # 模式识别

# 综合示例
ecommerce_demo       # 电商应用演示
research_assistant   # 研究助手演示
```

**示例代码结构**:

```cpp
// examples/cpp/hello_world/main.cpp
#include <agentos.h>

int main() {
    // 初始化 AgentOS
    agentos_init();
    
    // 创建任务
    const char* task_id = agentos_task_submit(
        "say_hello",
        "{\"message\": \"Hello, World!\"}"
    );
    
    // 等待任务完成
    agentos_task_wait(task_id);
    
    // 清理资源
    agentos_shutdown();
    
    return 0;
}
```

---

## 🔍 故障排查

### generate_docs.py 常见问题

#### 1. Doxygen 未安装

**错误**: `Doxygen not found`

**解决**:
```bash
# Ubuntu/Debian
sudo apt-get install doxygen

# macOS (Homebrew)
brew install doxygen

# Windows (Chocolatey)
choco install doxygen.install
```

#### 2. Graphviz 缺失

**警告**: `PlantUML: GraphViz backend not found`

**解决**:
```bash
# Ubuntu/Debian
sudo apt-get install graphviz

# macOS (Homebrew)
brew install graphviz
```

### run_example.sh 常见问题

#### 1. 编译产物不存在

**错误**: `Build artifacts not found`

**解决**:
```bash
# 先构建项目
cd scripts/build
./build.sh --release

# 然后运行示例
cd ../development
./run_example.sh hello_world
```

#### 2. 环境变量未设置

**错误**: `AGENTOS_CONFIG environment variable not set`

**解决**:
```bash
# 设置配置文件路径
export AGENTOS_CONFIG=$(pwd)/agentos/manager/kernel/settings.yaml

# 或添加到 ~/.bashrc
echo 'export AGENTOS_CONFIG=/path/to/manager.yaml' >> ~/.bashrc
source ~/.bashrc
```

---

## 📝 最佳实践

### 文档生成自动化

```yaml
# .github/workflows/docs.yml
name: Generate Documentation

on:
  push:
    branches: [main]

jobs:
  docs:
    runs-on: ubuntu-latest
    
    steps:
    - uses: actions/checkout@v3
    
    - name: Install Doxygen
      run: sudo apt-get install -y doxygen
    
    - name: Generate Docs
      run: |
        cd scripts/dev
        python generate_docs.py --all
    
    - name: Deploy to GitHub Pages
      uses: peaceiris/actions-gh-pages@v3
      with:
        github_token: ${{ secrets.GITHUB_TOKEN }}
        publish_dir: ./paper/generated
```

### 示例测试集成

```python
# tests/test_examples.py
import subprocess
import pytest

@pytest.mark.parametrize("example", [
    "hello_world",
    "task_submit",
    "memory_write"
])
def test_examples(example):
    """测试所有示例程序"""
    result = subprocess.run(
        ["./run_example.sh", example],
        cwd="scripts/dev",
        capture_output=True,
        text=True
    )
    
    assert result.returncode == 0
    assert "Error" not in result.stdout
```

### 注册表版本控制

```bash
#!/bin/bash
# scripts/development/auto_update_registry.sh

set -euo pipefail

echo "=== 自动更新注册表 ==="

# 扫描新增的 Agent
find openlab/app -name "agent.json" | while read file; do
    echo "发现新 Agent: $file"
    python update_registry.py --add-agent "$file"
done

# 扫描新增的 Skill
find openlab/contrib/skills -name "skill.json" | while read file; do
    echo "发现新 Skill: $file"
    python update_registry.py --add-skill "$file"
done

# 提交变更
git add agentos/manager/agent/registry.yaml
git add agentos/manager/skill/registry.yaml
git commit -m "chore: 更新组件注册表 $(date +%Y-%m-%d)"
```

---

## 🛡️ 安全建议

### 文档生成安全

1. **审查生成的内容**:
   - 不要直接发布自动生成的文档
   - 人工审查敏感信息

2. **限制访问权限**:
```bash
# 设置文档目录权限
chmod -R 755 paper/generated
chown -R agentos:agentos paper/generated
```

### 注册表安全

1. **签名验证**:
```python
# 验证注册表签名
import json
from cryptography.hazmat.primitives import hashes

def verify_registry_signature(registry_file):
    with open(registry_file, 'r') as f:
        data = json.load(f)
    
    signature = data.pop('signature')
    # 验证签名逻辑...
    return True
```

2. **访问控制**:
```yaml
# registry_access.yaml
access_control:
  read:
    - role: developer
    - role: maintainer
  write:
    - role: maintainer
    - role: admin
```

---

## 📞 相关文档

- [主 README](../README.md) - 脚本总览
- [架构文档](../../paper/architecture/README.md) - 系统设计
- [贡献指南](../../CONTRIBUTING.md) - 如何贡献代码

---

## 🤝 贡献

欢迎提交新的开发工具和示例！

**Issue 追踪**: https://github.com/SpharxTeam/AgentOS/issues  
**讨论区**: https://github.com/SpharxTeam/AgentOS/discussions

---

© 2026 SPHARX Ltd. All Rights Reserved.

*From data intelligence emerges.*  
*始于数据，终于智能。*

