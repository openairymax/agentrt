# AgentOS 内核编译指南

## 1. 环境要求

- **操作系统**: Linux (推荐 Ubuntu 22.04+), macOS 13+, Windows 11 (需 WSL2)
- **编译器**: GCC 11+ 或 Clang 14+
- **构建工具**: CMake 3.20+, Ninja 或 Make
- **依赖库**:
  - OpenSSL (用于加密)
  - libevent (事件循环)
  - pthread (线程)

## 2. 获取源码

git clone https://github.com/agentos/agentos.git
cd agentos

## 3. 配置构建

mkdir build && cd build
cmake ../atoms -DCMAKE_BUILD_TYPE=Release \
                 -DBUILD_TESTS=ON \
                 -DENABLE_TRACING=ON

### 可选 CMake 变量

| 变量 | 说明 | 默认值 |
| :--- | :--- | :--- |
| `CMAKE_BUILD_TYPE` | Debug/Release/RelWithDebInfo | `Release` |
| `BUILD_TESTS` | 构建单元测试 | `OFF` |
| `ENABLE_TRACING` | 启用 OpenTelemetry 追踪 | `OFF` |
| `ENABLE_ASAN` | 启用 AddressSanitizer | `OFF` |
| `USE_LLVM` | 使用 LLVM 工具链 | `OFF` |

## 4. 编译
```
cmake --build . --parallel 4
```
## 5. 运行测试
```
ctest --output-on-failure
```
## 6. 安装
```
sudo cmake --install .
默认安装到 `/usr/local/agentos`。
```
## 7. 交叉编译示例

以 ARM64 为例：
```
cmake ../atoms \
    -DCMAKE_TOOLCHAIN_FILE=../cmake/toolchain/arm64.cmake \
    -DCMAKE_BUILD_TYPE=Release
```
## 8. 常见问题

**Q: 编译时找不到 OpenSSL**
A: 安装开发包：`sudo apt install libssl-dev` (Ubuntu/Debian) 或 `brew install openssl` (macOS)。

**Q: 单元测试失败**
A: 检查环境变量 `AGENTOS_TEST_DIR` 是否有写权限，并确保所有依赖服务已启动。

---

© 2026 SPHARX Ltd. All Rights Reserved.
