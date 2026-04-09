---
copyright: "Copyright (c) 2026 SPHARX. All Rights Reserved."
slogan: "From data intelligence emerges."
title: "AgentOS 多语言 README"
version: "Doc V1.5"
last_updated: "2026-03-23"
author: "AgentOS 国际化团队"
status: "production_ready"
review_due: "2026-06-23"
theoretical_basis: "工程两论、五维正交系统、双系统认知理论"
target_audience: "翻译者/国际化专家/社区贡献者"
prerequisites: "了解Markdown语法，熟悉翻译工作流程"
estimated_reading_time: "2小时"
core_concepts: "国际化, 本地化, 翻译流程, 术语管理, 质量保证"
---

# AgentOS 多语言 README

## 📋 文档信息卡
- **目标读者**: 翻译者/国际化专家/社区贡献者
- **前置知识**: 了解Markdown语法，熟悉翻译工作流程
- **预计阅读时间**: 2小时
- **核心概念**: 国际化, 本地化, 翻译流程, 术语管理, 质量保证
- **文档状态**: 🟢 生产就绪
- **复杂度标识**: ⭐⭐ 中级

## 🎯 概述

AgentOS 提供多语言 README 文档，支持全球开发者快速了解和使用 AgentOS。所有语言版本的 README 都保持内容一致性和技术准确性，确保全球用户获得相同的优质体验。

### 支持的语言

| 语言 | 代码 | 状态 | 维护者 |
|------|------|------|--------|
| **English** | `en` | ✅ 生产就绪 | AgentOS 核心团队 |
| **简体中文** | `zh-CN` | ✅ 生产就绪 | AgentOS 中文社区 |
| **Deutsch** | `de` | ✅ 生产就绪 | AgentOS 德语社区 |
| **Français** | `fr` | ✅ 生产就绪 | AgentOS 法语社区 |
| **日本語** | `ja` | 🔄 翻译中 | AgentOS 日语社区 |
| **한국어** | `ko` | 🔄 翻译中 | AgentOS 韩语社区 |
| **Español** | `es` | 🔄 翻译中 | AgentOS 西班牙语社区 |
| **Português** | `pt` | 🔄 翻译中 | AgentOS 葡萄牙语社区 |

### 文档结构

```
┌─────────────────────────────────────────┐
│         AgentOS 多语言 README            │
├─────────────────────────────────────────┤
│  🌍 国际化支持 (Internationalization)    │
│  • 多语言架构 • 本地化策略 • 翻译流程     │
│  • 文化适配 • 区域设置                   │
├─────────────────────────────────────────┤
│  📖 文档标准 (Documentation Standards)   │
│  • 格式规范 • 术语一致 • 风格指南         │
│  • 质量检查 • 版本同步                   │
├─────────────────────────────────────────┤
│  🔧 翻译工具 (Translation Tools)         │
│  • 自动化翻译 • 术语库 • 协作平台         │
│  • 质量评估 • 持续集成                   │
├─────────────────────────────────────────┤
│  🤝 社区贡献 (Community Contribution)    │
│  • 翻译指南 • 贡献流程 • 质量保证         │
│  • 社区认可 • 维护者指南                 │
└─────────────────────────────────────────┘
```

---

## 🌍 国际化支持 (Internationalization)

AgentOS 提供完整的国际化支持，确保全球开发者都能获得优质的使用体验。

### 1. 多语言架构

**核心设计原则**:
- **分离内容与代码**: 所有用户可见文本与代码逻辑分离
- **统一资源管理**: 使用标准化的资源文件格式
- **动态语言切换**: 支持运行时语言切换
- **回退机制**: 当目标语言翻译缺失时，自动回退到默认语言

**资源文件结构**:
```
resources/
├── locales/
│   ├── en/              # 英语（默认语言）
│   │   ├── commons.json
│   │   ├── ui.json
│   │   ├── errors.json
│   │   └── docs.json
│   ├── zh-CN/           # 简体中文
│   │   ├── commons.json
│   │   ├── ui.json
│   │   ├── errors.json
│   │   └── docs.json
│   ├── de/              # 德语
│   │   └── ...
│   └── fr/              # 法语
│       └── ...
└── templates/           # 模板文件
    ├── email/
    ├── notifications/
    └── reports/
```

### 2. 本地化策略

**内容本地化**:
- **技术术语**: 保持一致性，建立术语库
- **文化适配**: 考虑文化差异，避免文化敏感内容
- **格式规范**: 日期、时间、数字、货币格式本地化
- **法律合规**: 符合当地法律法规要求

