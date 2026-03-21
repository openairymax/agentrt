# AgentOS 技术文档中心 (PartDocs)

**版本**: 1.0.0.5  
**最后更新**: 2026-03-18  

---

## 📚 概述

`partdocs/` 是 AgentOS 的技术文档中心，包含完整的架构设计、API 参考、开发指南和技术规范。

---

## 📁 文档结构

```
partdocs/
├── README.md                    # 本文件
│
├── api/                         # API 文档
│   ├── syscall/                # 系统调用 API
│   │   ├── task_api.md         # 任务系统调用
│   │   ├── memory_api.md       # 记忆系统调用
│   │   ├── session_api.md      # 会话系统调用
│   │   └── telemetry_api.md    # 遥测系统调用
│   ├── python/                 # Python SDK API
│   │   └── agentos.md          # Python SDK 详细文档
│   ├── rust/                   # Rust SDK API
│   │   └── agentos_rs.md       # Rust SDK 文档
│   └── go/                     # Go SDK API
│       └── agentos_go.md       # Go SDK 文档
│
├── architecture/                # 架构设计文档 ⭐ 核心
│   ├── diagrams/               # 架构图源文件
│   │   ├── system_overview.png
│   │   ├── coreloopthree.png
│   │   └── memoryrovol.png
│   ├── coreloopthree.md        # CoreLoopThree 三层一体架构 ⭐
│   ├── memoryrovol.md          # MemoryRovol 四层记忆架构 ⭐
│   ├── microkernel.md          # 微内核设计
│   ├── ipc.md                  # IPC Binder 通信机制
│   ├── syscall.md              # 系统调用设计
│   └── logging_system.md       # 统一日志系统架构 ⭐
│
├── guides/                      # 开发指南 📖
│   ├── getting_started.md      # 快速入门 ⭐
│   ├── create_agent.md         # Agent 开发教程 ⭐
│   ├── create_skill.md         # 技能开发教程 ⭐
│   ├── deployment.md           # 生产环境部署指南
│   ├── kernel_tuning.md        # 内核调优指南
│   └── troubleshooting.md      # 故障排查手册
│
├── philosophy/                  # 设计哲学 💡
│   ├── design_principles.md    # 设计原则
│   ├── cognition_theory.md     # 认知理论基础
│   ├── memory_theory.md        # 记忆理论基础
│   ├── control_theory.md       # 控制理论
│   ├── dual_systems.md         # 双系统理论
│   └── engineering_art.md      # 工程的艺术
│
└── specifications/              # 技术规范 📋
    ├── coding_standards.md     # 编码规范
    ├── testing.md              # 测试规范
    ├── security.md             # 安全规范
    ├── performance.md          # 性能指标要求
    ├── protocol.md             # 通信协议规范
    ├── agent_contract.md       # Agent 合约规范
    ├── skill_contract.md       # 技能合约规范
    ├── logging_format.md       # 日志格式规范
    └── syscall_api.md          # 系统调用 API 规范
```

---

## 🎯 核心文档导读

### 新人入门路径

1. **快速开始** → [getting_started.md](guides/getting_started.md)
   - 环境搭建
   - Hello World 示例
   - 基础概念

2. **理解架构** → [architecture/](architecture/)
   - [CoreLoopThree 架构](architecture/coreloopthree.md) - 三层核心运行时
   - [MemoryRovol 架构](architecture/memoryrovol.md) - 四层记忆系统
   - [微内核设计](architecture/microkernel.md) - 内核架构

3. **动手实践** → [guides/](guides/)
   - [创建第一个 Agent](guides/create_agent.md)
   - [开发一个技能](guides/create_skill.md)

### 开发者进阶路径

1. **深入理解** → [philosophy/](philosophy/)
   - [设计原则](philosophy/design_principles.md)
   - [认知理论](philosophy/cognition_theory.md)
   - [记忆理论](philosophy/memory_theory.md)

2. **技术规范** → [specifications/](specifications/)
   - [编码规范](specifications/coding_standards.md)
   - [系统调用 API 规范](specifications/syscall_api.md)
   - [日志格式规范](specifications/logging_format.md)

3. **性能优化** → [guides/kernel_tuning.md](guides/kernel_tuning.md)
   - 内存管理优化
   - 调度策略调优
   - 向量化检索加速

### 运维人员路径

1. **部署指南** → [guides/deployment.md](guides/deployment.md)
   - 单机部署
   - 容器化部署
   - 集群部署

2. **监控运维** → [partdata/README.md](../partdata/README.md)
   - 日志管理
   - 数据备份
   - 性能监控

3. **故障排查** → [guides/troubleshooting.md](guides/troubleshooting.md)
   - 常见问题
   - 诊断工具
   - 案例分析

---

## 📖 文档详细说明

### Architecture（架构文档）

#### CoreLoopThree 三层一体架构

**内容**:
- 认知层：意图理解、任务规划、Agent 调度
- 行动层：任务执行、补偿事务、责任链追踪
- 记忆层：MemoryRovol FFI 封装、记忆服务

**适合读者**: 架构师、核心开发人员

#### MemoryRovol 四层记忆架构

**内容**:
- L1 Raw Layer: 原始数据存储
- L2 Feature Layer: 向量化表示
- L3 Structure Layer: 结构化编码
- L4 Pattern Layer: 高级模式挖掘
- 吸引子网络检索机制
- 艾宾浩斯遗忘曲线实现

