# AgentOS 轻量级内核层 (AtomsLite)

**版本**: v1.0.0.6  
**最后更新**: 2026-03-25  
**许可证**: Apache License 2.0

---

## 🎯 模块定位

`atomslite/` 是 AgentOS 的**精简版内核实现**，为资源受限环境设计。相比完整的 `atoms/` 层，本模块提供：

- **轻量化核心** (`corekernlite/`): 仅包含最基础的 IPC 和内存管理
- **裁剪的设计**: 去除复杂调度器和大容量记忆系统
- **嵌入式友好**: 适用于 IoT 设备、边缘计算等场景
- **API 兼容**: 与完整版 atoms 保持接口一致性

**适用场景**:
- ✅ 资源受限的嵌入式设备
- ✅ 对启动时间敏感的应用 (< 100ms)
- ✅ 功能单一的专用智能体
- ❌ 需要复杂认知的大型智能体 (请使用 `atoms/`)

---

## 📁 目录结构

```
atomslite/
├── README.md                 # 本文档
├── CMakeLists.txt            # 编译配置
└── corekernlite/
    ├── README.md             # 轻量级内核文档
    ├── include/
    │   ├── agentos_lite.h    # 统一头文件
    │   ├── ipc_lite.h        # 轻量级 IPC
    │   └── mem_lite.h        # 轻量级内存管理
    ├── src/
    │   ├── ipc_lite.c        # IPC 实现
    │   └── mem_lite.c        # 内存实现
    └── tests/
        └── test_corekernlite.c
```

---

## 🔧 核心功能

### 1. 轻量级 IPC (`ipc_lite.h`)

简化的进程间通信机制，仅支持同步调用。

```c
#include <agentos_lite.h>

// 创建 IPC 通道
ipc_channel_t* channel = ipc_lite_create("service:echo");

// 同步调用
int ret = ipc_lite_call(channel, ECHO_CMD, data, size);

// 销毁通道
ipc_lite_destroy(channel);
```

**与完整版对比**:
| 特性 | atomslite | atoms |
|------|-----------|-------|
| 通信方式 | 仅同步 | 同步 + 异步 |
| 带宽 | < 1MB/s | 无限制 |
| 延迟 | < 10μs | < 50μs |
| 代码体积 | ~5KB | ~50KB |

### 2. 轻量级内存管理 (`mem_lite.h`)

静态内存池分配器，无碎片化。

```c
#include <agentos_lite.h>

// 初始化内存池 (固定大小)
mem_pool_t* pool = mem_lite_pool_create(4096);  // 4KB

// 分配内存
void* ptr = mem_lite_alloc(pool, 256);

// 释放内存
mem_lite_free(pool, ptr);

// 获取使用统计
mem_lite_stats_t stats;
mem_lite_get_stats(pool, &stats);
printf("Used: %d bytes\n", stats.used);
```

**内存模型**:
```
+------------------+
|  Static Pool     | 4KB
|  +------------+  |
|  | Used       |  |
|  +------------+  |
|  | Free       |  |
|  +------------+  |
+------------------+
```

---

## 🚀 快速开始

### 编译

```bash
cd AgentOS/atomslite
mkdir build && cd build
cmake ..
make                    # 编译所有
make corekernlite       # 只编译核心
```

### 测试

```bash
cd build
ctest                   # 运行所有测试
ctest -R corekernlite   # 只测核心模块
```

### 集成示例

```c
// main.c
#include <agentos_lite.h>

int main() {
    // 初始化内核 (耗时 < 50ms)
    agentos_lite_init();
    
    // 创建 IPC 服务
    ipc_service_create("my_service", service_handler);
    
    // 运行事件循环
    agentos_lite_run();
    
    // 清理
    agentos_lite_cleanup();
    return 0;
}
```

**编译命令**:
```bash
gcc -I../include main.c -L./build -lcorekernlite -o my_agent
```

---

## 📊 性能指标

| 指标 | 数值 | 测试条件 |
|------|------|---------|
| 启动时间 | < 50ms | Cold start |
| 内存占用 | < 100KB | 典型场景 |
| IPC 延迟 | < 10μs | 同步调用 |
| 代码体积 | ~20KB | 编译后二进制 |
| 最小 RAM 需求 | 256KB | 可运行 |

**与完整版对比**:
| 指标 | atomslite | atoms | 倍数 |
|------|-----------|-------|------|
| 启动时间 | 50ms | 500ms | 10x |
| 内存占用 | 100KB | 2MB | 20x |
| 代码体积 | 20KB | 200KB | 10x |

---

## 🔗 与完整版 Atom 的兼容性

### API 兼容

大部分接口保持一致，可以直接替换：

```c
// 代码在 atomslite 和 atoms 中都可以编译
#include <agentos.h>  // 或 <agentos_lite.h>

ipc_channel_t* ch = ipc_create("service");
mem_pool_t* pool = mem_pool_create(1024);
```

### 功能差异

| 功能 | atomslite | atoms | 说明 |
|------|-----------|-------|------|
| IPC | ✅ (简化版) | ✅ (完整版) | Lite 版仅支持同步 |
| 内存管理 | ✅ (静态池) | ✅ (动态 + 池) | Lite 版无碎片整理 |
| 任务调度 | ❌ | ✅ | Lite 版使用轮询 |
| 时间服务 | ❌ | ✅ | Lite 版依赖系统时钟 |
| 记忆系统 | ❌ | ✅ | Lite 版无分层记忆 |

---

## 🛠️ 使用场景

### ✅ 推荐场景

1. **IoT 设备智能体**
   ```
   传感器 → atomslite → 本地决策 → 执行器
   ```

2. **边缘计算节点**
   ```
   数据预处理 → atomslite → 云端协同
   ```

3. **嵌入式 AI 应用**
   ```
   摄像头 → atomslite + TinyML → 控制信号
   ```

### ❌ 不推荐场景

1. **复杂认知智能体** → 使用 `atoms/`
2. **多智能体协作** → 使用 `atoms/`
3. **需要长期记忆** → 使用 `atoms/memoryrovol/`

---

## 📖 相关文档

- [完整内核层 atoms/](../atoms/README.md)
- [微内核基础 corekern](../atoms/corekern/README.md)
- [系统调用层](../atoms/syscall/README.md)
- [项目根目录 README](../../README.md)

---

## 🤝 贡献指南

欢迎提交 Issue 或 Pull Request！

## 📞 联系方式

- **维护者**: AgentOS 架构委员会
- **技术支持**: lidecheng@spharx.cn
- **问题反馈**: https://github.com/SpharxTeam/AgentOS/issues

---

© 2026 SPHARX Ltd. All Rights Reserved.

*"Lightweight but powerful. 轻量而强大。"*