**本地化示例**:
```json
// resources/locales/en/commons.json
{
  "welcome": "Welcome to AgentOS",
  "description": "An intelligent agent operating system",
  "get_started": "Get Started",
  "documentation": "Documentation"
}

// resources/locales/zh-CN/commons.json
{
  "welcome": "欢迎使用 AgentOS",
  "description": "智能代理操作系统",
  "get_started": "快速开始",
  "documentation": "文档"
}

// resources/locales/de/commons.json
{
  "welcome": "Willkommen bei AgentOS",
  "description": "Ein intelligentes Agenten-Betriebssystem",
  "get_started": "Erste Schritte",
  "documentation": "Dokumentation"
}
```

### 3. 翻译流程

**标准化翻译流程**:
```
源文本提取 → 翻译准备 → 翻译执行 → 质量检查 → 集成部署
     ↓           ↓          ↓          ↓          ↓
提取字符串    术语统一    人工翻译    双语校对    自动测试
     ↓           ↓          ↓          ↓          ↓
PO文件生成   风格指南     工具辅助   文化审核   持续集成
```

**翻译质量保证**:
- **术语一致性**: 使用统一的术语库
- **风格统一**: 遵循语言特定的风格指南
- **文化适配**: 考虑目标文化的习惯和偏好
- **技术准确性**: 确保技术术语翻译准确

### 4. 文化适配

**日期和时间格式**:
```python
# 日期时间本地化示例
import locale
from datetime import datetime

def format_datetime(dt: datetime, lang: str) -> str:
    """根据语言格式化日期时间"""
    locale_map = {
        'en': 'en_US.UTF-8',
        'zh-CN': 'zh_CN.UTF-8',
        'de': 'de_DE.UTF-8',
        'fr': 'fr_FR.UTF-8',
        'ja': 'ja_JP.UTF-8',
        'ko': 'ko_KR.UTF-8',
        'es': 'es_ES.UTF-8',
        'pt': 'pt_BR.UTF-8'
    }
    
    try:
        locale.setlocale(locale.LC_TIME, locale_map.get(lang, 'en_US.UTF-8'))
        return dt.strftime('%c')  # 本地化的日期时间格式
    except locale.Error:
        # 回退到 ISO 格式
        return dt.isoformat()
```

**数字和货币格式**:
```python
def format_number(number: float, lang: str) -> str:
    """根据语言格式化数字"""
    formatting_rules = {
        'en': {
            'decimal_separator': '.',
            'thousands_separator': ',',
            'currency_symbol': '$',
            'currency_position': 'before'
        },
        'zh-CN': {
            'decimal_separator': '.',
            'thousands_separator': ',',
            'currency_symbol': '¥',
            'currency_position': 'before'
        },
        'de': {
            'decimal_separator': ',',
            'thousands_separator': '.',
            'currency_symbol': '€',
            'currency_position': 'after'
        },
        'fr': {
            'decimal_separator': ',',
            'thousands_separator': ' ',
            'currency_symbol': '€',
            'currency_position': 'after'
        }
    }
    
    rules = formatting_rules.get(lang, formatting_rules['en'])
    # 应用格式化规则
    return format_with_rules(number, rules)
```

### 5. 区域设置

**区域设置管理**:
```yaml
# agentos/manager/locales.yaml
locales:
  supported:
    - code: "en"
      name: "English"
      native_name: "English"
      direction: "ltr"
      enabled: true
      default: true
      
    - code: "zh-CN"
      name: "Chinese (Simplified)"
      native_name: "简体中文"
      direction: "ltr"
      enabled: true
      default: false
      
    - code: "de"
      name: "German"
      native_name: "Deutsch"
      direction: "ltr"
      enabled: true
      default: false
      
    - code: "fr"
      name: "French"
      native_name: "Français"
      direction: "ltr"
      enabled: true
      default: false

  fallback_chain:
    zh-CN: ["zh-TW", "en"]
    zh-TW: ["zh-CN", "en"]
    de: ["en"]
    fr: ["en"]
    ja: ["en"]
    ko: ["en"]
    es: ["en"]
    pt: ["en"]

  auto_detection:
    enabled: true
    methods:
      - browser_language
      - system_language
      - ip_geolocation
    confidence_threshold: 0.7
```

