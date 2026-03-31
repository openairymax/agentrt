﻿﻿﻿﻿﻿# cupolas 虚拟工位模块 (Workbench)

**版本**: v1.0.0.6  
**最后更新**: 2026-03-25  
**许可证**: Apache License 2.0

---

## 🎯 模块定位

`workbench/` 是 cupolas 安全沙箱的**隔离执行环境**，为 Agent 提供安全的运行沙箱。

核心功能：
- **进程级隔离**: 基于 namespaces + cgroups 的资源限制
- **容器级隔离**: 基于 runc/Docker 的完整容器化（可选）
- **资源管控**: CPU、内存、网络 IO 限制
- **文件系统隔离**: 挂载命名空间、只读根文件系统

**重要**: 所有不受信任的 Agent 代码都必须在虚拟工位中执行。

---

## 📁 目录结构

```
workbench/
├── workbench.h               # 统一头文件
├── workbench.c               # 核心协调器
├── workbench_process.c       # 进程模式实现
├── workbench_process_exec.c  # 进程执行管理
├── workbench_process_child.c # 子进程管理
├── workbench_container.c     # 容器模式实现（可选）
└── workbench_container.h     # 容器接口
```

---

## 🔧 核心功能详解

### 1. 进程级隔离 (`workbench_process.c`)

轻量级的进程隔离方案，基于 Linux namespaces 和 cgroups。

#### 隔离机制

```c
#include <workbench.h>

// 创建隔离的进程环境
workbench_config_t manager = {
    .isolation_type = WORKBENCH_ISOLATION_PROCESS,
    
    // 命名空间隔离
    .namespaces = CLONE_NEWPID |    // PID 命名空间
                  CLONE_NEWNET |    // 网络命名空间
                  CLONE_NEWNS |     // 挂载命名空间
                  CLONE_NEWUTS |    // Hostname 命名空间
                  CLONE_NEWIPC,     // IPC 命名空间
    
    // cgroups 资源限制
    .memory_limit_mb = 512,         // 内存限制 512MB
    .cpu_quota_us = 500000,         // CPU 配额 50% (500000μs)
    .pids_max = 10,                 // 最多 10 个进程
    
    // 文件系统隔离
    .root_dir = "/var/agentos/sandbox",  // 根目录
    .readonly_root = true,                // 只读根文件系统
    .tmpfs_size_mb = 64,                  // tmpfs 大小
    
    // 网络隔离
    .network_enabled = false,             // 禁用网络
    .allowed_ports = NULL,                // 允许的端口列表
};

workbench_t* wb = workbench_create(&manager);
```

#### 使用示例

```c
// 在沙箱中执行命令
pid_t child_pid = workbench_spawn(wb, 
    "/usr/bin/python3",      // 可执行文件
    args,                     // 参数列表 ["python3", "script.py"]
    env                       // 环境变量
);

// 等待执行完成
int status;
waitpid(child_pid, &status, 0);

// 获取退出码
if (WIFEXITED(status)) {
    int exit_code = WEXITSTATUS(status);
    printf("Process exited with code: %d\n", exit_code);
}

// 销毁沙箱
workbench_destroy(wb);
```

#### 命名空间详解

```
+------------------+
| Parent Namespace |
|  PID: 1, 2, 3... |
|  Net: eth0...   |
|  Mount: / ...   |
+------------------+
         ↓ (clone)
+------------------+
| Child Namespace  |
|  PID: 1          | ← 沙箱内的 init 进程
|  Net: lo only    | ← 仅 loopback
|  Mount: /sandbox| ← 隔离的挂载点
+------------------+
```

### 2. 容器级隔离 (`workbench_container.c`)

完整的容器化方案，基于 runc 或 Docker。

#### 架构设计

```
+------------------+
|   Container      |
|  Runtime (runc)  |
+------------------+
         ↓
+------------------+
|   OCI Runtime    |
|   Spec           |
+------------------+
         ↓
+------------------+
|   Linux Kernel   |
|  (cgroups + ns)  |
+------------------+
```

#### 使用示例

```c
#include <workbench.h>

// 容器配置
workbench_container_config_t container_cfg = {
    .image = "agentos/sandbox:latest",  // 容器镜像
    .runtime = "runc",                   // 运行时
    
    // 资源限制
    .memory_limit_mb = 1024,
    .cpu_cores = 2.0,
    
    // 挂载卷
    .volumes = {
        "/data/input:/input:ro",   // 只读输入
        "/data/output:/output:rw", // 可写输出
        NULL
    },
    
    // 环境变量
    .env = {
        "AGENT_ID=agent-123",
        "SANDBOX_MODE=true",
        NULL
    },
    
    // 工作目录
    .workdir = "/workspace",
    
    // 启动命令
    .cmd = {"/bin/bash", "-c", "python3 agent.py", NULL}
};

// 创建并启动容器
workbench_container_t* container = 
    workbench_container_create(&container_cfg);

workbench_container_start(container);

// 等待容器完成
workbench_container_wait(container, &exit_code);

// 清理
workbench_container_destroy(container);
```

