﻿#!/usr/bin/env python3
# Copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
# AgentOS 安全模块
# 输入净化、权限管理、安全审计

"""
AgentOS 安全模块

提供全面的安全保障，包括：
- 输入验证和净化
- 路径安全检查
- 命令注入防护
- 权限最小化
- 安全审计

Security Principles:
    - 输入永不相信
    - 最小权限原则
    - 防御深度
    - 安全默认值
"""

import os
import re
import shlex
import stat
from abc import ABC, abstractmethod
from dataclasses import dataclass, field
from enum import Enum
from pathlib import Path
from typing import Any, Callable, Dict, List, Optional, Set


class SecurityLevel(Enum):
    """安全级别"""
    DISABLED = 0
    LOW = 1
    MEDIUM = 2
    HIGH = 3
    PARANOID = 4


@dataclass
class SecurityConfig:
    """安全配置"""
    level: SecurityLevel = SecurityLevel.MEDIUM
    allowed_paths: List[str] = field(default_factory=list)
    blocked_patterns: List[str] = field(default_factory=list)
    max_path_length: int = 4096
    max_string_length: int = 1024 * 1024
    allow_subprocess: bool = False
    audit_enabled: bool = True
    quarantine_suspicious: bool = True


@dataclass
class ValidationResult:
    """验证结果"""
    valid: bool
    message: str = ""
    sanitized_value: Any = None
    risk_level: SecurityLevel = SecurityLevel.DISABLED


class SecurityManager:
    """安全管理器"""

    ALLOWED_PATH_PREFIXES = [
        "/usr/local",
        "/opt",
        "/tmp",
        "/var/lib/agentos",
        "/etc/agentos",
    ]

    DANGEROUS_PATTERNS = [
        r"\.\.",           # 路径遍历
        r"~",              # 家目录展开
        r"\$",             # 变量展开
        r"`",              # 命令替换
        r"\|",             # 管道
        r";",              # 命令分隔
        r"&&",             # 条件执行
        r"\|\|",           # 条件执行
        r">",              # 输出重定向
        r"<",              # 输入重定向
        r"\n",             # 换行符
        r"\r",             # 回车符
        r"\x00",           # 空字节
    ]

    def __init__(self, manager: SecurityConfig = None):
        self.manager = manager or SecurityConfig()
        self._blocked_paths: Set[str] = set()
        self._audit_log: List[Dict[str, Any]] = []

    def validate_path(self, path: str, allow_create: bool = False) -> ValidationResult:
        """验证路径安全性"""
        if not path:
            return ValidationResult(False, "Path is empty", risk_level=SecurityLevel.HIGH)

        if len(path) > self.manager.max_path_length:
            return ValidationResult(
                False,
                f"Path exceeds maximum length {self.manager.max_path_length}",
                risk_level=SecurityLevel.HIGH
            )

        for pattern in self.DANGEROUS_PATTERNS:
            if re.search(pattern, path):
                self._audit("path_dangerous", {"path": path, "pattern": pattern})
                return ValidationResult(
                    False,
                    f"Path contains dangerous pattern: {pattern}",
                    path,
                    risk_level=SecurityLevel.CRITICAL
                )

        try:
            resolved = os.path.realpath(path)
        except Exception:
            resolved = path

        is_safe = False
        for prefix in self.ALLOWED_PATH_PREFIXES:
            if resolved.startswith(prefix):
                is_safe = True
                break

        if not is_safe and self.manager.allowed_paths:
            for allowed in self.manager.allowed_paths:
                if resolved.startswith(allowed):
                    is_safe = True
                    break

        if not is_safe:
            self._audit("path_rejected", {"path": path, "resolved": resolved})
            return ValidationResult(
                False,
                f"Path escapes allowed directory: {path}",
                risk_level=SecurityLevel.HIGH
            )

        if os.path.exists(resolved):
            if not os.path.is_file(resolved) and not os.path.is_dir(resolved):
                return ValidationResult(
                    False,
                    "Path exists but is neither file nor directory",
                    risk_level=SecurityLevel.MEDIUM
                )

        return ValidationResult(True, "Path is safe", resolved, SecurityLevel.LOW)

    def validate_command(self, command: str) -> ValidationResult:
        """验证命令安全性"""
        if not command:
            return ValidationResult(False, "Command is empty", risk_level=SecurityLevel.HIGH)

        if len(command) > self.manager.max_string_length:
            return ValidationResult(
                False,
                f"Command exceeds maximum length",
                risk_level=SecurityLevel.HIGH
            )

        if self.manager.level == SecurityLevel.PARANOID:
            dangerous_chars = ["'", '"', '$', '`', '|', ';', '&', '>', '<']
            for char in dangerous_chars:
                if char in command:
                    return ValidationResult(
                        False,
                        f"Command contains potentially dangerous character: {char}",
                        risk_level=SecurityLevel.HIGH
                    )

        return ValidationResult(True, "Command appears safe", SecurityLevel.LOW)

    def sanitize_string(self, value: str, max_length: int = None) -> str:
        """净化字符串"""
        if not isinstance(value, str):
            value = str(value)

        max_len = max_length or self.manager.max_string_length
        if len(value) > max_len:
            value = value[:max_len]

        value = value.replace('\x00', '')
        value = value.replace('\r\n', '\n')
        value = value.replace('\r', '\n')

        return value

    def sanitize_for_shell(self, value: str) -> str:
        """为 shell 净化字符串"""
        return shlex.quote(self.sanitize_string(value))

    def validate_environment(self, env: Dict[str, str]) -> ValidationResult:
        """验证环境变量"""
        dangerous_vars = ["PATH", "LD_PRELOAD", "LD_LIBRARY_PATH", "DYLD_INSERT_LIBRARIES"]
        warnings = []

        for key in dangerous_vars:
            if key in env:
                warnings.append(f"Environment variable {key} is set")

        if warnings and self.manager.level >= SecurityLevel.HIGH:
            return ValidationResult(
                False,
                "; ".join(warnings),
                risk_level=SecurityLevel.HIGH
            )

        return ValidationResult(True, "Environment is acceptable", SecurityLevel.LOW)

    def check_file_permissions(self, path: str) -> ValidationResult:
        """检查文件权限"""
        if not os.path.exists(path):
            return ValidationResult(False, "File does not exist", risk_level=SecurityLevel.LOW)

        try:
            st = os.stat(path)
            mode = st.st_mode

            if mode & stat.S_IWOTH:
                self._audit("permission_warning", {"path": path, "issue": "world_writable"})
                return ValidationResult(
                    True,
                    "File is world-writable (security risk)",
                    risk_level=SecurityLevel.MEDIUM
                )

            if mode & stat.S_IXOTH:
                return ValidationResult(
                    True,
                    "File is world-executable",
                    risk_level=SecurityLevel.LOW
                )

            return ValidationResult(True, "Permissions are acceptable", SecurityLevel.LOW)

        except Exception as e:
            return ValidationResult(False, f"Failed to check permissions: {e}")

    def _audit(self, event: str, data: Dict[str, Any]) -> None:
        """审计日志"""
        if not self.manager.audit_enabled:
            return

        self._audit_log.append({
            "event": event,
            "data": data,
            "timestamp": str(datetime.now().isoformat())
        })

    def get_audit_log(self) -> List[Dict[str, Any]]:
        """获取审计日志"""
        return self._audit_log.copy()