**语言检测**:
```python
def detect_user_language(request) -> str:
    """检测用户偏好的语言"""
    # 1. 检查 URL 参数
    lang_param = request.args.get('lang')
    if lang_param and lang_param in SUPPORTED_LANGUAGES:
        return lang_param
    
    # 2. 检查会话设置
    session_lang = request.session.get('language')
    if session_lang and session_lang in SUPPORTED_LANGUAGES:
        return session_lang
    
    # 3. 检查浏览器语言
    browser_lang = parse_accept_language(request.headers.get('Accept-Language'))
    for lang in browser_lang:
        if lang in SUPPORTED_LANGUAGES:
            return lang
    
    # 4. 检查 IP 地理位置（可选）
    if AUTO_DETECTION_ENABLED:
        geo_lang = detect_language_by_ip(request.remote_addr)
        if geo_lang in SUPPORTED_LANGUAGES:
            return geo_lang
    
    # 5. 返回默认语言
    return DEFAULT_LANGUAGE
```

---

## 📖 文档标准 (Documentation Standards)

AgentOS 文档遵循严格的标准，确保多语言文档的质量和一致性。

### 1. 格式规范

**Markdown 规范**:
```markdown
# 文档标题 (H1)

**版本**: v1.0.0  
**最后更新**: 2026-03-23  
**状态**: 🟢 生产就绪

---

## 章节标题 (H2)

### 子章节标题 (H3)

#### 小标题 (H4)

**粗体文本**用于强调重要概念。

*斜体文本*用于表示术语或引用。

`代码片段`用于表示代码或命令。

> 引用块用于重要说明或引用。

- 无序列表项
- 另一个列表项

1. 有序列表项
2. 另一个有序列表项

| 表头1 | 表头2 | 表头3 |
|-------|-------|-------|
| 内容1 | 内容2 | 内容3 |
| 内容4 | 内容5 | 内容6 |

```python
# 代码块
def example_function():
    """函数文档字符串"""
    return "Hello, World!"
```

[链接文本](https://example.com)

![图片描述](image.png)
```

**文件命名规范**:
- 使用小写字母、数字和连字符
- 避免使用空格和特殊字符
- 保持一致的命名模式
- 示例: `getting-started.md`, `api-reference.md`, `troubleshooting.md`

### 2. 术语一致

**术语库管理**:
```yaml
# terminology/glossary.yaml
terms:
  - term: "Agent"
    definition: "自主执行任务的智能实体"
    translations:
      en: "Agent"
      zh-CN: "代理"
      de: "Agent"
      fr: "Agent"
    usage: "名词，首字母大写"
    notes: "在技术文档中保持大写"
    
  - term: "Skill"
    definition: "Agent 的能力单元"
    translations:
      en: "Skill"
      zh-CN: "技能"
      de: "Fähigkeit"
      fr: "Compétence"
    usage: "名词，首字母大写"
    notes: "与 Ability 区分"
    
  - term: "Task"
    definition: "要执行的工作单元"
    translations:
      en: "Task"
      zh-CN: "任务"
      de: "Aufgabe"
      fr: "Tâche"
    usage: "名词，首字母大写"
    notes: "在调度上下文中使用"
    
  - term: "Memory"
    definition: "Agent 的记忆存储系统"
    translations:
      en: "Memory"
      zh-CN: "记忆"
      de: "Speicher"
      fr: "Mémoire"
    usage: "名词，首字母大写"
    notes: "指四层记忆模型"
```

**术语检查工具**:
```bash
# 检查术语一致性
python scripts/check_terminology.py --lang zh-CN --file README.md

# 提取文档中的术语
python scripts/extract_terms.py --input docs/ --output terms.json

# 验证翻译一致性
python scripts/validate_translations.py --source en --target zh-CN
```

### 3. 风格指南

**中文文档风格指南**:
- 使用简体中文，符合现代汉语规范
- 避免文言文和过度口语化
- 技术术语保持中英文对照（首次出现时）
- 标点符号使用全角符号
- 数字和英文使用半角字符
- 段落之间空一行

**英文文档风格指南**:
- 使用美式英语拼写
- 保持简洁明了的句子结构
- 技术术语首字母大写
- 使用主动语态
- 避免过度使用被动语态
- 段落长度控制在3-5句话

