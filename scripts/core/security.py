#!/usr/bin/env python3
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
from datetime import datetime
from enum import Enum
from pathlib import Path
from typing import Any, Callable, Dict, List, Optional, Set, Tuple


class SecurityLevel(Enum):
    """安全级别"""
    DISABLED = 0
    LOW = 1
    MEDIUM = 2
    HIGH = 3
    CRITICAL = 4
    PARANOID = 5


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
class ValidationContext:
    """验证上下文"""
    original_path: str
    resolved_path: Optional[str] = None
    allow_create: bool = False
    data: Dict[str, Any] = field(default_factory=dict)

@dataclass
class ValidationResult:
    """验证结果"""
    valid: bool
    message: str = ""
    sanitized_value: Any = None
    risk_level: SecurityLevel = SecurityLevel.DISABLED
    
    @classmethod
    def success(cls, message: str = "", sanitized_value: Any = None, 
                risk_level: SecurityLevel = SecurityLevel.LOW) -> 'ValidationResult':
        """创建成功结果"""
        return cls(valid=True, message=message, sanitized_value=sanitized_value, risk_level=risk_level)
    
    @classmethod
    def failure(cls, message: str, sanitized_value: Any = None,
                risk_level: SecurityLevel = SecurityLevel.HIGH) -> 'ValidationResult':
        """创建失败结果"""
        return cls(valid=False, message=message, sanitized_value=sanitized_value, risk_level=risk_level)


class ValidationStep(ABC):
    """验证步骤基类"""
    
    def __init__(self, next_step: Optional['ValidationStep'] = None):
        self.next_step = next_step
    
    def validate(self, path: str, context: ValidationContext) -> ValidationResult:
        """执行验证"""
        result = self._validate(path, context)
        
        if not result.valid:
            return result
        
        if self.next_step:
            return self.next_step.validate(path, context)
        
        return ValidationResult.success("All validations passed", context.data)
    
    @abstractmethod
    def _validate(self, path: str, context: ValidationContext) -> ValidationResult:
        """具体验证逻辑"""
        pass


class EmptyCheck(ValidationStep):
    """空值检查"""
    
    def _validate(self, path: str, context: ValidationContext) -> ValidationResult:
        if not path:
            return ValidationResult.failure("Path is empty", risk_level=SecurityLevel.HIGH)
        context.data['original_path'] = path
        return ValidationResult.success("Path is not empty")


class LengthCheck(ValidationStep):
    """长度检查"""
    
    def __init__(self, max_length: int, next_step: Optional[ValidationStep] = None):
        super().__init__(next_step)
        self.max_length = max_length
    
    def _validate(self, path: str, context: ValidationContext) -> ValidationResult:
        if len(path) > self.max_length:
            return ValidationResult.failure(
                f"Path exceeds maximum length {self.max_length}",
                risk_level=SecurityLevel.HIGH
            )
        return ValidationResult.success("Path length is acceptable")


class PatternCheck(ValidationStep):
    """模式检查"""
    
    def __init__(self, dangerous_patterns: List[str], next_step: Optional[ValidationStep] = None):
        super().__init__(next_step)
        self.dangerous_patterns = dangerous_patterns
    
    def _validate(self, path: str, context: ValidationContext) -> ValidationResult:
        for pattern in self.dangerous_patterns:
            if re.search(pattern, path):
                context.data['dangerous_pattern'] = pattern
                return ValidationResult.failure(
                    f"Path contains dangerous pattern: {pattern}",
                    risk_level=SecurityLevel.CRITICAL
                )
        return ValidationResult.success("No dangerous patterns found")


class PathResolutionCheck(ValidationStep):
    """路径解析检查"""
    
    def _validate(self, path: str, context: ValidationContext) -> ValidationResult:
        try:
            resolved = os.path.realpath(path)
            context.resolved_path = resolved
            context.data['resolved_path'] = resolved
            return ValidationResult.success("Path resolved successfully")
        except Exception as e:
            context.resolved_path = path
            return ValidationResult.success(f"Path resolution failed, using original: {e}")


