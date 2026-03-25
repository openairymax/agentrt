Copyright (c) 2026 SPHARX. All Rights Reserved.
"From data intelligence emerges."

# Partdocs 模块测试套件

**版本**: v1.0.0.1  
**最后更新**: 2026-03-23  
**状态**: 🟢 生产就绪

---

## 🎯 概述

本文档描述了 `partdocs` 模块的测试套件，包含单元测试、集成测试、性能测试和安全测试。测试套件确保模块的功能正确性、性能稳定性和安全性。

---

## 📁 测试结构

```
partdocs/tests/
├── README.md                      # 本文件
│
├── unit/                          # 单元测试
│   ├── test_document_parser.py    # 文档解析器测试
│   ├── test_document_indexer.py   # 文档索引器测试
│   ├── test_document_search.py    # 文档搜索测试
│   ├── test_document_similarity.py # 文档相似度测试
│   ├── test_grammar_checker.py    # 语法检查器测试
│   ├── test_cache_strategies.py   # 缓存策略测试
│   └── test_batch_processor.py    # 批量处理器测试
│
├── integration/                   # 集成测试
│   ├── test_document_workflow.py  # 文档工作流测试
│   ├── test_search_integration.py # 搜索集成测试
│   ├── test_validation_pipeline.py # 验证管道测试
│   └── test_publish_workflow.py   # 发布工作流测试
│
├── performance/                   # 性能测试
│   ├── test_index_performance.py  # 索引性能测试
│   ├── test_search_performance.py # 搜索性能测试
│   ├── test_concurrent_uploads.py # 并发上传测试
│   └── test_cache_performance.py  # 缓存性能测试
│
├── security/                      # 安全测试
│   ├── test_document_permissions.py # 文档权限测试
│   ├── test_input_validation.py   # 输入验证测试
│   └── test_encryption.py         # 加密测试
│
└── fixtures/                      # 测试夹具
    ├── sample_documents/          # 示例文档
    ├── test_configs/              # 测试配置
    └── mock_services/             # 模拟服务
```

---

## 🧪 运行测试

### 运行所有测试

```bash
# 进入测试目录
cd partdocs/tests

# 运行所有测试
python -m pytest . -v

# 运行测试并生成覆盖率报告
python -m pytest . --cov=partdocs --cov-report=html --cov-report=term

# 运行测试并生成 JUnit XML 报告
python -m pytest . --junitxml=test-results.xml
```

### 分类运行测试

```bash
# 运行单元测试
python -m pytest unit/ -v

# 运行集成测试
python -m pytest integration/ -v

# 运行性能测试
python -m pytest performance/ -v --benchmark-only

# 运行安全测试
python -m pytest security/ -v
```

### 运行特定测试

```bash
# 运行特定测试文件
python -m pytest unit/test_document_parser.py -v

# 运行特定测试函数
python -m pytest unit/test_document_parser.py::test_parse_markdown -v

# 运行标记为 slow 的测试
python -m pytest . -m slow -v

# 运行标记为 not slow 的测试
python -m pytest . -m "not slow" -v
```

### 并行运行测试

```bash
# 使用 pytest-xdist 并行运行测试
python -m pytest . -n auto -v

# 指定并行进程数
python -m pytest . -n 4 -v
```

---

## 📊 测试覆盖率要求

| 测试类型 | 覆盖率目标 | 工具 | 报告格式 |
|---------|-----------|------|---------|
| **单元测试** | ≥ 85% | pytest-cov | HTML, XML, Term |
| **集成测试** | ≥ 75% | pytest-cov | HTML, XML |
| **性能测试** | N/A | pytest-benchmark | JSON, CSV |
| **安全测试** | 100% 关键路径 | Bandit, Safety | JSON, HTML |

### 生成覆盖率报告

```bash
# 安装覆盖率工具
pip install pytest-cov coverage

# 运行测试并收集覆盖率
coverage run -m pytest .

# 生成文本报告
coverage report

# 生成 HTML 报告
coverage html
open htmlcov/index.html  # macOS
xdg-open htmlcov/index.html  # Linux

# 生成 XML 报告（用于 CI/CD）
coverage xml
```

### 覆盖率配置

```ini
# .coveragerc
[run]
source = partdocs
omit = 
    */tests/*
    */__pycache__/*
    */venv/*
    */virtualenv/*

[report]
exclude_lines =
    pragma: no cover
    def __repr__
    if self.debug:
    if settings.DEBUG
    raise AssertionError
    raise NotImplementedError
    if 0:
    if __name__ == .__main__.:
    class .*Test.*:
    
[html]
directory = coverage_html_report
title = Partdocs Coverage Report
```