**多语言风格示例**:
```markdown
<!-- 英文版本 -->
## Getting Started with AgentOS

AgentOS is an intelligent agent operating system designed for building, deploying, and managing autonomous agents. This guide will help you install AgentOS and create your first agent.

**Prerequisites**:
- Python 3.8 or higher
- 4GB RAM minimum
- 10GB free disk space

<!-- 中文版本 -->
## AgentOS 快速开始

AgentOS 是一个智能代理操作系统，用于构建、部署和管理自主代理。本指南将帮助您安装 AgentOS 并创建第一个代理。

**前提条件**:
- Python 3.8 或更高版本
- 至少 4GB 内存
- 10GB 可用磁盘空间

<!-- 德语版本 -->
## Erste Schritte mit AgentOS

AgentOS ist ein intelligentes Agenten-Betriebssystem zum Erstellen, Bereitstellen und Verwalten autonomer Agenten. Diese Anleitung hilft Ihnen bei der Installation von AgentOS und der Erstellung Ihres ersten Agenten.

**Voraussetzungen**:
- Python 3.8 oder höher
- Mindestens 4 GB RAM
- 10 GB freier Festplattenspeicher
```

### 4. 质量检查

**自动化质量检查**:
```yaml
# .github/workflows/docs-quality.yml
name: Documentation Quality Check

on:
  push:
    paths:
      - 'docs/**'
      - 'README.md'
  pull_request:
    paths:
      - 'docs/**'
      - 'README.md'

jobs:
  quality-check:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v3
      
      - name: Check Markdown formatting
        uses: gaurav-nelson/github-action-markdown-link-check@v1
        with:
          manager-file: '.github/markdown-link-check.json'
          
      - name: Check spelling
        uses: codespell-project/actions-codespell@v1
        with:
          ignore_words_file: '.codespellignore'
          
      - name: Check terminology consistency
        run: python scripts/check_terminology.py --all
        
      - name: Check translation completeness
        run: python scripts/check_translations.py --source en
        
      - name: Generate documentation coverage report
        run: python scripts/doc_coverage.py --output coverage.json
```

**人工审查清单**:
- [ ] 术语使用是否一致？
- [ ] 翻译是否准确？
- [ ] 文化适配是否恰当？
- [ ] 格式是否符合规范？
- [ ] 链接是否有效？
- [ ] 代码示例是否正确？
- [ ] 图片和图表是否清晰？
- [ ] 版本信息是否更新？

### 5. 版本同步

**文档版本管理**:
```yaml
# docs/versions.yaml
versions:
  - version: "1.0.0"
    release_date: "2026-03-23"
    status: "stable"
    docs_path: "docs/v1.0.0/"
    languages:
      en: "docs/v1.0.0/en/"
      zh-CN: "docs/v1.0.0/zh-CN/"
      de: "docs/v1.0.0/de/"
      fr: "docs/v1.0.0/fr/"
      
  - version: "0.9.0"
    release_date: "2026-02-15"
    status: "deprecated"
    docs_path: "docs/v0.9.0/"
    languages:
      en: "docs/v0.9.0/en/"
      zh-CN: "docs/v0.9.0/zh-CN/"
      
  - version: "main"
    release_date: "latest"
    status: "development"
    docs_path: "docs/main/"
    languages:
      en: "docs/main/en/"
      zh-CN: "docs/main/zh-CN/"  # 可能不完整
```

**版本同步工具**:
```bash
# 同步文档版本
python scripts/sync_docs_version.py --source v1.0.0 --target main --lang zh-CN

# 检查版本差异
python scripts/check_version_diff.py --version1 v1.0.0 --version2 main --lang zh-CN

# 更新版本信息
python scripts/update_version_info.py --version 1.0.0 --date 2026-03-23
```

---

## 🔧 翻译工具 (Translation Tools)

AgentOS 提供完整的翻译工具链，支持高效的文档翻译和管理。

### 1. 自动化翻译

