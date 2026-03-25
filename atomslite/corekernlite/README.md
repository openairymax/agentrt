# AgentOS CoreKernLite

**版本**: 1.0.0  
**状态**: 开发中  
**许可证**: GPL-3.0  
**版权所有**: (c) 2026 SPHARX. All Rights Reserved.

## 概述

AgentOS CoreKernLite 是 AgentOS 微内核的轻量级实现版本。它保留了核心功能，移除了复杂特性，提供简洁、易用、便于集成和移植的微内核核心。

### 设计目标

- **简洁**: 移除复杂特性，保留核心功能
- **易用**: 提供简洁的API接口
- **可移植**: 跨平台支持（Windows/Linux/macOS）
- **高效**: 最小化开销，优化性能

### 与 CoreKern 的区别

| 特性 | CoreKern | CoreKernLite |
|------|----------|--------------|
| 复杂IPC | ✓ | ✗ |
| 内存池 | ✓ | ✗ |
| 内存守卫 | ✓ | ✗ |
| 事件系统 | ✓ | ✗ |
| 定时器 | ✓ | ✗ |
| 基本内存管理 | ✓ | ✓ |
| 任务调度 | ✓ | ✓ |
| 同步原语 | ✓ | ✓ |
| 时间管理 | ✓ | ✓ |

## 模块结构

```
corekernlite/
├── include/              # 公共头文件
│   ├── export.h          # 符号导出定义
│   ├── error.h           # 错误码定义
│   ├── mem.h             # 内存管理接口
│   ├── task.h            # 任务调度接口
│   ├── time.h            # 时间管理接口
│   └── agentos_lite.h    # 统一入口头文件
├── src/                  # 源文件实现
│   ├── error.c           # 错误处理实现
│   ├── mem.c             # 内存管理实现
│   ├── task.c            # 任务调度实现
│   ├── time.c            # 时间管理实现
│   └── agentos_lite.c    # 主入口实现
├── tests/                # 测试用例
│   └── test_main.c       # 单元测试主程序
├── CMakeLists.txt        # CMake构建配置
└── README.md             # 本文档
```

## 快速开始

### 构建要求

- CMake 3.10+
- C11 兼容编译器
  - Windows: MSVC 2015+ 或 MinGW-w64
  - Linux: GCC 4.9+ 或 Clang 3.5+
  - macOS: Clang 3.5+ (Xcode 6+)

### 构建步骤

```bash
# 创建构建目录
mkdir build && cd build

# 配置
cmake .. -DCMAKE_BUILD_TYPE=Release

# 构建
cmake --build .

# 运行测试
ctest
```

### CMake 选项

| 选项 | 默认值 | 说明 |
|------|--------|------|
| `AGENTOS_LITE_BUILD_SHARED` | ON | 构建动态库 |
| `AGENTOS_LITE_BUILD_TESTS` | ON | 构建测试 |
| `AGENTOS_LITE_ENABLE_DEBUG` | OFF | 启用调试日志 |

## API 使用

### 初始化与清理

```c
#include <agentos_lite.h>

int main(void) {
    // 初始化内核
    agentos_lite_error_t err = agentos_lite_init();
    if (err != AGENTOS_LITE_SUCCESS) {
        fprintf(stderr, "初始化失败: %s\n", agentos_lite_strerror(err));
        return -1;
    }
    
    // 获取版本信息
    printf("AgentOS Lite %s\n", agentos_lite_version());
    
    // ... 使用其他API ...
    
    // 清理内核
    agentos_lite_cleanup();
    return 0;
}
```

### 内存管理

```c
// 分配内存
void* ptr = agentos_lite_mem_alloc(1024);
if (ptr) {
    // 使用内存
    memset(ptr, 0, 1024);
    
    // 释放内存
    agentos_lite_mem_free(ptr);
}

// 对齐内存分配
void* aligned = agentos_lite_mem_aligned_alloc(4096, 64);
if (aligned) {
    // 使用对齐内存
    agentos_lite_mem_aligned_free(aligned);
}

// 内存统计
size_t total, used, peak;
agentos_lite_mem_stats(&total, &used, &peak);
printf("内存使用: %zu/%zu 字节 (峰值: %zu)\n", used, total, peak);
```

### 任务调度

```c
// 创建互斥锁
agentos_lite_mutex_t* mutex = agentos_lite_mutex_create();

// 加锁/解锁
agentos_lite_mutex_lock(mutex);
// 临界区代码
agentos_lite_mutex_unlock(mutex);

// 销毁互斥锁
agentos_lite_mutex_destroy(mutex);

// 创建条件变量
agentos_lite_cond_t* cond = agentos_lite_cond_create();

// 等待条件（带超时）
agentos_lite_error_t err = agentos_lite_cond_wait(cond, mutex, 1000);
if (err == AGENTOS_LITE_ETIMEDOUT) {
    printf("等待超时\n");
}

// 唤醒等待线程
agentos_lite_cond_signal(cond);
agentos_lite_cond_broadcast(cond);

// 销毁条件变量
agentos_lite_cond_destroy(cond);
```

