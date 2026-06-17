# AgentPlugin 示例 — 自定义 RAG Agent
# 演示如何通过 AgentPlugin 定义 Agent 的推理策略和行为模式

from agentos.plugin_types import AgentPlugin


class MyCustomAgent(AgentPlugin):
    """自定义 RAG Agent，先检索相关文档再生成回答。"""

    name = "my_custom_agent"
    description = "基于检索增强生成的自定义 Agent，先检索后回答"

    async def run(self, task: str, context: dict | None = None) -> str:
        """
        自定义推理循环：
        1. 分析用户任务，提取检索关键词
        2. 调用检索工具获取相关文档
        3. 基于检索结果生成最终回答
        """
        context = context or {}

        # 步骤1：提取检索关键词
        query = await self._extract_query(task)

        # 步骤2：检索相关文档（通过 AgentRT 内置工具调用）
        docs = await self._retrieve(query, context)

        # 步骤3：基于文档生成回答
        answer = await self._generate(task, docs)

        return answer

    async def _extract_query(self, task: str) -> str:
        """从用户任务中提取检索关键词。"""
        # 实际项目中可调用 LLM 提取关键词
        return task

    async def _retrieve(self, query: str, context: dict) -> list[str]:
        """检索相关文档。"""
        # 实际项目中调用向量数据库或搜索工具
        return [f"关于「{query}」的相关文档内容..."]

    async def _generate(self, task: str, docs: list[str]) -> str:
        """基于检索文档生成回答。"""
        context_text = "\n".join(docs)
        return f"基于以下文档内容回答问题「{task}」：\n{context_text}"