**翻译管道**:
```python
# scripts/translation_pipeline.py
import json
import yaml
from pathlib import Path
from typing import Dict, Any

class TranslationPipeline:
    def __init__(self, config_path: str):
        with open(config_path, 'r', encoding='utf-8') as f:
            self.manager = yaml.safe_load(f)
        
    def extract_strings(self, source_dir: str) -> Dict[str, Any]:
        """从源文档提取字符串"""
        extracted = {}
        
        for file_path in Path(source_dir).rglob('*.md'):
            with open(file_path, 'r', encoding='utf-8') as f:
                content = f.read()
                
            # 提取需要翻译的文本
            strings = self._extract_from_markdown(content)
            if strings:
                relative_path = str(file_path.relative_to(source_dir))
                extracted[relative_path] = {
                    'path': relative_path,
                    'strings': strings,
                    'metadata': self._extract_metadata(content)
                }
        
        return extracted
    
    def translate_strings(self, strings: Dict[str, Any], target_lang: str) -> Dict[str, Any]:
        """翻译字符串"""
        translated = {}
        
        for file_path, data in strings.items():
            translated_strings = []
            
            for string in data['strings']:
                # 使用机器翻译（可配置使用不同引擎）
                translation = self._machine_translate(
                    string['text'],
                    source_lang='en',
                    target_lang=target_lang
                )
                
                translated_strings.append({
                    'id': string['id'],
                    'source': string['text'],
                    'translation': translation,
                    'context': string.get('context', ''),
                    'notes': string.get('notes', '')
                })
            
            translated[file_path] = {
                'path': file_path,
                'strings': translated_strings,
                'metadata': data['metadata']
            }
        
        return translated
    
    def apply_translations(self, translations: Dict[str, Any], target_dir: str):
        """应用翻译到目标文档"""
        for file_path, data in translations.items():
            target_path = Path(target_dir) / file_path
            
            # 确保目标目录存在
            target_path.parent.mkdir(parents=True, exist_ok=True)
            
            # 应用翻译
            translated_content = self._apply_to_markdown(data)
            
            with open(target_path, 'w', encoding='utf-8') as f:
                f.write(translated_content)
    
    def _extract_from_markdown(self, content: str) -> list:
        """从 Markdown 提取字符串的具体实现"""
        # 实现字符串提取逻辑
        pass
    
    def _machine_translate(self, text: str, source_lang: str, target_lang: str) -> str:
        """机器翻译的具体实现"""
        # 实现机器翻译逻辑（可集成 DeepL、Google Translate 等）
        pass
    
    def _apply_to_markdown(self, data: Dict[str, Any]) -> str:
        """将翻译应用到 Markdown 的具体实现"""
        # 实现翻译应用逻辑
        pass
```

### 2. 术语库

**术语库管理工具**:
```python
# scripts/termbase_manager.py
import sqlite3
from typing import List, Dict, Any

class TermbaseManager:
    def __init__(self, db_path: str = 'terminology/termbase.db'):
        self.conn = sqlite3.connect(db_path)
        self._init_database()
    
    def _init_database(self):
        """初始化数据库"""
        cursor = self.conn.cursor()
        
        # 创建术语表
        cursor.execute('''
            CREATE TABLE IF NOT EXISTS terms (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                term TEXT NOT NULL,
                domain TEXT,
                definition TEXT,
                created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
                updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
            )
        ''')
        
        # 创建翻译表
        cursor.execute('''
            CREATE TABLE IF NOT EXISTS translations (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                term_id INTEGER,
                language TEXT NOT NULL,
                translation TEXT NOT NULL,
                notes TEXT,
                status TEXT DEFAULT 'pending',
                reviewed_by TEXT,
                reviewed_at TIMESTAMP,
                FOREIGN KEY (term_id) REFERENCES terms (id)
            )
        ''')
        
        # 创建使用示例表
        cursor.execute('''
            CREATE TABLE IF NOT EXISTS examples (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                term_id INTEGER,
                language TEXT NOT NULL,
                example TEXT NOT NULL,
                context TEXT,
                FOREIGN KEY (term_id) REFERENCES terms (id)
            )
        ''')
        
        self.conn.commit()
    
    def add_term(self, term: str, domain: str = None, definition: str = None) -> int:
        """添加新术语"""
        cursor = self.conn.cursor()
        cursor.execute(
            'INSERT INTO terms (term, domain, definition) VALUES (?, ?, ?)',
            (term, domain, definition)
        )
        self.conn.commit()
        return cursor.lastrowid
    
    def add_translation(self, term_id: int, language: str, translation: str, 
                       notes: str = None) -> int:
        """添加翻译"""
        cursor = self.conn.cursor()
        cursor.execute(
            '''INSERT INTO translations 
               (term_id, language, translation, notes) 
               VALUES (?, ?, ?, ?)''',
            (term_id, language, translation, notes)
        )
        self.conn.commit()
        return cursor.lastrowid
    
    def search_terms(self, query: str, language: str = None) -> List[Dict[str, Any]]:
        """搜索术语"""
        cursor = self.conn.cursor()
        
        if language:
            cursor.execute('''
                SELECT t.term, t.definition, tr.translation, tr.language
                FROM terms t
                LEFT JOIN translations tr ON t.id = tr.term_id
                WHERE (t.term LIKE ? OR t.definition LIKE ? OR tr.translation LIKE ?)
                AND tr.language = ?
                ORDER BY t.term
            ''', (f'%{query}%', f'%{query}%', f'%{query}%', language))
        else:
            cursor.execute('''
                SELECT t.term, t.definition, tr.translation, tr.language
                FROM terms t
                LEFT JOIN translations tr ON t.id = tr.term_id
                WHERE t.term LIKE ? OR t.definition LIKE ? OR tr.translation LIKE ?
                ORDER BY t.term, tr.language
            ''', (f'%{query}%', f'%{query}%', f'%{query}%'))
        
        results = []
        for row in cursor.fetchall():
            results.append({
                'term': row[0],
                'definition': row[1],
                'translation': row[2],
                'language': row[3]
            })
        
        return results
    
    def export_glossary(self, language: str) -> Dict[str, Any]:
        """导出术语表"""
        cursor = self.conn.cursor()
        
        cursor.execute('''
            SELECT t.term, t.definition, tr.translation, tr.notes
            FROM terms t
            JOIN translations tr ON t.id = tr.term_id
            WHERE tr.language = ? AND tr.status = 'approved'
            ORDER BY t.term
        ''', (language,))
        
        glossary = {}
        for row in cursor.fetchall():
            glossary[row[0]] = {
                'definition': row[1],
                'translation': row[2],
                'notes': row[3]
            }
        
        return glossary
```

