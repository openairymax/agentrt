# AgentOS Scripts - 开发与运维工具集 V2.1

**版本**: v2.1.0  
**最后更新**: 2026-04-09  
**整理状态**: ✅ 已完成全面重组 + i18n 国际化

---

## 📋 概述

`scripts/` 模块是 AgentOS 的**统一工具平台**，提供从开发构建到生产部署的全生命周期自动化支持。经过V2.1全面重组，现已实现：

- 🎯 **清晰的模块化架构** - 10个功能子目录，职责明确
- 🌍 **完整的跨平台支持** - Windows/macOS/Linux 原生脚本
- 🖥️ **桌面客户端集成** - Tauri GUI应用，可视化操作
- 🌐 **中英文国际化** - 全页面 i18n 支持，实时语言切换
- 🎨 **明暗主题切换** - Dark/Light/Auto 三种主题模式
- 🔧 **统一的入口点** - install.sh / install.ps1 一键部署
- 📚 **完善的文档体系** - 每个子目录独立README

### 核心能力矩阵

| 能力域 | 支持程度 | 主要工具 |
|--------|---------|----------|
| **跨平台部署** | ✅✅✅ 完整 | `install.sh` + `install.ps1` |
| **Docker管理** | ✅✅✅ 完整 | `deploy/docker/` |
| **桌面客户端** | ✅✅✅ 完善 | `desktop-client/` (Tauri + i18n) |
| **国际化** | ✅✅✅ 新增 | `desktop-client/src/i18n/` (中/英) |
| **CI/CD流水线** | ✅✅ 成熟 | `ci/` (GitHub Actions) |
| **开发辅助** | ✅✅ 完善 | `dev/` + `tools/` |
| **运维监控** | ✅✅ 完善 | `ops/` (doctor/benchmark) |
| **测试框架** | ✅✅ 成熟 | `tests/` (Shell+Python) |
| **核心库** | ✅✅ 稳定 | `lib/` (Shell) + `core/` (Python) |

---

## 📁 V2.1 模块结构

