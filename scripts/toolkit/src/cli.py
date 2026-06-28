#!/usr/bin/env python3
# Copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
# AgentRT 交互式 CLI 增强模块
# 提供 TUI 和交互式诊断功能

"""
AgentRT 交互式 CLI

提供增强的用户交互体验，包括：
- 彩色输出和格式化
- 进度条和spinner
- 选择菜单和确认提示
- 表格显示
- 分页输出

Usage:
    from agentos_scripts.toolkit.cli import AgentOSCLI
    from agentos_scripts.toolkit.logger import ProgressBar, Spinner, Table, Color, Style

    cli = AgentOSCLI()
    cli.info("Starting process...")
    cli.success("Operation completed")
"""

import asyncio
import sys
import time
from typing import Any, Callable, Dict, List, Optional, Union

from .logger import (
    Color,
    Style,
    Logger,
    OutputFormatter,
    ProgressBar,
    Spinner,
    Table,
)


class AgentOSCLI:
    """AgentRT CLI 主类 - 增强的用户交互体验"""

    def __init__(self, verbose: bool = False):
        self.verbose = verbose
        self.formatter = OutputFormatter()
        self._indent_level = 0

    def info(self, message: str) -> None:
        print(self.formatter.format_message(message, Style.INFO))

    def success(self, message: str) -> None:
        print(self.formatter.format_message(message, Style.SUCCESS))

    def warning(self, message: str) -> None:
        print(self.formatter.format_message(message, Style.WARNING))

    def error(self, message: str) -> None:
        print(self.formatter.format_message(message, Style.ERROR))

    def debug(self, message: str) -> None:
        if self.verbose:
            print(self.formatter.format_message(f"[DEBUG] {message}", Style.DIM))

    def section(self, title: str) -> None:
        print(self.formatter.format_section(title))

    def key_value(self, key: str, value: str) -> None:
        print(self.formatter.format_key_value(key, value))

    def bullet(self, item: str) -> None:
        indent = "  " * self._indent_level
        print(f"{indent}{self.formatter.format_bullet(item)}")

    def table(self, headers: List[str], rows: List[List[str]]) -> None:
        table = Table(headers, rows)
        print(table.render())

    def indented(self, func: Callable[[], None]) -> None:
        self._indent_level += 1
        try:
            func()
        finally:
            self._indent_level -= 1

    def confirm(self, message: str, default: bool = False) -> bool:
        suffix = "[Y/n]" if default else "[y/N]"
        response = input(f"{message} {suffix}: ").strip().lower()

        if not response:
            return default

        return response in ["y", "yes"]

    def select(
        self,
        message: str,
        options: List[str],
        allow_multiple: bool = False
    ) -> Union[List[int], int]:
        print(f"\n{message}")
        for i, option in enumerate(options, 1):
            print(f"  {i}) {option}")

        range_str = f"1-{len(options)}" if allow_multiple else f"1-{len(options)}"

        while True:
            try:
                response = input(f"Select [{range_str}]: ").strip()

                if allow_multiple:
                    indices = [int(x.strip()) - 1 for x in response.split(",")]
                    if all(0 <= i < len(options) for i in indices):
                        return indices
                else:
                    index = int(response) - 1
                    if 0 <= index < len(options):
                        return index

            except ValueError:
                pass

            print("Invalid selection. Please try again.")

    def progress(self, total: int, prefix: str = "") -> ProgressBar:
        return ProgressBar(total, prefix)

    def spinner(self, message: str = "") -> Spinner:
        return Spinner(message)


# Note: For full system diagnostics, use scripts.toolkit.AgentOSDoctor
# The InteractiveDoctor class has been migrated to toolkit/doctor.py
# to provide comprehensive 8-category health checking with JSON output.
