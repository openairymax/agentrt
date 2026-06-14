# DocGen — 智能文档生成应用

**模块路径**: `agentos/openlab/app/docgen/`
**版本**: v0.1.0

> **Status**: 本模块作为 AgentOS 的正式组成部分，API 持续演进中。本模块通过 JSON-RPC 2.0 协议与 AgentOS 核心运行时集成。

## 概述

DocGen 是基于 AgentOS 平台的智能文档生成应用，核心引擎 `DocumentationGenerator` 支持从多种源格式（Markdown/Python/YAML/JSON）自动生成结构化文档，输出 HTML/PDF/Markdown 等格式。采用 Jinja2 模板渲染引擎，支持文件监听自动重建、导航生成、搜索索引创建和缓存优化。

## 目录结构

```
docgen/
├── src/
│   ├── __init__.py             # 模块导出
│   ├── main.py                 # 应用入口，CLI 接口
│   └── generator.py            # DocumentationGenerator 核心引擎
├── templates/
│   └── default.html.j2         # Jinja2 默认 HTML 模板
├── config.yaml                 # 应用配置
├── manifest.json               # 应用清单
├── run.sh                      # 启动脚本
└── README.md                   # 本文件
```

## 核心组件

### DocumentationGenerator (`src/generator.py`)

文档生成核心引擎，负责完整的文档生成流程：

| 类/枚举 | 说明 |
|---------|------|
| `DocumentationGenerator` | 核心生成器，管理文件发现、解析、模板渲染和输出生成 |
| `FileType` | 支持的源文件类型：MARKDOWN/PYTHON/YAML/JSON/TEXT/UNKNOWN |
| `OutputFormat` | 输出格式：HTML/PDF/MARKDOWN |
| `FileMetadata` | 文件元数据，包含 path/file_type/size/checksum/title/description/tags/frontmatter |
| `GenerationResult` | 生成结果，包含 success/generated_files/skipped_files/failed_files/warnings/errors/stats |

### 核心能力

- **多格式输入**：支持 Markdown、Python、YAML、JSON 源文件解析
- **多格式输出**：支持 HTML、PDF、Markdown 输出格式
- **模板系统**：基于 Jinja2 的模板渲染，支持自定义过滤器（slugify/markdown/tojson）和全局变量
- **Frontmatter 解析**：自动提取 YAML frontmatter 中的标题、描述和标签
- **文件监听**：基于 Watchdog 的文件变更监听，自动触发重建（支持防抖）
- **导航生成**：自动生成文档导航结构（navigation.json）
- **搜索索引**：自动创建搜索索引（search_index.json）
- **缓存优化**：基于文件 MD5 校验和的缓存机制，支持 TTL 过期
- **并行处理**：支持 ThreadPoolExecutor 并行文件处理

## 接口说明

### DocumentationGenerator API

```python
class DocumentationGenerator:
    def __init__(self, config: Dict[str, Any], logger: Optional[Logger] = None)

    async def generate(self) -> GenerationResult     # 生成文档
    async def watch(self) -> None                    # 启动文件监听

    # 内部方法
    def _should_process_file(self, path) -> bool     # 判断是否处理文件
    def _get_file_metadata(self, path) -> FileMetadata  # 提取文件元数据
    def _generate_content(self, metadata, format) -> Optional[str]  # 生成内容
    def _is_cached(self, metadata, format) -> bool   # 检查缓存
    def _save_to_cache(self, metadata, format, content)  # 保存缓存
```

### GenerationResult

```python
@dataclass
class GenerationResult:
    success: bool
    generated_files: List[Path]
    skipped_files: List[Path]
    failed_files: List[Tuple[Path, str]]
    warnings: List[str]
    errors: List[str]
    stats: Dict[str, Any]  # files_processed/generated/skipped/failed/duration
```

### CLI 接口

```bash
python -m docgen.src.main [config_path] [--watch] [--verbose] [--validate]
```

## 配置说明

`config.yaml` 主要配置项：

| 配置项 | 说明 | 默认值 |
|--------|------|--------|
| `input_dir` | 输入目录 | `./src` |
| `output_dir` | 输出目录 | `./docs` |
| `template_dir` | 模板目录 | `./templates` |
| `cache_enabled` | 启用缓存 | `true` |
| `cache_ttl` | 缓存 TTL（秒） | 3600 |
| `parallel_processing` | 并行处理 | `true` |
| `max_workers` | 最大工作线程 | 4 |
| `generate_navigation` | 生成导航 | `true` |
| `enable_search` | 启用搜索 | `true` |
| `markdown_extensions` | Markdown 扩展 | extra, codehilite |

## 依赖关系

- **核心依赖**: Jinja2, Markdown, PyYAML
- **可选依赖**: WeasyPrint (PDF 输出), Watchdog (文件监听)
- **标准库**: asyncio, hashlib, json, re, shutil, tempfile, concurrent.futures

## 使用示例

```python
from docgen.src.generator import DocumentationGenerator, generate_documentation

config = {
    "input_dir": "./src",
    "output_dir": "./docs",
    "template_dir": "./templates",
    "output_formats": [
        {"format": "html", "extension": ".html", "template": "default.html.j2"},
        {"format": "markdown", "extension": ".md", "template": "markdown.j2"},
    ],
    "cache_enabled": True,
    "parallel_processing": True,
}

generator = DocumentationGenerator(config)
result = await generator.generate()

print(f"Generated: {len(result.generated_files)} files")
print(f"Duration: {result.stats['duration']:.2f}s")
```

---

© 2026 SPHARX Ltd. All Rights Reserved.
