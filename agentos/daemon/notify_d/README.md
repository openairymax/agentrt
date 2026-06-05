# Notify Daemon (notify_d)

AgentOS 通知守护进程，负责系统事件的通知分发与推送。

## 目录结构

```
notify_d/
├── src/
│   └── main.c
├── CMakeLists.txt
└── README.md
```

## 构建

```bash
cd build && cmake .. && make notify_d
```

## 运行

```bash
./build/notify_d
```

## 版本

v0.1.0