### 3. 协作平台

**翻译协作平台配置**:
```yaml
# agentos/manager/translation-platform.yaml
translation_platform:
  # 平台选择
  platform: "crowdin"  # 可选: crowdin, transifex, weblate
  
  # Crowdin 配置
  crowdin:
    project_id: "agentos"
    api_key: "${CROWDIN_API_KEY}"
    base_url: "https://api.crowdin.com/api/v2"
    
    # 文件配置
    files:
      - source: "docs/en/**/*.md"
        translation: "docs/%locale%/**/%original_file_name%"
        update_option: "update_as_unapproved"
        
      - source: "resources/locales/en/*.json"
        translation: "resources/locales/%locale%/%original_file_name%"
    
    # 分支管理
    branches:
      main:
        name: "main"
        export_pattern: "docs/%locale%/**"
        
      version_1_0_0:
        name: "v1.0.0"
        export_pattern: "docs/v1.0.0/%locale%/**"
    
    # 自动化
    automation:
      auto_upload_sources: true
      auto_download_translations: true
      webhook_url: "https://agentos.io/api/translation-webhook"
  
  # 质量保证
  quality_assurance:
    enabled: true
    checks:
      - empty_translation
      - inconsistent_placeholder
      - inconsistent_html
      - spellcheck
      - terminology
    
    thresholds:
      approval_percentage: 90
      proofread_percentage: 100
  
  # 团队管理
  teams:
    zh-CN:
      manager: "zh-team-lead@agentos.io"
      translators: 5
      proofreaders: 2
      
    de:
      manager: "de-team-lead@agentos.io"
      translators: 3
      proofreaders: 1
      
    fr:
      manager: "fr-team-lead@agentos.io"
      translators: 3
      proofreaders: 1
  
  # 报告
  reports:
    weekly:
      enabled: true
      recipients:
        - "docs-team@agentos.io"
        - "translation-managers@agentos.io"
      
    monthly:
      enabled: true
      recipients:
        - "leadership@agentos.io"
```

### 4. 质量评估