**适合读者**: AI 工程师、算法工程师

#### Logging System 统一日志系统

**内容**:
- 集中式日志存储 (`partdata/logs/`)
- 模块独立日志文件
- 跨语言统一日志接口
- OpenTelemetry 集成
- 全链路追踪 (trace_id)

**适合读者**: 所有开发人员

### Guides（开发指南）

#### Getting Started 快速入门

**内容**:
- 环境要求和依赖安装
- 克隆项目和初始化配置
- 构建和运行第一个示例
- 基础概念解释

**预计时间**: 30 分钟

#### Create Agent 创建 Agent

**内容**:
- Agent 基础结构
- 实现认知接口
- 注册到系统
- 测试和调试

**示例代码**: Python、Go 双语言示例

#### Create Skill 创建技能

**内容**:
- 技能接口定义
- Rust/C++ 技能开发
- 编译为动态库
- 注册和发布

**示例**: browser_skill 完整示例

### Specifications（技术规范）

#### Coding Standards 编码规范

**C 语言规范**:
```c
// 命名规范
agentos_task_submit()      // 函数：小写 + 下划线
AGENTOS_LOG_LEVEL_INFO     // 宏：大写 + 下划线
agentos_config_t           // 类型：小写 + _t 后缀

// 注释规范
/**
 * @brief 提交任务
 * @param description 任务描述
 * @param out_id 输出任务 ID
 * @return 0 成功，负值失败
 */
int agentos_task_submit(const char* description, uint64_t* out_id);
```

**Python 规范**:
```python
class TaskManager:
    """任务管理器"""
    
    def submit_task(self, description: str) -> Task:
        """
        提交任务
        
        Args:
            description: 任务描述
            
        Returns:
            Task 对象
        """
        pass
```

#### Testing 测试规范

**单元测试覆盖率要求**:
- 内核层 (atoms/): ≥ 85%
- 服务层 (backs/): ≥ 80%
- SDK 层 (tools/): ≥ 90%

**测试分类**:
```bash
# 单元测试
ctest -R unit --output-on-failure

# 集成测试
ctest -R integration --output-on-failure

# 端到端测试
python tests/e2e/test_full_workflow.py
```

---

## 🔍 文档搜索

### 按主题搜索

| 主题 | 相关文档 |
|------|----------|
| **系统调用** | `api/syscall/*`, `architecture/syscall.md` |
| **记忆系统** | `architecture/memoryrovol.md`, `guides/create_skill.md` |
| **任务管理** | `architecture/coreloopthree.md`, `guides/create_agent.md` |
| **日志系统** | `architecture/logging_system.md`, `specifications/logging_format.md` |
| **安全隔离** | `architecture/microkernel.md`, `specifications/security.md` |

### 按角色搜索

| 角色 | 推荐文档 |
|------|----------|
| **架构师** | `architecture/*`, `philosophy/*` |
| **开发工程师** | `guides/*`, `specifications/coding_standards.md` |
| **测试工程师** | `specifications/testing.md`, `tests/README.md` |
| **运维工程师** | `guides/deployment.md`, `guides/troubleshooting.md` |

---

## 🛠️ 文档工具

### 生成 API 文档

```bash
# Python SDK 文档
cd tools/python
pdoc --html agentos -o ../partdocs/api/python/

# Rust SDK 文档
cd tools/rust
cargo doc --no-deps --open
```

### 构建完整文档站点

```bash
# 使用 Sphinx 构建
cd partdocs
make html

# 输出在 build/html/ 目录
# 可用浏览器打开 index.html 查看
```

### 验证文档链接

```bash
# 检查失效链接
python scripts/validate_docs.py

# 输出示例：
# ✅ architecture/coreloopthree.md
# ✅ guides/getting_started.md
# ❌ old_page.md (404 Not Found)
```

---

## 🤝 贡献文档

### 文档结构标准

```markdown
# 标题

**版本**: 1.0.0.5  
**最后更新**: 2026-03-18  

## 概述

简要说明文档目的和适用范围。

## 核心概念

解释关键术语和概念。

## 使用方法

提供详细的操作步骤和代码示例。

## 最佳实践

分享经验和注意事项。

## 相关文档

链接到相关主题的文档。
```

### 提交文档 PR

1. Fork 项目
2. 创建文档分支：`git checkout -b docs/add-feature-x`
3. 编写文档
4. 验证链接和示例代码
5. 提交 PR：`git push origin docs/add-feature-x`

---

## 📊 文档统计

| 类别 | 文档数量 | 总字数 |
|------|----------|--------|
| **架构文档** | 7 篇 | ~50k |
| **开发指南** | 6 篇 | ~30k |
| **API 文档** | 10+ 篇 | ~40k |
| **技术规范** | 8 篇 | ~25k |
| **设计哲学** | 6 篇 | ~20k |
| **总计** | 37+ 篇 | ~165k |

---

## 📚 相关资源

- [主项目文档](../README.md) - AgentOS 总体介绍
- [Workshop 文档](../../Workshop/README.md) - 数据采集工厂
- [Deepness 文档](../../Deepness/README.md) - 深度加工系统
- [Benchmark 文档](../../Benchmark/metrics/README.md) - 评测指标

---

**Apache License 2.0 © 2026 SPHARX. "From data intelligence emerges."**

---

© 2026 SPHARX Ltd. All Rights Reserved.
