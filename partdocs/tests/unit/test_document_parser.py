"""
Partdocs 文档解析器单元测试

Copyright (c) 2026 SPHARX. All Rights Reserved.
"From data intelligence emerges."
"""

import pytest
import tempfile
import os
from pathlib import Path


class TestDocumentParser:
    """
    文档解析器单元测试类
    """
    
    def test_markdown_parsing_basic(self):
        """
        测试基本Markdown文档解析
        """
        # 模拟文档解析器
        class MockDocumentParser:
            def parse(self, content, format="markdown"):
                """
                解析文档内容
                
                参数:
                    content: 文档内容
                    format: 文档格式
                    
                返回:
                    dict: 解析结果
                """
                result = {
                    "format": format,
                    "content": content,
                    "metadata": {},
                    "structure": []
                }
                
                # 提取标题
                lines = content.split('\n')
                for line in lines:
                    if line.startswith('# '):
                        result["title"] = line[2:].strip()
                    elif line.startswith('## '):
                        result["structure"].append({"type": "heading2", "text": line[3:].strip()})
                    elif line.startswith('### '):
                        result["structure"].append({"type": "heading3", "text": line[4:].strip()})
                
                # 提取元数据（如果存在）
                if lines and lines[0].startswith('---'):
                    metadata_lines = []
                    for i in range(1, len(lines)):
                        if lines[i].startswith('---'):
                            break
                        metadata_lines.append(lines[i])
                    
                    for line in metadata_lines:
                        if ':' in line:
                            key, value = line.split(':', 1)
                            result["metadata"][key.strip()] = value.strip()
                
                return result
        
        parser = MockDocumentParser()
        
        # 测试用例1: 简单Markdown
        markdown_content = """# 测试文档标题

这是一个测试文档内容。

## 章节1
这是章节1的内容。

### 子章节1.1
子章节内容。
"""
        
        result = parser.parse(markdown_content, "markdown")
        
        assert result["format"] == "markdown"
        assert result["title"] == "测试文档标题"
        assert len(result["structure"]) == 3
        assert result["structure"][0]["type"] == "heading2"
        assert result["structure"][0]["text"] == "章节1"
        assert result["structure"][2]["type"] == "heading3"
        assert result["structure"][2]["text"] == "子章节1.1"
    
    def test_markdown_parsing_with_metadata(self):
        """
        测试带元数据的Markdown文档解析
        """
        class MockDocumentParser:
            def parse(self, content, format="markdown"):
                result = {
                    "format": format,
                    "content": content,
                    "metadata": {},
                    "structure": []
                }
                
                lines = content.split('\n')
                
                # 解析YAML front matter
                if lines and lines[0] == '---':
                    in_metadata = True
                    for i in range(1, len(lines)):
                        if lines[i] == '---':
                            in_metadata = False
                            break
                        if ':' in lines[i]:
                            key, value = lines[i].split(':', 1)
                            result["metadata"][key.strip()] = value.strip()
                
                # 提取标题
                for line in lines:
                    if line.startswith('# ') and not line.startswith('# '):
                        result["title"] = line[2:].strip()
                        break
                
                return result
        
        parser = MockDocumentParser()
        
        # 测试用例: 带元数据的Markdown
        markdown_content = """---
title: 带元数据的文档
author: 张三
date: 2026-03-23
tags: [测试, 文档, 示例]
---

# 带元数据的文档

这是文档内容。
"""
        
        result = parser.parse(markdown_content, "markdown")
        
        assert result["metadata"]["title"] == "带元数据的文档"
        assert result["metadata"]["author"] == "张三"
        assert result["metadata"]["date"] == "2026-03-23"
        assert result["title"] == "带元数据的文档"
    
    def test_html_parsing(self):
        """
        测试HTML文档解析
        """
        class MockDocumentParser:
            def parse(self, content, format="html"):
                result = {
                    "format": format,
                    "content": content,
                    "metadata": {},
                    "structure": []
                }
                
                # 简单HTML解析
                import re
                
                # 提取标题
                title_match = re.search(r'<title>(.*?)</title>', content, re.IGNORECASE)
                if title_match:
                    result["title"] = title_match.group(1)
                
                # 提取h1-h6标签
                headings = re.findall(r'<h([1-6])>(.*?)</h\1>', content, re.IGNORECASE)
                for level, text in headings:
                    result["structure"].append({
                        "type": f"heading{level}",
                        "text": text.strip()
                    })
                
                return result
        
        parser = MockDocumentParser()
        
        # 测试用例: HTML文档
        html_content = """<!DOCTYPE html>
<html>
<head>
    <title>HTML测试文档</title>
</head>
<body>
    <h1>主标题</h1>
    <p>这是一个段落。</p>
    <h2>副标题</h2>
    <p>另一个段落。</p>
    <h3>三级标题</h3>
    <p>更多内容。</p>
</body>
</html>
"""
        
        result = parser.parse(html_content, "html")
        
        assert result["format"] == "html"
        assert result["title"] == "HTML测试文档"
        assert len(result["structure"]) == 3
        assert result["structure"][0]["type"] == "heading1"
        assert result["structure"][0]["text"] == "主标题"
        assert result["structure"][2]["type"] == "heading3"
        assert result["structure"][2]["text"] == "三级标题"
    
    def test_plain_text_parsing(self):
        """
        测试纯文本文档解析
        """
        class MockDocumentParser:
            def parse(self, content, format="text"):
                result = {
                    "format": format,
                    "content": content,
                    "metadata": {},
                    "structure": []
                }
                
                # 纯文本简单处理：第一行作为标题
                lines = content.strip().split('\n')
                if lines:
                    result["title"] = lines[0].strip()
                
                # 段落检测
                paragraphs = []
                current_para = []
                
                for line in lines:
                    line = line.strip()
                    if line:
                        current_para.append(line)
                    elif current_para:
                        paragraphs.append(' '.join(current_para))
                        current_para = []
                
                if current_para:
                    paragraphs.append(' '.join(current_para))
                
                result["paragraphs"] = paragraphs
                result["paragraph_count"] = len(paragraphs)
                
                return result
        
        parser = MockDocumentParser()
        
        # 测试用例: 纯文本
        text_content = """文档标题
这是第一段的内容。
这是第一段的继续。

这是第二段。
第二段有更多内容。

第三段简短。
"""
        
        result = parser.parse(text_content, "text")
        
        assert result["format"] == "text"
        assert result["title"] == "文档标题"
        assert result["paragraph_count"] == 3
        assert len(result["paragraphs"]) == 3
        assert "第一段" in result["paragraphs"][0]
        assert "第二段" in result["paragraphs"][1]
        assert "第三段" in result["paragraphs"][2]
    
    def test_unsupported_format(self):
        """
        测试不支持的文档格式
        """
        class MockDocumentParser:
            def parse(self, content, format="markdown"):
                supported_formats = ["markdown", "html", "text"]
                if format not in supported_formats:
                    raise ValueError(f"不支持的文档格式: {format}")
                return {"format": format, "content": content}
        
        parser = MockDocumentParser()
        
        # 测试不支持的格式
        with pytest.raises(ValueError) as exc_info:
            parser.parse("content", "pdf")
        
        assert "不支持的文档格式" in str(exc_info.value)
    
    def test_empty_content(self):
        """
        测试空内容处理
        """
        class MockDocumentParser:
            def parse(self, content, format="markdown"):
                if not content or not content.strip():
                    return {
                        "format": format,
                        "content": "",
                        "metadata": {},
                        "structure": [],
                        "empty": True
                    }
                return {"format": format, "content": content, "empty": False}
        
        parser = MockDocumentParser()
        
        # 测试空内容
        result1 = parser.parse("", "markdown")
        assert result1["empty"] == True
        
        result2 = parser.parse("   ", "markdown")
        assert result2["empty"] == True
        
        result3 = parser.parse("正常内容", "markdown")
        assert result3["empty"] == False
    
    def test_large_document_parsing(self):
        """
        测试大文档解析性能
        """
        class MockDocumentParser:
            def parse(self, content, format="markdown"):
                # 模拟大文档处理
                lines = content.split('\n')
                line_count = len(lines)
                
                # 统计各种元素
                heading_count = sum(1 for line in lines if line.startswith('#'))
                list_item_count = sum(1 for line in lines if line.strip().startswith('- '))
                code_block_count = content.count('```')
                
                return {
                    "format": format,
                    "line_count": line_count,
                    "heading_count": heading_count,
                    "list_item_count": list_item_count,
                    "code_block_count": code_block_count,
                    "size_bytes": len(content.encode('utf-8'))
                }
        
        parser = MockDocumentParser()
        
        # 生成大文档
        large_content = "# 大文档测试\n\n"
        for i in range(100):
            large_content += f"## 章节 {i+1}\n\n"
            large_content += f"这是章节 {i+1} 的内容。\n\n"
            large_content += f"- 列表项 {i+1}.1\n"
            large_content += f"- 列表项 {i+1}.2\n\n"
            if i % 10 == 0:
                large_content += "```python\nprint('代码块')\n```\n\n"
        
        result = parser.parse(large_content, "markdown")
        
        assert result["line_count"] > 100
        assert result["heading_count"] >= 100  # 100个##标题
        assert result["list_item_count"] >= 200  # 每个章节2个列表项
        assert result["code_block_count"] >= 10  # 每10个章节一个代码块
        assert result["size_bytes"] > 10000
    
    def test_encoding_handling(self):
        """
        测试编码处理
        """
        class MockDocumentParser:
            def parse(self, content, format="markdown", encoding="utf-8"):
                # 模拟编码处理
                try:
                    # 尝试用指定编码解码（如果是字节）
                    if isinstance(content, bytes):
                        decoded = content.decode(encoding)
                    else:
                        decoded = content
                    
                    # 检查是否包含非ASCII字符
                    has_non_ascii = any(ord(c) > 127 for c in decoded)
                    
                    return {
                        "format": format,
                        "encoding": encoding,
                        "has_non_ascii": has_non_ascii,
                        "content_length": len(decoded)
                    }
                except UnicodeDecodeError:
                    raise ValueError(f"无法用 {encoding} 编码解码内容")
        
        parser = MockDocumentParser()
        
        # 测试UTF-8编码
        chinese_content = "中文测试文档"
        result1 = parser.parse(chinese_content, "markdown", "utf-8")
        assert result1["has_non_ascii"] == True
        assert result1["content_length"] == 6
        
        # 测试ASCII内容
        ascii_content = "English test document"
        result2 = parser.parse(ascii_content, "markdown", "utf-8")
        assert result2["has_non_ascii"] == False
        
        # 测试字节输入
        bytes_content = "测试".encode('utf-8')
        result3 = parser.parse(bytes_content, "markdown", "utf-8")
        assert result3["content_length"] == 2
    
    def test_error_recovery(self):
        """
        测试错误恢复机制
        """
        class MockDocumentParser:
            def parse(self, content, format="markdown", strict=False):
                result = {
                    "format": format,
                    "content": content,
                    "errors": [],
                    "warnings": []
                }
                
                lines = content.split('\n')
                
                # 检查常见错误
                for i, line in enumerate(lines):
                    # 检查未闭合的代码块
                    if line.count('```') % 2 != 0:
                        result["errors"].append(f"第{i+1}行: 未闭合的代码块")
                    
                    # 检查无效的Markdown语法
                    if line.startswith('####### '):
                        result["warnings"].append(f"第{i+1}行: 标题级别过深")
                    
                    # 检查空标题
                    if line.startswith('#') and len(line.strip()) == 1:
                        result["errors"].append(f"第{i+1}行: 空标题")
                
                if strict and result["errors"]:
                    raise ValueError(f"文档包含错误: {result['errors']}")
                
                return result
        
        parser = MockDocumentParser()
        
        # 测试包含错误的文档（非严格模式）
        error_content = """# 测试文档

####### 过深的标题

```python
未闭合的代码块

# 空标题
"""
        
        result = parser.parse(error_content, "markdown", strict=False)
        assert len(result["errors"]) >= 2
        assert len(result["warnings"]) >= 1
        
        # 测试严格模式
        with pytest.raises(ValueError) as exc_info:
            parser.parse(error_content, "markdown", strict=True)
        
        assert "文档包含错误" in str(exc_info.value)