class InputValidator:
    """输入验证器"""

    EMAIL_PATTERN = re.compile(r'^[a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\.[a-zA-Z]{2,}$')
    URL_PATTERN = re.compile(r'^https?://[^\s]+$')
    VERSION_PATTERN = re.compile(r'^\d+\.\d+\.\d+(-[a-zA-Z0-9]+)?$')

    @staticmethod
    def validate_string(value: Any, min_length: int = 0, max_length: int = 1000,
                       pattern: str = None) -> ValidationResult:
        """验证字符串"""
        if not isinstance(value, str):
            return ValidationResult(False, f"Expected string, got {type(value).__name__}")

        if len(value) < min_length:
            return ValidationResult(False, f"String too short (min: {min_length})")

        if len(value) > max_length:
            return ValidationResult(False, f"String too long (max: {max_length})")

        if pattern and not re.match(pattern, value):
            return ValidationResult(False, f"String does not match pattern")

        return ValidationResult(True, "Valid string", value)

    @staticmethod
    def validate_email(value: str) -> ValidationResult:
        """验证邮箱"""
        if InputValidator.EMAIL_PATTERN.match(value):
            return ValidationResult(True, "Valid email", SecurityLevel.LOW)
        return ValidationResult(False, "Invalid email format", SecurityLevel.MEDIUM)

    @staticmethod
    def validate_url(value: str) -> ValidationResult:
        """验证 URL"""
        if InputValidator.URL_PATTERN.match(value):
            return ValidationResult(True, "Valid URL", SecurityLevel.LOW)
        return ValidationResult(False, "Invalid URL format", SecurityLevel.MEDIUM)

    @staticmethod
    def validate_version(value: str) -> ValidationResult:
        """验证版本号"""
        if InputValidator.VERSION_PATTERN.match(value):
            return ValidationResult(True, "Valid version", SecurityLevel.LOW)
        return ValidationResult(False, "Invalid version format (expected x.y.z)", SecurityLevel.LOW)

    @staticmethod
    def validate_port(value: Any) -> ValidationResult:
        """验证端口号"""
        try:
            port = int(value)
            if 1 <= port <= 65535:
                return ValidationResult(True, "Valid port", port, SecurityLevel.LOW)
            return ValidationResult(False, f"Port {port} out of range (1-65535)", SecurityLevel.HIGH)
        except (ValueError, TypeError):
            return ValidationResult(False, "Port must be an integer", SecurityLevel.HIGH)

    @staticmethod
    def validate_ip(value: str) -> ValidationResult:
        """验证 IP 地址"""
        parts = value.split('.')
        if len(parts) != 4:
            return ValidationResult(False, "Invalid IP address format", SecurityLevel.MEDIUM)

        try:
            if all(0 <= int(part) <= 255 for part in parts):
                return ValidationResult(True, "Valid IP address", SecurityLevel.LOW)
            return ValidationResult(False, "Invalid IP address value", SecurityLevel.MEDIUM)
        except ValueError:
            return ValidationResult(False, "Invalid IP address", SecurityLevel.MEDIUM)


_global_security_manager: Optional[SecurityManager] = None


def get_security_manager() -> SecurityManager:
    """获取全局安全管理器"""
    global _global_security_manager
    if _global_security_manager is None:
        _global_security_manager = SecurityManager()
    return _global_security_manager