#### OCI Runtime Spec 配置

```json
{
  "ociVersion": "1.0.2",
  "process": {
    "terminal": true,
    "user": {"uid": 1000, "gid": 1000},
    "args": ["python3", "agent.py"],
    "env": [
      "PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin",
      "HOME=/home/agent"
    ],
    "cwd": "/workspace",
    "capabilities": {
      "bounding": [],
      "effective": [],
      "inheritable": [],
      "permitted": [],
      "ambient": []
    }
  },
  "root": {
    "path": "rootfs",
    "readonly": true
  },
  "hostname": "sandbox-agent-123",
  "mounts": [
    {
      "destination": "/tmp",
      "type": "tmpfs",
      "source": "tmpfs",
      "options": ["nosuid","noexec","nodev","size=64m"]
    }
  ],
  "linux": {
    "resources": {
      "devices": [{"allow": false, "access": "rwm"}],
      "memory": {"limit": 1073741824},
      "cpu": {"quota": 200000, "period": 100000}
    },
    "namespaces": [
      {"type": "pid"},
      {"type": "network"},
      {"type": "mount"},
      {"type": "uts"},
      {"type": "ipc"}
    ]
  }
}
```

### 3. 资源管控

详细的资源限制配置。

#### 内存限制

```c
// 硬限制和软限制
manager.memory_hard_limit_mb = 512;   // 硬限制，超出即 OOM
manager.memory_soft_limit_mb = 400;   // 软限制，触发告警

// OOM 策略
manager.oom_policy = WORKBENCH_OOM_KILL;  // 直接杀死
// 或
manager.oom_policy = WORKBENCH_OOM_PAUSE; // 暂停并告警
```

#### CPU 限制

```c
// CPU 配额（微秒级别）
manager.cpu_quota_us = 500000;   // 每周期 500ms
manager.cpu_period_us = 1000000; // 周期 1s
// 相当于 50% CPU

// CPU 核心绑定
manager.cpu_affinity = {0, 1};   // 只允许在 Core 0,1 运行
```

#### 进程数限制

```c
manager.pids_max = 10;  // 包括子进程
```

### 4. 文件系统隔离

#### 挂载命名空间

```c
// 创建私有挂载点
manager.root_dir = "/var/agentos/sandbox/root";
manager.mount_tmpfs = true;
manager.tmpfs_size_mb = 64;

// 绑定挂载（Bind Mount）
manager.bind_mounts = {
    {"/data/input", "/input", "ro"},   // 只读挂载
    {"/data/output", "/output", "rw"}, // 读写挂载
    {NULL, NULL, NULL}
};
```

#### 文件系统视图

```
沙箱内视角:
/
├── bin/         (只读，基础工具)
├── lib/         (只读，系统库)
├── tmp/         (tmpfs, 64MB)
├── input/       (绑定挂载，只读)
├── output/      (绑定挂载，读写)
└── workspace/   (工作目录)
```

---

## 🚀 快速开始

### 编译

```bash
cd AgentOS/cupolas
mkdir build && cd build
cmake ..
make workbench  # 只编译 workbench 模块
```

### 测试

```bash
cd build
ctest -R workbench          # 运行 workbench 测试
./test_workbench_process    # 测试进程模式
./test_workbench_container  # 测试容器模式（需要 runc）
```

### 完整集成示例

```c
#include <cupolas.h>

int main() {
    // 1. 初始化 cupolas（包含 workbench）
    domes_config_t manager = {
        .workbench_enabled = true,
        .workbench_type = WORKBENCH_ISOLATION_PROCESS,
        .workbench_memory_limit_mb = 512,
        .workbench_cpu_quota_percent = 50,
        .workbench_network_enabled = false,
        .workbench_root_dir = "/var/agentos/sandbox"
    };
    
    domes_t* cupolas = domes_init(&manager);
    
    // 2. 创建沙箱环境
    workbench_t* sandbox = domes_workbench_create(cupolas,
        "agent-data-processor",  // Agent 名称
        NULL                     // 使用默认配置
    );
    
    if (!sandbox) {
        LOG_ERROR("Failed to create sandbox");
        return -1;
    }
    
    // 3. 在沙箱中执行 Agent 代码
    char* args[] = {
        "python3",
        "/agents/data_processor.py",
        "--input", "/input/data.csv",
        "--output", "/output/result.json",
        NULL
    };
    
    pid_t pid = workbench_spawn(sandbox,
        "/usr/bin/python3",
        args,
        NULL
    );
    
    if (pid < 0) {
        LOG_ERROR("Failed to spawn process");
        domes_workbench_destroy(cupolas, sandbox);
        return -1;
    }
    
    // 4. 监控执行状态
    int status;
    struct rusage usage;
    
    // 等待子进程完成
    if (wait4(pid, &status, 0, &usage) < 0) {
        LOG_ERROR("Wait failed");
    }
    
    // 5. 检查资源使用
    printf("Max RSS: %ld KB\n", usage.ru_maxrss);
    printf("User CPU: %ld.%lds\n", 
        usage.ru_utime.tv_sec, usage.ru_utime.tv_usec);
    
    // 6. 记录审计日志
    if (WIFEXITED(status)) {
        domes_audit(cupolas, AUDIT_LEVEL_INFO,
            "workbench.completed", "agent-data-processor",
            "execution", "completed",
            "exit_code=%d,max_rss=%ld",
            WEXITSTATUS(status), usage.ru_maxrss);
    } else if (WIFSIGNALED(status)) {
        // 可能被 OOM killer 杀死
        domes_audit(cupolas, AUDIT_LEVEL_WARNING,
            "workbench.killed", "agent-data-processor",
            "execution", "killed_by_signal",
            "signal=%d", WTERMSIG(status));
    }
    
    // 7. 清理
    domes_workbench_destroy(cupolas, sandbox);
    domes_destroy(cupolas);
    return 0;
}
```

