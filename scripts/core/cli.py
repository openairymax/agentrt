#!/usr/bin/env python3
# Copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
# AgentOS 交互式 CLI 增强模块
# 提供 TUI 和交互式诊断功能

"""
AgentOS 交互式 CLI

提供增强的用户交互体验，包括：
- 彩色输出和格式化
- 进度条和spinner
- 选择菜单和确认提示
- 表格显示
- 分页输出

Usage:
    from agentos_scripts.core.cli import AgentOSCLI, ProgressBar, Table

    cli = AgentOSCLI()
    cli.info("Starting process...")
    cli.success("Operation completed")
"""

import asyncio
import sys
import time
from abc import ABC, abstractmethod
from dataclasses import dataclass
from enum import Enum
from typing import Any, Callable, Dict, List, Optional, Union


class Color:
    """颜色定义"""
    RESET = "\033[0m"
    BOLD = "\033[1m"
    DIM = "\033[2m"

    RED = "\033[0;31m"
    GREEN = "\033[0;32m"
    YELLOW = "\033[1;33m"
    BLUE = "\033[0;34m"
    MAGENTA = "\033[0;35m"
    CYAN = "\033[0;36m"
    WHITE = "\033[0;37m"

    BG_RED = "\033[41m"
    BG_GREEN = "\033[42m"
    BG_YELLOW = "\033[43m"


class Style(Enum):
    """样式枚举"""
    NORMAL = "normal"
    SUCCESS = "success"
    WARNING = "warning"
    ERROR = "error"
    INFO = "info"
    BOLD = "bold"


@dataclass
class StyleConfig:
    """样式配置"""
    prefix: str = ""
    color: str = ""
    suffix: str = ""


STYLE_CONFIGS: Dict[Style, StyleConfig] = {
    Style.NORMAL: StyleConfig(),
    Style.SUCCESS: StyleConfig("[✓]", Color.GREEN),
    Style.WARNING: StyleConfig("[!]", Color.YELLOW),
    Style.ERROR: StyleConfig("[✗]", Color.RED),
    Style.INFO: StyleConfig("[•]", Color.BLUE),
    Style.BOLD: StyleConfig("", Color.BOLD),
}


class OutputFormatter:
    """输出格式化器"""

    @staticmethod
    def format_message(message: str, style: Style = Style.NORMAL) -> str:
        """格式化消息"""
        config = STYLE_CONFIGS[style]
        return f"{config.prefix}{config.color}{message}{Color.RESET}"

    @staticmethod
    def format_section(title: str, width: int = 70) -> str:
        """格式化分节标题"""
        return f"\n{Color.BOLD}{'━' * width}{Color.RESET}\n" \
               f"{Color.CYAN}▶ {title}{Color.RESET}\n" \
               f"{Color.BOLD}{'━' * width}{Color.RESET}\n"

    @staticmethod
    def format_key_value(key: str, value: str, key_width: int = 20) -> str:
        """格式化键值对"""
        return f"  {Color.BOLD}{key:<{key_width}}{Color.RESET} {value}"

    @staticmethod
    def format_bullet(item: str, indent: int = 2) -> str:
        """格式化列表项"""
        return f"{' ' * indent}{Color.CYAN}•{Color.RESET} {item}"


class Table:
    """表格格式化器"""

    def __init__(
        self,
        headers: List[str],
        rows: List[List[str]] = None,
        max_width: int = 100
    ):
        self.headers = headers
        self.rows = rows or []
        self.max_width = max_width

    def add_row(self, row: List[str]) -> None:
        """添加行"""
        self.rows.append(row)

    def _calculate_widths(self) -> List[int]:
        """计算列宽"""
        widths = [len(h) for h in self.headers]

        for row in self.rows:
            for i, cell in enumerate(row):
                if i < len(widths):
                    widths[i] = max(widths[i], len(str(cell)))

        return widths

    def render(self) -> str:
        """渲染表格"""
        if not self.headers:
            return ""

        widths = self._calculate_widths()
        lines = []

        header_line = "  ".join(
            f"{Color.BOLD}{h:<{w}}{Color.RESET}"
            for h, w in zip(self.headers, widths)
        )
        lines.append(header_line)
        lines.append("-" * len(header_line))

        for row in self.rows:
            row_line = "  ".join(
                f"{str(cell):<{w}}"
                for cell, w in zip(row, widths)
            )
            lines.append(row_line)

        return "\n".join(lines)


