# Channel Daemon (channel_d)

AgentOS 进程间通信通道守护进程，负责 AgentOS 各模块间的消息路由与传输。

## 目录结构

```
channel_d/
├── include/          # 头文件
│   └── channel_service.h
├── src/              # 源代码
│   ├── channel_service.c
│   └── main.c
├── tests/            # 测试代码
│   └── test_channel_e2e.c
├── CMakeLists.txt
└── README.md
```

## 构建

```bash
cd build && cmake .. && make channel_d
```

## 运行

```bash
./build/channel_d
```

## 版本

v0.1.0