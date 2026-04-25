# AgentOS 示例插件 - 日志记录器
# Version: 1.0.0

"""
LoggerPlugin 示例插件

提供日志记录和查询功能，演示PluginSDK的基本用法。
"""

from typing import Any, Dict, List

from agentos.framework.plugin import BasePlugin


class LoggerPlugin(BasePlugin):
    """日志记录器插件 - 提供结构化日志记录和查询"""

    __version__ = "1.0.0"

    def __init__(self):
        super().__init__()
        self._logs: List[Dict[str, Any]] = []
        self._max_entries = 1000

    async def on_load(self, context: Dict[str, Any]) -> None:
        max_entries = context.get("max_entries", 1000)
        if isinstance(max_entries, int) and max_entries > 0:
            self._max_entries = max_entries

    async def on_activate(self, context: Dict[str, Any]) -> None:
        self._logs = []

    async def on_deactivate(self) -> None:
        self._logs.clear()

    async def on_unload(self) -> None:
        self._logs = []

    async def on_error(self, error: Exception) -> None:
        pass

    def get_capabilities(self) -> List[str]:
        return ["logging", "structured_logs", "log_query"]

    def log(self, level: str, message: str, metadata: Dict[str, Any] = None) -> int:
        """记录一条日志"""
        import time
        entry = {
            "level": level,
            "message": message,
            "metadata": metadata or {},
            "timestamp": time.time(),
        }
        self._logs.append(entry)
        if len(self._logs) > self._max_entries:
            self._logs = self._logs[-self._max_entries:]
        return len(self._logs)

    def query(self, level: str = None, limit: int = 100) -> List[Dict[str, Any]]:
        """查询日志"""
        results = self._logs
        if level:
            results = [e for e in results if e["level"] == level]
        return results[-limit:]

    def count(self) -> int:
        """获取日志条数"""
        return len(self._logs)
