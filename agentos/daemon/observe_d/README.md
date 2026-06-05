# Observe Daemon (observe_d)

AgentOS 系统观测守护进程，负责系统运行状态的实时监控与数据采集。

## 目录结构

```
observe_d/
├── src/
│   └── main.c
├── CMakeLists.txt
└── README.md
```

## 构建

```bash
cd build && cmake .. && make observe_d
```

## 运行

```bash
./build/observe_d
```

## 版本

v0.1.0