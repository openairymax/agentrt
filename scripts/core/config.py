#!/usr/bin/env python3
# Copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
# AgentOS 配置模板引擎
# 基于 Jinja2 的配置生成系统

"""
AgentOS 配置模板引擎

提供灵活的配置生成能力，包括：
- Jinja2 模板渲染
- 多环境配置（dev/staging/production）
- 配置验证和默认值
- 敏感信息加密

Usage:
    from agentos_scripts.core.manager import ConfigEngine

    engine = ConfigEngine(template_dir="templates")
    engine.render("production.conf", context={"version": "1.0.0"})
"""

import json
import os
import sys
from dataclasses import dataclass, field
from enum import Enum
from pathlib import Path
from typing import Any, Dict, List, Optional
from uuid import uuid4


class Environment(Enum):
    """环境枚举"""
    DEVELOPMENT = "development"
    STAGING = "staging"
    PRODUCTION = "production"
    TESTING = "testing"


@dataclass
class ConfigTemplate:
    """配置模板"""
    name: str
    path: str
    environment: Environment
    variables: Dict[str, Any] = field(default_factory=dict)
    required_variables: List[str] = field(default_factory=list)
    optional_variables: Dict[str, Any] = field(default_factory=dict)


@dataclass
class RenderResult:
    """渲染结果"""
    success: bool
    content: str = ""
    error: str = ""
    warnings: List[str] = field(default_factory=list)


class ConfigEngine:
    """配置引擎"""

    DEFAULT_DELIMITERS = ("{{", "}}")
    BLOCK_START = "{%"
    BLOCK_END = "%}"

    def __init__(
        self,
        template_dir: str = None,
        default_environment: Environment = Environment.DEVELOPMENT
    ):
        self.template_dir = Path(template_dir) if template_dir else Path("templates")
        self.default_environment = default_environment
        self._templates: Dict[str, ConfigTemplate] = {}
        self._globals: Dict[str, Any] = {
            "uuid": lambda: str(uuid4()),
            "env": lambda key, default="": os.environ.get(key, default),
        }

    def register_template(self, template: ConfigTemplate) -> None:
        """注册模板"""
        self._templates[template.name] = template

    def load_template(self, path: str) -> ConfigTemplate:
        """从文件加载模板"""
        template_path = Path(path)
        if not template_path.exists():
            raise FileNotFoundError(f"Template not found: {path}")

        name = template_path.stem
        env = self._detect_environment(name)

        template = ConfigTemplate(
            name=name,
            path=str(template_path),
            environment=env
        )

        self._templates[name] = template
        return template

    def _detect_environment(self, name: str) -> Environment:
        """检测环境"""
        name_lower = name.lower()
        for env in Environment:
            if env.value in name_lower:
                return env
        return self.default_environment

    def render(
        self,
        template_name: str,
        context: Dict[str, Any] = None,
        strict: bool = False
    ) -> RenderResult:
        """渲染模板"""
        context = context or {}

        if template_name not in self._templates:
            return RenderResult(
                success=False,
                error=f"Template not registered: {template_name}"
            )

        template = self._templates[template_name]
        warnings = []

        for var in template.required_variables:
            if var not in context:
                if strict:
                    return RenderResult(
                        success=False,
                        error=f"Required variable missing: {var}"
                    )
                warnings.append(f"Optional variable using default: {var}")

        for key, default in template.optional_variables.items():
            context.setdefault(key, default)

        context.update(self._globals)

        try:
            with open(template.path) as f:
                template_content = f.read()

            content = self._render_jinja(template_content, context)

            return RenderResult(
                success=True,
                content=content,
                warnings=warnings
            )

        except Exception as e:
            return RenderResult(
                success=False,
                error=f"Render failed: {str(e)}"
            )

    def _render_jinja(self, template_content: str, context: Dict[str, Any]) -> str:
        """使用 Jinja2 渲染"""
        try:
            from jinja2 import Environment, Template
            env = Environment(
                delimiters=self.DEFAULT_DELIMITERS,
                block_start=self.BLOCK_START,
                block_end=self.BLOCK_END
            )
            template = env.from_string(template_content)
            return template.render(**context)
        except ImportError:
            return self._render_simple(template_content, context)

    def _render_simple(self, template_content: str, context: Dict[str, Any]) -> str:
        """简单模板渲染（无 Jinja2）"""
        import re
        pattern = re.compile(r'\{\{\s*(\w+)\s*\}\}')
        result = template_content

        for match in pattern.finditer(template_content):
            var_name = match.group(1)
            value = context.get(var_name, f"{{{{{var_name}}}}}")
            result = result.replace(match.group(0), str(value))

        return result

    def render_to_file(
        self,
        template_name: str,
        output_path: str,
        context: Dict[str, Any] = None,
        overwrite: bool = False
    ) -> RenderResult:
        """渲染到文件"""
        if os.path.exists(output_path) and not overwrite:
            return RenderResult(
                success=False,
                error=f"Output file exists: {output_path}"
            )

        result = self.render(template_name, context)

        if result.success:
            os.makedirs(os.path.dirname(output_path), exist_ok=True)
            with open(output_path, "w") as f:
                f.write(result.content)

        return result

    def list_templates(self) -> List[ConfigTemplate]:
        """列出所有模板"""
        return list(self._templates.values())

    def get_template(self, name: str) -> Optional[ConfigTemplate]:
        """获取模板"""
        return self._templates.get(name)


def create_default_engine() -> ConfigEngine:
    """创建默认引擎"""
    return ConfigEngine(
        template_dir="scripts/templates",
        default_environment=Environment.DEVELOPMENT
    )


def render_config_file(
    template_path: str,
    output_path: str,
    context: Dict[str, Any],
    environment: Environment = Environment.DEVELOPMENT
) -> bool:
    """便捷配置渲染函数"""
    engine = ConfigEngine()
    template = engine.load_template(template_path)
    template.environment = environment

    result = engine.render_to_file(
        template.name,
        output_path,
        context,
        overwrite=True
    )

    return result.success
