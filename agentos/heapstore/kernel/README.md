# Heapstore Kernel

AgentOS Heapstore 核心内核模块，提供内存管理、IPC 通信和基础服务。

## 目录结构

```
kernel/
├── ipc/              # 进程间通信
│   ├── binder/       # Binder IPC 实现
│   └── channels/     # 消息通道
├── memory/           # 内存管理
│   ├── index/        # 内存索引
│   ├── meta/         # 元数据管理
│   ├── patterns/     # 内存模式
│   └── raw/          # 原始内存操作
└── services/         # 内核服务
    ├── log_store_service.c
    └── trace_store_service.c
```

## 版本

v0.1.0