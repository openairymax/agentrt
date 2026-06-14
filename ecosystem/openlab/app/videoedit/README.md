# VideoEdit — 智能视频编辑应用

**模块路径**: `agentos/openlab/app/videoedit/`
**版本**: v0.1.0

> **Status**: 本模块作为 AgentOS 的正式组成部分，API 持续演进中。本模块通过 JSON-RPC 2.0 协议与 AgentOS 核心运行时集成。

## 概述

VideoEdit 是基于 AgentOS 平台的智能视频编辑应用，采用 FastAPI + FFmpeg 架构，通过 `EditPipeline` 管线编排器和 `FFmpegWrapper` FFmpeg 封装层实现视频剪辑、合并、特效、格式转换、字幕添加、缩略图提取、GIF 创建和音频提取等全功能视频处理。支持异步任务处理和进度追踪。

## 目录结构

```
videoedit/
├── src/
│   ├── __init__.py             # 模块导出
│   ├── main.py                 # FastAPI 应用入口（VideoEditApp）
│   ├── edit_pipeline.py        # 编辑管线核心
│   └── requirements.txt        # Python 依赖
├── config.yaml                 # 应用配置（含 FFmpeg/编解码器参数）
├── manifest.json               # 应用清单
├── run.sh                      # 启动脚本
└── README.md                   # 本文件
```

## 核心组件

### EditPipeline (`src/edit_pipeline.py`)

视频编辑管线编排器，管理任务队列和进度追踪：

| 类 | 说明 |
|----|------|
| `EditPipeline` | 主编排器，提供 trim/merge/filter/convert/thumbnail/gif/audio/subtitle 处理方法 |
| `FFmpegWrapper` | FFmpeg 操作封装，支持 trim/merge/filter/thumbnail/convert/audio/subtitle/gif，异步子进程执行 |
| `VideoValidator` | 视频验证器，验证文件格式/时间范围/分辨率 |
| `PipelineConfig` | 配置管理器（单例模式），支持 YAML 配置加载和点分键访问 |

### 支持的枚举类型

| 枚举 | 说明 |
|------|------|
| `VideoFormat` | 视频格式：MP4/AVI/MOV/MKV/WEBM/GIF/FLV/WMV/MPEG |
| `VideoCodec` | 视频编解码器：H264/H265/VP8/VP9/MPEG4/GIF |
| `AudioCodec` | 音频编解码器：AAC/MP3/OPUS/VORBIS/WAV |
| `FilterType` | 滤镜类型：BRIGHTNESS/CONTRAST/SATURATION/BLUR/SHARPEN/GRAYSCALE/SEPIA/INVERT/POSTERIZE |
| `TransitionType` | 转场类型：FADE/DISSOLVE/WIPE/SLIDE |
| `TaskStatus` | 任务状态：PENDING/RUNNING/COMPLETED/FAILED/CANCELLED |

### 辅助数据类

| 类 | 说明 |
|----|------|
| `VideoMetadata` | 视频元数据，包含 file_path/duration/width/height/fps/codec/audio_codec/bitrate/file_size |
| `ClipInfo` | 视频片段信息，包含 source_path/start_time/end_time/effects/volume/speed |
| `TransitionInfo` | 转场信息，包含 type/duration/direction |
| `TaskResult` | 任务结果，包含 task_id/status/output_path/metadata/error/duration/progress |

### VideoEditApp (`src/main.py`)

FastAPI 应用主类，提供 RESTful API 端点：

| 请求模型 | 说明 |
|----------|------|
| `TrimRequest` | 剪辑请求，包含 start_time/end_time/output_format |
| `MergeRequest` | 合并请求，包含 output_format |
| `FilterRequest` | 滤镜请求，包含 filter_type/filter_value/output_format |
| `ConvertRequest` | 转换请求，包含 output_format/video_codec/bitrate |
| `GifRequest` | GIF 请求，包含 start_time/duration/fps/width |
| `AudioExtractRequest` | 音频提取请求，包含 output_format |
| `SubtitleRequest` | 字幕请求，包含 subtitle_path/output_format |

## 接口说明

### RESTful API

| 端点 | 方法 | 说明 |
|------|------|------|
| `/` | GET | 服务信息 |
| `/health` | GET | 健康检查（含 FFmpeg 可用性检测） |
| `/api/upload` | POST | 上传视频文件（支持流式写入） |
| `/api/trim/{file_id}` | POST | 视频剪辑 |
| `/api/merge` | POST | 视频合并 |
| `/api/filter/{file_id}` | POST | 应用滤镜 |
| `/api/convert/{file_id}` | POST | 格式转换 |
| `/api/thumbnails/{file_id}` | POST | 提取缩略图 |
| `/api/gif/{file_id}` | POST | 创建 GIF |
| `/api/audio/{file_id}` | POST | 提取音频 |
| `/api/subtitles/{file_id}` | POST | 添加字幕 |
| `/api/task/{task_id}` | GET | 查询任务状态 |
| `/api/task/{task_id}/cancel` | POST | 取消任务 |
| `/api/download/{filename}` | GET | 下载文件 |
| `/api/file/{file_id}` | DELETE | 删除上传文件 |
| `/api/formats` | GET | 获取支持格式和编解码器 |
| `/api/cleanup` | POST | 清理临时文件 |

## 配置说明

`config.yaml` 主要配置项：

| 配置项 | 说明 | 默认值 |
|--------|------|--------|
| `host` | 服务地址 | `0.0.0.0` |
| `port` | 服务端口 | 8001 |
| `max_file_size` | 最大文件大小 | 5GB |
| `ffmpeg.path` | FFmpeg 路径 | `ffmpeg` |
| `ffmpeg.timeout` | 操作超时（秒） | 3600 |
| `ffmpeg.preset` | 编码预设 | medium |
| `ffmpeg.crf` | 质量因子 | 23 |
| `video.default_codec` | 默认视频编解码器 | libx264 |
| `audio.default_codec` | 默认音频编解码器 | aac |
| `tasks.max_concurrent` | 最大并发任务 | 2 |
| `tasks.auto_cleanup_hours` | 自动清理时间 | 24 |

## 依赖关系

- **核心依赖**: FastAPI, Uvicorn, PyYAML, aiofiles
- **视频处理**: FFmpeg（系统级依赖）
- **可选依赖**: OpenCV, MoviePy, Pillow, NumPy

## 构建说明

```bash
# 安装 FFmpeg（Ubuntu/Debian）
sudo apt install ffmpeg

# 安装 Python 依赖
pip install -e ".[videoedit]"

# 启动服务
python src/main.py --config config.yaml --host 0.0.0.0 --port 8001
```

## 使用示例

```python
from videoedit.src.edit_pipeline import EditPipeline, PipelineConfig

# 通过管线直接使用
config = PipelineConfig()
config.load("config.yaml")
pipeline = EditPipeline(config._config)

result = await pipeline.process_trim("input.mp4", 10.0, 60.0)
print(f"Status: {result.status}, Output: {result.output_path}")
```

---

© 2026 SPHARX Ltd. All Rights Reserved.
