# AgentOS Python SDK - Task Module
# Version: 2.0.0.0

"""
Task management module.

Provides functionality for:
    - Submitting tasks to AgentOS
    - Querying task status
    - Waiting for task completion
    - Cancelling tasks
"""

try:
    from .manager import TaskManager
    from .models import TaskInfo, TaskResult, TaskStatus
    from .errors import TaskError

    __all__ = [
        "TaskManager",
        "TaskInfo",
        "TaskResult",
        "TaskStatus",
        "TaskError",
    ]
except ImportError:
    # 允许延迟导入
    __all__ = []