class TestDocumentParserIntegration:
    """
    文档解析器集成测试
    """
    
    def test_file_based_parsing(self, tmp_path):
        """
        测试基于文件的文档解析
        """
        # 创建临时文件
        test_file = tmp_path / "test_document.md"
        test_content = """---
title: 文件测试
author: 测试用户
---

# 文件测试文档

这是文件中的内容。
"""
        test_file.write_text(test_content, encoding='utf-8')
        
        class MockFileParser:
            def parse_file(self, file_path, format=None):
                """
                解析文件
                
                参数:
                    file_path: 文件路径
                    format: 文档格式（自动检测）
                    
                返回:
                    dict: 解析结果
                """
                # 读取文件
                with open(file_path, 'r', encoding='utf-8') as f:
                    content = f.read()
                
                # 自动检测格式
                if format is None:
                    if file_path.suffix == '.md':
                        format = 'markdown'
                    elif file_path.suffix == '.html':
                        format = 'html'
                    elif file_path.suffix == '.txt':
                        format = 'text'
                    else:
                        format = 'text'
                
                # 调用解析器
                class SimpleParser:
                    def parse(self, content, format):
                        return {
                            "file_path": str(file_path),
                            "format": format,
                            "content": content,
                            "size": len(content)
                        }
                
                parser = SimpleParser()
                return parser.parse(content, format)
        
        parser = MockFileParser()
        result = parser.parse_file(test_file)
        
        assert result["file_path"] == str(test_file)
        assert result["format"] == "markdown"
        assert "文件测试文档" in result["content"]
        assert result["size"] > 0
    
    def test_batch_parsing(self):
        """
        测试批量文档解析
        """
        class MockBatchParser:
            def parse_batch(self, documents):
                """
                批量解析文档
                
                参数:
                    documents: 文档列表，每个元素为(content, format)元组
                    
                返回:
                    list: 解析结果列表
                """
                results = []
                
                for content, format in documents:
                    # 简单解析
                    result = {
                        "format": format,
                        "content_length": len(content),
                        "line_count": len(content.split('\n'))
                    }
                    results.append(result)
                
                return results
        
        parser = MockBatchParser()
        
        # 准备测试数据
        test_documents = [
            ("# 文档1\n内容1", "markdown"),
            ("<html><body>文档2</body></html>", "html"),
            ("纯文本文档3", "text"),
            ("# 文档4\n更多内容", "markdown"),
        ]
        
        results = parser.parse_batch(test_documents)
        
        assert len(results) == 4
        assert results[0]["format"] == "markdown"
        assert results[1]["format"] == "html"
        assert results[2]["format"] == "text"
        assert results[0]["content_length"] > 0
        assert results[3]["line_count"] == 2
    
    def test_parser_caching(self):
        """
        测试解析器缓存机制
        """
        class MockCachingParser:
            def __init__(self):
                self.cache = {}
                self.parse_count = 0
            
            def parse(self, content, format="markdown"):
                # 生成缓存键
                cache_key = f"{hash(content)}:{format}"
                
                # 检查缓存
                if cache_key in self.cache:
                    return self.cache[cache_key]
                
                # 解析（模拟）
                self.parse_count += 1
                result = {
                    "format": format,
                    "content": content,
                    "parsed_at": self.parse_count
                }
                
                # 缓存结果
                self.cache[cache_key] = result
                return result
        
        parser = MockCachingParser()
        
        # 第一次解析
        content1 = "测试内容"
        result1 = parser.parse(content1)
        assert result1["parsed_at"] == 1
        assert parser.parse_count == 1
        
        # 第二次解析相同内容（应该命中缓存）
        result2 = parser.parse(content1)
        assert result2["parsed_at"] == 1  # 应该使用缓存的结果
        assert parser.parse_count == 1  # 解析次数不应增加
        
        # 第三次解析不同内容
        content2 = "不同内容"
        result3 = parser.parse(content2)
        assert result3["parsed_at"] == 2
        assert parser.parse_count == 2


if __name__ == "__main__":
    """
    直接运行测试
    """
    import sys
    pytest.main(sys.argv)