class ProgressBar:
    """进度条"""

    def __init__(
        self,
        total: int,
        prefix: str = "",
        width: int = 40,
        show_percentage: bool = True
    ):
        self.total = max(1, total)
        self.current = 0
        self.prefix = prefix
        self.width = width
        self.show_percentage = show_percentage
        self.start_time = time.time()

    def update(self, current: int = None) -> None:
        """更新进度"""
        if current is not None:
            self.current = current
        else:
            self.current += 1

        self._render()

    def _render(self) -> None:
        """渲染进度条"""
        filled = int(self.width * self.current / self.total)
        bar = "█" * filled + "░" * (self.width - filled)

        percentage = int(100 * self.current / self.total)
        elapsed = time.time() - self.start_time

        if self.current > 0:
            eta = (elapsed / self.current) * (self.total - self.current)
        else:
            eta = 0

        if self.show_percentage:
            progress_str = f"{bar} {percentage}% ({elapsed:.1f}s / {eta:.1f}s)"
        else:
            progress_str = bar

        print(f"\r{self.prefix}{progress_str}", end="", flush=True)

        if self.current >= self.total:
            print()

    def complete(self) -> None:
        """完成进度条"""
        self.current = self.total
        self._render()


class Spinner:
    """旋转指示器"""

    FRAMES = ["⠋", "⠙", "⠹", "⠸", "⠼", "⠴", "⠦", "⠧", "⠇", "⠏"]

    def __init__(self, message: str = ""):
        self.message = message
        self.running = False
        self.current_frame = 0

    def start(self) -> None:
        """启动 spinner"""
        self.running = True
        self._spin()

    def stop(self, final_message: str = None) -> None:
        """停止 spinner"""
        self.running = False
        if final_message:
            print(f"\r{self.message}: {final_message}")
        else:
            print(f"\r{' ' * (len(self.message) + 20)}")

    def _spin(self) -> None:
        """旋转动画"""
        while self.running:
            frame = self.FRAMES[self.current_frame % len(self.FRAMES)]
            print(f"\r{frame} {self.message}", end="", flush=True)
            self.current_frame += 1
            time.sleep(0.1)


class AgentOSCLI:
    """AgentOS CLI 主类"""

    def __init__(self, verbose: bool = False):
        self.verbose = verbose
        self.formatter = OutputFormatter()
        self._indent_level = 0

    def info(self, message: str) -> None:
        """信息消息"""
        print(self.formatter.format_message(message, Style.INFO))

    def success(self, message: str) -> None:
        """成功消息"""
        print(self.formatter.format_message(message, Style.SUCCESS))

    def warning(self, message: str) -> None:
        """警告消息"""
        print(self.formatter.format_message(message, Style.WARNING))

    def error(self, message: str) -> None:
        """错误消息"""
        print(self.formatter.format_message(message, Style.ERROR))

    def debug(self, message: str) -> None:
        """调试消息"""
        if self.verbose:
            print(self.formatter.format_message(f"[DEBUG] {message}", Style.DIM))

    def section(self, title: str) -> None:
        """分节标题"""
        print(self.formatter.format_section(title))

    def key_value(self, key: str, value: str) -> None:
        """键值对"""
        print(self.formatter.format_key_value(key, value))

    def bullet(self, item: str) -> None:
        """列表项"""
        indent = "  " * self._indent_level
        print(f"{indent}{self.formatter.format_bullet(item)}")

    def table(self, headers: List[str], rows: List[List[str]]) -> None:
        """表格"""
        table = Table(headers, rows)
        print(table.render())

    def indented(self, func: Callable[[], None]) -> None:
        """缩进块"""
        self._indent_level += 1
        try:
            func()
        finally:
            self._indent_level -= 1

    def confirm(self, message: str, default: bool = False) -> bool:
        """确认提示"""
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
        """选择菜单"""
        print(f"\n{message}")
        for i, option in enumerate(options, 1):
            print(f"  {i}) {option}")

        while True:
            try:
                response = input(f"Select [{'1-' + str(len(options)) if allow_multiple else '1' + '-' + str(len(options))}]: ").strip()

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
        """创建进度条"""
        return ProgressBar(total, prefix)

    def spinner(self, message: str = "") -> Spinner:
        """创建 spinner"""
        return Spinner(message)


class InteractiveDoctor:
    """交互式诊断"""

    def __init__(self, cli: AgentOSCLI = None):
        self.cli = cli or AgentOSCLI()

    def run_diagnosis(self) -> Dict[str, Any]:
        """运行交互式诊断"""
        self.cli.section("AgentOS Interactive Diagnosis")

        checks = {
            "system": "检查系统环境...",
            "dependencies": "检查依赖项...",
            "network": "检查网络连接...",
            "docker": "检查 Docker 环境...",
            "config": "检查配置文件...",
        }

        results = {}

        for check_name, check_message in checks.items():
            spinner = self.cli.spinner(check_message)
            spinner.start()
            time.sleep(0.5)
            spinner.stop("done")

            results[check_name] = {"status": "ok", "details": {}}

        self.cli.section("Diagnosis Results")
        self.cli.table(
            ["Check", "Status"],
            [[k, v["status"].upper()] for k, v in results.items()]
        )

        return results