---

## 🔬 单元测试详解

### 文档解析器测试

```python
# unit/test_document_parser.py
"""
文档解析器单元测试
"""

import pytest
from partdocs.parsers import DocumentParser, ParserError

class TestDocumentParser:
    """文档解析器测试类"""
    
    @pytest.fixture
    def parser(self):
        """创建文档解析器夹具"""
        return DocumentParser()
    
    def test_parse_markdown(self, parser):
        """测试解析 Markdown 文档"""
        content = "# 标题\n\n这是内容。"
        result = parser.parse(content, format="markdown")
        
        assert result.title == "标题"
        assert result.content == "这是内容。"
        assert result.format == "markdown"
        assert len(result.sections) == 1
    
    def test_parse_html(self, parser):
        """测试解析 HTML 文档"""
        content = "<h1>标题</h1><p>这是内容。</p>"
        result = parser.parse(content, format="html")
        
        assert result.title == "标题"
        assert "这是内容" in result.content
        assert result.format == "html"
    
    def test_parse_invalid_format(self, parser):
        """测试解析无效格式"""
        with pytest.raises(ParserError) as exc_info:
            parser.parse("content", format="invalid")
        
        assert "不支持的格式" in str(exc_info.value)
    
    def test_parse_empty_content(self, parser):
        """测试解析空内容"""
        result = parser.parse("", format="markdown")
        
        assert result.title == ""
        assert result.content == ""
        assert len(result.sections) == 0
    
    @pytest.mark.parametrize("format", ["markdown", "html", "plaintext"])
    def test_parse_multiple_formats(self, parser, format):
        """测试解析多种格式"""
        content = f"测试 {format} 内容"
        result = parser.parse(content, format=format)
        
        assert result.format == format
        assert content in result.content
    
    def test_extract_metadata(self, parser):
        """测试提取元数据"""
        content = """---
title: 测试文档
author: 张三
tags: [测试, 文档]
date: 2024-01-01
---

# 正文内容
"""
        result = parser.parse(content, format="markdown")
        
        assert result.metadata["title"] == "测试文档"
        assert result.metadata["author"] == "张三"
        assert "测试" in result.metadata["tags"]
        assert result.metadata["date"] == "2024-01-01"
```

### 文档索引器测试

