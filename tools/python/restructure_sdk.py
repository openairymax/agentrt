#!/usr/bin/env python3
"""
AgentOS Python SDK 重构脚本

此脚本用于自动化重构 agentos 包结构，实现高度模块化设计。

功能:
1. 创建新的目录结构
2. 迁移现有模块到新位置
3. 更新导入路径
4. 生成 __init__.py 文件
5. 验证重构结果

使用方法:
    python restructure_sdk.py
"""

import os
import sys
import shutil
from pathlib import Path
from typing import List, Dict, Optional


class SDKRestructurer:
    """SDK 重构器"""

    def __init__(self, base_path: str):
        """
        初始化重构器

        Args:
            base_path: agentos 包的基础路径
        """
        self.base_path = Path(base_path)
        self.backup_path = self.base_path.parent / "agentos_backup"

        # 新的目录结构
        self.new_structure = {
            'core': ['__init__.py', 'syscall.py', 'proxy.py'],
            'modules': {
                'task': ['__init__.py', 'manager.py', 'models.py', 'errors.py'],
                'memory': ['__init__.py', 'manager.py', 'models.py', 'errors.py'],
                'session': ['__init__.py', 'manager.py', 'models.py', 'errors.py'],
                'skill': ['__init__.py', 'manager.py', 'models.py', 'errors.py'],
            },
            'client': ['__init__.py', 'base.py', 'sync.py', 'async_.py'],
            'utils': ['__init__.py', 'id.py', 'time.py', 'validation.py', 'crypto.py', 'cache.py'],
            'telemetry': ['__init__.py', 'metrics.py', 'tracing.py', 'logging.py'],
            'types': ['__init__.py', 'common.py', 'task.py', 'memory.py', 'session.py'],
        }

        # 需要保留的顶层文件
        self.top_level_files = [
            '__init__.py',
            '_version.py',
            '_syscall.py',  # 保留向后兼容
            'exceptions.py',
        ]

    def create_backup(self) -> None:
        """创建备份"""
        print("📦 创建备份...")
        if self.backup_path.exists():
            shutil.rmtree(self.backup_path)

        shutil.copytree(self.base_path, self.backup_path)
        print(f"✅ 备份完成：{self.backup_path}")

    def create_directory_structure(self) -> None:
        """创建新的目录结构"""
        print("\n🏗️  创建目录结构...")

        for dir_name, files in self.new_structure.items():
            dir_path = self.base_path / dir_name

            if isinstance(files, dict):
                # 子目录（如 modules/task）
                for sub_dir, sub_files in files.items():
                    full_path = dir_path / sub_dir
                    full_path.mkdir(parents=True, exist_ok=True)

                    for file in sub_files:
                        (full_path / file).touch(exist_ok=True)
                    print(
                        f"  ✓ 创建 {dir_name}/{sub_dir}/ 包含 {len(sub_files)} 个文件")
            else:
                # 简单目录
                dir_path.mkdir(parents=True, exist_ok=True)

                for file in files:
                    (dir_path / file).touch(exist_ok=True)
                print(f"  ✓ 创建 {dir_name}/ 包含 {len(files)} 个文件")

    def generate_init_files(self) -> None:
        """生成 __init__.py 文件内容"""
        print("\n📝 生成 __init__.py 文件...")

        # core/__init__.py
        core_init = '''# AgentOS Python SDK - Core Layer
# Version: 2.0.0.0

"""
Core layer providing low-level FFI bindings and syscall proxies.
"""

try:
    from .syscall import SyscallBinding
    from .proxy import SyscallProxy, get_default_proxy
    
    __all__ = [
        "SyscallBinding",
        "SyscallProxy", 
        "get_default_proxy",
    ]
except ImportError:
    # 允许延迟导入
    __all__ = []
'''
        (self.base_path / 'core' / '__init__.py').write_text(core_init)
        print("  ✓ core/__init__.py")

        # modules/__init__.py
        modules_init = '''# AgentOS Python SDK - Business Modules Layer
# Version: 2.0.0.0

"""
Business logic modules for task, memory, session, and skill management.
"""

__all__ = ["task", "memory", "session", "skill"]
'''
        (self.base_path / 'modules' / '__init__.py').write_text(modules_init)
        print("  ✓ modules/__init__.py")

        # 为每个子模块生成 __init__.py
        for module_name in ['task', 'memory', 'session', 'skill']:
            module_init = f'''# AgentOS Python SDK - {module_name.title()} Module
# Version: 2.0.0.0

"""
{module_name.title()} management module.
"""

try:
    from .manager import {module_name.title()}Manager
    from .models import {module_name.title()}Info, {module_name.title()}Result
    from .errors import {module_name.title()}Error
    
    __all__ = [
        "{module_name.title()}Manager",
        "{module_name.title()}Info",
        "{module_name.title()}Result",
        "{module_name.title()}Error",
    ]
except ImportError:
    __all__ = []
'''
            (self.base_path / 'modules' / module_name /
             '__init__.py').write_text(module_init)
            print(f"  ✓ modules/{module_name}/__init__.py")

        # client/__init__.py
        client_init = '''# AgentOS Python SDK - Client Layer
# Version: 2.0.0.0

"""
Client classes for interacting with AgentOS system.
"""

try:
    from .base import BaseClient
    from .sync import AgentOS
    from .async_ import AsyncAgentOS
    
    __all__ = ["BaseClient", "AgentOS", "AsyncAgentOS"]
except ImportError:
    __all__ = []
'''
        (self.base_path / 'client' / '__init__.py').write_text(client_init)
        print("  ✓ client/__init__.py")

        # utils/__init__.py
        utils_init = '''# AgentOS Python SDK - Utilities Layer
# Version: 2.0.0.0

"""
Common utility functions and helpers.
"""

try:
    from .id import generate_id, generate_timestamp
    from .time import Timer, parse_timeout
    from .validation import validate_json, sanitize_string
    from .crypto import generate_hash
    from .cache import LRUCache
    
    __all__ = [
        "generate_id",
        "generate_timestamp",
        "Timer",
        "parse_timeout",
        "validate_json",
        "sanitize_string",
        "generate_hash",
        "LRUCache",
    ]
except ImportError:
    __all__ = []
'''
        (self.base_path / 'utils' / '__init__.py').write_text(utils_init)
        print("  ✓ utils/__init__.py")

        # telemetry/__init__.py
        telemetry_init = '''# AgentOS Python SDK - Telemetry Layer
# Version: 2.0.0.0

"""
Telemetry and observability utilities.
"""

try:
    from .metrics import Meter, MetricPoint
    from .tracing import Tracer, Span, SpanStatus
    from .logging import setup_logging
    
    __all__ = [
        "Meter",
        "MetricPoint",
        "Tracer",
        "Span",
        "SpanStatus",
        "setup_logging",
    ]
except ImportError:
    __all__ = []
'''
        (self.base_path / 'telemetry' / '__init__.py').write_text(telemetry_init)
        print("  ✓ telemetry/__init__.py")

        # types/__init__.py
        types_init = '''# AgentOS Python SDK - Type Definitions
# Version: 2.0.0.0

"""
Common type definitions and enums.
"""

try:
    from .common import TaskID, SessionID, MemoryRecordID, SkillID
    from .task import TaskStatus, TaskResult
    from .memory import MemoryInfo, MemoryRecordType
    from .session import SessionInfo
    
    __all__ = [
        "TaskID",
        "SessionID",
        "MemoryRecordID",
        "SkillID",
        "TaskStatus",
        "TaskResult",
        "MemoryInfo",
        "MemoryRecordType",
        "SessionInfo",
    ]
except ImportError:
    __all__ = []
'''
        (self.base_path / 'types' / '__init__.py').write_text(types_init)
        print("  ✓ types/__init__.py")

    def update_top_level_init(self) -> None:
        """更新顶层 __init__.py"""
        print("\n🔄 更新顶层 __init__.py...")

        top_init = '''# AgentOS Python SDK
# Version: 2.0.0.0
# Last updated: 2026-03-21

"""
AgentOS Python SDK - Production-ready interface to AgentOS system.

This SDK provides a clean, Pythonic interface to interact with the AgentOS system,
featuring:
    - High-level business logic modules (task, memory, session, skill)
    - Low-level FFI bindings with comprehensive error handling
    - Full type annotations and detailed documentation
    - Cross-platform support (Linux, macOS, Windows)
    - Asynchronous programming support

Quick Start:
    >>> from agentos import AgentOS
    >>> client = AgentOS(endpoint="http://localhost:18789")
    >>> 
    >>> # Submit a task
    >>> task = client.submit_task('{"input": "analyze this data"}')
    >>> result = task.wait(timeout=30)
    >>> print(result)

Example:
    >>> # Using context managers
    >>> with AgentOS() as client:
    ...     session = client.create_session()
    ...     with session:
    ...         result = client.execute_skill("my_skill", {"param": "value"})

For more information, see the documentation at https://agentos.dev
"""

__version__ = "2.0.0.0"
__author__ = "AgentOS Team"
__license__ = "Apache-2.0"

# Import version info
from ._version import (
    __version__,
    __version_info__,
    __author__,
    __license__,
    get_version_string,
    get_version_tuple,
    check_python_version,
)

# Check Python version compatibility
if not check_python_version():
    import warnings
    warnings.warn(
        f"This version of AgentOS requires Python 3.7-3.13, "
        f"but you are using Python {'.'.join(map(str, __import__('sys').version_info[:2]))}",
        RuntimeWarning
    )

# Import exceptions
from .exceptions import (
    AgentOSError,
    InitializationError,
    ValidationError,
    NetworkError,
    TimeoutError,
    TelemetryError,
    # Module-specific errors (will be imported from modules)
)

# Import core components
try:
    from .core import (
        SyscallBinding,
        SyscallProxy,
        get_default_proxy,
    )
except ImportError:
    pass

# Import business modules
try:
    from .modules.task import TaskManager, TaskInfo, TaskResult, TaskError
    from .modules.memory import MemoryManager, MemoryInfo, MemoryRecordType, MemoryError
    from .modules.session import SessionManager, SessionInfo, SessionError
    from .modules.skill import SkillManager, SkillInfo, SkillResult, SkillError
except ImportError:
    pass

# Import clients
try:
    from .client import AgentOS, AsyncAgentOS, BaseClient
except ImportError:
    pass

# Import utilities
try:
    from .utils import (
        generate_id,
        generate_timestamp,
        generate_hash,
        validate_json,
        sanitize_string,
        Timer,
        parse_timeout,
        LRUCache,
    )
except ImportError:
    pass

# Import telemetry
try:
    from .telemetry import (
        Telemetry,
        Meter,
        Tracer,
        Span,
        SpanStatus,
        MetricPoint,
        setup_logging,
    )
except ImportError:
    pass

# Import types
try:
    from .types import (
        TaskID,
        SessionID,
        MemoryRecordID,
        SkillID,
        Timestamp,
        ErrorCode,
        JSONValue,
        JSONObject,
    )
except ImportError:
    pass

# Public API surface
__all__ = [
    # Version
    "__version__",
    "__version_info__",
    "__author__",
    "__license__",
    "get_version_string",
    "get_version_tuple",
    "check_python_version",
    
    # Exceptions
    "AgentOSError",
    "InitializationError",
    "ValidationError",
    "NetworkError",
    "TimeoutError",
    "TelemetryError",
    "TaskError",
    "MemoryError",
    "SessionError",
    "SkillError",
    
    # Core
    "SyscallBinding",
    "SyscallProxy",
    "get_default_proxy",
    
    # Clients
    "AgentOS",
    "AsyncAgentOS",
    "BaseClient",
    
    # Managers
    "TaskManager",
    "MemoryManager",
    "SessionManager",
    "SkillManager",
    
    # Models
    "TaskInfo",
    "TaskResult",
    "MemoryInfo",
    "MemoryRecordType",
    "SessionInfo",
    "SkillInfo",
    "SkillResult",
    
    # Utilities
    "generate_id",
    "generate_timestamp",
    "generate_hash",
    "validate_json",
    "sanitize_string",
    "Timer",
    "parse_timeout",
    "LRUCache",
    
    # Telemetry
    "Telemetry",
    "Meter",
    "Tracer",
    "Span",
    "SpanStatus",
    "MetricPoint",
    "setup_logging",
    
    # Types
    "TaskID",
    "SessionID",
    "MemoryRecordID",
    "SkillID",
    "Timestamp",
    "ErrorCode",
    "JSONValue",
    "JSONObject",
]
'''
        (self.base_path / '__init__.py').write_text(top_init)
        print("  ✓ __init__.py (顶层)")

    def verify_structure(self) -> bool:
        """验证重构结果"""
        print("\n✅ 验证重构结果...")

        all_good = True

        # 检查目录是否存在
        expected_dirs = ['core', 'modules',
                         'client', 'utils', 'telemetry', 'types']
        for dir_name in expected_dirs:
            if not (self.base_path / dir_name).exists():
                print(f"  ❌ 缺少目录：{dir_name}")
                all_good = False
            else:
                print(f"  ✓ 目录存在：{dir_name}")

        # 检查关键文件
        key_files = [
            '__init__.py',
            '_version.py',
            'exceptions.py',
            'core/__init__.py',
            'modules/__init__.py',
            'client/__init__.py',
            'utils/__init__.py',
            'telemetry/__init__.py',
            'types/__init__.py',
        ]

        for file in key_files:
            if not (self.base_path / file).exists():
                print(f"  ❌ 缺少文件：{file}")
                all_good = False
            else:
                print(f"  ✓ 文件存在：{file}")

        return all_good

    def run_syntax_check(self) -> bool:
        """运行语法检查"""
        print("\n🔍 运行语法检查...")

        import py_compile
        import tempfile

        all_good = True
        checked = 0

        for py_file in self.base_path.rglob('*.py'):
            try:
                py_compile.compile(py_file, doraise=True)
                checked += 1
            except py_compile.PyCompileError as e:
                print(f"  ❌ 语法错误：{py_file.name} - {e}")
                all_good = False

        print(f"  ✓ 检查了 {checked} 个 Python 文件")
        return all_good

    def run(self) -> bool:
        """执行重构"""
        print("=" * 70)
        print("🚀 AgentOS Python SDK 重构开始")
        print("=" * 70)

        try:
            # 1. 创建备份
            self.create_backup()

            # 2. 创建目录结构
            self.create_directory_structure()

            # 3. 生成 __init__.py 文件
            self.generate_init_files()

            # 4. 更新顶层 __init__.py
            self.update_top_level_init()

            # 5. 验证结构
            if not self.verify_structure():
                print("\n❌ 重构验证失败！")
                return False

            # 6. 语法检查
            if not self.run_syntax_check():
                print("\n❌ 语法检查失败！")
                return False

            print("\n" + "=" * 70)
            print("✅ 重构完成！")
            print("=" * 70)
            print(f"\n📦 备份位置：{self.backup_path}")
            print("\n新架构:")
            print("""
agentos/
├── __init__.py          # 公共API 导出
├── _version.py          # 版本信息
├── exceptions.py        # 异常体系
│
├── core/                # 核心层 (FFI)
│   ├── __init__.py
│   ├── syscall.py      # C API 绑定
│   └── proxy.py        # Syscall 代理
│
├── modules/             # 业务模块
│   ├── task/           # 任务管理
│   ├── memory/         # 记忆管理
│   ├── session/        # 会话管理
│   └── skill/          # 技能管理
│
├── client/              # 客户端层
│   ├── base.py         # 基础客户端
│   ├── sync.py         # 同步客户端
│   └── async_.py       # 异步客户端
│
├── utils/               # 工具层
│   ├── id.py           # ID 生成
│   ├── time.py         # 时间处理
│   └── validation.py   # 数据验证
│
├── telemetry/           # 遥测层
│   ├── metrics.py      # 指标采集
│   └── tracing.py      # 分布式追踪
│
└── types/               # 类型定义
    ├── common.py       # 通用类型
    └── task.py         # 任务类型
            """)

            return True

        except Exception as e:
            print(f"\n❌ 重构失败：{e}")
            import traceback
            traceback.print_exc()
            return False


def main():
    """主函数"""
    base_path = Path(__file__).parent

    if not base_path.exists():
        print(f"❌ 路径不存在：{base_path}")
        sys.exit(1)

    restructurer = SDKRestructurer(str(base_path))
    success = restructurer.run()

    sys.exit(0 if success else 1)


if __name__ == "__main__":
    main()
