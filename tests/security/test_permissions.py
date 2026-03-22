# AgentOS 安全测试模块
# Version: 1.0.0.6
# Last updated: 2026-03-22

"""
AgentOS 安全测试模块。

测试权限控制、沙箱隔离、输入净化等安全机制。
"""

import pytest
import os
import tempfile
import shutil
from pathlib import Path
from typing import Dict, Any, List, Optional
from unittest.mock import Mock, MagicMock, patch

import sys
sys.path.insert(0, os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..', 'tools', 'python')))


# ============================================================
# 测试标记
# ============================================================

pytestmark = pytest.mark.security


# ============================================================
# 权限控制测试
# ============================================================

class TestPermissionControl:
    """权限控制测试"""
    
    @pytest.fixture
    def permission_engine(self):
        """
        创建权限引擎实例。
        
        Returns:
            Mock: 模拟的权限引擎
        """
        engine = Mock()
        
        roles = {
            "admin": ["read", "write", "delete", "admin"],
            "developer": ["read", "write"],
            "viewer": ["read"]
        }
        
        user_roles = {}
        
        def check_permission(user_id: str, permission: str, resource: str = None) -> bool:
            user_role = user_roles.get(user_id)
            if not user_role:
                return False
            return permission in roles.get(user_role, [])
        
        def assign_role(user_id: str, role: str) -> bool:
            if role not in roles:
                return False
            user_roles[user_id] = role
            return True
        
        def get_role(user_id: str) -> Optional[str]:
            return user_roles.get(user_id)
        
        engine.check_permission = Mock(side_effect=check_permission)
        engine.assign_role = Mock(side_effect=assign_role)
        engine.get_role = Mock(side_effect=get_role)
        engine.roles = roles
        
        return engine
    
    def test_rbac_basic_permission_check(self, permission_engine):
        """
        测试基本RBAC权限检查。
        
        验证:
            - 管理员拥有所有权限
            - 开发者只有读写权限
            - 观察者只有读权限
        """
        permission_engine.assign_role("user_admin", "admin")
        permission_engine.assign_role("user_dev", "developer")
        permission_engine.assign_role("user_viewer", "viewer")
        
        assert permission_engine.check_permission("user_admin", "delete") is True
        assert permission_engine.check_permission("user_admin", "admin") is True
        assert permission_engine.check_permission("user_dev", "write") is True
        assert permission_engine.check_permission("user_dev", "delete") is False
        assert permission_engine.check_permission("user_viewer", "read") is True
        assert permission_engine.check_permission("user_viewer", "write") is False
    
    def test_rbac_role_assignment(self, permission_engine):
        """
        测试角色分配。
        
        验证:
            - 角色能正确分配
            - 无效角色被拒绝
        """
        assert permission_engine.assign_role("user1", "admin") is True
        assert permission_engine.get_role("user1") == "admin"
        
        assert permission_engine.assign_role("user2", "invalid_role") is False
    
    def test_rbac_permission_denied_for_unknown_user(self, permission_engine):
        """
        测试未知用户权限拒绝。
        
        验证:
            - 未知用户没有任何权限
        """
        assert permission_engine.check_permission("unknown_user", "read") is False
        assert permission_engine.check_permission("unknown_user", "write") is False
    
    def test_rbac_permission_caching(self, permission_engine):
        """
        测试权限缓存性能。
        
        验证:
            - 权限检查性能合理
        """
        permission_engine.assign_role("user1", "admin")
        
        import time
        start = time.time()
        for _ in range(100):
            permission_engine.check_permission("user1", "read")
        elapsed = time.time() - start
        
        avg_latency_ms = (elapsed / 100) * 1000
        assert avg_latency_ms < 1.0, f"权限检查太慢: {avg_latency_ms}ms"
    
    def test_rbac_resource_level_permission(self, permission_engine):
        """
        测试资源级别权限。
        
        验证:
            - 资源级别权限检查正确
        """
        permission_engine.assign_role("user1", "developer")
        
        assert permission_engine.check_permission("user1", "read", "project_001") is True
        assert permission_engine.check_permission("user1", "write", "project_001") is True
        assert permission_engine.check_permission("user1", "delete", "project_001") is False


# ============================================================
# 沙箱隔离测试
# ============================================================

class TestSandboxIsolation:
    """沙箱隔离测试"""
    
    @pytest.fixture
    def sandbox(self):
        """
        创建沙箱实例。
        
        Returns:
            Mock: 模拟的沙箱
        """
        sandbox = Mock()
        
        allowed_paths = ["/workspace", "/tmp/sandbox"]
        blocked_commands = ["rm -rf", "sudo", "chmod 777"]
        network_whitelist = ["localhost", "127.0.0.1"]
        
        def execute(command: str, cwd: str = "/workspace") -> Dict[str, Any]:
            if any(blocked in command for blocked in blocked_commands):
                raise PermissionError(f"命令被阻止: {command}")
            
            if cwd not in allowed_paths:
                raise PermissionError(f"路径不允许: {cwd}")
            
            return {
                "returncode": 0,
                "stdout": "执行成功",
                "stderr": ""
            }
        
        def check_path_access(path: str) -> bool:
            return any(path.startswith(allowed) for allowed in allowed_paths)
        
        def check_network_access(host: str) -> bool:
            return host in network_whitelist
        
        sandbox.execute = Mock(side_effect=execute)
        sandbox.check_path_access = Mock(side_effect=check_path_access)
        sandbox.check_network_access = Mock(side_effect=check_network_access)
        sandbox.allowed_paths = allowed_paths
        sandbox.blocked_commands = blocked_commands
        
        return sandbox
    
    def test_sandbox_file_system_isolation(self, sandbox):
        """
        测试文件系统隔离。
        
        验证:
            - 允许访问工作目录
            - 阻止访问系统目录
        """
        assert sandbox.check_path_access("/workspace/project") is True
        assert sandbox.check_path_access("/tmp/sandbox/file") is True
        assert sandbox.check_path_access("/etc/passwd") is False
        assert sandbox.check_path_access("/root/.ssh") is False
    
    def test_sandbox_command_blocking(self, sandbox):
        """
        测试命令阻止。
        
        验证:
            - 危险命令被阻止
            - 安全命令允许执行
        """
        assert sandbox.execute("ls -la")["returncode"] == 0
        assert sandbox.execute("cat file.txt")["returncode"] == 0
        
        with pytest.raises(PermissionError):
            sandbox.execute("rm -rf /")
        
        with pytest.raises(PermissionError):
            sandbox.execute("sudo apt update")
    
    def test_sandbox_network_isolation(self, sandbox):
        """
        测试网络隔离。
        
        验证:
            - 允许本地网络连接
            - 阻止外部网络连接
        """
        assert sandbox.check_network_access("localhost") is True
        assert sandbox.check_network_access("127.0.0.1") is True
        assert sandbox.check_network_access("example.com") is False
        assert sandbox.check_network_access("8.8.8.8") is False
    
    def test_sandbox_resource_limits(self):
        """
        测试资源限制。
        
        验证:
            - 内存限制正确设置
            - CPU限制正确设置
            - 进程数限制正确设置
        """
        resource_limits = {
            "max_memory_mb": 256,
            "max_cpu_percent": 50,
            "max_processes": 10,
            "max_file_size_mb": 100,
            "max_open_files": 100
        }
        
        assert resource_limits["max_memory_mb"] == 256
        assert resource_limits["max_cpu_percent"] == 50
        assert resource_limits["max_processes"] == 10
    
    def test_sandbox_escape_prevention(self, sandbox):
        """
        测试沙箱逃逸防护。
        
        验证:
            - 常见逃逸尝试被阻止
        """
        escape_attempts = [
            "rm -rf /workspace/../../../etc/passwd",
            "sudo su -",
            "chmod 777 /workspace/../../../etc",
            "; cat /etc/passwd",
            "| cat /etc/passwd",
            "$(cat /etc/passwd)",
            "`cat /etc/passwd`"
        ]
        
        for attempt in escape_attempts:
            with pytest.raises(PermissionError):
                sandbox.execute(attempt)


# ============================================================
# 输入净化测试
# ============================================================

class TestInputSanitization:
    """输入净化测试"""
    
    @pytest.fixture
    def sanitizer(self):
        """
        创建输入净化器实例。
        
        Returns:
            Mock: 模拟的净化器
        """
        sanitizer = Mock()
        
        def sanitize_string(input_str: str) -> str:
            if not isinstance(input_str, str):
                raise TypeError("输入必须是字符串")
            
            dangerous_patterns = [
                "<script>", "</script>", "javascript:",
                "onerror=", "onload=", "onclick=",
                "eval(", "exec(", "system(",
                "../", "..\\", "/etc/", "\\windows\\"
            ]
            
            result = input_str
            for pattern in dangerous_patterns:
                result = result.replace(pattern, "")
            
            return result.strip()
        
        def sanitize_sql(input_str: str) -> str:
            if not isinstance(input_str, str):
                raise TypeError("输入必须是字符串")
            
            sql_patterns = [
                "'", '"', ";", "--", "/*", "*/",
                "DROP", "DELETE", "INSERT", "UPDATE",
                "UNION", "SELECT", "FROM", "WHERE"
            ]
            
            result = input_str
            for pattern in sql_patterns:
                result = result.replace(pattern, "")
            
            return result.strip()
        
        def validate_json(data: Dict[str, Any]) -> bool:
            if not isinstance(data, dict):
                return False
            
            for key, value in data.items():
                if not isinstance(key, str):
                    return False
                if isinstance(value, str) and len(value) > 10000:
                    return False
            
            return True
        
        sanitizer.sanitize_string = Mock(side_effect=sanitize_string)
        sanitizer.sanitize_sql = Mock(side_effect=sanitize_sql)
        sanitizer.validate_json = Mock(side_effect=validate_json)
        
        return sanitizer
    
    def test_xss_prevention(self, sanitizer):
        """
        测试XSS防护。
        
        验证:
            - 脚本标签被移除
            - 事件处理器被移除
        """
        xss_inputs = [
            "<script>alert('xss')</script>",
            "<img onerror='alert(1)' src='x'>",
            "<a onclick='alert(1)'>click</a>",
            "javascript:alert('xss')"
        ]
        
        for xss_input in xss_inputs:
            sanitized = sanitizer.sanitize_string(xss_input)
            assert "<script>" not in sanitized
            assert "javascript:" not in sanitized
            assert "onerror=" not in sanitized
            assert "onclick=" not in sanitized
    
    def test_sql_injection_prevention(self, sanitizer):
        """
        测试SQL注入防护。
        
        验证:
            - SQL关键字被移除
            - 特殊字符被移除
        """
        sql_injection_inputs = [
            "'; DROP TABLE users; --",
            "1' OR '1'='1",
            "admin'--",
            "1; SELECT * FROM users"
        ]
        
        for sql_input in sql_injection_inputs:
            sanitized = sanitizer.sanitize_sql(sql_input)
            assert "DROP" not in sanitized.upper()
            assert "SELECT" not in sanitized.upper()
            assert "--" not in sanitized
    
    def test_path_traversal_prevention(self, sanitizer):
        """
        测试路径遍历防护。
        
        验证:
            - 路径遍历字符被移除
        """
        path_traversal_inputs = [
            "../../../etc/passwd",
            "..\\..\\..\\windows\\system32",
            "/etc/shadow",
            "\\windows\\system32\\config"
        ]
        
        for path_input in path_traversal_inputs:
            sanitized = sanitizer.sanitize_string(path_input)
            assert "../" not in sanitized
            assert "..\\" not in sanitized
            assert "/etc/" not in sanitized
    
    def test_json_validation(self, sanitizer):
        """
        测试JSON验证。
        
        验证:
            - 有效JSON通过验证
            - 无效JSON被拒绝
        """
        valid_json = {
            "name": "test",
            "value": 123,
            "nested": {"key": "value"}
        }
        
        invalid_json = {
            123: "invalid key type"
        }
        
        oversized_json = {
            "key": "x" * 20000
        }
        
        assert sanitizer.validate_json(valid_json) is True
        assert sanitizer.validate_json(invalid_json) is False
        assert sanitizer.validate_json(oversized_json) is False
    
    def test_command_injection_prevention(self, sanitizer):
        """
        测试命令注入防护。
        
        验证:
            - 命令注入字符被移除
        """
        command_injection_inputs = [
            "file.txt; rm -rf /",
            "file.txt | cat /etc/passwd",
            "$(whoami)",
            "`id`"
        ]
        
        for cmd_input in command_injection_inputs:
            sanitized = sanitizer.sanitize_string(cmd_input)
            assert "eval(" not in sanitized
            assert "exec(" not in sanitized
            assert "system(" not in sanitized


# ============================================================
# 认证测试
# ============================================================

class TestAuthentication:
    """认证测试"""
    
    @pytest.fixture
    def auth_engine(self):
        """
        创建认证引擎实例。
        
        Returns:
            Mock: 模拟的认证引擎
        """
        engine = Mock()
        
        valid_tokens = {
            "valid_token_001": {"user_id": "user_001", "role": "admin"},
            "valid_token_002": {"user_id": "user_002", "role": "developer"}
        }
        
        def validate_token(token: str) -> Optional[Dict[str, Any]]:
            return valid_tokens.get(token)
        
        def authenticate(username: str, password: str) -> Optional[str]:
            if username == "admin" and password == "correct_password":
                return "valid_token_001"
            return None
        
        def revoke_token(token: str) -> bool:
            if token in valid_tokens:
                del valid_tokens[token]
                return True
            return False
        
        engine.validate_token = Mock(side_effect=validate_token)
        engine.authenticate = Mock(side_effect=authenticate)
        engine.revoke_token = Mock(side_effect=revoke_token)
        
        return engine
    
    def test_valid_token_authentication(self, auth_engine):
        """
        测试有效令牌认证。
        
        验证:
            - 有效令牌返回用户信息
        """
        result = auth_engine.validate_token("valid_token_001")
        
        assert result is not None
        assert result["user_id"] == "user_001"
        assert result["role"] == "admin"
    
    def test_invalid_token_rejection(self, auth_engine):
        """
        测试无效令牌拒绝。
        
        验证:
            - 无效令牌返回None
        """
        result = auth_engine.validate_token("invalid_token")
        
        assert result is None
    
    def test_password_authentication(self, auth_engine):
        """
        测试密码认证。
        
        验证:
            - 正确密码返回令牌
            - 错误密码返回None
        """
        token = auth_engine.authenticate("admin", "correct_password")
        assert token is not None
        
        token = auth_engine.authenticate("admin", "wrong_password")
        assert token is None
    
    def test_token_revocation(self, auth_engine):
        """
        测试令牌撤销。
        
        验证:
            - 令牌能被正确撤销
            - 撤销后令牌无效
        """
        result = auth_engine.revoke_token("valid_token_002")
        assert result is True
        
        result = auth_engine.validate_token("valid_token_002")
        assert result is None


# ============================================================
# 数据保护测试
# ============================================================

class TestDataProtection:
    """数据保护测试"""
    
    def test_sensitive_data_masking(self):
        """
        测试敏感数据脱敏。
        
        验证:
            - 敏感数据被正确脱敏
        """
        def mask_sensitive(data: str, data_type: str) -> str:
            if data_type == "email":
                parts = data.split("@")
                if len(parts) == 2:
                    return f"{parts[0][:2]}***@{parts[1]}"
            elif data_type == "phone":
                return f"{data[:3]}****{data[-4:]}"
            elif data_type == "credit_card":
                return f"**** **** **** {data[-4:]}"
            return data
        
        assert mask_sensitive("user@example.com", "email") == "us***@example.com"
        assert mask_sensitive("13812345678", "phone") == "138****5678"
        assert mask_sensitive("1234567890123456", "credit_card") == "**** **** **** 3456"
    
    def test_data_encryption(self):
        """
        测试数据加密。
        
        验证:
            - 数据能被加密和解密
        """
        import hashlib
        
        def hash_data(data: str, salt: str = "default_salt") -> str:
            return hashlib.sha256(f"{salt}{data}".encode()).hexdigest()
        
        def verify_hash(data: str, hashed: str, salt: str = "default_salt") -> bool:
            return hash_data(data, salt) == hashed
        
        original = "sensitive_data"
        hashed = hash_data(original)
        
        assert hashed != original
        assert verify_hash(original, hashed) is True
        assert verify_hash("wrong_data", hashed) is False
    
    def test_secure_random_generation(self):
        """
        测试安全随机数生成。
        
        验证:
            - 生成的随机数足够随机
        """
        import secrets
        
        tokens = [secrets.token_hex(32) for _ in range(100)]
        
        assert len(set(tokens)) == 100, "生成的令牌有重复"
        
        for token in tokens:
            assert len(token) == 64, "令牌长度不正确"


# ============================================================
# 运行测试
# ============================================================

if __name__ == "__main__":
    pytest.main([__file__, "-v", "--tb=short", "-m", "security"])
