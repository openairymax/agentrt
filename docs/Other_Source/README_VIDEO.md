# AgentOS 项目媒体资源说明

**版本**: Doc V1.0  
**最后更新**: 2026-04-10  
**维护者**: AgentOS 架构委员会

---

## 📹 项目演示视频

### 视频位置

本项目演示视频位于：
```
docs/Other_Source/Spharx AgentOS 客户端演示.mp4
```

### Git LFS 配置

由于视频文件较大，本项目使用 **Git LFS (Large File Storage)** 进行版本管理。

#### 1. 安装 Git LFS

在克隆仓库之前，请先安装 Git LFS：

```bash
# Windows (Chocolatey)
choco install git-lfs

# macOS (Homebrew)
brew install git-lfs

# Linux (apt)
sudo apt install git-lfs

# 或者直接下载
# https://git-lfs.github.com/
```

#### 2. 初始化 Git LFS

```bash
git lfs install
```

#### 3. 克隆仓库

```bash
git clone https://github.com/SpharxTeam/AgentOS.git
cd AgentOS
```

#### 4. 拉取 LFS 文件

```bash
git lfs pull
```

或者重新拉取所有 LFS 文件：

```bash
git lfs fetch --all
git lfs checkout
```

---

## 🎥 在 GitHub 上查看视频

### 方法一：GitHub 网页播放器

GitHub 支持直接播放 `.mp4` 文件：

1. 访问文件页面：
   ```
   https://github.com/SpharxTeam/AgentOS/blob/main/docs/Other_Source/Spharx%20AgentOS%20客户端演示.mp4
   ```

2. 点击文件即可在线播放

### 方法二：README 内嵌播放

项目 README 已配置 HTML5 video 标签，在 GitHub 上会自动渲染为播放器。

### 方法三：下载本地播放

如果在线播放有问题，可以下载视频文件：

```bash
# 使用 Git LFS
git lfs pull docs/Other_Source/Spharx\ AgentOS\ 客户端演示.mp4

# 或者直接在 GitHub 上点击"Download"按钮
```

---

## 📊 视频信息

| 属性 | 值 |
|------|-----|
| 文件名 | Spharx AgentOS 客户端演示.mp4 |
| 格式 | MP4 (H.264 编码) |
| 位置 | docs/Other_Source/ |
| 大小 | ~50MB (预估) |
| 时长 | ~5 分钟 (预估) |
| 分辨率 | 1920x1080 (推荐) |

---

## 🔧 故障排查

### 问题 1: 视频文件显示为 Git LFS 指针

**症状**: 视频文件只有几 KB，内容为：
```
version https://git-lfs.github.com/spec/v1
oid sha256:...
size ...
```

**解决方案**:
```bash
# 确保已安装 Git LFS
git lfs install

# 重新拉取 LFS 文件
git lfs fetch --all
git lfs checkout
```

### 问题 2: GitHub 上无法播放视频

**可能原因**:
1. 浏览器不支持视频编码
2. 文件过大，GitHub 加载慢
3. 网络问题

**解决方案**:
- 尝试下载后本地播放
- 使用 Chrome/Firefox 等现代浏览器
- 检查网络连接

### 问题 3: README 中视频无法显示

**可能原因**:
1. 路径错误
2. 文件未正确提交到 Git LFS

**解决方案**:
```bash
# 检查文件是否存在
ls docs/Other_Source/

# 检查 LFS 状态
git lfs ls-files

# 如果文件不在 LFS 中，重新跟踪
git lfs track "docs/Other_Source/*.mp4"
git add .gitattributes
git add docs/Other_Source/Spharx\ AgentOS\ 客户端演示.mp4
git commit -m "Add demo video with LFS"
git push
```

---

## 📝 最佳实践

### 1. 视频格式建议

- **推荐格式**: MP4 (H.264 编码 + AAC 音频)
- **分辨率**: 1080p (1920x1080) 或 720p (1280x720)
- **帧率**: 30fps
- **码率**: 5-10 Mbps

### 2. 文件大小控制

- 单个视频文件建议 < 100MB
- 使用 Git LFS 管理所有 > 1MB 的二进制文件
- 考虑使用 GIF 作为短视频预览

### 3. 命名规范

- 使用英文文件名（避免编码问题）
- 使用连字符分隔单词：`agentos-demo.mp4`
- 避免特殊字符和空格

### 4. 目录组织

```
docs/
├── Other_Source/
│   ├── videos/           # 视频文件
│   │   ├── demo.mp4
│   │   └── tutorial.mp4
│   └── images/           # 图片文件
│       └── screenshot.png
└── README.md
```

---

## 🔗 相关资源

- [Git LFS 官方文档](https://git-lfs.github.com/)
- [GitHub 视频播放支持](https://docs.github.com/en/repositories/working-with-files/managing-large-files/about-storage-and-file-management)
- [HTML5 Video 标签](https://developer.mozilla.org/en-US/docs/Web/HTML/Element/video)

---

© 2026 SPHARX Ltd. All Rights Reserved.
"From data intelligence emerges."
