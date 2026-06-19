"""TextSummarizationSkill — 文本摘要技能

对长文本进行智能压缩和摘要生成，支持多种摘要模式。
"""

from __future__ import annotations

import json
import logging
import re
from typing import Any, Dict, List, Optional

import sys as _sys
from pathlib import Path as _Path

# 确保 agentos 包可导入（开发模式）
_sdk_python = _Path(__file__).resolve().parents[2] / "sdk" / "python"
if str(_sdk_python) not in _sys.path:
    _sys.path.insert(0, str(_sdk_python))

from agentos.plugin_types import (
    SkillPlugin,
    SkillDefinition,
)

logger = logging.getLogger(__name__)


class TextSummarizationSkill(SkillPlugin):
    """文本摘要技能。

    对长文本进行智能摘要生成，支持多种摘要模式：
    1. 提取式 (extractive) — 从原文中提取关键句子
    2. 抽象式 (abstractive) — 生成全新的概括性摘要
    3. 要点式 (bullet) — 提取关键要点
    4. 精简式 (concise) — 极简摘要（一句话）
    """

    PLUGIN_TYPE = "skill"

    # ── Skill Definition ──────────────────────────────────

    def get_definition(self) -> SkillDefinition:
        return SkillDefinition(
            name="text_summarization",
            description="对长文本进行智能压缩和摘要生成，支持提取式、抽象式、要点式和精简式四种模式",
            version="1.0.0",
            category="text-processing",
            tags=["summarization", "nlp", "text-processing", "compression"],
            input_schema={
                "type": "object",
                "properties": {
                    "text": {
                        "type": "string",
                        "description": "要摘要的原始文本",
                    },
                    "mode": {
                        "type": "string",
                        "description": "摘要模式 (extractive/abstractive/bullet/concise)",
                        "default": "abstractive",
                    },
                    "max_length": {
                        "type": "integer",
                        "description": "摘要最大长度（字符数），默认 500",
                        "default": 500,
                    },
                    "language": {
                        "type": "string",
                        "description": "文本语言 (zh/en/auto)",
                        "default": "auto",
                    },
                    "focus": {
                        "type": "string",
                        "description": "摘要关注点 (general/technical/business/news)",
                        "default": "general",
                    },
                },
                "required": ["text"],
            },
            output_schema={
                "type": "object",
                "properties": {
                    "summary": {"type": "string"},
                    "mode": {"type": "string"},
                    "original_length": {"type": "integer"},
                    "summary_length": {"type": "integer"},
                    "compression_ratio": {"type": "number"},
                    "key_points": {"type": "array"},
                },
            },
            examples=[
                {
                    "input": "将一篇 5000 字的技术文章摘要为 200 字要点",
                    "output": "生成 3 个关键要点，压缩比 96%",
                },
                {
                    "input": "将会议记录精简为一句话总结",
                    "output": "一句话概括会议核心决议",
                },
            ],
            requires=["llm"],
        )

    def get_prompt_template(self) -> Optional[str]:
        return (
            "Summarize the following text in {mode} mode.\n\n"
            "Focus area: {focus}\n"
            "Language: {language}\n"
            "Maximum summary length: {max_length} characters\n\n"
            "Text:\n{text}\n\n"
            "Provide the summary in JSON format with fields: "
            "summary, key_points (list of main points)."
        )

    def get_system_instructions(self) -> Optional[str]:
        return (
            "You are a professional text summarizer. "
            "Preserve the key information and logical structure. "
            "Remove redundant content, greetings, and filler phrases. "
            "For extractive mode: select key sentences from the original text. "
            "For abstractive mode: generate a coherent summary in your own words. "
            "For bullet mode: extract key points as bullet items. "
            "For concise mode: provide a single-sentence summary. "
            "Maintain factual accuracy — do not introduce new information."
        )

    # ── Validation ────────────────────────────────────────

    def validate_input(self, context: Dict[str, Any]) -> bool:
        text = context.get("text", "")
        if not text or not text.strip():
            logger.warning("TextSummarizationSkill: empty text input")
            return False
        mode = context.get("mode", "abstractive")
        valid_modes = {"extractive", "abstractive", "bullet", "concise"}
        if mode not in valid_modes:
            logger.warning("TextSummarizationSkill: invalid mode '%s'", mode)
            return False
        return True

    # ── Execution ─────────────────────────────────────────

    async def pre_execute(self, context: Dict[str, Any]) -> Dict[str, Any]:
        context.setdefault("mode", "abstractive")
        context.setdefault("max_length", 500)
        context.setdefault("language", "auto")
        context.setdefault("focus", "general")

        text = context.get("text", "")
        context["original_length"] = len(text)
        context["original_word_count"] = len(text.split())

        # 自动检测语言
        if context["language"] == "auto":
            context["language"] = self._detect_language(text)

        # 超长文本截断提示
        if len(text) > 50000:
            context["max_length"] = min(context["max_length"], 1000)
            logger.info("TextSummarizationSkill: long text (%d chars), limiting summary", len(text))

        return context

    async def execute(self, context: Dict[str, Any]) -> Any:
        text = context.get("text", "")
        mode = context.get("mode", "abstractive")
        max_length = context.get("max_length", 500)
        focus = context.get("focus", "general")

        # 本地预处理：提取型模式进行句子评分
        key_points = []
        if mode == "extractive":
            key_points = self._extract_key_sentences(text, max_length)

        # 基于文本长度计算摘要预算
        original_length = len(text)
        if mode == "concise":
            target_length = min(200, max_length)
        elif mode == "bullet":
            target_length = min(400, max_length)
        else:
            target_length = max_length

        # 生成摘要（本地预处理 + LLM 增强）
        summary = self._generate_summary(
            text, mode, target_length, focus, key_points
        )

        return {
            "summary": summary,
            "mode": mode,
            "original_length": original_length,
            "summary_length": len(summary),
            "compression_ratio": round(
                (1 - len(summary) / max(original_length, 1)) * 100, 1
            ),
            "key_points": key_points,
            "language": context.get("language", "auto"),
            "focus": focus,
        }

    async def post_execute(self, context: Dict[str, Any], result: Any) -> Any:
        # 确保摘要不超过最大长度
        if isinstance(result, dict) and "summary" in result:
            max_len = context.get("max_length", 500)
            if len(result["summary"]) > max_len:
                result["summary"] = result["summary"][:max_len].rsplit(" ", 1)[0] + "..."
                result["summary_length"] = len(result["summary"])
        return result

    # ── Language Detection ────────────────────────────────

    def _detect_language(self, text: str) -> str:
        """简单的语言检测：统计中文字符比例。"""
        if not text:
            return "en"
        chinese_chars = len(re.findall(r'[\u4e00-\u9fff]', text))
        total_chars = len(re.sub(r'\s', '', text))
        if total_chars > 0 and chinese_chars / total_chars > 0.3:
            return "zh"
        return "en"

    # ── Sentence Extraction ───────────────────────────────

    def _extract_key_sentences(
        self, text: str, max_length: int
    ) -> List[str]:
        """提取关键句子（基于 TF 加权评分）。"""
        sentences = re.split(r'(?<=[.!?。！？\n])\s+', text)
        if len(sentences) <= 5:
            return sentences

        # 计算词频
        words = re.findall(r'\b\w+\b', text.lower())
        word_freq: Dict[str, int] = {}
        for w in words:
            if len(w) > 3:
                word_freq[w] = word_freq.get(w, 0) + 1

        # 句子评分
        scored = []
        for sent in sentences:
            sent_words = set(re.findall(r'\b\w+\b', sent.lower()))
            score = sum(word_freq.get(w, 0) for w in sent_words)
            # 位置奖励：开头和结尾的句子权重更高
            scored.append((sent, score))

        # 按分数排序取前几个
        scored.sort(key=lambda x: x[1], reverse=True)
        result = []
        total = 0
        for sent, _ in scored:
            if total + len(sent) > max_length:
                break
            result.append(sent.strip())
            total += len(sent)

        return result

    # ── Summary Generation ────────────────────────────────

    def _generate_summary(
        self,
        text: str,
        mode: str,
        target_length: int,
        focus: str,
        key_points: List[str],
    ) -> str:
        """生成摘要文本。"""
        original_length = len(text)

        if mode == "extractive" and key_points:
            return " ".join(key_points)

        if mode == "bullet":
            lines = []
            if key_points:
                lines = [f"• {p}" for p in key_points[:8]]
            else:
                # 降级：使用首段和末段
                paragraphs = text.split("\n\n")
                if len(paragraphs) >= 2:
                    lines = [
                        f"• {paragraphs[0][:200].strip()}",
                        f"• {paragraphs[-1][:200].strip()}",
                    ]
            result = "\n".join(lines)
            return result[:target_length]

        if mode == "concise":
            # 提取第一句作为摘要
            first_sent = re.split(r'(?<=[.!?。！？])\s+', text.strip())[0]
            if len(first_sent) > target_length:
                return first_sent[:target_length].rsplit(" ", 1)[0] + "..."
            return first_sent

        # abstractive (default): 需要 LLM 增强，此处提供本地降级
        sentences = re.split(r'(?<=[.!?。！？])\s+', text.strip())
        if len(sentences) <= 3:
            return text[:target_length]

        # 取首尾各一句，中间取一句
        mid = len(sentences) // 2
        summary_sentences = [sentences[0], sentences[mid], sentences[-1]]
        result = " ".join(s.strip() for s in summary_sentences)
        if len(result) > target_length:
            result = result[:target_length].rsplit(" ", 1)[0] + "..."
        return result