**翻译质量评估工具**:
```python
# scripts/translation_quality.py
import json
from typing import Dict, Any, List
from dataclasses import dataclass

@dataclass
class QualityMetric:
    name: str
    weight: float
    score: float
    details: Dict[str, Any]

class TranslationQualityAssessor:
    def __init__(self, config_path: str):
        with open(config_path, 'r', encoding='utf-8') as f:
            self.manager = json.load(f)
    
    def assess_quality(self, source_text: str, translated_text: str, 
                      language: str) -> Dict[str, Any]:
        """评估翻译质量"""
        metrics = []
        
        # 1. 准确性评估
        accuracy_score = self._assess_accuracy(source_text, translated_text)
        metrics.append(QualityMetric(
            name="accuracy",
            weight=0.4,
            score=accuracy_score,
            details=self._get_accuracy_details(source_text, translated_text)
        ))
        
        # 2. 流畅性评估
        fluency_score = self._assess_fluency(translated_text, language)
        metrics.append(QualityMetric(
            name="fluency",
            weight=0.3,
            score=fluency_score,
            details=self._get_fluency_details(translated_text, language)
        ))
        
        # 3. 术语一致性评估
        terminology_score = self._assess_terminology(source_text, translated_text, language)
        metrics.append(QualityMetric(
            name="terminology",
            weight=0.2,
            score=terminology_score,
            details=self._get_terminology_details(source_text, translated_text, language)
        ))
        
        # 4. 风格一致性评估
        style_score = self._assess_style(source_text, translated_text, language)
        metrics.append(QualityMetric(
            name="style",
            weight=0.1,
            score=style_score,
            details=self._get_style_details(source_text, translated_text, language)
        ))
        
        # 计算总分
        total_score = sum(m.score * m.weight for m in metrics)
        
        return {
            'total_score': total_score,
            'metrics': [m.__dict__ for m in metrics],
            'grade': self._get_grade(total_score),
            'recommendations': self._get_recommendations(metrics)
        }
    
    def _assess_accuracy(self, source: str, translation: str) -> float:
        """评估翻译准确性"""
        # 实现准确性评估逻辑
        pass
    
    def _assess_fluency(self, translation: str, language: str) -> float:
        """评估翻译流畅性"""
        # 实现流畅性评估逻辑
        pass
    
    def _assess_terminology(self, source: str, translation: str, language: str) -> float:
        """评估术语一致性"""
        # 实现术语一致性评估逻辑
        pass
    
    def _assess_style(self, source: str, translation: str, language: str) -> float:
        """评估风格一致性"""
        # 实现风格一致性评估逻辑
        pass
    
    def _get_grade(self, score: float) -> str:
        """根据分数获取等级"""
        if score >= 0.9:
            return "A+"
        elif score >= 0.8:
            return "A"
        elif score >= 0.7:
            return "B"
        elif score >= 0.6:
            return "C"
        else:
            return "D"
    
    def _get_recommendations(self, metrics: List[QualityMetric]) -> List[str]:
        """获取改进建议"""
        recommendations = []
        
        for metric in metrics:
            if metric.score < 0.7:
                if metric.name == "accuracy":
                    recommendations.append("检查翻译准确性，确保没有漏译或误译")
                elif metric.name == "fluency":
                    recommendations.append("改善翻译流畅性，使其更符合目标语言习惯")
                elif metric.name == "terminology":
                    recommendations.append("统一术语使用，参考术语库")
                elif metric.name == "style":
                    recommendations.append("调整翻译风格，使其更符合文档风格指南")
        
        return recommendations
```

### 5. 持续集成

**翻译 CI/CD 管道**:
```yaml
# .github/workflows/translations.yml
name: Translation Pipeline

on:
  push:
    paths:
      - 'docs/en/**'
      - 'resources/locales/en/**'
  schedule:
    - cron: '0 0 * * 0'  # 每周日运行
  workflow_dispatch:  # 手动触发

jobs:
  extract-strings:
    runs-on: ubuntu-latest
    outputs:
      strings_json: ${{ steps.extract.outputs.strings }}
    steps:
      - uses: actions/checkout@v3
      
      - name: Extract translatable strings
        id: extract
        run: |
          python scripts/extract_strings.py \
            --source docs/en \
            --output extracted_strings.json
          echo "strings_json=$(cat extracted_strings.json | jq -c .)" >> $GITHUB_OUTPUT
      
      - name: Upload extracted strings
        uses: actions/upload-artifact@v3
        with:
          name: extracted-strings
          path: extracted_strings.json
  
  machine-translate:
    needs: extract-strings
    runs-on: ubuntu-latest
    strategy:
      matrix:
        language: [zh-CN, de, fr, ja, ko, es, pt]
    steps:
      - uses: actions/checkout@v3
      
      - name: Download extracted strings
        uses: actions/download-artifact@v3
        with:
          name: extracted-strings
      
      - name: Machine translate
        run: |
          python scripts/machine_translate.py \
            --input extracted_strings.json \
            --output translated_${{ matrix.language }}.json \
            --target-lang ${{ matrix.language }} \
            --engine deepl  # 或 google, azure 等
      
      - name: Upload machine translations
        uses: actions/upload-artifact@v3
        with:
          name: machine-translations-${{ matrix.language }}
          path: translated_${{ matrix.language }}.json
  
  human-review:
    needs: machine-translate
    runs-on: ubuntu-latest
    strategy:
      matrix:
        language: [zh-CN, de, fr, ja, ko, es, pt]
    steps:
      - uses: actions/checkout@v3
      
      - name: Download machine translations
        uses: actions/download-artifact@v3
        with:
          name: machine-translations-${{ matrix.language }}
      
      - name: Create review task
        run: |
          python scripts/create_review_task.py \
            --input translated_${{ matrix.language }}.json \
            --language ${{ matrix.language }} \
            --platform crowdin
      
      - name: Notify translation team
        run: |
          python scripts/notify_team.py \
            --language ${{ matrix.language }} \
            --message "New translation review task created"
  
  quality-check:
    needs: human-review
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v3
      
      - name: Download reviewed translations
        run: |
          python scripts/download_translations.py \
            --platform crowdin \
            --output translations/
      
      - name: Quality assessment
        run: |
          python scripts/assess_quality.py \
            --source docs/en \
            --translations translations/ \
            --output quality_report.json
      
      - name: Generate quality report
        run: |
          python scripts/generate_report.py \
            --input quality_report.json \
            --output report.md
      
      - name: Upload quality report
        uses: actions/upload-artifact@v3
        with:
          name: quality-report
          path: report.md
  
  deploy-docs:
    needs: quality-check
    runs-on: ubuntu-latest
    if: github.ref == 'refs/heads/main'
    steps:
      - uses: actions/checkout@v3
      
      - name: Download quality report
        uses: actions/download-artifact@v3
        with:
          name: quality-report
      
      - name: Check quality threshold
        run: |
          python scripts/check_threshold.py \
            --report quality_report.json \
            --threshold 0.8
      
      - name: Deploy documentation
        run: |
          python scripts/deploy_docs.py \
            --source docs/en \
            --translations translations/ \
            --target /var/www/docs.agentos.io
      
      - name: Invalidate CDN cache
        run: |
          aws cloudfront create-invalidation \
            --distribution-id ${{ secrets.CLOUDFRONT_DISTRIBUTION_ID }} \
            --paths "/docs/*"
```