### 线程管理

```c
// 线程函数
void my_thread_func(void* arg) {
    int* value = (int*)arg;
    *value = 42;
    agentos_lite_task_sleep(100);  // 休眠100ms
}

// 创建线程
agentos_lite_thread_t thread;
agentos_lite_thread_attr_t attr = {
    .name = "worker",
    .priority = AGENTOS_LITE_TASK_PRIORITY_NORMAL,
    .stack_size = 0  // 使用默认栈大小
};

int result = 0;
agentos_lite_error_t err = agentos_lite_thread_create(
    &thread, &attr, my_thread_func, &result);

if (err == AGENTOS_LITE_SUCCESS) {
    // 等待线程结束
    agentos_lite_thread_join(thread);
    printf("线程结果: %d\n", result);
}
```

### 时间管理

```c
// 获取高精度时间戳
uint64_t ns = agentos_lite_time_get_ns();  // 纳秒
uint64_t us = agentos_lite_time_get_us();  // 微秒
uint64_t ms = agentos_lite_time_get_ms();  // 毫秒
uint64_t unix_ts = agentos_lite_time_get_unix();  // Unix时间戳

// 计时
uint64_t start = agentos_lite_time_get_ns();
// 执行操作
uint64_t end = agentos_lite_time_get_ns();
uint64_t elapsed = agentos_lite_time_diff_ns(start, end);
printf("耗时: %llu 纳秒\n", (unsigned long long)elapsed);
```

## 错误码

| 错误码 | 值 | 说明 |
|--------|-----|------|
| `AGENTOS_LITE_SUCCESS` | 0 | 成功 |
| `AGENTOS_LITE_EINVAL` | -1 | 参数无效 |
| `AGENTOS_LITE_ENOMEM` | -2 | 内存不足 |
| `AGENTOS_LITE_EBUSY` | -3 | 资源忙 |
| `AGENTOS_LITE_ENOENT` | -4 | 实体不存在 |
| `AGENTOS_LITE_EPERM` | -5 | 操作不允许 |
| `AGENTOS_LITE_ETIMEDOUT` | -6 | 操作超时 |
| `AGENTOS_LITE_EEXIST` | -7 | 实体已存在 |
| `AGENTOS_LITE_ECANCELED` | -8 | 操作取消 |
| `AGENTOS_LITE_ENOTSUP` | -9 | 不支持 |
| `AGENTOS_LITE_EIO` | -10 | I/O错误 |

## 性能特点

- **内存开销**: 最小化，仅维护基本统计信息
- **线程开销**: 使用系统原生线程，无额外调度层
- **时间精度**: 纳秒级精度（取决于平台）
- **启动时间**: 毫秒级初始化

## 平台支持

### Windows
- Windows 7 及以上
- MSVC 2015+ 或 MinGW-w64
- 使用 `CRITICAL_SECTION` 和 `CONDITION_VARIABLE`

### Linux
- glibc 2.17+ 或 musl 1.1+
- GCC 4.9+ 或 Clang 3.5+
- 使用 `pthread` 线程库

### macOS
- macOS 10.12+
- Clang 3.5+ (Xcode 6+)
- 使用 `pthread` 线程库

## 设计原则

### 工程控制论
- **反馈闭环**: 所有操作都有明确的返回值和错误处理
- **自适应**: 根据系统状态动态调整

### 系统工程
- **层次分解**: 清晰的模块划分
- **接口稳定**: API保持向后兼容

### 双系统理论
- **System 1**: 快速路径，简单操作无额外开销
- **System 2**: 复杂操作，提供完整功能

## 贡献指南

1. Fork 项目
2. 创建特性分支 (`git checkout -b feature/amazing-feature`)
3. 提交更改 (`git commit -m 'Add amazing feature'`)
4. 推送到分支 (`git push origin feature/amazing-feature`)
5. 创建 Pull Request

### 编码规范

- 使用 C11 标准
- 遵循 `clang-format` 格式化
- 所有公共API必须有Doxygen注释
- 单元测试覆盖率 > 80%

## 许可证

本项目采用 GPL-3.0 许可证。详见 [LICENSE](LICENSE) 文件。

## 联系方式

- **维护者**: AgentOS 架构委员会
- **问题反馈**: https://github.com/agentos/agentos/issues
- **文档**: https://docs.agentos.io/corekernlite

---

**版权所有** (c) 2026 SPHARX. All Rights Reserved.  
"From data intelligence emerges."
