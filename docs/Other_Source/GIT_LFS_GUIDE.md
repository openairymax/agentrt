# Git LFS 快速指南

## 🚀 快速开始

### 1. 安装 Git LFS

```bash
# Windows (推荐 Chocolatey)
choco install git-lfs

# Windows (PowerShell)
git lfs install

# macOS (Homebrew)
brew install git-lfs

# Linux (Ubuntu/Debian)
sudo apt-get install git-lfs

# 通用方法：下载安装
# https://git-lfs.github.com/
```

### 2. 初始化（只需一次）

```bash
git lfs install
```

### 3. 克隆项目

```bash
git clone https://github.com/SpharxTeam/AgentOS.git
cd AgentOS
```

### 4. 拉取大文件

```bash
# 拉取所有 LFS 文件
git lfs pull

# 或只拉取特定文件
git lfs pull docs/Other_Source/Spharx\ AgentOS\ 客户端演示.mp4
```

---

## 🔍 常用命令

### 查看 LFS 文件

```bash
# 列出所有 LFS 文件
git lfs ls-files

# 查看 LFS 状态
git lfs status
```

### 拉取 LFS 文件

```bash
# 拉取所有分支的 LFS 文件
git lfs fetch --all

# 检出 LFS 文件
git lfs checkout
```

### 跟踪新文件

```bash
# 跟踪 mp4 文件
git lfs track "*.mp4"

# 跟踪特定目录
git lfs track "docs/Other_Source/*"

# 保存配置
git add .gitattributes
git commit -m "Add LFS tracking"
```

---

## ⚠️ 常见问题

### 问题 1: 文件显示为 LFS 指针

**症状**:
```
version https://git-lfs.github.com/spec/v1
oid sha256:abc123...
size 1234567
```

**解决**:
```bash
git lfs fetch --all
git lfs checkout
```

### 问题 2: 忘记安装 LFS 就克隆了

**解决**:
```bash
# 重新拉取所有文件
git lfs fetch --all
git lfs checkout

# 或重新克隆
rm -rf AgentOS
git clone https://github.com/SpharxTeam/AgentOS.git
```

### 问题 3: 推送失败

**解决**:
```bash
# 确保 LFS 文件已上传
git lfs push origin main
```

---

## 📚 更多信息

- [Git LFS 官方文档](https://github.com/git-lfs/git-lfs/wiki)
- [GitHub LFS 指南](https://docs.github.com/en/repositories/working-with-files/managing-large-files/configuring-git-large-file-storage)

---

© 2026 SPHARX Ltd. All Rights Reserved.