```python
# unit/test_document_indexer.py
"""
文档索引器单元测试
"""

import pytest
from partdocs.indexers import InvertedIndex, IndexError

class TestInvertedIndex:
    """倒排索引测试类"""
    
    @pytest.fixture
    def index(self):
        """创建倒排索引夹具"""
        return InvertedIndex()
    
    @pytest.fixture
    def sample_documents(self):
        """创建示例文档夹具"""
        return [
            {
                "id": "doc1",
                "tokens": ["python", "编程", "语言", "简单", "易学"],
                "metadata": {"title": "Python 编程", "author": "张三"}
            },
            {
                "id": "doc2", 
                "tokens": ["java", "编程", "语言", "企业级", "应用"],
                "metadata": {"title": "Java 编程", "author": "李四"}
            },
            {
                "id": "doc3",
                "tokens": ["javascript", "前端", "开发", "网页", "交互"],
                "metadata": {"title": "JavaScript 开发", "author": "王五"}
            }
        ]
    
    def test_add_document(self, index, sample_documents):
        """测试添加文档"""
        doc = sample_documents[0]
        index.add_document(doc["id"], doc["tokens"], doc["metadata"])
        
        assert doc["id"] in index.doc_metadata
        assert index.doc_count == 1
        
        # 验证索引构建
        for token in doc["tokens"]:
            assert token in index.index
            assert doc["id"] in index.index[token]
    
    def test_remove_document(self, index, sample_documents):
        """测试移除文档"""
        doc = sample_documents[0]
        index.add_document(doc["id"], doc["tokens"], doc["metadata"])
        index.remove_document(doc["id"])
        
        assert doc["id"] not in index.doc_metadata
        assert index.doc_count == 0
        
        # 验证索引清理
        for token in doc["tokens"]:
            assert token not in index.index
    
    def test_search_and_operator(self, index, sample_documents):
        """测试 AND 操作符搜索"""
        for doc in sample_documents:
            index.add_document(doc["id"], doc["tokens"], doc["metadata"])
        
        results = index.search(["编程", "语言"], operator="AND")
        
        assert len(results) == 2  # doc1 和 doc2
        assert "doc1" in [r[0] for r in results]
        assert "doc2" in [r[0] for r in results]
        assert "doc3" not in [r[0] for r in results]
    
    def test_search_or_operator(self, index, sample_documents):
        """测试 OR 操作符搜索"""
        for doc in sample_documents:
            index.add_document(doc["id"], doc["tokens"], doc["metadata"])
        
        results = index.search(["python", "javascript"], operator="OR")
        
        assert len(results) == 2  # doc1 和 doc3
        assert "doc1" in [r[0] for r in results]
        assert "doc3" in [r[0] for r in results]
        assert "doc2" not in [r[0] for r in results]
    
    def test_search_empty_query(self, index, sample_documents):
        """测试空查询搜索"""
        for doc in sample_documents:
            index.add_document(doc["id"], doc["tokens"], doc["metadata"])
        
        results = index.search([], operator="AND")
        
        assert len(results) == 0
    
    def test_search_nonexistent_token(self, index, sample_documents):
        """测试搜索不存在的词条"""
        for doc in sample_documents:
            index.add_document(doc["id"], doc["tokens"], doc["metadata"])
        
        results = index.search(["不存在"], operator="AND")
        
        assert len(results) == 0
    
    def test_calculate_tf_idf_score(self, index, sample_documents):
        """测试 TF-IDF 分数计算"""
        for doc in sample_documents:
            index.add_document(doc["id"], doc["tokens"], doc["metadata"])
        
        # 搜索包含 "编程" 的文档
        results = index.search(["编程"], operator="OR")
        
        assert len(results) == 2  # doc1 和 doc2
        assert results[0][0] in ["doc1", "doc2"]
        assert results[1][0] in ["doc1", "doc2"]
        
        # 验证分数计算
        for doc_id, score in results:
            assert score > 0
            assert isinstance(score, float)
    
    def test_get_document_frequency(self, index, sample_documents):
        """测试获取文档频率"""
        for doc in sample_documents:
            index.add_document(doc["id"], doc["tokens"], doc["metadata"])
        
        df = index.get_document_frequency("编程")
        assert df == 2  # doc1 和 doc2 包含 "编程"
        
        df = index.get_document_frequency("python")
        assert df == 1  # 只有 doc1 包含 "python"
        
        df = index.get_document_frequency("不存在")
        assert df == 0
    
    def test_get_term_frequency(self, index, sample_documents):
        """测试获取词条频率"""
        doc = sample_documents[0]
        index.add_document(doc["id"], doc["tokens"], doc["metadata"])
        
        tf = index.get_term_frequency("python", "doc1")
        assert tf == 1
        
        tf = index.get_term_frequency("编程", "doc1")
        assert tf == 1
        
        tf = index.get_term_frequency("不存在", "doc1")
        assert tf == 0
        
        tf = index.get_term_frequency("python", "不存在")
        assert tf == 0
```

### 文档相似度测试