```
scripts/
│
├── 📜 README.md                    # 本文档（模块总览）
├── 🚀 install.sh                   # [入口] 跨平台部署脚本 (Unix/Linux/macOS)
├── 🚀 install.ps1                  # [入口] 跨平台部署脚本 (Windows PowerShell)
│
├── 📂 desktop-client/              # Tauri 桌面客户端应用
│   ├── src-tauri/                  #   Rust 后端 (18个Tauri命令)
│   ├── src/                        #   React 前端 (8个页面)
│   │   ├── pages/
│   │   │   ├── Dashboard.tsx       #     系统仪表盘
│   │   │   ├── Services.tsx        #     Docker服务管理
│   │   │   ├── Agents.tsx          #     AI Agent管理
│   │   │   ├── Tasks.tsx           #     任务提交/跟踪
│   │   │   ├── Config.tsx          #     配置文件编辑器
│   │   │   ├── Logs.tsx            #     日志查看器
│   │   │   ├── Terminal.tsx        #     集成终端
│   │   │   └── Settings.tsx        #     设置（主题/语言/连接）
│   │   ├── i18n/                   #   国际化模块
│   │   │   ├── index.tsx           #     I18n Provider + Hook
│   │   │   ├── en.json             #     英文翻译 (220+ 键)
│   │   │   └── zh.json             #     中文翻译 (220+ 键)
│   │   ├── utils/
│   │   │   └── tauriCompat.ts      #     Tauri API 兼容层
│   │   └── styles/
│   │       └── globals.css         #     全局样式 + 主题变量
│   ├── package.json                #   前端依赖
│   └── README.md                   #   客户端使用文档
│
├── 📂 lib/                         # Shell 核心库（基础设施层）
│   ├── common.sh                   # 通用工具函数
│   ├── log.sh                      # 统一日志系统
│   ├── error.sh                    # 错误码定义
│   └── platform.sh                 # 跨平台检测
│
├── 📂 core/                        # Python 核心模块（业务逻辑层）
│   ├── __init__.py                 # 模块入口
│   ├── cli.py                      # 命令行接口
│   ├── config.py                   # 配置管理
│   ├── events.py                   # 事件总线
│   ├── plugin.py                   # 插件系统
│   ├── security.py                 # 安全管理
│   └── telemetry.py                # 遥测收集
│
├── 📂 deploy/                      # 部署配置（Docker编排层）
│   ├── docker/                     # Docker 相关
│   │   ├── docker-compose.yml      #     开发环境
│   │   ├── docker-compose.prod.yml #     生产环境
│   │   ├── Dockerfile.kernel       #     内核镜像
│   │   ├── Dockerfile.service      #     服务镜像
│   │   └── quickstart.sh           #     快速启动
│   └── deploy_config.py            # 配置部署工具
│
├── 📂 ci/                          # CI/CD 流水线（持续集成层）
│   ├── ci-run.sh                   # 主流水线入口
│   ├── build-module.sh             # 模块构建
│   ├── run-tests.sh                # 测试执行
│   ├── quality-gate.sh             # 质量门禁
│   ├── install-deps.sh             # 依赖安装
│   ├── deploy-artifacts.sh         # 制品部署
│   └── CI_CD_DOCUMENTATION.md       # CI/CD文档
│
├── 📂 dev/                         # 开发工具（开发环境层）
│   ├── config/                     # 开发配置文件
│   │   ├── .clang-format           # C/C++格式化
│   │   ├── .editorconfig          # 编辑器配置
│   │   ├── .pre-commit-config.yaml # Git钩子
│   │   └── vcpkg.json             # 包管理
│   ├── quickstart.sh               # 快速入门
│   └── validate.sh                 # 环境验证
│
├── 📂 ops/                         # 运维工具（生产运维层）
│   ├── doctor.py                   # 系统健康检查
│   ├── benchmark.py                # 性能基准测试
│   ├── token_counter.py             # Token统计
│   ├── token_budget.py              # Token配额
│   ├── memory_manager.py            # 内存管理
│   ├── checkpoint_manager.py        # 检查点管理
│   └── validate_contracts.py        # 契约验证
│
├── 📂 tests/                       # 测试框架（质量保障层）
│   ├── shell/                      # Shell测试
│   │   ├── test_framework.sh        # 测试框架
│   │   └── test_common_utils.sh     # 工具测试
│   ├── python/                     # Python测试
│   │   ├── conftest.py             # pytest配置
│   │   ├── test_core.py            # 核心测试
│   │   ├── test_token_counter.py    # Token测试
│   │   ├── test_token_budget.py     # 配额测试
│   │   ├── test_memory_manager.py   # 内存测试
│   │   └── test_checkpoint_manager.py # 检查点测试
│   └── README.md                   # 测试指南
│
├── 📂 tools/                        # 通用工具集（辅助工具层）
│   ├── analyze_quality.py           # 代码质量分析
│   ├── check-quality.sh             # 质量检查入口
│   ├── enhance_coverage.py           # 覆盖率增强
│   ├── encoding/                    # 编码工具
│   │   ├── check_encoding.py        #   编码检查
│   │   └── fix_bom.py               #   BOM修复
│   ├── docs/                        # 文档工具
│   │   └── verify_consistency.py    #   文档一致性验证
│   ├── requirements.txt             # Python依赖
│   └── README.md                   # 工具文档
│
├── 📂 init/                        # 初始化配置（项目初始化层）
│   ├── init_config.py              # 配置向导
│   └── README.md                   # 初始化指南
│
└── 📂 archive/                     # 历史归档（废弃代码层）
    ├── deploy.sh.v1.0-legacy       # 旧版部署脚本（已被install.sh替代）
    └── README.md                   # 归档说明
```

---

## 🚀 快速开始

### 方式一：命令行一键部署（推荐）

#### Unix/Linux/macOS
```bash
cd scripts

# 交互式部署（推荐新手）
chmod +x install.sh
./install.sh

# 一键部署开发环境
./install.sh --mode dev --auto

# 一键部署生产环境
./install.sh --mode prod --auto

# 仅部署后端服务
./install.sh --mode dev --target backend

# 仅构建桌面客户端
./install.sh --target client

# 全部部署（后端+客户端）
./install.sh --target all

# 仅检查环境
./install.sh --check-only
```

