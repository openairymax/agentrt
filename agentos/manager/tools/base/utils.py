#!/usr/bin/env python3
"""
AgentOS Manager Module - Base Utilities

提供Manager模块各工具共享的基础工具类，包括：
- ConfigLoader: 配置文件加载器
- ReportExporter: 报告导出器（JSON/Markdown）
- FileHelper: 文件操作辅助

Usage:
    from tools.base.utils import ConfigLoader, ReportExporter, FileHelper
"""

import hashlib
import json
import os
from datetime import datetime, timezone
from pathlib import Path
from typing import Any, Dict, List, Optional, Union


class ConfigLoader:
    """配置文件加载器
    
    统一处理YAML/JSON配置文件的加载，支持：
    - UTF-8编码（自动移除BOM）
    - 多种格式（.yaml, .yml, .json）
    - 错误处理和日志记录
    """
    
    @staticmethod
    def load(file_path: Union[str, Path]) -> tuple:
        """加载配置文件
        
        Args:
            file_path: 配置文件路径
            
        Returns:
            tuple: (配置数据字典, 错误信息)
                   成功时错误信息为None，失败时配置数据为None
        """
        try:
            with open(str(file_path), 'r', encoding='utf-8') as f:
                content = f.read()
            
            if content.startswith('\ufeff'):
                content = content[1:]
            
            ext = os.path.splitext(str(file_path))[1].lower()
            
            if ext in ('.yaml', '.yml'):
                import yaml
                return yaml.safe_load(content), None
            elif ext == '.json':
                return json.loads(content), None
            else:
                return None, f"Unsupported file type: {ext}"
                
        except FileNotFoundError:
            return None, f"File not found: {file_path}"
        except Exception as e:
            return None, f"Failed to load file: {file_path}, error: {str(e)}"
    
    @staticmethod
    def load_yaml(file_path: Union[str, Path]) -> Optional[Dict]:
        """加载YAML配置文件（简化接口）
        
        Args:
            file_path: YAML文件路径
            
        Returns:
            Optional[Dict]: 配置字典，失败返回None
        """
        data, error = ConfigLoader.load(file_path)
        if error:
            raise ValueError(f"Failed to load YAML: {error}")
        return data


class ReportExporter:
    """报告导出器
    
    提供统一的报告导出功能，支持JSON和Markdown格式。
    所有Manager模块工具应使用此类进行报告输出。
    """
    
    @staticmethod
    def export_json(
        data: Dict[str, Any],
        output_path: Path,
        indent: int = 2,
        ensure_ascii: bool = False
    ) -> None:
        """导出为JSON文件
        
        Args:
            data: 要导出的数据字典
            output_path: 输出文件路径
            indent: JSON缩进空格数
            ensure_ascii: 是否转义非ASCII字符
        """
        output_path.parent.mkdir(parents=True, exist_ok=True)
        
        output_path.write_text(
            json.dumps(data, indent=indent, ensure_ascii=ensure_ascii),
            encoding='utf-8'
        )
    
    @staticmethod
    def export_markdown(
        content: str,
        output_path: Path
    ) -> None:
        """导出为Markdown文件
        
        Args:
            content: Markdown内容字符串
            output_path: 输出文件路径
        """
        output_path.parent.mkdir(parents=True, exist_ok=True)
        output_path.write_text(content, encoding='utf-8')
    
    @staticmethod
    def generate_timestamp() -> str:
        """生成ISO格式时间戳
        
        Returns:
            str: UTC时间戳字符串
        """
        return datetime.now(timezone.utc).isoformat()


class FileHelper:
    """文件操作辅助类
    
    提供常用的文件操作方法。
    """
    
    DEFAULT_IGNORE_PATTERNS = [
        "*.pyc", "__pycache__/", ".git/", ".gitignore",
        "*.log", "node_modules/", ".env*", "*.tmp",
        "*.swp", "*.bak", ".DS_Store", "Thumbs.db",
        ".baseline/"
    ]
    
    @staticmethod
    def calculate_sha256(file_path: Path) -> str:
        """计算文件的SHA256哈希值
        
        Args:
            file_path: 文件路径
            
        Returns:
            str: 十六进制哈希值，文件不存在时返回空字符串
        """
        if not file_path.exists():
            return ""
        return hashlib.sha256(file_path.read_bytes()).hexdigest()
    
    @staticmethod
    def ensure_directory(path: Path) -> None:
        """确保目录存在（不存在则创建）
        
        Args:
            path: 目录路径
        """
        path.mkdir(parents=True, exist_ok=True)
    
    @staticmethod
    def is_ignored_file(
        file_path: Path,
        ignore_patterns: Optional[List[str]] = None
    ) -> bool:
        """检查文件是否应该被忽略
        
        Args:
            file_path: 文件路径
            ignore_patterns: 忽略模式列表（默认使用常见模式）
            
        Returns:
            bool: True表示应该忽略
        """
        if ignore_patterns is None:
            ignore_patterns = FileHelper.DEFAULT_IGNORE_PATTERNS
        
        for pattern in ignore_patterns:
            if pattern.endswith('/'):
                if pattern[:-1] in file_path.parts:
                    return True
            elif pattern.startswith('*'):
                if file_path.name.endswith(pattern[1:]):
                    return True
            elif file_path.match(pattern):
                return True
        
        return False