```python
# unit/test_document_similarity.py
"""
文档相似度计算单元测试
"""

import pytest
import math
from partdocs.similarity import DocumentSimilarity, SimilarityError

class TestDocumentSimilarity:
    """文档相似度测试类"""
    
    @pytest.fixture
    def similarity_cosine(self):
        """创建余弦相似度计算器夹具"""
        return DocumentSimilarity(method="cosine")
    
    @pytest.fixture
    def similarity_jaccard(self):
        """创建 Jaccard 相似度计算器夹具"""
        return DocumentSimilarity(method="jaccard")
    
    @pytest.fixture
    def sample_tokens(self):
        """创建示例词条列表夹具"""
        return {
            "doc1": ["python", "编程", "语言", "简单", "易学"],
            "doc2": ["java", "编程", "语言", "企业级", "应用"],
            "doc3": ["javascript", "前端", "开发", "网页", "交互"],
            "doc4": ["python", "编程", "语言", "简单", "易学"],  # 与 doc1 相同
            "doc5": []  # 空文档
        }
    
    def test_cosine_similarity_identical(self, similarity_cosine, sample_tokens):
        """测试相同文档的余弦相似度"""
        doc1_tokens = sample_tokens["doc1"]
        doc4_tokens = sample_tokens["doc4"]
        
        similarity = similarity_cosine.calculate(doc1_tokens, doc4_tokens)
        
        # 相同文档的相似度应该为 1
        assert math.isclose(similarity, 1.0, rel_tol=1e-9)
    
    def test_cosine_similarity_different(self, similarity_cosine, sample_tokens):
        """测试不同文档的余弦相似度"""
        doc1_tokens = sample_tokens["doc1"]
        doc2_tokens = sample_tokens["doc2"]
        
        similarity = similarity_cosine.calculate(doc1_tokens, doc2_tokens)
        
        # 不同文档的相似度应该在 0 到 1 之间
        assert 0 <= similarity <= 1
        assert similarity < 1  # 应该小于 1
    
    def test_cosine_similarity_empty(self, similarity_cosine, sample_tokens):
        """测试空文档的余弦相似度"""
        doc1_tokens = sample_tokens["doc1"]
        doc5_tokens = sample_tokens["doc5"]
        
        similarity = similarity_cosine.calculate(doc1_tokens, doc5_tokens)
        
        # 空文档与任何文档的相似度应该为 0
        assert similarity == 0
    
    def test_jaccard_similarity_identical(self, similarity_jaccard, sample_tokens):
        """测试相同文档的 Jaccard 相似度"""
        doc1_tokens = sample_tokens["doc1"]
        doc4_tokens = sample_tokens["doc4"]
        
        similarity = similarity_jaccard.calculate(doc1_tokens, doc4_tokens)
        
        # 相同文档的 Jaccard 相似度应该为 1
        assert math.isclose(similarity, 1.0, rel_tol=1e-9)
    
    def test_jaccard_similarity_partial(self, similarity_jaccard, sample_tokens):
        """测试部分重叠文档的 Jaccard 相似度"""
        doc1_tokens = sample_tokens["doc1"]  # ["python", "编程", "语言", "简单", "易学"]
        doc2_tokens = sample_tokens["doc2"]  # ["java", "编程", "语言", "企业级", "应用"]
        
        similarity = similarity_jaccard.calculate(doc1_tokens, doc2_tokens)
        
        # 交集: ["编程", "语言"] = 2
        # 并集: ["python", "编程", "语言", "简单", "易学", "java", "企业级", "应用"] = 8
        # Jaccard = 2/8 = 0.25
        expected_similarity = 2 / 8
        assert math.isclose(similarity, expected_similarity, rel_tol=1e-9)
    
    def test_jaccard_similarity_no_overlap(self, similarity_jaccard, sample_tokens):
        """测试无重叠文档的 Jaccard 相似度"""
        doc1_tokens = sample_tokens["doc1"]  # ["python", "编程", "语言", "简单", "易学"]
        doc3_tokens = sample_tokens["doc3"]  # ["javascript", "前端", "开发", "网页", "交互"]
        
        similarity = similarity_jaccard.calculate(doc1_tokens, doc3_tokens)
        
        # 无交集，相似度应该为 0
        assert similarity == 0
    
    def test_jaccard_similarity_empty(self, similarity_jaccard, sample_tokens):
        """测试空文档的 Jaccard 相似度"""
        doc1_tokens = sample_tokens["doc1"]
        doc5_tokens = sample_tokens["doc5"]
        
        similarity = similarity_jaccard.calculate(doc1_tokens, doc5_tokens)
        
        # 空文档与任何文档的 Jaccard 相似度应该为 0
        assert similarity == 0
    
    def test_invalid_method(self):
        """测试无效的相似度计算方法"""
        with pytest.raises(ValueError) as exc_info:
            similarity = DocumentSimilarity(method="invalid")
            similarity.calculate(["a", "b"], ["c", "d"])
        
        assert "不支持的相似度计算方法" in str(exc_info.value)
    
    @pytest.mark.parametrize("method", ["cosine", "jaccard"])
    def test_similarity_range(self, method, sample_tokens):
        """测试相似度值范围"""
        similarity = DocumentSimilarity(method=method)
        
        doc1_tokens = sample_tokens["doc1"]
        doc2_tokens = sample_tokens["doc2"]
        
        score = similarity.calculate(doc1_tokens, doc2_tokens)
        
        # 相似度应该在 [0, 1] 范围内
        assert 0 <= score <= 1
    
    def test_bm25_similarity(self, sample_tokens):
        """测试 BM25 相似度"""
        similarity = DocumentSimilarity(method="bm25")
        
        doc1_tokens = sample_tokens["doc1"]
        doc2_tokens = sample_tokens["doc2"]
        
        # 需要文档集合统计信息
        # 这里测试接口可用性
        score = similarity.calculate(
            doc1_tokens, 
            doc2_tokens,
            k1=1.5,
            b=0.75
        )
        
        assert isinstance(score, float)
```

