#!/usr/bin/env python3
# Copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
# AgentOS Logger - Terminal output formatting and interaction utilities

"""
AgentOS Logger Module

Provides terminal color/style constants, formatted output, progress bars,
spinners, and table rendering for CLI applications.

Usage:
    from core.logger import Color, Style, Logger, OutputFormatter, ProgressBar, Spinner, Table
"""

import itertools
import sys
import threading
import time
from enum import Enum
from typing import Any, Dict, List, Optional, Tuple


class Color(Enum):
    """Terminal color codes"""
    BLACK = "30"
    RED = "31"
    GREEN = "32"
    YELLOW = "33"
    BLUE = "34"
    MAGENTA = "35"
    CYAN = "36"
    WHITE = "37"
    BRIGHT_BLACK = "90"
    BRIGHT_RED = "91"
    BRIGHT_GREEN = "92"
    BRIGHT_YELLOW = "93"
    BRIGHT_BLUE = "94"
    BRIGHT_MAGENTA = "95"
    BRIGHT_CYAN = "96"
    BRIGHT_WHITE = "97"
    RESET = "0"


class Style(Enum):
    """Text style constants mapped to color combinations"""
    INFO = (Color.CYAN, None)
    SUCCESS = (Color.GREEN, None)
    WARNING = (Color.YELLOW, None)
    ERROR = (Color.RED, None)
    DIM = (Color.BRIGHT_BLACK, None)
    HEADER = (Color.BRIGHT_WHITE, Color.BLUE)
    ACCENT = (Color.BRIGHT_CYAN, None)
    PRIMARY = (Color.BLUE, None)
    SECONDARY = (Color.BRIGHT_BLACK, None)


class OutputFormatter:
    """Formats text with colors and styles for terminal output"""

    def __init__(self, use_color: bool = True):
        self.use_color = use_color and sys.stdout.isatty()

    @staticmethod
    def _colorize(text: str, fg: Optional[Color] = None,
                  bg: Optional[Color] = None,
                  bold: bool = False,
                  dim: bool = False) -> str:
        codes = []
        if bold:
            codes.append("1")
        if dim:
            codes.append("2")
        if fg:
            codes.append(fg.value)
        if bg:
            code = int(bg.value) + 10
            codes.append(str(code))
        if not codes:
            return text
        return f"\033[{';'.join(codes)}m{text}\033[0m"

    def format_message(self, message: str,
                       style: Style = Style.INFO) -> str:
        if isinstance(style.value, tuple):
            fg, bg = style.value
        else:
            fg, bg = style.value, None
        return self._colorize(message, fg=fg, bg=bg)

    def format_section(self, title: str) -> str:
        line = "=" * min(len(title) + 4, 60)
        header = self._colorize(line, bold=True)
        title_text = self._colorize(f"  {title}  ", fg=Color.BRIGHT_WHITE,
                                     bg=Color.BLUE, bold=True)
        footer = self._colorize(line, bold=True)
        return f"{header}\n{title_text}\n{footer}"

    def format_key_value(self, key: str, value: str) -> str:
        key_text = self._colorize(key, fg=Color.CYAN, bold=True)
        return f"  {key_text}: {value}"

    def format_bullet(self, item: str) -> str:
        bullet = self._colorize(">", fg=Color.GREEN, bold=True)
        return f"  {bullet} {item}"

    def format_status(self, status: str, message: str) -> str:
        status_colors = {
            "ok": (Color.GREEN, "[OK]"),
            "fail": (Color.RED, "[FAIL]"),
            "warn": (Color.YELLOW, "[WARN]"),
            "info": (Color.CYAN, "[INFO]"),
            "skip": (Color.BRIGHT_BLACK, "[SKIP]"),
        }
        fg, prefix = status_colors.get(status.lower(),
                                         (Color.WHITE, f"[{status.upper()}]"))
        prefix_text = self._colorize(prefix, fg=fg, bold=True)
        return f"  {prefix_text} {message}"


class Logger:
    """Structured logger with levels and colored output"""

    LEVELS = {"DEBUG": 10, "INFO": 20, "WARNING": 30, "ERROR": 40}

    def __init__(self, name: str = "agentos",
                 level: str = "INFO",
                 use_color: bool = True):
        self.name = name
        self.level = self.LEVELS.get(level, 20)
        self.formatter = OutputFormatter(use_color=use_color)

    def _log(self, level_name: str, level_num: int,
             message: str, *args, **kwargs):
        if level_num < self.level:
            return
        style_map = {
            "DEBUG": Style.DIM,
            "INFO": Style.INFO,
            "WARNING": Style.WARNING,
            "ERROR": Style.ERROR,
        }
        style = style_map.get(level_name, Style.INFO)
        timestamp = time.strftime("%H:%M:%S")
        prefix = self.formatter.format_message(
            f"[{timestamp}] [{self.name}] [{level_name}]", style)
        print(f"{prefix} {message}", *args, **kwargs)

    def debug(self, msg: str, *args, **kwargs):
        self._log("DEBUG", 10, msg, *args, **kwargs)

    def info(self, msg: str, *args, **kwargs):
        self._log("INFO", 20, msg, *args, **kwargs)

    def warning(self, msg: str, *args, **kwargs):
        self._log("WARNING", 30, msg, *args, **kwargs)

    def error(self, msg: str, *args, **kwargs):
        self._log("ERROR", 40, msg, *args, **kwargs)


