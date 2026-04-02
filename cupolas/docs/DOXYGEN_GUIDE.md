# cupolas Doxygen API 文档生成指南

## 概述

本目录包含 cupolas 模块的 Doxygen API 文档配置和生成工具。

**文档特点**：
- ✅ 完全符合 AgentOS 架构设计原则 (E-7: 文档即代码)
- ✅ 遵循 `Code_comment_template.md` C/C++ 注释规范
- ✅ 支持中文输出，完整的架构说明
- ✅ 包含交互式 SVG 类图和调用图
- ✅ 代码浏览器，可直接查看源码

## 快速开始

### 前置条件

1. **安装 Doxygen**

   **方式一：官网下载（推荐）**
   - 访问 https://www.doxygen.nl/download.html
   - 下载 Windows 安装包: `doxygen-x.x.x.x-windows.x64.bin.zip`
   - 解压到任意目录 (如 `C:\doxygen`)
   - 将 `bin` 目录添加到系统 `PATH` 环境变量
   - 重启终端

   **方式二：Chocolatey**
   ```powershell
   choco install doxygen
   ```

   **方式三：Scoop**
   ```powershell
   scoop install doxygen
   ```

2. **安装 Graphviz (可选，用于生成类图)**

   如果需要生成 UML 类图和调用关系图：

   ```powershell
   choco install graphviz
   ```

### 生成文档

#### 方式一：使用批处理脚本（推荐）

双击运行或命令行执行：

```bash
generate_api_docs.bat
```

脚本会自动：
- ✓ 检查 Doxygen 是否已安装
- ✓ 清理旧文档
- ✓ 生成新的 API 文档
- ✓ 在浏览器中打开结果

#### 方式二：手动执行

```bash
# 进入 cupolas 目录
cd AgentOS/cupolas

# 运行 Doxygen
doxygen Doxyfile

# 打开生成的文档
start docs/api/html/index.html
```

## 输出结构

生成的文档位于 `docs/api/` 目录：

```
docs/api/
├── html/                    # HTML 格式 API 文档
│   ├── index.html          # 首页（模块概览）
│   ├── modules.html        # 模块列表
│   ├── classes.html        # 类/结构体列表
│   ├── files.html          # 文件列表
│   ├── annotated.html      # 数据类型索引
│   ├── globals.html        # 全局符号索引
│   └── ...                 # 各源文件的详细文档
├── doxygen_warnings.log    # 警告日志（如有）
└── ...
```

## 文档内容

### 主要章节

1. **模块概述**: 四重防护体系介绍
2. **架构设计**: 五维正交架构映射
3. **快速开始**: 完整的代码示例
4. **安全特性**: iOS 级别安全、零信任、纵深防御
5. **监控指标**: Prometheus/OpenTelemetry/StatsD 支持
6. **测试覆盖**: 测试套件和覆盖率目标
7. **合规性**: 许可证和标准符合性

### 生成的图表

- **类图 (Class Diagrams)**: 结构体和类的继承/组合关系
- **协作图 (Collaboration Graphs)**: 模块间交互关系
- **调用图 (Call Graphs)**: 函数调用链
- **被调用图 (Caller Graphs)**: 函数被谁调用
- **包含图 (Include Graphs)**: 头文件依赖关系
- **目录图 (Directory Graphs)**: 目录结构可视化

## 配置文件说明

### Doxyfile

主配置文件，关键设置：

| 配置项 | 值 | 说明 |
|--------|-----|------|
| PROJECT_NAME | "cupolas - AgentOS 安全穹顶" | 项目名称 |
| OUTPUT_LANGUAGE | Chinese | 中文输出 |
| EXTRACT_ALL | YES | 提取所有实体 |
| RECURSIVE | YES | 递归扫描子目录 |
| INPUT | src, include | 源代码目录 |
| OPTIMIZE_OUTPUT_FOR_C | YES | 针对 C 语言优化 |
| GENERATE_TREEVIEW | YES | 侧边栏导航 |
| UML_LOOK | YES | UML 风格类图 |
| INTERACTIVE_SVG | YES | 交互式 SVG 图表 |

### mainpage.dox

首页文档，包含：
- 模块简介和设计理念
- 五维正交架构映射
- 各子模块功能说明
- 快速开始示例代码
- 安全特性详解
- 监控与可观测性说明
- 合规性和参考资料

## 自定义配置

### 修改输出格式

编辑 `Doxyfile`：

```ini
# 启用 LaTeX/PDF 输出
GENERATE_LATEX         = YES

# 启用 XML 输出（用于其他工具）
GENERATE_XML           = YES

# 启用 RTF 输出
GENERATE_RTF           = YES
```

