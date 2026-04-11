#!/usr/bin/env python3
# Copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
# AgentOS Configuration Template Engine
# Migrated from scripts/core/config.py

"""
AgentOS Configuration Template Engine

Jinja2-based configuration template rendering system:
- Multi-environment support (dev/staging/production/testing)
- Template registration and file loading
- Variable validation and defaults
- Graceful fallback to simple rendering when Jinja2 unavailable

Usage:
    from scripts.toolkit import ConfigEngine
    
    engine = ConfigEngine(template_dir="templates")
    result = engine.render("production.conf", context={"version": "1.0.0"})
"""

import json
import os
from dataclasses import dataclass, field
from enum import Enum
from pathlib import Path
from typing import Any, Dict, List, Optional
from uuid import uuid4


class Environment(Enum):
    DEVELOPMENT = "development"
    STAGING = "staging"
    PRODUCTION = "production"
    TESTING = "testing"


@dataclass
class ConfigTemplate:
    name: str
    path: str
    environment: Environment
    variables: Dict[str, Any] = field(default_factory=dict)
    required_variables: List[str] = field(default_factory=list)
    optional_variables: Dict[str, Any] = field(default_factory=dict)


@dataclass
class RenderResult:
    success: bool
    content: str = ""
    error: str = ""
    warnings: List[str] = field(default_factory=list)


class ConfigEngine:
    """Configuration template engine with Jinja2 and fallback rendering"""

    DEFAULT_DELIMITERS = ("{{", "}}")
    BLOCK_START = "{%"
    BLOCK_END = "%}"

    def __init__(self, template_dir: Optional[str] = None,
                 default_environment: Environment = Environment.DEVELOPMENT):
        self.template_dir = Path(template_dir) if template_dir else Path("templates")
        self.default_environment = default_environment
        self._templates: Dict[str, ConfigTemplate] = {}
        self._globals: Dict[str, Any] = {
            "uuid": lambda: str(uuid4()),
            "env": lambda key, default="": os.environ.get(key, default),
        }

    def register_template(self, template: ConfigTemplate) -> None:
        self._templates[template.name] = template

    def load_template(self, path: str) -> ConfigTemplate:
        template_path = Path(path)
        if not template_path.exists():
            raise FileNotFoundError(f"Template not found: {path}")
        
        name = template_path.stem
        env = self._detect_environment(name)
        template = ConfigTemplate(name=name, path=str(template_path), environment=env)
        self._templates[name] = template
        return template

    def _detect_environment(self, name: str) -> Environment:
        name_lower = name.lower()
        for env in Environment:
            if env.value in name_lower:
                return env
        return self.default_environment

    def render(self, template_name: str,
                context: Optional[Dict[str, Any]] = None,
                strict: bool = False) -> RenderResult:
        context = context or {}
        
        if template_name not in self._templates:
            return RenderResult(success=False,
                               error=f"Template not registered: {template_name}")
        
        template = self._templates[template_name]
        warnings = []
        
        for var in template.required_variables:
            if var not in context:
                if strict:
                    return RenderResult(success=False,
                                       error=f"Required variable missing: {var}")
                warnings.append(f"Optional variable using default: {var}")
        
        for key, default in template.optional_variables.items():
            context.setdefault(key, default)
        
        context.update(self._globals)
        
        try:
            with open(template.path) as f:
                template_content = f.read()
            
            content = self._render_jinja(template_content, context)
            return RenderResult(success=True, content=content, warnings=warnings)
        
        except Exception as e:
            return RenderResult(success=False, error=f"Render failed: {str(e)}")

    def _render_jinja(self, content: str, ctx: Dict[str, Any]) -> str:
        try:
            from jinja2 import Environment as JEnv, Template
            env = JEnv(delimiters=self.DEFAULT_DELIMITERS,
                        block_start=self.BLOCK_START, block_end=self.BLOCK_END)
            return env.from_string(content).render(**ctx)
        except ImportError:
            return self._render_simple(content, ctx)

    def _render_simple(self, content: str, ctx: Dict[str, Any]) -> str:
        import re
        pattern = re.compile(r'\{\{\s*(\w+)\s*\}\}')
        result = content
        for match in pattern.finditer(content):
            var_name = match.group(1)
            value = ctx.get(var_name, f"{{{{{var_name}}}}}")
            result = result.replace(match.group(0), str(value))
        return result

    def render_to_file(self, template_name: str, output_path: str,
                       context: Optional[Dict[str, Any]] = None,
                       overwrite: bool = False) -> RenderResult:
        if os.path.exists(output_path) and not overwrite:
            return RenderResult(success=False,
                               error=f"Output file exists: {output_path}")
        
        result = self.render(template_name, context)
        if result.success:
            os.makedirs(os.path.dirname(output_path), exist_ok=True)
            with open(output_path, "w", encoding="utf-8") as f:
                f.write(result.content)
        return result

    def list_templates(self) -> List[ConfigTemplate]:
        return list(self._templates.values())

    def get_template(self, name: str) -> Optional[ConfigTemplate]:
        return self._templates.get(name)


def create_default_engine() -> ConfigEngine:
    return ConfigEngine(template_dir="scripts/templates",
                         default_environment=Environment.DEVELOPMENT)


def render_config_file(template_path: str, output_path: str,
                       context: Dict[str, Any],
                       environment: Environment = Environment.DEVELOPMENT) -> bool:
    engine = ConfigEngine()
    template = engine.load_template(template_path)
    template.environment = environment
    result = engine.render_to_file(template.name, output_path, context, overwrite=True)
    return result.success