class ProgressBar:
    """Terminal progress bar with percentage and ETA"""

    CHARS_FILLED = "█"
    CHARS_EMPTY = "░"
    WIDTH = 40

    def __init__(self, total: int, prefix: str = "",
                 suffix: str = "", use_color: bool = True):
        self.total = total
        self.prefix = prefix
        self.suffix = suffix
        self.current = 0
        self.use_color = use_color and sys.stdout.isatty()
        self._start_time = time.time()
        self._formatter = OutputFormatter(use_color=self.use_color)

    def update(self, n: int = 1) -> None:
        self.current = min(self.current + n, self.total)
        self._render()

    def _render(self) -> None:
        if self.total == 0:
            pct = 1.0
        else:
            pct = self.current / self.total
        filled = int(self.WIDTH * pct)
        bar = self.CHARS_FILLED * filled + self.CHARS_EMPTY * (self.WIDTH - filled)
        pct_str = f"{pct * 100:5.1f}%"

        elapsed = time.time() - self._start_time
        if self.current > 0 and elapsed > 0:
            eta = elapsed / self.current * (self.total - self.current)
            eta_str = f" ETA:{eta:.0f}s"
        else:
            eta_str = ""

        bar_colored = self._formatter._colorize(
            bar, fg=Color.GREEN, bold=True)
        line = f"\r{self.prefix} {bar_colored} {pct_str}"
        line += f" ({self.current}/{self.total}){eta_str} {self.suffix}"
        sys.stdout.write(line)
        sys.stdout.flush()

    def finish(self, message: str = "done") -> None:
        self.current = self.total
        self._render()
        done_msg = self._formatter.format_message(
            f" {message}", style=Style.SUCCESS)
        print(done_msg)


class Spinner:
    """Animated spinner for long-running operations"""

    FRAMES = ["⠋", "⠙", "⠹", "⠸", "⠼", "⠴", "⠦", "⠧", "⠇", "⠏"]

    def __init__(self, message: str = "",
                 use_color: bool = True):
        self.message = message
        self.use_color = use_color and sys.stdout.isatty()
        self._running = False
        self._thread: Optional[threading.Thread] = None
        _frame_iter = itertools.cycle(self.FRAMES)
        self._next_frame = lambda: next(_frame_iter)
        _frame_iter = itertools.cycle(self.FRAMES)

    def start(self) -> None:
        self._running = True
        self._thread = threading.Thread(target=self._spin, daemon=True)
        self._thread.start()

    def _spin(self) -> None:
        formatter = OutputFormatter(use_color=self.use_color)
        while self._running:
            frame = self.FRAMES[
                itertools.cycle(range(len(self.FRAMES))).__next__()
            ]
            frame_colored = formatter._colorize(
                frame, fg=Color.CYAN, bold=True)
            sys.stdout.write(
                f"\r{frame_colored} {self.message}")
            sys.stdout.flush()
            time.sleep(0.08)

    def stop(self, final_message: str = "") -> None:
        self._running = False
        if self._thread:
            self._thread.join(timeout=1.0)
        if final_message:
            formatter = OutputFormatter(use_color=self.use_color)
            msg = formatter.format_message(
                final_message, style=Style.SUCCESS)
            print(f"\r  {msg}")

    def __enter__(self):
        self.start()
        return self

    def __exit__(self, *args):
        self.stop()


class Table:
    """Renders aligned tabular data in the terminal"""

    def __init__(self, headers: List[str],
                 rows: List[List[str]],
                 padding: int = 2):
        self.headers = headers
        self.rows = rows
        self.padding = padding
        self._formatter = OutputFormatter()

    def render(self) -> str:
        all_rows = [self.headers] + self.rows
        col_count = len(self.headers)
        widths = [0] * col_count

        for row in all_rows:
            for i, cell in enumerate(row):
                if i < col_count:
                    widths[i] = max(widths[i], len(str(cell)))

        pad = " " * self.padding
        separator = "+" + "+".join(
            "-" * (w + 2 * self.padding) for w in widths) + "+"
        lines = [separator]

        header_cells = [
            self._formatter._colorize(
                self.headers[i].ljust(widths[i]),
                fg=Color.BRIGHT_WHITE, bold=True)
            for i in range(col_count)
        ]
        lines.append("|" + pad + (pad + "|").join(header_cells) + pad + "|")
        lines.append(separator)

        for row in self.rows:
            cells = [
                str(row[i]).ljust(widths[i])
                if i < len(row) else "".ljust(widths[i])
                for i in range(col_count)
            ]
            lines.append("|" + pad + (pad + "|").join(cells) + pad + "|")
        lines.append(separator)
        return "\n".join(lines)