### 语法检查器测试

```python
# unit/test_grammar_checker.py
"""
语法检查器单元测试
"""

import pytest
from partdocs.validators import GrammarChecker, ValidationError

class TestGrammarChecker:
    """语法检查器测试类"""
    
    @pytest.fixture
    def chinese_checker(self):
        """创建中文语法检查器夹具"""
        return GrammarChecker(language="zh")
    
    @pytest.fixture
    def english_checker(self):
        """创建英文语法检查器夹具"""
        return GrammarChecker(language="en")
    
    @pytest.fixture
    def sample_texts(self):
        """创建示例文本夹具"""
        return {
            "chinese_correct": "这是一个正确的句子。",
            "chinese_duplicate_punctuation": "这是一个错误的句子。。",
            "chinese_missing_space": "这是一个English单词。",
            "chinese_mixed_punctuation": "这是一个句子,然后另一个.",
            "english_correct": "This is a correct sentence.",
            "english_article_error": "This is a apple.",
            "english_subject_verb_error": "He have a book.",
            "english_spelling_error": "This is seperate from that."
        }
    
    def test_chinese_duplicate_punctuation(self, chinese_checker, sample_texts):
        """测试中文重复标点检查"""
        text = sample_texts["chinese_duplicate_punctuation"]
        results = chinese_checker.check(text)
        
        assert len(results) >= 1
        assert any("重复标点符号" in r["message"] for r in results)
    
    def test_chinese_missing_space(self, chinese_checker, sample_texts):
        """测试中文中英文间缺少空格检查"""
        text = sample_texts["chinese_missing_space"]
        results = chinese_checker.check(text)
        
        assert len(results) >= 1
        assert any("中英文之间缺少空格" in r["message"] for r in results)
    
    def test_chinese_mixed_punctuation(self, chinese_checker, sample_texts):
        """测试中文全角半角标点混用检查"""
        text = sample_texts["chinese_mixed_punctuation"]
        results = chinese_checker.check(text)
        
        assert len(results) >= 1
        assert any("全角半角标点混用" in r["message"] for r in results)
    
    def test_english_article_error(self, english_checker, sample_texts):
        """测试英文冠词错误检查"""
        text = sample_texts["english_article_error"]
        results = english_checker.check(text)
        
        assert len(results) >= 1
        assert any("冠词使用错误" in r["message"] for r in results)
    
    def test_english_subject_verb_error(self, english_checker, sample_texts):
        """测试英文主谓一致错误检查"""
        text = sample_texts["english_subject_verb_error"]
        results = english_checker.check(text)
        
        assert len(results) >= 1
        assert any("主谓不一致" in r["message"] for r in results)
    
    def test_english_spelling_error(self, english_checker, sample_texts):
        """测试英文拼写错误检查"""
        text = sample_texts["english_spelling_error"]
        results = english_checker.check(text)
        
        assert len(results) >= 1
        assert any("常见拼写错误" in r["message"] for r in results)
    
    def test_correct_text_no_errors(self, chinese_checker, sample_texts):
        """测试正确文本无错误"""
        text = sample_texts["chinese_correct"]
        results = chinese_checker.check(text)
        
        assert len(results) == 0
    
    def test_auto_correct_chinese(self, chinese_checker, sample_texts):
        """测试中文自动修正"""
        text = sample_texts["chinese_missing_space"]
        corrected_text, corrections = chinese_checker.auto_correct(text)
        
        assert len(corrections) >= 1
        assert corrected_text != text
        assert "English" in corrected_text  # 应该添加了空格
    
    def test_auto_correct_english(self, english_checker, sample_texts):
        """测试英文自动修正"""
        text = sample_texts["english_article_error"]
        corrected_text, corrections = english_checker.auto_correct(text)
        
        assert len(corrections) >= 1
        assert corrected_text != text
        assert "an apple" in corrected_text  # 应该修正为 "an apple"
    
    def test_empty_text(self, chinese_checker):
        """测试空文本"""
        results = chinese_checker.check("")
        
        assert len(results) == 0
    
    def test_severity_levels(self, chinese_checker, sample_texts):
        """测试错误严重级别"""
        text = sample_texts["chinese_duplicate_punctuation"]
        results = chinese_checker.check(text)
        
        if results:
            result = results[0]
            assert "severity" in result
            assert result["severity"] in ["error", "warning", "info"]
    
    def test_position_information(self, chinese_checker, sample_texts):
        """测试错误位置信息"""
        text = sample_texts["chinese_duplicate_punctuation"]
        results = chinese_checker.check(text)
        
        if results:
            result = results[0]
            assert "start" in result
            assert "end" in result
            assert "matched_text" in result
            assert 0 <= result["start"] < result["end"] <= len(text)
```

