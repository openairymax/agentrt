#!/usr/bin/env python3
"""
AgentOS 配置模块工具函数

提供通用的工具函数，用于配置文件加载、备份等操作
"""

import json
import shutil
from pathlib import Path
from typing import Dict, Optional, Union

import yaml


def load_yaml_file(path: Path) -> Optional[Dict]:
    """
    加载 YAML 配置文件
    
    Args:
        path: YAML 文件路径
        
    Returns:
        Dict: 配置字典，如果加载失败则返回 None
    """
    if not path.exists():
        return None
    
    try:
        with open(path, 'r', encoding='utf-8') as f:
            return yaml.safe_load(f)
    except yaml.YAMLError as e:
        print(f"YAML 解析失败 {path}: {e}")
        return None


def load_json_file(path: Path) -> Optional[Dict]:
    """
    加载 JSON 配置文件
    
    Args:
        path: JSON 文件路径
        
    Returns:
        Dict: 配置字典，如果加载失败则返回 None
    """
    if not path.exists():
        return None
    
    try:
        with open(path, 'r', encoding='utf-8') as f:
            return json.load(f)
    except json.JSONDecodeError as e:
        print(f"JSON 解析失败 {path}: {e}")
        return None


def load_config_file(path: Path) -> Optional[Dict]:
    """
    加载配置文件（自动识别 YAML 或 JSON 格式）
    
    Args:
        path: 配置文件路径
        
    Returns:
        Dict: 配置字典，如果加载失败则返回 None
    """
    if not path.exists():
        return None
    
    suffix = path.suffix.lower()
    
    if suffix in ['.yaml', '.yml']:
        return load_yaml_file(path)
    elif suffix == '.json':
        return load_json_file(path)
    else:
        print(f"不支持的配置文件格式：{path}")
        return None


def backup_file(path: Path, suffix: str = '.backup') -> Optional[Path]:
    """
    备份文件
    
    Args:
        path: 要备份的文件路径
        suffix: 备份文件后缀，默认为 .backup
        
    Returns:
        Path: 备份文件路径，如果备份失败则返回 None
    """
    if not path.exists():
        return None
    
    try:
        backup_path = path.with_suffix(path.suffix + suffix)
        shutil.copy2(path, backup_path)
        return backup_path
    except Exception as e:
        print(f"备份失败 {path}: {e}")
        return None


def copy_file(source: Path, target: Path, create_dirs: bool = True) -> bool:
    """
    复制文件
    
    Args:
        source: 源文件路径
        target: 目标文件路径
        create_dirs: 是否自动创建目标目录
        
    Returns:
        bool: 复制是否成功
    """
    if not source.exists():
        print(f"源文件不存在：{source}")
        return False
    
    try:
        if create_dirs:
            target.parent.mkdir(parents=True, exist_ok=True)
        
        shutil.copy2(source, target)
        return True
    except Exception as e:
        print(f"复制失败 {source} -> {target}: {e}")
        return False


def ensure_dir(path: Path) -> bool:
    """
    确保目录存在
    
    Args:
        path: 目录路径
        
    Returns:
        bool: 是否成功确保目录存在
    """
    try:
        path.mkdir(parents=True, exist_ok=True)
        return True
    except Exception as e:
        print(f"创建目录失败 {path}: {e}")
        return False


def file_exists(path: Path) -> bool:
    """
    检查文件是否存在
    
    Args:
        path: 文件路径
        
    Returns:
        bool: 文件是否存在
    """
    return path.exists()


def dir_exists(path: Path) -> bool:
    """
    检查目录是否存在
    
    Args:
        path: 目录路径
        
    Returns:
        bool: 目录是否存在
    """
    return path.exists() and path.is_dir()
