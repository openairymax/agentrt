"""WebSearchSkill — 网络搜索技能

执行网络搜索，整合多源结果，返回结构化搜索报告。
"""

from __future__ import annotations

import json
import logging
import re
from typing import Any, Dict, List, Optional
from urllib.parse import quote_plus, urljoin

import sys as _sys
from pathlib import Path as _Path

# 确保 agentos 包可导入（开发模式）
_sdk_python = _Path(__file__).resolve().parents[3] / "sdk" / "python"
if str(_sdk_python) not in _sys.path:
    _sys.path.insert(0, str(_sdk_python))

from agentos.plugin_types import (
    SkillPlugin,
    SkillDefinition,
)

logger = logging.getLogger(__name__)


class WebSearchSkill(SkillPlugin):
    """网络搜索技能。

    支持多搜索引擎查询、结果去重、相关性排序和摘要提取。

    功能：
    1. 多引擎搜索 — 支持 DuckDuckGo、SearXNG 等引擎
    2. 结果去重 — 基于 URL 规范化去重
    3. 相关性排序 — 基于关键词匹配度和来源权威度
    4. 摘要提取 — 从搜索结果中提取关键信息
    """

    PLUGIN_TYPE = "skill"

    # ── Skill Definition ──────────────────────────────────

    def get_definition(self) -> SkillDefinition:
        return SkillDefinition(
            name="web_search",
            description="执行网络搜索，整合多源结果，返回结构化搜索报告",
            version="1.0.0",
            category="information",
            tags=["search", "web", "information-retrieval", "research"],
            input_schema={
                "type": "object",
                "properties": {
                    "query": {
                        "type": "string",
                        "description": "搜索查询关键词",
                    },
                    "max_results": {
                        "type": "integer",
                        "description": "最大返回结果数",
                        "default": 10,
                    },
                    "engine": {
                        "type": "string",
                        "description": "搜索引擎 (duckduckgo/searxng/auto)",
                        "default": "auto",
                    },
                    "time_range": {
                        "type": "string",
                        "description": "时间范围 (day/week/month/year/all)",
                        "default": "all",
                    },
                    "region": {
                        "type": "string",
                        "description": "搜索区域 (us/cn/jp/de等)",
                        "default": "us",
                    },
                },
                "required": ["query"],
            },
            output_schema={
                "type": "object",
                "properties": {
                    "query": {"type": "string"},
                    "results": {"type": "array"},
                    "total_found": {"type": "integer"},
                    "summary": {"type": "string"},
                },
            },
            examples=[
                {
                    "input": "搜索 Rust 异步编程最佳实践",
                    "output": "返回 10 条相关结果，包含博客、文档和示例代码链接",
                },
                {
                    "input": "搜索最近的 AI 安全研究论文",
                    "output": "返回 arXiv 和顶级会议的最新论文",
                },
            ],
            requires=["network"],
        )

    def get_prompt_template(self) -> Optional[str]:
        return (
            "Search the web for: {query}\n\n"
            "Summarize the key findings from the search results. "
            "Focus on the most relevant and authoritative sources."
        )

    def get_system_instructions(self) -> Optional[str]:
        return (
            "You are a research assistant. When searching the web, "
            "prioritize authoritative sources (official docs, peer-reviewed papers, "
            "reputable tech blogs). Always note the publication date when available. "
            "Distinguish between factual information and opinions."
        )

    # ── Validation ────────────────────────────────────────

    def validate_input(self, context: Dict[str, Any]) -> bool:
        query = context.get("query", "")
        if not query or not query.strip():
            logger.warning("WebSearchSkill: empty query")
            return False
        if len(query) > 500:
            logger.warning("WebSearchSkill: query too long (%d chars)", len(query))
            return False
        return True

    # ── Execution ─────────────────────────────────────────

    async def pre_execute(self, context: Dict[str, Any]) -> Dict[str, Any]:
        context.setdefault("max_results", 10)
        context.setdefault("engine", "auto")
        context.setdefault("time_range", "all")
        context.setdefault("region", "us")
        return context

    async def execute(self, context: Dict[str, Any]) -> Any:
        query = context.get("query", "")
        max_results = context.get("max_results", 10)
        engine = context.get("engine", "auto")
        time_range = context.get("time_range", "all")

        # 尝试通过 Gateway 执行搜索
        results = await self._search_via_gateway(query, max_results, engine, time_range)

        # 如果 Gateway 不可用，返回搜索建议
        if not results:
            results = self._generate_search_links(query)

        # 去重
        results = self._deduplicate(results)

        # 排序
        results = self._rank_results(results, query)

        # 截断
        results = results[:max_results]

        # 生成摘要
        summary = self._generate_summary(query, results)

        return {
            "query": query,
            "results": results,
            "total_found": len(results),
            "summary": summary,
            "engine": engine,
        }

    # ── Gateway Search ────────────────────────────────────

    async def _search_via_gateway(
        self,
        query: str,
        max_results: int,
        engine: str,
        time_range: str,
    ) -> List[Dict[str, Any]]:
        """通过 Gateway API 执行搜索。"""
        try:
            import requests

            payload = {
                "action": "web_search",
                "query": query,
                "max_results": max_results,
                "engine": engine,
                "time_range": time_range,
            }
            resp = requests.post(
                "http://localhost:8080/v1/tools/execute",
                json=payload,
                timeout=15,
            )
            resp.raise_for_status()
            data = resp.json()
            return data.get("results", [])
        except ImportError:
            logger.debug("requests not available")
            return []
        except Exception as exc:
            logger.debug("Gateway search failed: %s", exc)
            return []

    # ── Fallback: Generate Search Links ───────────────────

    def _generate_search_links(self, query: str) -> List[Dict[str, Any]]:
        """当搜索不可用时，生成搜索引擎链接。"""
        encoded = quote_plus(query)
        return [
            {
                "title": f"Search: {query}",
                "url": f"https://duckduckgo.com/?q={encoded}",
                "snippet": f"DuckDuckGo search results for: {query}",
                "source": "duckduckgo",
                "relevance": 1.0,
            },
            {
                "title": f"Search: {query}",
                "url": f"https://www.google.com/search?q={encoded}",
                "snippet": f"Google search results for: {query}",
                "source": "google",
                "relevance": 0.9,
            },
        ]

    # ── Deduplication ─────────────────────────────────────

    @staticmethod
    def _normalize_url(url: str) -> str:
        """规范化 URL 用于去重。"""
        url = url.lower().rstrip("/")
        url = re.sub(r"^https?://(www\.)?", "", url)
        url = re.sub(r"[?&](utm_[^&=]+|ref|source)=[^&]*", "", url)
        return url

    def _deduplicate(self, results: List[Dict[str, Any]]) -> List[Dict[str, Any]]:
        """基于 URL 规范化去重。"""
        seen: set = set()
        unique = []
        for r in results:
            norm = self._normalize_url(r.get("url", ""))
            if norm not in seen:
                seen.add(norm)
                unique.append(r)
        return unique

    # ── Ranking ───────────────────────────────────────────

    def _rank_results(
        self, results: List[Dict[str, Any]], query: str
    ) -> List[Dict[str, Any]]:
        """基于关键词匹配度和来源权威度排序。"""
        query_terms = set(query.lower().split())

        # 权威来源加分
        authority_domains = {
            "github.com": 1.2,
            "docs.rs": 1.3,
            "python.org": 1.3,
            "rust-lang.org": 1.3,
            "arxiv.org": 1.2,
            "stackoverflow.com": 1.1,
            "wikipedia.org": 1.1,
            "mozilla.org": 1.2,
        }

        for r in results:
            score = r.get("relevance", 0.5)

            # 关键词匹配
            title = r.get("title", "").lower()
            snippet = r.get("snippet", "").lower()
            text = f"{title} {snippet}"
            matched = sum(1 for t in query_terms if t in text)
            keyword_score = matched / max(len(query_terms), 1)
            score = score * 0.6 + keyword_score * 0.4

            # 来源权威度
            url = r.get("url", "").lower()
            for domain, boost in authority_domains.items():
                if domain in url:
                    score *= boost
                    break

            r["relevance"] = round(score, 3)

        results.sort(key=lambda r: r.get("relevance", 0), reverse=True)
        return results

    # ── Summary ───────────────────────────────────────────

    def _generate_summary(
        self, query: str, results: List[Dict[str, Any]]
    ) -> str:
        """生成搜索结果摘要。"""
        if not results:
            return f"No results found for: {query}"

        top = results[:3]
        parts = [f"Found {len(results)} results for '{query}'. Top results:"]
        for i, r in enumerate(top, 1):
            title = r.get("title", "Untitled")
            source = r.get("source", "unknown")
            parts.append(f"  {i}. [{source}] {title}")

        return "\n".join(parts)
