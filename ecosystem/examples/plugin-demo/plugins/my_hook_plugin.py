# HookPlugin 示例 — 日志与权限钩子
# 演示如何通过 HookPlugin 在 Agent 生命周期中注入自定义逻辑

from agentos.plugin_types import HookPlugin


class LoggingHook(HookPlugin):
    """日志记录钩子，在 Agent 调用前后记录请求和响应信息。"""

    name = "logging_hook"
    description = "记录 Agent 每次调用的请求和响应日志"

    async def on_before_invoke(self, context: dict) -> dict:
        """
        Agent 调用前的钩子。
        可用于：日志记录、权限检查、请求改写等。
        """
        agent_name = context.get("agent_name", "unknown")
        task = context.get("task", "")

        # 记录请求日志
        print(f"[Hook] Agent「{agent_name}」开始处理任务：{task}")

        # 可以修改 context，例如注入额外信息
        context["metadata"] = {
            "start_time": self._get_timestamp(),
            "source": "hook_injected",
        }

        return context

    async def on_after_invoke(self, context: dict) -> dict:
        """
        Agent 调用后的钩子。
        可用于：响应日志、结果过滤、性能统计等。
        """
        agent_name = context.get("agent_name", "unknown")
        result = context.get("result", "")
        metadata = context.get("metadata", {})

        # 记录响应日志
        print(f"[Hook] Agent「{agent_name}」处理完成，结果长度：{len(result)}")
        if "start_time" in metadata:
            print(f"[Hook] 耗时：{self._get_timestamp() - metadata['start_time']}ms")

        return context

    async def on_error(self, context: dict, error: Exception) -> dict:
        """
        Agent 调用出错时的钩子。
        可用于：错误上报、降级处理等。
        """
        agent_name = context.get("agent_name", "unknown")
        print(f"[Hook] Agent「{agent_name}」发生错误：{error}")

        # 可以返回降级结果
        context["result"] = f"抱歉，处理过程中发生错误：{error}"
        return context

    @staticmethod
    def _get_timestamp() -> int:
        """获取当前时间戳（毫秒）。"""
        import time
        return int(time.time() * 1000)