---

## 📊 性能指标

### 启动时间

| 模式 | 启动时间 | 说明 |
|------|----------|------|
| 进程模式 | < 10ms | 轻量级，推荐 |
| 容器模式 | < 500ms | 完整隔离，较重 |

### 资源开销

| 指标 | 进程模式 | 容器模式 |
|------|----------|----------|
| 内存占用 | < 10MB | ~100MB |
| 磁盘占用 | < 50MB | ~500MB |
| CPU 开销 | < 1% | < 5% |

### 隔离强度

| 特性 | 进程模式 | 容器模式 |
|------|----------|----------|
| PID 隔离 | ✅ | ✅ |
| 网络隔离 | ✅ | ✅ |
| 文件系统隔离 | ⚠️ (部分) | ✅ (完整) |
| 用户隔离 | ❌ | ⚠️ (需 rootless) |
| 内核隔离 | ❌ | ❌ (需 gVisor/Kata) |

---

## 🛠️ 最佳实践

### 1. 选择合适的隔离模式

```c
// ✅ 推荐：根据安全需求选择
if (trusted_agent) {
    // 受信任的 Agent：进程模式即可
    manager.isolation_type = WORKBENCH_ISOLATION_PROCESS;
} else if (untrusted_agent) {
    // 不受信任：容器模式
    manager.isolation_type = WORKBENCH_ISOLATION_CONTAINER;
} else if (high_security_required) {
    // 高安全场景：考虑 gVisor 或 Kata Containers
    manager.isolation_type = WORKBENCH_ISOLATION_GVISOR;
}
```

### 2. 合理设置资源限制

```c
// 根据 Agent 类型设置
if (cpu_intensive_agent) {
    manager.cpu_quota_percent = 200;  // 2 核
    manager.memory_limit_mb = 2048;   // 2GB
} else if (io_intensive_agent) {
    manager.io_read_bps = 100 * 1024 * 1024;  // 100MB/s
    manager.io_write_bps = 50 * 1024 * 1024;  // 50MB/s
} else {
    // 通用配置
    manager.cpu_quota_percent = 50;
    manager.memory_limit_mb = 512;
}
```

### 3. 最小权限原则

```c
// ✅ 推荐：禁用不必要的功能
manager.network_enabled = false;      // 默认禁用网络
manager.device_access = false;        // 禁用设备访问
manager.capabilities = 0;             // 清空所有能力

// 按需开放
if (needs_network) {
    manager.allowed_ports = (int[]){80, 443, -1};  // 仅允许特定端口
}
```

### 4. 监控和告警

```c
// 定期检查资源使用
while (workbench_is_running(sandbox)) {
    workbench_stats_t stats;
    workbench_get_stats(sandbox, &stats);
    
    // CPU 超限告警
    if (stats.cpu_usage_percent > 90) {
        LOG_WARNING("CPU usage high: %.2f%%", 
            stats.cpu_usage_percent);
    }
    
    // 内存接近限制告警
    if (stats.memory_usage_mb > manager.memory_limit_mb * 0.9) {
        LOG_WARNING("Memory near limit: %.2fMB / %dMB",
            stats.memory_usage_mb, manager.memory_limit_mb);
    }
    
    sleep(1);
}
```

---

## 🔗 相关文档

- [cupolas 总览](../README.md)
- [审计日志模块](../audit/README.md)
- [权限裁决模块](../permission/README.md)
- [输入净化模块](../sanitizer/README.md)
- [容器安全指南](../../../paper/specifications/container_security.md)

---

## 🤝 贡献指南

欢迎提交 Issue 或 Pull Request！

## 📞 联系方式

- **维护者**: AgentOS 安全委员会
- **安全问题**: wangliren@spharx.cn
- **问题反馈**: https://github.com/SpharxTeam/AgentOS/issues

---

© 2026 SPHARX Ltd. All Rights Reserved.

*"Safe execution in isolated space. 隔离空间，安全执行。"*