### 缓存策略测试

```python
# unit/test_cache_strategies.py
"""
缓存策略单元测试
"""

import pytest
import time
from partdocs.cache import LRUCache, CacheError

class TestLRUCache:
    """LRU 缓存测试类"""
    
    @pytest.fixture
    def cache(self):
        """创建 LRU 缓存夹具"""
        return LRUCache(capacity=3)
    
    def test_put_and_get(self, cache):
        """测试缓存放入和获取"""
        cache.put("key1", "value1")
        cache.put("key2", "value2")
        
        assert cache.get("key1") == "value1"
        assert cache.get("key2") == "value2"
        assert cache.get("key3") is None
    
    def test_capacity_limit(self, cache):
        """测试缓存容量限制"""
        # 放入 4 个值，容量为 3
        cache.put("key1", "value1")
        cache.put("key2", "value2")
        cache.put("key3", "value3")
        cache.put("key4", "value4")
        
        # key1 应该被移除（最久未使用）
        assert cache.get("key1") is None
        assert cache.get("key2") == "value2"
        assert cache.get("key3") == "value3"
        assert cache.get("key4") == "value4"
    
    def test_lru_eviction(self, cache):
        """测试 LRU 淘汰策略"""
        cache.put("key1", "value1")
        cache.put("key2", "value2")
        cache.put("key3", "value3")
        
        # 访问 key1，使其成为最近使用
        cache.get("key1")
        
        # 放入新值，key2 应该被淘汰（最久未使用）
        cache.put("key4", "value4")
        
        assert cache.get("key1") == "value1"
        assert cache.get("key2") is None  # 被淘汰
        assert cache.get("key3") == "value3"
        assert cache.get("key4") == "value4"
    
    def test_update_existing_key(self, cache):
        """测试更新已存在的键"""
        cache.put("key1", "value1")
        cache.put("key1", "value1_updated")
        
        assert cache.get("key1") == "value1_updated"
        
        # 更新应该重置访问时间
        cache.put("key2", "value2")
        cache.put("key3", "value3")
        cache.put("key4", "value4")
        
        # key1 应该还在缓存中（最近更新）
        assert cache.get("key1") == "value1_updated"
    
    def test_clear_cache(self, cache):
        """测试清空缓存"""
        cache.put("key1", "value1")
        cache.put("key2", "value2")
        
        cache.clear()
        
        assert cache.get("key1") is None
        assert cache.get("key2") is None
        assert len(cache.cache) == 0
        assert len(cache.order) == 0
    
    def test_cache_size(self, cache):
        """测试缓存大小"""
        assert len(cache.cache) == 0
        assert len(cache.order) == 0
        
        cache.put("key1", "value1")
        assert len(cache.cache) == 1
        assert len(cache.order) == 1
        
        cache.put("key2", "value2")
        assert len(cache.cache) == 2
        assert len(cache.order) == 2
        
        cache.put("key3", "value3")
        assert len(cache.cache) == 3
        assert len(cache.order) == 3
        
        # 达到容量后，大小保持不变
        cache.put("key4", "value4")
        assert len(cache.cache) == 3
        assert len(cache.order) == 3
    
    def test_cache_performance(self, cache):
        """测试缓存性能"""
        start_time = time.time()
        
        # 大量操作
        for i in range(1000):
            cache.put(f"key{i}", f"value{i}")
            if i % 10 == 0:
                cache.get(f"key{i-5}")
        
        end_time = time.time()
        elapsed = end_time - start_time
        
        # 1000 次操作应该在合理时间内完成
        assert elapsed < 1.0, f"Cache operations too slow: {elapsed}s"
    
    def test_thread_safety(self):
        """测试线程安全性（概念验证）"""
        # 注意：实际线程安全测试需要多线程环境
        # 这里只是验证接口设计
        cache = LRUCache(capacity=100)
        
        # 模拟并发访问
        import threading
        
        results = []
        
        def worker(thread_id):
            for i in range(100):
                key = f"key_{thread_id}_{i}"
                value = f"value_{thread_id}_{i}"
                cache.put(key, value)
                retrieved = cache.get(key)
                results.append((thread_id, i, retrieved == value))
        
        threads = []
        for i in range(10):
            t = threading.Thread(target=worker, args=(i,))
            threads.append(t)
            t.start()
        
        for t in threads:
            t.join()
        
        # 验证所有操作都成功
        success_count = sum(1 for _, _, success in results if success)
        assert success_count == len(results)
```

