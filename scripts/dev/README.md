# AgentOS 开发辅助脚本

**版本**: v1.0.0.6  
**最后更新**: 2026-03-21  

---

## 📋 概述

`scripts/dev/` 目录包含 AgentOS 开发过程中的辅助工具，提供文档生成、注册表更新和示例运行等功能。

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
| `config/.clangd` | Clangd 语言服务器配置 | Config | ✅ 生产就绪 |
| `config/.editorconfig` | 编辑器统一配置 | Config | ✅ 生产就绪 |
| `config/.lizardrc` | 圈复杂度检查配置 | Config | ✅ 生产就绪 |
| `config/.jscpd.json` | 重复代码检测配置 | Config | ✅ 生产就绪 |
| `config/.pre-commit-config.yaml` | Git 钩子配置 | Config | ✅ 生产就绪 |
| `config/vcpkg.json` | C++ 依赖配置 (Windows) | Config | ✅ 生产就绪 |

> 📌 **注意**: 这些配置文件原位于项目根目录，已统一迁移到 `scripts/dev/config/` 目录，以便保持根目录整洁。

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
cd scripts/dev

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
cd scripts/dev

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
# manager/agent/registry.yaml
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

# manager/skill/registry.yaml
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
cd scripts/dev

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
cd ../dev
./run_example.sh hello_world
```

#### 2. 环境变量未设置

**错误**: `AGENTOS_CONFIG environment variable not set`

**解决**:
```bash
# 设置配置文件路径
export AGENTOS_CONFIG=$(pwd)/manager/kernel/settings.yaml

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
# scripts/dev/auto_update_registry.sh

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
git add manager/agent/registry.yaml
git add manager/skill/registry.yaml
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