#### Windows PowerShell
```powershell
cd scripts

# 交互式部署
.\install.ps1

# 一键部署（推荐）
.\install.ps1 -Mode dev -Auto

# 以管理员身份运行
.\install.ps1 -Mode prod -Auto -AsAdmin

# 仅检查环境
.\install.ps1 -CheckOnly
```

### 方式二：桌面客户端GUI（推荐新手）

```bash
cd scripts/desktop-client

# 安装前端依赖
npm install

# 启动开发模式（浏览器预览，热重载）
npm run dev

# 启动Tauri开发模式（完整桌面应用）
npm run tauri dev

# 构建生产版本
npm run tauri build
```

**桌面客户端功能：**
- 📊 **Dashboard** - 实时系统监控仪表盘
- 🐳 **Services** - 可视化Docker服务管理（开发/生产模式切换）
- 🤖 **Agents** - AI Agent注册与管理
- 📋 **Tasks** - 任务提交与跟踪
- ⚙️ **Config** - 配置文件编辑器（含安全警告）
- 📄 **Logs** - 实时日志查看器（彩色高亮）
- 💻 **Terminal** - 集成命令行终端（快捷命令）
- 🔧 **Settings** - 主题/语言/连接配置

**国际化支持：**
- 🇺🇸 English - 完整英文界面
- 🇨🇳 简体中文 - 完整中文界面
- 实时切换，无需重启

**主题模式：**
- 🌙 Dark - 深色主题（默认）
- ☀️ Light - 浅色主题
- 🔄 Auto - 跟随系统

---

## 📖 详细文档导航

### 📘 核心文档

| 文档 | 路径 | 说明 |
|------|------|------|
| **本文档** | `scripts/README.md` | 模块总览和快速开始 |
| **部署指南** | `scripts/install.sh --help` | 部署脚本详细参数 |
| **桌面客户端** | `scripts/desktop-client/README.md` | GUI应用完整文档 |
| **CI/CD文档** | `scripts/ci/CI_CD_DOCUMENTATION.md` | 流水线配置说明 |
| **测试指南** | `scripts/tests/README.md` | 测试框架使用方法 |
| **归档说明** | `scripts/archive/README.md` | 历史版本参考 |

### 🛠️ 子模块文档

每个子模块都有独立的README文档，可通过以下方式访问：

```bash
# Shell库文档
cat scripts/lib/README.md

# Python核心模块
cat scripts/core/README.md

# 运维工具
cat scripts/ops/README.md

# 通用工具
cat scripts/tools/README.md
```

---

## 🏗️ 架构设计原则

### 分层架构（5层模型）

```
┌─────────────────────────────────────────┐
│         入口层 (Entry Points)           │
│   install.sh / install.ps1 / desktop    │
├─────────────────────────────────────────┤
│       应用层 (Application)              │
│   deploy/ ops/ init/ tools/             │
├─────────────────────────────────────────┤
│       业务逻辑层 (Business Logic)       │
│   core/ (Python)                       │
├─────────────────────────────────────────┤
│       基础设施层 (Infrastructure)        │
│   lib/ (Shell)                         │
├─────────────────────────────────────────┤
│       质量保障层 (Quality Assurance)     │
│   tests/ ci/                           │
└─────────────────────────────────────────┘
```

### 桌面客户端架构

```
desktop-client/
├── src/
│   ├── main.tsx          → I18nProvider 包裹 App
│   ├── App.tsx           → 路由 + 侧边栏导航 (i18n)
│   ├── pages/            → 8个页面组件 (全部 i18n)
│   ├── i18n/             → 国际化系统
│   │   ├── index.tsx     → Context + Hook + Provider
│   │   ├── en.json       → 英文翻译
│   │   └── zh.json       → 中文翻译
│   ├── utils/
│   │   └── tauriCompat.ts → Tauri API 兼容层 (Mock)
│   └── styles/
│       └── globals.css   → CSS 变量 + 主题 + 工具类
└── src-tauri/            → Rust 后端 (Tauri Commands)
```

### 设计原则遵循