---

## 🤝 社区贡献 (Community Contribution)

AgentOS 欢迎社区成员参与文档翻译和维护工作。

### 1. 翻译指南

**新翻译者入门指南**:
```markdown
# AgentOS 翻译贡献指南

## 🎯 开始之前

### 1. 了解项目
- 阅读 [项目概述](../philosophy/README.md) 了解 AgentOS 的设计哲学
- 查看 [架构文档](../architecture/README.md) 理解技术概念
- 浏览 [现有文档](en/) 熟悉文档风格

### 2. 设置环境
```bash
# 克隆仓库
git clone https://github.com/SpharxTeam/AgentOS.git
cd AgentOS

# 安装依赖
pip install -r requirements-docs.txt

# 配置翻译工具
cp agentos/manager/translation.example.yaml agentos/manager/translation.yaml
# 编辑配置文件，设置你的偏好
```

### 3. 选择任务
- 查看 [翻译看板](https://github.com/SpharxTeam/AgentOS/projects/2)
- 选择标记为 "help wanted" 或 "good first issue" 的任务
- 或者从待翻译文件列表中选择

## 📝 翻译流程

### 1. 领取任务
1. 在 Issue 中留言表示你要翻译
2. 等待维护者分配任务
3. 创建分支: `git checkout -b translate-zh-CN-filename`

### 2. 准备翻译
1. 阅读源文件，理解内容
2. 查阅术语库，统一术语
3. 参考风格指南，确保符合规范

### 3. 执行翻译
1. 使用翻译工具辅助
2. 保持术语一致性
3. 注意文化适配
4. 确保技术准确性

### 4. 质量检查
1. 自我检查: 通读翻译，检查错误
2. 工具检查: 运行质量检查脚本
3. 术语检查: 确保术语使用正确

### 5. 提交审核
1. 提交 Pull Request
2. 填写 PR 模板
3. 等待代码审查
4. 根据反馈修改

## 🔧 工具使用

### 术语库工具
```bash
# 搜索术语
python scripts/search_terms.py --query "Agent" --lang zh-CN

# 添加新术语
python scripts/add_term.py --term "Skill" --translation "技能" --lang zh-CN

# 导出术语表
python scripts/export_glossary.py --lang zh-CN --output glossary-zh-CN.json
```

### 质量检查工具
```bash
# 检查术语一致性
python scripts/check_terminology.py --file docs/zh-CN/README.md

# 检查翻译质量
python scripts/check_quality.py --source docs/en/README.md --translation docs/zh-CN/README.md

# 检查链接有效性
python scripts/check_links.py --file docs/zh-CN/README.md
```

### 翻译辅助工具
```bash
# 机器翻译辅助
python scripts/translate_assist.py --input docs/en/README.md --output temp.md --lang zh-CN

# 翻译记忆库查询
python scripts/tm_query.py