### 调整图表深度

```ini
# 最大图表深度（层数）
MAX_DOT_GRAPH_DEPTH    = 3

# 最大节点数（避免过大图）
MAX_DOT_GRAPH_NODES    = 100
```

### 排除特定文件

```ini
EXCLUDE_PATTERNS       = */tests/* \
                         */fuzz/* \
                         */CMakeFiles/*
```

## 故障排除

### 问题 1: 找不到 doxygen 命令

**错误信息**:
```
doxygen : 无法将"doxygen"项识别为 cmdlet...
```

**解决方案**:
1. 确认 Doxygen 已正确安装
2. 确认 `bin` 目录在 PATH 中
3. 重启终端/IDE
4. 使用完整路径: `C:\doxygen\bin\doxygen.exe Doxyfile`

### 问题 2: 图表未生成

**原因**: 未安装 Graphviz 或 DOT_PATH 配置错误

**解决方案**:
```powershell
# 安装 Graphviz
choco install graphviz

# 在 Doxyfile 中配置路径
DOT_PATH = C:\Program Files\Graphviz\bin
```

### 问题 3: 中文显示为乱码

**原因**: 字体缺失或编码问题

**解决方案**:
1. 确保系统安装了中文字体（如 Microsoft YaHei）
2. 确认 INPUT_ENCODING = UTF-8
3. 清理后重新生成文档

### 问题 4: 警告过多

**常见警告**:
- `documented symbol XXX was not declared or defined`: 声明不匹配
- `return type of member XXX is not documented`: 缺少返回值文档
- `parameters of member XXX are not documented`: 缺少参数文档

**处理方法**:
1. 查看 `docs/doxygen_warnings.log`
2. 根据警告补充 Doxygen 注释
3. 或在 Doxyfile 中调整:

```ini
WARN_IF_UNDOCUMENTED   = NO  # 降低严格度
EXTRACT_ALL            = YES # 提取未注释实体
```

## 与 CI/CD 集成

### GitHub Actions 示例

```yaml
name: Generate API Docs

on:
  push:
    branches: [main]
    paths:
      - 'AgentOS/cupolas/**'

jobs:
  docs:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v3

      - name: Install Doxygen
        run: sudo apt-get update && sudo apt-get install -y doxygen graphviz

      - name: Generate Docs
        working-directory: AgentOS/cupolas
        run: doxygen Doxyfile

      - name: Deploy to GitHub Pages
        uses: peaceiris/actions-gh-pages@v3
        with:
          github_token: ${{ secrets.GITHUB_TOKEN }}
          publish_dir: ./AgentOS/cupolas/docs/api/html
```

### GitLab CI 示例

```yaml
generate-docs:
  stage: deploy
  image: alpine
  before_script:
    - apk add --no-cache doxygen graphviz
  script:
    - cd AgentOS/cupolas
    - doxygen Doxyfile
  artifacts:
    paths:
      - AgentOS/cupolas/docs/api/html
    expire_in: 30 days
  only:
    - main
```

## 最佳实践

### 1. 保持文档同步

每次修改代码时同步更新 Doxygen 注释：

```c
/**
 * @brief 新增的功能函数
 * @param[in] param1 参数1说明
 * @param[out] result 结果缓冲区
 * @return 成功返回0，失败返回负错误码
 * @since 1.1.0 新增此函数
 */
int new_function(int param1, char* result);
```

### 2. 定期生成文档

建议在 CI 流水线中自动生成文档，确保：
- 无新增警告
- 文档完整性
- 可追溯历史版本

### 3. 版本管理

将生成的文档纳入 Git 版本控制（可选）：

```bash
# 提交生成的文档
git add docs/api/
git commit -m "docs: update API documentation"
```

或者通过 CI 自动发布到 GitHub Pages/GitLab Pages。

## 参考资料

- **Doxygen 官方手册**: https://www.doxygen.nl/manual/
- **AgentOS 架构原则**: ../../manuals/ARCHITECTURAL_PRINCIPLES.md
- **代码注释规范**: ../../manuals/specifications/coding_standard/Code_comment_template.md
- **Doxygen 特殊命令**: https://www.doxygen.nl/manual/commands.html

---

**版本**: 1.0.0
**更新日期**: 2026-04-02
**维护者**: SPHARX Ltd. - AgentOS Team

**SPDX-FileCopyrightText: 2026 SPHARX Ltd.**
**SPDX-License-Identifier: Apache-2.0 OR BSD-3-Clause**