✅ **E-4 跨平台一致性** - 三平台统一接口  
✅ **E-5 接口最小化** - 精简的API设计  
✅ **Cognitive View** - 直观的用户体验  
✅ **Engineering View** - 模块化可维护  
✅ **Aesthetics View** - 专业的暗色UI设计  

---

## 📊 使用场景矩阵

### 场景1：首次部署AgentOS

```bash
# 步骤1: 克隆项目
git clone https://github.com/SpharxTeam/AgentOS.git
cd AgentOS/scripts

# 步骤2: 一键部署（自动检测环境、安装依赖、启动服务）
# Unix:
./install.sh --mode dev --auto

# Windows:
.\install.ps1 -Mode dev -Auto

# 步骤3: 访问服务
# Gateway: http://localhost:18789
# OpenLab: http://localhost:3000
```

**预计时间**: < 10分钟（含Docker镜像拉取）

---

### 场景2：日常开发工作流

```bash
# 1. 启动开发环境
./install.sh --mode dev --auto

# 2. 代码质量检查
cd tools
python analyze_quality.py --path ../../agentos/

# 3. 运行测试
cd ../tests/python
pytest -v

# 4. 启动桌面客户端进行可视化管理
cd ../../desktop-client
npm run tauri dev
```

---

### 场景3：生产环境部署

```bash
# 1. 环境准备（生产服务器）
./install.sh --check-only

# 2. 编辑敏感配置
nano .env.production  # 或使用桌面客户端Config页面

# 3. 部署生产环境
./install.sh --mode prod --auto

# 4. 健康验证
./install.sh --health

# 5. 监控日志
./install.sh --logs
```

---

### 场景4：仅构建桌面客户端

```bash
# 仅构建客户端（不部署后端）
./install.sh --target client

# 或手动构建
cd desktop-client
npm install
npm run tauri build
```

---

## 🔧 高级用法

### 自定义部署配置

```bash
# 使用自定义配置文件
./install.sh --mode prod --env-file my-custom.env

# 指定Docker Compose文件
./install.sh --compose-file docker/my-compose.yml

# 跳过健康检查
./install.sh --skip-health-check
```

### 桌面客户端高级操作

```typescript
// 在浏览器开发者工具中调用Tauri命令
const { invoke } = window.__TAURI__.core;

// 获取系统信息
const sysInfo = await invoke('get_system_info');

// 执行CLI命令
const result = await invoke('execute_cli_command', {
  command: 'docker',
  args: ['ps', '--format', 'json']
});

// 提交任务给Agent
const task = await invoke('submit_task', {
  agentId: 'agent-001',
  taskDescription: 'Analyze code quality',
  priority: 'high'
});
```

### 浏览器开发模式

桌面客户端支持纯浏览器开发模式，无需 Tauri 运行时：

```bash
cd desktop-client
npm run dev    # 启动 Vite 开发服务器 (http://localhost:1420)
```

浏览器模式下，`tauriCompat.ts` 会自动提供 Mock 数据，所有页面功能均可预览。

---

## 🛡️ 安全最佳实践

### 1. 敏感信息保护

```bash
# 确保配置文件权限正确
chmod 600 .env.production

# 不将敏感文件提交到Git
echo ".env*" >> .gitignore
echo "*.key" >> .gitignore
echo "*.pem" >> .gitignore
```

### 2. 生产环境加固

```bash
# 使用非root用户运行Docker容器
# 已在docker-compose.prod.yml中配置

# 启用TLS加密
# 在config/agentos.yaml中配置network.tls.enabled: true

# 定期轮换密钥
./ops/rotate_keys.sh --quarterly  # 如有此脚本
```

### 3. 安全扫描

```bash
# Python代码安全扫描
bandit -r core/ -f json -o security-report.json

# Shell脚本安全扫描
shellcheck lib/**/*.sh ops/**/*.sh

# 依赖漏洞检查
safety check -r tools/requirements.txt
```

---

## ❓ 故障排查

### 常见问题快速诊断

