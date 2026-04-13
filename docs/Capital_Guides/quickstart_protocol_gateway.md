# AgentOS 多协议网关快速入门

**版本**: Doc V1.8 | **最后更新**: 2026-04-13 | **作者**: AgentOS Team

## 概述

AgentOS 智能网关支持四种主流协议的统一接入与互转，让你可以在同一个网关上同时服务不同类型的客户端：

| 协议 | 版本 | 典型场景 |
|------|------|---------|
| JSON-RPC 2.0 | 2.0 | 内部服务调用、CLI工具 |
| MCP | 0.5+ | 工具集成、Claude Desktop |
| A2A | 0.2+ | 智能体间协作 |
| OpenAI API | v1 | ChatGPT兼容客户端 |

**30分钟目标**: 启动网关 → 发送第一个请求 → 验证协议兼容性

---

## 第一步：环境准备

### 系统要求

| 项目 | 最低要求 |
|------|---------|
| 操作系统 | Ubuntu 22.04+ / macOS 13+ / Windows 11 (WSL2) |
| 编译器 | GCC 11+ / Clang 14+ (C11标准) |
| CMake | 3.20+ |
| Python | 3.10+ (用于测试脚本) |

### 安装依赖

```bash
# Ubuntu/Debian
sudo apt-get install cmake ninja-build gcc g++ \
  libcurl4-openssl-dev libyaml-dev libcjson-dev libssl-dev

# macOS (Homebrew)
brew install cmake ninja curl yaml cjson openssl
```

### 获取源码

```bash
git clone https://github.com/your-org/AgentOS.git
cd AgentOS
git submodule update --init --recursive
```

---

## 第二步：构建与启动

### 编译网关

```bash
mkdir build && cd build

cmake .. -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug \
  -DENABLE_MCP=ON \
  -DENABLE_A2A=ON \
  -DENABLE_OPENAI=ON

cmake --build . --parallel $(nproc)
```

### 启动网关服务

```bash
# 直接启动
./bin/gateway_d --port 18789

# 或使用Docker Compose一键启动完整环境
cd agentos/gateway/docker
docker compose -f docker-compose.dev.yml up -d
```

### 验证网关运行

```bash
curl http://localhost:18789/health
# 预期输出: {"status":"ok","version":"2.0.0","protocols":["jsonrpc","mcp","a2a","openai"]}
```

---

## 第三步：发送第一个请求

### JSON-RPC 2.0 请求

```bash
curl -X POST http://localhost:18789/api/v1/rpc \
  -H "Content-Type: application/json" \
  -d '{
    "jsonrpc": "2.0",
    "method": "agent.list",
    "params": {},
    "id": 1
  }'
```

### OpenAI API 兼容请求

```bash
curl -X POST http://localhost:18789/v1/chat/completions \
  -H "Content-Type: application/json" \
  -H "Authorization: Bearer your-api-key" \
  -d '{
    "model": "agentos-default",
    "messages": [{"role": "user", "content": "Hello, AgentOS!"}]
  }'
```

### MCP 工具调用

```bash
curl -X POST http://localhost:18789/api/v1/mcp \
  -H "Content-Type: application/json" \
  -d '{
    "jsonrpc": "2.0",
    "method": "tools/list",
    "params": {},
    "id": 1
  }'
```

### A2A 智能体发现

```bash
curl -X POST http://localhost:18789/api/v1/a2a \
  -H "Content-Type: application/json" \
  -d '{
    "jsonrpc": "2.0",
    "method": "agent/discover",
    "params": {"capability": "code-generation"},
    "id": 1
  }'
```

---

## 第四步：协议自动检测

网关支持自动检测入站请求的协议类型，无需手动指定路由：

```bash
# 网关自动识别为 JSON-RPC
curl -X POST http://localhost:18789/api/v1/invoke \
  -H "Content-Type: application/json" \
  -d '{"jsonrpc":"2.0","method":"service.list","id":1}'

# 网关自动识别为 OpenAI API
curl -X POST http://localhost:18789/api/v1/invoke \
  -H "Content-Type: application/json" \
  -H "Authorization: Bearer key" \
  -d '{"model":"gpt-4","messages":[]}'
```

---

## 第五步：运行兼容性测试

```bash
cd AgentOS

# 运行完整协议兼容性测试套件
python3 tests/integration/test_protocol_compatibility.py \
  --gateway http://localhost:18789 \
  --verbose

# 运行性能基准测试
python3 tests/benchmarks/benchmark_performance.py \
  --gateway http://localhost:18789 \
  --iterations 50
```

---

## 使用桌面客户端

AgentOS 提供基于 Tauri v2 的桌面客户端，内置协议游乐场：

```bash
cd scripts/desktop-client

# 安装前端依赖
npm install

# 开发模式启动
npm run tauri dev

# 构建生产版本
npm run tauri build
```

在桌面客户端中，导航到 **Protocol Playground** 页面可以：
- 查看支持的协议列表
- 测试协议连接
- 发送协议消息
- 查看协议能力

---

## 下一步

| 想要... | 阅读 |
|---------|------|
| 深入了解协议集成 | [协议兼容性专项文档](../Capital_Guides/protocol_integration_guide.md) |
| 配置生产环境 | [部署指南](deployment.md) |
| 性能调优 | [性能调优指南](performance-tuning.md) |
| 安全加固 | [安全加固指南](security-hardening.md) |
| 故障排查 | [故障排查手册](../Capital_Guides/troubleshooting_faq.md) |
| 贡献代码 | [CONTRIBUTING.md](../../CONTRIBUTING.md) |

---

## 常见问题

**Q: 网关启动失败，端口被占用？**

```bash
# 检查端口占用
lsof -i :18789 || netstat -tlnp | grep 18789

# 使用其他端口启动
./bin/gateway_d --port 18790
```

**Q: 编译时找不到 cjson？**

```bash
# Ubuntu
sudo apt-get install libcjson-dev

# macOS
brew install cjson
```

**Q: Docker Compose 启动后服务不健康？**

```bash
# 查看服务状态
docker compose -f docker-compose.dev.yml ps

# 查看日志
docker compose -f docker-compose.dev.yml logs gateway
```
