# Info Daemon (info_d)

AgentOS 系统信息守护进程，提供系统状态、版本信息、运行指标等查询服务。

## 目录结构

```
info_d/
├── src/
│   └── main.c
├── CMakeLists.txt
└── README.md
```

## 构建

```bash
cd build && cmake .. && make info_d
```

## 运行

```bash
./build/info_d
```

## 版本

v0.1.0