| 问题 | 可能原因 | 解决方案 |
|------|---------|----------|
| `Permission denied` | 脚本无执行权限 | `chmod +x install.sh` |
| `Docker not found` | 未安装Docker | 访问 https://docs.docker.com/get-docker/ |
| `ModuleNotFoundError` | Python依赖缺失 | `pip install -r tools/requirements.txt` |
| `port already in use` | 端口被占用 | `./install.sh --stop` 或修改端口配置 |
| 内存不足 | 系统资源不够 | 关闭其他应用或增加swap空间 |
| 客户端白屏 | Tauri未安装 | 使用 `npm run dev` 浏览器模式预览 |

### 获取帮助

```bash
# 查看详细帮助
./install.sh --help
.\install.ps1 -Help

# 运行诊断
./install.sh --check-only
.\install.ps1 -CheckOnly

# 收集日志用于问题报告
./install.sh --logs
.\install.ps1 -Logs
```

---

## 📈 版本历史

### V2.1.0 (2026-04-09) - 国际化与完善

**新特性:**
- ✅ 全页面 i18n 国际化支持（中/英 220+ 翻译键）
- ✅ 新增 Settings 页面（主题/语言/连接/系统信息）
- ✅ Dark/Light/Auto 三种主题模式切换
- ✅ Tauri API 兼容层（浏览器开发模式 Mock）
- ✅ install.sh 新增 `--target` 参数（backend/client/all）
- ✅ 编码工具迁移至 `tools/encoding/`
- ✅ 文档工具迁移至 `tools/docs/`

**变更:**
- 🔄 所有8个页面完成 i18n 集成
- 🔄 tauriCompat.ts 改用动态 import 加载 Tauri API
- 🔄 globals.css 新增 50+ CSS 工具类
- 🔄 清理根目录非必要脚本文件

### V2.0.0 (2026-04-08) - 重大重组

**新特性:**
- ✅ 新增跨平台部署脚本 (`install.sh` + `install.ps1`)
- ✅ 新增Tauri桌面客户端 (`desktop-client/`)
- ✅ 新增通用工具集目录 (`tools/`)
- ✅ 新增历史归档目录 (`archive/`)
- ✅ 全面重组目录结构，提升可维护性
- ✅ 更新所有文档，反映最新架构

**变更:**
- 🔄 将 `deploy.sh` 归档为 `archive/deploy.sh.v1.0-legacy`
- 🔄 将根目录散落脚本迁移至 `tools/`
- 🔄 重写主README，采用分层架构说明

**移除:**
- ❌ 废弃旧的单一平台部署方案

---

### V1.0.6 (2026-03-29) - 之前版本

详见 `archive/README.md`

---

## 🤝 贡献指南

欢迎提交改进建议和新功能！

### 开发流程

1. Fork 本仓库
2. 创建特性分支: `git checkout -b feature/amazing-feature`
3. 提交更改: `git commit -m 'Add amazing feature'`
4. 推送分支: `git push origin feature/amazing-feature`
5. 提交Pull Request

### 代码规范

- **Shell脚本**: 遵循 `lib/common.sh` 中的函数命名规范
- **Python代码**: 遵循 `core/` 模块的类型注解风格
- **React组件**: 参考 `desktop-client/src/pages/` 的组件结构
- **Rust后端**: 参考 `desktop-client/src-tauri/src/commands.rs` 的命令注册方式
- **国际化**: 新增文本必须在 `en.json` 和 `zh.json` 中同时添加

---

## 📞 支持与反馈

- **Issue追踪**: https://github.com/SpharxTeam/AgentOS/issues  
- **讨论区**: https://github.com/SpharxTeam/AgentOS/discussions  
- **在线文档**: https://docs.agentos.io  

---

## 📚 相关资源

- **架构设计**: [`agentos/manuals/ARCHITECTURAL_PRINCIPLES.md`](../agentos/manuals/ARCHITECTURAL_PRINCIPLES.md)
- **部署指南**: [`agentos/manuals/guides/deployment.md`](../agentos/manuals/guides/deployment.md)
- **API文档**: [`apis/README.md`](../apis/README.md)
- **运维手册**: [`agentos/manuals/guides/troubleshooting.md`](../agentos/manuals/guides/troubleshooting.md)

---

© 2026 SPHARX Ltd. All Rights Reserved.

*From data intelligence emerges.*  
*始于数据，终于智能。*
