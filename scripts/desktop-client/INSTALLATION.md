# AgentOS Desktop Client 安装指南

## 系统要求

### Windows
- **操作系统**: Windows 10 1903+ 或 Windows 11
- **架构**: x64 (Intel/AMD 64位)
- **内存**: 最低 4GB，推荐 8GB+
- **磁盘空间**: 200MB 可用空间
- **运行时**: 无需额外运行时（自包含）

### macOS
- **操作系统**: macOS 12.0 (Monterey) 或更高版本
- **架构**: Intel x64 或 Apple Silicon (M1/M2/M3)
- **内存**: 最低 4GB，推荐 8GB+
- **磁盘空间**: 200MB 可用空间

### Linux
- **操作系统**: Ubuntu 22.04+ / Debian 12+ / Fedora 38+
- **架构**: x64
- **内存**: 最低 4GB，推荐 8GB+
- **磁盘空间**: 200MB 可用空间
- **依赖**: glibc 2.31+

---

## 下载安装

### 方式一：官方发布版（推荐）

访问 [AgentOS 发布页面](https://github.com/SpharxTeam/AgentOS/releases) 下载最新版本：

#### Windows
1. 下载 `AgentOS-Desktop_x64_en-US.msi` 或 `AgentOS-Desktop_x64-setup.exe`
2. 双击安装文件
3. 按照安装向导完成安装
4. 从开始菜单启动 "AgentOS Desktop"

#### macOS
1. 下载 `AgentOS-Desktop_x64.dmg` (Intel) 或 `AgentOS-Desktop_aarch64.dmg` (Apple Silicon)
2. 双击 DMG 文件
3. 将 AgentOS Desktop 拖拽到 Applications 文件夹
4. 从启动台或 Applications 文件夹启动

**首次启动提示**：
- 如果提示"无法验证开发者"，请前往 系统偏好设置 > 安全性与隐私 > 点击"仍要打开"

#### Linux

**Ubuntu/Debian (DEB)**:
```bash
# 下载 .deb 包
wget https://github.com/SpharxTeam/AgentOS/releases/latest/download/AgentOS-Desktop_amd64.deb

# 安装
sudo dpkg -i AgentOS-Desktop_amd64.deb

# 如有依赖问题，运行：
sudo apt-get install -f

# 启动
agentos-desktop
```

**Fedora/RHEL (RPM)**:
```bash
# 下载 .rpm 包
wget https://github.com/SpharxTeam/AgentOS/releases/latest/download/AgentOS-Desktop.x86_64.rpm

# 安装
sudo rpm -i AgentOS-Desktop.x86_64.rpm

# 启动
agentos-desktop
```

**AppImage (通用)**:
```bash
# 下载 AppImage
wget https://github.com/SpharxTeam/AgentOS/releases/latest/download/AgentOS-Desktop_x86_64.AppImage

# 添加执行权限
chmod +x AgentOS-Desktop_x86_64.AppImage

# 运行
./AgentOS-Desktop_x86_64.AppImage
```

---

### 方式二：从源码构建

#### 前置要求

1. **Node.js** >= 18.x
   ```bash
   # 使用 nvm 安装
   curl -o- https://raw.githubusercontent.com/nvm-sh/nvm/v0.39.0/install.sh | bash
   nvm install 18
   nvm use 18
   ```

2. **Rust** >= 1.70
   ```bash
   curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh
   rustup default stable
   ```

3. **平台依赖**

   **Windows**:
   - 安装 [Visual Studio Build Tools](https://visualstudio.microsoft.com/visual-cpp-build-tools/)
   - 选择 "Desktop development with C++"

   **macOS**:
   ```bash
   xcode-select --install
   ```

   **Linux (Ubuntu/Debian)**:
   ```bash
   sudo apt update
   sudo apt install -y libwebkit2gtk-4.1-dev \
     build-essential \
     curl \
     wget \
     file \
     libssl-dev \
     libgtk-3-dev \
     libayatana-appindicator3-dev \
     librsvg2-dev
   ```

#### 构建步骤

```bash
# 1. 克隆仓库
git clone https://github.com/SpharxTeam/AgentOS.git
cd AgentOS/scripts/desktop-client

# 2. 安装依赖
npm install

# 3. 生成图标（首次构建）
cd src-tauri/icons
python generate_icons.py
cd ../..

# 4. 开发模式运行
npm run tauri dev

# 5. 构建生产版本
npm run tauri build

# 构建产物位置：
#   Windows: src-tauri/target/release/bundle/msi/
#   macOS:   src-tauri/target/release/bundle/dmg/
#   Linux:   src-tauri/target/release/bundle/deb/
#            src-tauri/target/release/bundle/appimage/
```

---

## 首次启动配置

### 1. 欢迎界面

首次启动时，AgentOS Desktop 会显示欢迎向导：

1. **语言选择**: 选择中文或 English
2. **工作目录**: 设置 AgentOS 项目路径
3. **服务配置**: 配置 Docker 服务连接

### 2. 连接 AgentOS 服务

客户端需要连接到运行中的 AgentOS 后端服务：

**选项 A: 使用 Docker（推荐）**
```bash
# 在 AgentOS 项目目录下
cd AgentOS
docker-compose -f scripts/deploy/docker/docker-compose.yml up -d
```

**选项 B: 本地开发服务**
```bash
# 启动开发服务
./scripts/install.sh --mode dev
```

### 3. 验证连接

在 Dashboard 页面查看服务状态：
- ✅ 绿色：服务正常运行
- 🟡 黄色：服务部分可用
- 🔴 红色：服务未连接

---

## 常见问题

### Windows

**Q: 安装时提示"Windows 已保护你的电脑"**
- 点击"更多信息" > "仍要运行"

**Q: 启动时闪退**
- 检查是否安装了最新版本
- 查看日志文件：`%APPDATA%\AgentOS Desktop\logs\`

**Q: 无法连接服务**
- 确认 Docker Desktop 已启动
- 检查防火墙设置，允许 AgentOS Desktop

### macOS

**Q: 提示"无法打开，因为无法验证开发者"**
```bash
# 方法 1: 系统偏好设置
系统偏好设置 > 安全性与隐私 > 通用 > 点击"仍要打开"

# 方法 2: 命令行移除隔离属性
xattr -cr /Applications/AgentOS\ Desktop.app
```

**Q: Apple Silicon Mac 提示需要 Rosetta**
- Intel 版本需要 Rosetta 2
- 推荐下载 ARM64 原生版本

### Linux

**Q: AppImage 无法运行**
```bash
# 安装 FUSE
sudo apt install libfuse2  # Ubuntu/Debian
sudo dnf install fuse      # Fedora
```

**Q: 缺少依赖**
```bash
# Ubuntu/Debian
sudo apt install libwebkit2gtk-4.1-0 libgtk-3-0

# Fedora
sudo dnf install webkit2gtk4.1 gtk3
```

---

## 自动更新

AgentOS Desktop 支持自动更新：

1. 启动时自动检查更新
2. 发现新版本时显示通知
3. 点击"立即更新"下载安装
4. 更新完成后自动重启

手动检查更新：
- 菜单栏：Help > Check for Updates
- 快捷键：`Ctrl+U` (Windows/Linux) / `Cmd+U` (macOS)

---

## 卸载

### Windows
- 控制面板 > 程序和功能 > AgentOS Desktop > 卸载

### macOS
- 将 AgentOS Desktop.app 从 Applications 拖到废纸篓

### Linux
```bash
# DEB 包
sudo dpkg --remove agentos-desktop

# RPM 包
sudo rpm --erase agentos-desktop

# AppImage
rm AgentOS-Desktop_x86_64.AppImage
```

---

## 获取帮助

- 📖 **文档**: https://docs.agentos.io
- 🐛 **问题反馈**: https://github.com/SpharxTeam/AgentOS/issues
- 💬 **社区讨论**: https://github.com/SpharxTeam/AgentOS/discussions
- 📧 **邮件支持**: support@spharx.cn

---

**© 2026 SPHARX Ltd. All Rights Reserved.**