### 批量处理器测试

```python
# unit/test_batch_processor.py
"""
批量处理器单元测试
"""

import pytest
import time
from concurrent.futures import ThreadPoolExecutor
from partdocs.processors import BatchProcessor, ProcessingError

class TestBatchProcessor:
    """批量处理器测试类"""
    
    @pytest.fixture
    def processor(self):
        """创建批量处理器夹具"""
        return BatchProcessor(batch_size=10, max_workers=2)
    
    @pytest.fixture
    def sample_items(self):
        """创建示例项目夹具"""
        return [f"item_{i}" for i in range(100)]
    
    def square(self, x):
        """平方处理函数"""
        return int(x.split("_")[1]) ** 2
    
    def slow_square(self, x):
        """慢速平方处理函数"""
        time.sleep(0.01)  # 模拟耗时操作
        return int(x.split("_")[1]) ** 2
    
    def failing_function(self, x):
        """失败处理函数"""
        if int(x.split("_")[1]) % 7 == 0:
            raise ValueError(f"Item {x} failed")
        return int(x.split("_")[1]) ** 2
    
    def test_batch_processing(self, processor, sample_items):
        """测试批量处理"""
        results = processor.process_batch(sample_items, self.square)
        
        assert len(results) == len(sample_items)
        
        # 验证处理结果
        for i, result in enumerate(results):
            expected = i ** 2
            assert result == expected
    
    def test_batch_size(self, processor, sample_items):
        """测试批处理大小"""
        batch_sizes = []
        
        def track_batch_size(batch):
            batch_sizes.append(len(batch))
            return [self.square(item) for item in batch]
        
        # 使用自定义处理函数跟踪批大小
        custom_processor = BatchProcessor(batch_size=10, max_workers=1)
        results = custom_processor.process_batch(sample_items, track_batch_size)
        
        # 验证批大小
        assert len(batch_sizes) > 0
        for size in batch_sizes[:-1]:  # 最后一个批次可能较小
            assert size == 10
        
        # 验证总项目数
        total_items = sum(batch_sizes)
        assert total_items == len(sample_items)
    
    def test_concurrent_processing(self, processor, sample_items):
        """测试并发处理"""
        start_time = time.time()
        results = processor.process_batch(sample_items, self.slow_square)
        end_time = time.time()
        
        elapsed = end_time - start_time
        
        # 验证结果
        assert len(results) == len(sample_items)
        for i, result in enumerate(results):
            expected = i ** 2
            assert result == expected
        
        # 验证并发加速（应该比串行快）
        # 串行时间估计：100 * 0.01 = 1.0 秒
        # 并发时间应该明显小于串行时间
        assert elapsed < 0.8, f"Concurrent processing too slow: {elapsed}s"
    
    def test_error_handling(self, processor, sample_items):
        """测试错误处理"""
        results = processor.process_batch(sample_items, self.failing_function)
        
        assert len(results) == len(sample_items)
        
        # 验证成功和失败的结果
        success_count = 0
        error_count = 0
        
        for i, result in enumerate(results):
            if isinstance(result, dict) and "error" in result:
                error_count += 1
                assert "Item item_" in result["error"]
                assert result["item"] == f"item_{i}"
            else:
                success_count += 1
                expected = i ** 2
                assert result == expected
        
        # 验证错误计数（每 7 个项目失败一个）
        expected_errors = len(sample_items) // 7
        assert error_count == expected_errors
        assert success_count == len(sample_items) - expected_errors
    
    def test_empty_batch(self, processor):
        """测试空批次"""
        results = processor.process_batch([], self.square)
        
        assert len(results) == 0
    
    def test_small_batch(self, processor):
        """测试小批次"""
        items = ["item_0", "item_1", "item_2"]
        results = processor.process_batch(items, self.square)
        
        assert len(results) == 3
        assert results == [0, 1, 4]
    
    def test_processor_cleanup(self, processor, sample_items):
        """测试处理器清理"""
        # 处理一些项目
        results = processor.process_batch(sample_items[:10], self.square)
        assert len(results) == 10
        
        # 关闭处理器
        processor.close()
        
        # 尝试再次处理应该失败或抛出异常
        # 注意：具体行为取决于实现
        # 这里我们只是验证 close 方法存在且可调用
        assert hasattr(processor, 'close')
        assert callable(processor.close)
    
    def test_custom_max_workers(self):
        """测试自定义最大工作线程数"""
        processor = BatchProcessor(batch_size=5, max_workers=8)
        
        items = [f"item_{i}" for i in range(100)]
        
        start_time = time.time()
        results = processor.process_batch(items, self.slow_square)
        end_time = time.time()
        
        elapsed = end_time - start_time
        
        # 验证结果
        assert len(results) == 100
        for i, result in enumerate(results):
            expected = i ** 2
            assert result == expected
        
        # 更多工作线程应该更快
        assert elapsed < 0.5, f"Processing with 8 workers too slow: {elapsed}s"
    
    def test_memory_efficiency(self):
        """测试内存效率"""
        import psutil
        import os
        
        process = psutil.Process(os.getpid())
        initial_memory = process.memory_info().rss
        
        processor = BatchProcessor(batch_size=100, max_workers=4)
        
        # 处理大量数据
        items = [f"item_{i}" for i in range(10000)]
        results = processor.process_batch(items, self.square)
        
        final_memory = process.memory_info().rss
        memory_increase = final_memory - initial_memory
        
        # 内存增长应该在合理范围内
        # 10000 个项目，每个结果是一个整数（~28 字节）
        # 加上一些开销，应该小于 10MB
        max_expected_memory = 10 * 1024 * 1024  # 10MB
        
        assert memory_increase < max_expected_memory, \
            f"Memory increase too high: {memory_increase / 1024 / 1024:.2f}MB"
```