class PrefixCheck(ValidationStep):
    """前缀检查"""
    
    def __init__(self, allowed_prefixes: List[str], allowed_paths: Optional[List[str]] = None,
                 next_step: Optional[ValidationStep] = None):
        super().__init__(next_step)
        self.allowed_prefixes = allowed_prefixes
        self.allowed_paths = allowed_paths or []
    
    def _validate(self, path: str, context: ValidationContext) -> ValidationResult:
        if not context.resolved_path:
            return ValidationResult.success("Path resolution not available, skipping prefix check")
        
        # 检查允许的前缀
        for prefix in self.allowed_prefixes:
            if context.resolved_path.startswith(prefix):
                return ValidationResult.success("Path is within allowed prefixes")
        
        # 检查允许的路径
        for allowed in self.allowed_paths:
            if context.resolved_path.startswith(allowed):
                return ValidationResult.success("Path is within allowed paths")
        
        return ValidationResult.failure(
            f"Path escapes allowed directory: {context.resolved_path}",
            risk_level=SecurityLevel.HIGH
        )


class ExistenceCheck(ValidationStep):
    """存在性检查"""
    
    def __init__(self, allow_create: bool, next_step: Optional[ValidationStep] = None):
        super().__init__(next_step)
        self.allow_create = allow_create
    
    def _validate(self, path: str, context: ValidationContext) -> ValidationResult:
        if not context.resolved_path:
            return ValidationResult.success("Path resolution not available, skipping existence check")
        
        if not os.path.exists(context.resolved_path):
            if self.allow_create:
                return ValidationResult.success("Path does not exist but creation is allowed")
            else:
                return ValidationResult.failure(
                    "Path does not exist and creation is not allowed",
                    risk_level=SecurityLevel.MEDIUM
                )
        
        # 检查文件类型
        if not os.path.isdir(context.resolved_path) and not os.path.isfile(context.resolved_path):
            return ValidationResult.failure(
                "Path exists but is neither file nor directory",
                risk_level=SecurityLevel.MEDIUM
            )
        
        return ValidationResult.success("Path exists and has valid type")


class ValidationChainBuilder:
    """验证链构建器"""
    
    def __init__(self):
        self.steps: List[ValidationStep] = []
    
    def add_empty_check(self) -> 'ValidationChainBuilder':
        self.steps.append(EmptyCheck())
        return self
    
    def add_length_check(self, max_length: int) -> 'ValidationChainBuilder':
        self.steps.append(LengthCheck(max_length))
        return self
    
    def add_pattern_check(self, dangerous_patterns: List[str]) -> 'ValidationChainBuilder':
        self.steps.append(PatternCheck(dangerous_patterns))
        return self
    
    def add_path_resolution_check(self) -> 'ValidationChainBuilder':
        self.steps.append(PathResolutionCheck())
        return self
    
    def add_prefix_check(self, allowed_prefixes: List[str], 
                         allowed_paths: Optional[List[str]] = None) -> 'ValidationChainBuilder':
        self.steps.append(PrefixCheck(allowed_prefixes, allowed_paths))
        return self
    
    def add_existence_check(self, allow_create: bool) -> 'ValidationChainBuilder':
        self.steps.append(ExistenceCheck(allow_create))
        return self
    
    def build(self) -> Optional[ValidationStep]:
        """构建验证链"""
        if not self.steps:
            return None
        
        # 从后向前构建链
        chain = None
        for step in reversed(self.steps):
            step.next_step = chain
            chain = step
        
        return chain


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
        """验证路径安全性 - 重构版本使用责任链模式"""
        # 构建验证链
        chain = ValidationChainBuilder() \
            .add_empty_check() \
            .add_length_check(self.manager.max_path_length) \
            .add_pattern_check(self.DANGEROUS_PATTERNS) \
            .add_path_resolution_check() \
            .add_prefix_check(self.ALLOWED_PATH_PREFIXES, self.manager.allowed_paths) \
            .add_existence_check(allow_create) \
            .build()
        
        if not chain:
            return ValidationResult.failure("Validation chain not configured")
        
        # 创建验证上下文
        context = ValidationContext(
            original_path=path,
            allow_create=allow_create
        )
        
        # 执行验证
        result = chain.validate(path, context)
        
        # 添加审计日志
        if not result.valid:
            if "dangerous_pattern" in context.data:
                self._audit("path_dangerous", {
                    "path": path, 
                    "pattern": context.data["dangerous_pattern"]
                })
            else:
                self._audit("path_rejected", {
                    "path": path,
                    "resolved": context.resolved_path,
                    "reason": result.message
                })
        elif context.resolved_path:
            # 验证成功，返回解析后的路径
            result.sanitized_value = context.resolved_path
        
        return result

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