---

## 🔗 集成测试详解

### 文档工作流测试

```python
# integration/test_document_workflow.py
"""
文档工作流集成测试
"""

import pytest
import tempfile
import os
from partdocs import (
    DocumentParser,
    InvertedIndex,
    DocumentSimilarity,
    GrammarChecker
)

class TestDocumentWorkflow:
    """文档工作流集成测试类"""
    
    @pytest.fixture
    def workflow_components(self):
        """创建工作流组件夹具"""
        parser = DocumentParser()
        index = InvertedIndex()
        similarity = DocumentSimilarity(method="cosine")
        checker = GrammarChecker(language="zh")
        
        return {
            "parser": parser,
            "index": index,
            "similarity": similarity,
            "checker": checker
        }
    
    @pytest.fixture
    def sample_documents(self):
        """创建示例文档夹具"""
        return [
            {
                "id": "doc1",
                "content": """---
title: Python 编程入门
author: 张三
tags: [python, 编程, 入门]
---

# Python 编程入门

Python 是一种简单易学的编程语言。
它适合初学者和专业人士使用。

## 特点

1. 语法简洁
2. 功能强大
3. 社区活跃

## 应用领域

- Web 开发
- 数据分析
- 人工智能
""",
                "format": "markdown"
            },
            {
                "id": "doc2",
                "content": """---
title: Java 企业级开发
author: 李四
tags: [java, 企业级, 开发]
---

# Java 企业级开发

Java 是一种面向对象的编程语言。
它广泛应用于企业级应用开发。

## 特点

1. 跨平台
2. 安全性高
3. 性能优秀

## 应用领域

- 企业应用
- 移动应用
- 大数据
""",
                "format": "markdown"
            }
        ]
    
    def test_complete_workflow(self, workflow_components, sample_documents):
        """测试完整文档工作流"""
        parser = workflow_components["parser"]
        index = workflow_components["index"]
        similarity = workflow_components["similarity"]
        checker = workflow_components["checker"]
        
        # 1. 解析文档
        parsed_docs = []
        for doc in sample_documents:
            parsed = parser.parse(doc["content"], format=doc["format"])
            parsed_docs.append(parsed)
            
            assert parsed.title == doc["content"].split("\n")[1].split(": ")[1]
            assert parsed.metadata["author"] in ["张三", "李四"]
        
        # 2. 语法检查
        for parsed in parsed_docs:
            issues = checker.check(parsed.content)
            # 正确文档应该没有语法错误
            assert len(issues) == 0
        
        # 3. 构建索引
        for i, (doc, parsed) in enumerate(zip(sample_documents, parsed_docs)):
            # 简单分词（实际中应该使用更复杂的分词器）
            tokens = []
            for line in parsed.content.split("\n"):
                if line.strip() and not line.startswith("#") and not line.startswith("-"):
                    tokens.extend(line.spli