﻿﻿#!/usr/bin/env python3
# Copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
# AgentOS 系统健康检查工具
# 遵循 AgentOS 架构设计原则：反馈闭环、安全内生

"""
AgentOS 系统健康检查工具

提供对 AgentOS 运行环境的全面检查，包括：
- 系统依赖检查
- 运行时环境检查
- 网络连接检查
- 磁盘空间检查
- 配置文件检查
- Docker 环境检查（可选）

Usage:
    python doctor.py [--verbose] [--output FORMAT]
    python doctor.py --fix 自动修复可修复的问题
    python doctor.py --check docker,network,disk
"""

import argparse
import json
import os
import platform
import shutil
import socket
import subprocess
import sys
from dataclasses import dataclass, field
from datetime import datetime
from enum import Enum
from typing import List, Dict, Any, Optional, Callable


class CheckStatus(Enum):
    OK = "ok"
    WARN = "warn"
    FAIL = "fail"
    SKIP = "skip"


class CheckCategory(Enum):
    SYSTEM = "system"
    DEPENDENCY = "dependency"
    NETWORK = "network"
    DISK = "disk"
    manager = "manager"
    DOCKER = "docker"
    SECURITY = "security"


@dataclass
class CheckResult:
    """检查结果"""
    name: str
    status: CheckStatus
    category: CheckCategory
    message: str
    details: Optional[str] = None
    fix_suggestion: Optional[str] = None


@dataclass
class HealthReport:
    """健康检查报告"""
    timestamp: str
    platform: str
    python_version: str
    checks: List[CheckResult] = field(default_factory=list)
    summary: Dict[str, int] = field(default_factory=dict)
    all_passed: bool = True


class DoctorChecker:
    """健康检查器基类"""

    def __init__(self, verbose: bool = False):
        self.verbose = verbose

    def check(self) -> CheckResult:
        raise NotImplementedError


class SystemInfoChecker(DoctorChecker):
    """系统信息检查"""

    def check(self) -> CheckResult:
        try:
            uname = platform.uname()
            message = f"{uname.system} {uname.release} ({uname.machine})"
            return CheckResult(
                name="System Info",
                status=CheckStatus.OK,
                category=CheckCategory.SYSTEM,
                message=message,
                details=f"Node: {uname.node}, Processor: {uname.processor}"
            )
        except Exception as e:
            return CheckResult(
                name="System Info",
                status=CheckStatus.FAIL,
                category=CheckCategory.SYSTEM,
                message=f"Failed to get system info: {e}"
            )


class PythonVersionChecker(DoctorChecker):
    """Python 版本检查"""

    MIN_VERSION = (3, 8)

    def check(self) -> CheckResult:
        version = sys.version_info
        version_str = f"{version.major}.{version.minor}.{version.micro}"

        if version >= self.MIN_VERSION:
            return CheckResult(
                name="Python Version",
                status=CheckStatus.OK,
                category=CheckCategory.SYSTEM,
                message=f"Python {version_str} (meets minimum {self.MIN_VERSION[0]}.{self.MIN_VERSION[1]})"
            )
        else:
            return CheckResult(
                name="Python Version",
                status=CheckStatus.FAIL,
                category=CheckCategory.SYSTEM,
                message=f"Python {version_str} is below minimum {self.MIN_VERSION[0]}.{self.MIN_VERSION[1]}",
                fix_suggestion=f"Upgrade to Python {'.'.join(map(str, self.MIN_VERSION))} or higher"
            )


class DependencyChecker(DoctorChecker):
    """依赖检查"""

    REQUIRED_COMMANDS = ["bash", "cmake", "make", "gcc"]
    OPTIONAL_COMMANDS = ["docker", "git", "clang", "ninja"]

    def check(self) -> List[CheckResult]:
        results = []

        for cmd in self.REQUIRED_COMMANDS:
            result = self._check_command(cmd, required=True)
            results.append(result)

        for cmd in self.OPTIONAL_COMMANDS:
            result = self._check_command(cmd, required=False)
            results.append(result)

        return results

    def _check_command(self, cmd: str, required: bool) -> CheckResult:
        executable = shutil.which(cmd)

        if executable:
            try:
                proc = subprocess.run(
                    [cmd, "--version"],
                    capture_output=True,
                    text=True,
                    timeout=5
                )
                version_info = proc.stdout.split("\n")[0] if proc.stdout else "unknown"
                return CheckResult(
                    name=f"Command: {cmd}",
                    status=CheckStatus.OK,
                    category=CheckCategory.DEPENDENCY,
                    message=f"{cmd} is installed",
                    details=f"Version: {version_info}"
                )
            except Exception as e:
                return CheckResult(
                    name=f"Command: {cmd}",
                    status=CheckStatus.FAIL if required else CheckStatus.WARN,
                    category=CheckCategory.DEPENDENCY,
                    message=f"{cmd} found but failed to get version: {e}"
                )
        else:
            status = CheckStatus.FAIL if required else CheckStatus.WARN
            msg = f"{cmd} is not installed"
            suggestion = f"Install {cmd}" if required else f"Optional: Install {cmd} for full functionality"
            return CheckResult(
                name=f"Command: {cmd}",
                status=status,
                category=CheckCategory.DEPENDENCY,
                message=msg,
                fix_suggestion=suggestion
            )


class NetworkChecker(DoctorChecker):
    """网络连接检查"""

    def check(self) -> List[CheckResult]:
        results = []

        results.append(self._check_dns())
        results.append(self._check_internet())

        return results

    def _check_dns(self) -> CheckResult:
        try:
            socket.gethostbyname("localhost")
            return CheckResult(
                name="DNS Resolution",
                status=CheckStatus.OK,
                category=CheckCategory.NETWORK,
                message="DNS resolution is working"
            )
        except Exception as e:
            return CheckResult(
                name="DNS Resolution",
                status=CheckStatus.FAIL,
                category=CheckCategory.NETWORK,
                message=f"DNS resolution failed: {e}",
                fix_suggestion="Check your DNS configuration and /etc/hosts file"
            )

    def _check_internet(self) -> CheckResult:
        test_hosts = ["8.8.8.8", "1.1.1.1"]
        for host in test_hosts:
            try:
                sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
                sock.settimeout(3)
                sock.connect((host, 53))
                sock.close()
                return CheckResult(
                    name="Internet Connectivity",
                    status=CheckStatus.OK,
                    category=CheckCategory.NETWORK,
                    message="Internet connectivity is available"
                )
            except Exception:
                continue

        return CheckResult(
            name="Internet Connectivity",
            status=CheckStatus.WARN,
            category=CheckCategory.NETWORK,
            message="Cannot reach external hosts",
            fix_suggestion="Check your network connection and firewall settings"
        )


class DiskSpaceChecker(DoctorChecker):
    """磁盘空间检查"""

    MIN_FREE_SPACE_MB = 1000

    def check(self) -> CheckResult:
        try:
            usage = shutil.disk_usage("/")
            free_mb = usage.free / (1024 * 1024)

            if free_mb >= self.MIN_FREE_SPACE_MB:
                return CheckResult(
                    name="Disk Space",
                    status=CheckStatus.OK,
                    category=CheckCategory.DISK,
                    message=f"{(usage.free / (1024**3)):.1f} GB free ({(usage.free / usage.total * 100):.1f}%)",
                    details=f"Total: {(usage.total / (1024**3)):.1f} GB, Used: {(usage.used / (1024**3)):.1f} GB"
                )
            else:
                return CheckResult(
                    name="Disk Space",
                    status=CheckStatus.FAIL,
                    category=CheckCategory.DISK,
                    message=f"Only {free_mb:.0f} MB free (minimum: {self.MIN_FREE_SPACE_MB} MB)",
                    fix_suggestion=f"Free up at least {self.MIN_FREE_SPACE_MB - free_mb:.0f} MB"
                )
        except Exception as e:
            return CheckResult(
                name="Disk Space",
                status=CheckStatus.FAIL,
                category=CheckCategory.DISK,
                message=f"Failed to check disk space: {e}"
            )


class ConfigFileChecker(DoctorChecker):
    """配置文件检查"""

    REQUIRED_CONFIGS = ["agentos.conf", "logging.conf", "memory.conf"]

    def check(self) -> List[CheckResult]:
        results = []
        config_paths = [
            "/etc/agentos",
            os.path.expanduser("~/.agentos/manager"),
            os.path.join(os.getcwd(), "manager")
        ]

        found_configs = set()

        for config_dir in config_paths:
            if not os.path.exists(config_dir):
                continue

            for manager in self.REQUIRED_CONFIGS:
                config_path = os.path.join(config_dir, manager)
                if os.path.exists(config_path):
                    found_configs.add(manager)
                    try:
                        stat = os.stat(config_path)
                        results.append(CheckResult(
                            name=f"manager: {manager}",
                            status=CheckStatus.OK,
                            category=CheckCategory.manager,
                            message=f"Found at {config_path}",
                            details=f"Size: {stat.st_size} bytes, Modified: {datetime.fromtimestamp(stat.st_mtime)}"
                        ))
                    except Exception as e:
                        results.append(CheckResult(
                            name=f"manager: {manager}",
                            status=CheckStatus.FAIL,
                            category=CheckCategory.manager,
                            message=f"Cannot stat manager file: {e}"
                        ))

        for manager in self.REQUIRED_CONFIGS:
            if manager not in found_configs:
                results.append(CheckResult(
                    name=f"manager: {manager}",
                    status=CheckStatus.WARN,
                    category=CheckCategory.manager,
                    message=f"{manager} not found in standard locations",
                    fix_suggestion=f"Create {manager} in one of: {', '.join(config_paths)}"
                ))

        return results


class DockerChecker(DoctorChecker):
    """Docker 环境检查"""

    def check(self) -> List[CheckResult]:
        results = []

        docker_path = shutil.which("docker")
        if not docker_path:
            results.append(CheckResult(
                name="Docker Installation",
                status=CheckStatus.SKIP,
                category=CheckCategory.DOCKER,
                message="Docker is not installed"
            ))
            return results

        results.append(CheckResult(
            name="Docker Installation",
            status=CheckStatus.OK,
            category=CheckCategory.DOCKER,
            message=f"Docker found at {docker_path}"
        ))

        try:
            proc = subprocess.run(
                ["docker", "--version"],
                capture_output=True,
                text=True,
                timeout=5
            )
            if proc.returncode == 0:
                results.append(CheckResult(
                    name="Docker Version",
                    status=CheckStatus.OK,
                    category=CheckCategory.DOCKER,
                    message=proc.stdout.strip()
                ))
        except Exception as e:
            results.append(CheckResult(
                name="Docker Version",
                status=CheckStatus.FAIL,
                category=CheckCategory.DOCKER,
                message=f"Failed to get Docker version: {e}"
            ))

        try:
            proc = subprocess.run(
                ["docker", "info"],
                capture_output=True,
                text=True,
                timeout=10
            )
            if proc.returncode == 0:
                results.append(CheckResult(
                    name="Docker Daemon",
                    status=CheckStatus.OK,
                    category=CheckCategory.DOCKER,
                    message="Docker daemon is running"
                ))
            else:
                results.append(CheckResult(
                    name="Docker Daemon",
                    status=CheckStatus.FAIL,
                    category=CheckCategory.DOCKER,
                    message="Docker daemon is not running",
                    fix_suggestion="Run 'sudo systemctl start docker' (Linux) or start Docker Desktop (macOS/Windows)"
                ))
        except Exception as e:
            results.append(CheckResult(
                name="Docker Daemon",
                status=CheckStatus.FAIL,
                category=CheckCategory.DOCKER,
                message=f"Failed to check Docker daemon: {e}"
            ))

        return results


class SecurityChecker(DoctorChecker):
    """安全检查"""

    def check(self) -> List[CheckResult]:
        results = []

        results.append(self._check_sensitive_files())

        return results

    def _check_sensitive_files(self) -> CheckResult:
        sensitive_patterns = [".env", "*.key", "*.pem", "*.cert", "credentials*"]
        found_sensitive = []

        for root, dirs, files in os.walk(os.getcwd()):
            for file in files:
                if any(pattern.replace("*", "") in file for pattern in sensitive_patterns):
                    if not file.startswith("."):
                        found_sensitive.append(file)

        if found_sensitive:
            return CheckResult(
                name="Sensitive Files",
                status=CheckStatus.WARN,
                category=CheckCategory.SECURITY,
                message=f"Found {len(found_sensitive)} potentially sensitive files",
                details=f"Files: {', '.join(found_sensitive[:5])}...",
                fix_suggestion="Ensure .gitignore includes these files and they are not committed to version control"
            )

        return CheckResult(
            name="Sensitive Files",
            status=CheckStatus.OK,
            category=CheckCategory.SECURITY,
            message="No obvious sensitive files found in working directory"
        )


class AgentOSDoctor:
    """AgentOS 健康检查主类"""

    def __init__(self, verbose: bool = False, fix: bool = False):
        self.verbose = verbose
        self.fix = fix
        self.checkers: List[DoctorChecker] = []

    def register_checker(self, checker: DoctorChecker):
        self.checkers.append(checker)

    def run_all_checks(self) -> HealthReport:
        report = HealthReport(
            timestamp=datetime.now().isoformat(),
            platform=platform.system(),
            python_version=sys.version.split()[0]
        )

        self.register_checker(SystemInfoChecker(self.verbose))
        self.register_checker(PythonVersionChecker(self.verbose))
        self.register_checker(DependencyChecker(self.verbose))
        self.register_checker(DiskSpaceChecker(self.verbose))
        self.register_checker(ConfigFileChecker(self.verbose))
        self.register_checker(DockerChecker(self.verbose))
        self.register_checker(SecurityChecker(self.verbose))
        self.register_checker(NetworkChecker(self.verbose))

        for checker in self.checkers:
            try:
                results = checker.check()
                if isinstance(results, CheckResult):
                    results = [results]

                for result in results:
                    report.checks.append(result)

            except Exception as e:
                report.checks.append(CheckResult(
                    name=checker.__class__.__name__,
                    status=CheckStatus.FAIL,
                    category=CheckCategory.SYSTEM,
                    message=f"Check failed with exception: {e}"
                ))

        status_counts = {status.value: 0 for status in CheckStatus}
        for check in report.checks:
            status_counts[check.status.value] += 1

        report.summary = status_counts
        report.all_passed = status_counts[CheckStatus.FAIL.value] == 0

        return report

    def print_report(self, report: HealthReport, format: str = "text"):
        if format == "json":
            print(json.dumps(asdict(report), indent=2, default=str))
            return

        print("=" * 70)
        print("AgentOS System Health Check Report")
        print("=" * 70)
        print(f"Timestamp: {report.timestamp}")
        print(f"Platform: {report.platform}")
        print(f"Python: {report.python_version}")
        print("")

        print(f"{'Check':<35} {'Status':<8} {'Message'}")
        print("-" * 70)

        for check in report.checks:
            status_icon = {
                CheckStatus.OK: "✓",
                CheckStatus.WARN: "!",
                CheckStatus.FAIL: "✗",
                CheckStatus.SKIP: "-"
            }.get(check.status, "?")

            status_color = {
                CheckStatus.OK: "\033[0;32m",
                CheckStatus.WARN: "\033[1;33m",
                CheckStatus.FAIL: "\033[0;31m",
                CheckStatus.SKIP: "\033[0;36m"
            }.get(check.status, "")

            reset = "\033[0m"
            print(f"{check.name:<35} {status_color}{status_icon} {check.status.value:<6}{reset} {check.message}")

            if self.verbose and check.details:
                print(f"{'':>35}   Details: {check.details}")

            if self.verbose and check.fix_suggestion:
                print(f"{'':>35}   Fix: {check.fix_suggestion}")

        print("-" * 70)
        print("")

        print("Summary:")
        for status, count in report.summary.items():
            status_color = {
                "ok": "\033[0;32m",
                "warn": "\033[1;33m",
                "fail": "\033[0;31m",
                "skip": "\033[0;36m"
            }.get(status, "")
            print(f"  {status_color}{status.upper()}: {count}{reset}")

        print("")
        if report.all_passed:
            print("\033[0;32m✓ All checks passed!\033[0m")
        else:
            print("\033[0;31m✗ Some checks failed. Please review the issues above.\033[0m")

        print("=" * 70)


def main():
    from dataclasses import asdict

    parser = argparse.ArgumentParser(
        description="AgentOS System Health Check Tool",
        formatter_class=argparse.RawDescriptionHelpFormatter
    )

    parser.add_argument(
        "--verbose", "-v",
        action="store_true",
        help="Show detailed information"
    )

    parser.add_argument(
        "--fix",
        action="store_true",
        help="Attempt to fix automatically fixable issues"
    )

    parser.add_argument(
        "--check",
        type=str,
        help="Comma-separated list of checks to run (system, dependency, network, disk, manager, docker, security)"
    )

    parser.add_argument(
        "--output", "-o",
        type=str,
        choices=["text", "json"],
        default="text",
        help="Output format (default: text)"
    )

    args = parser.parse_args()

    doctor = AgentOSDoctor(verbose=args.verbose, fix=args.fix)
    report = doctor.run_all_checks()
    doctor.print_report(report, format=args.output)

    return 0 if report.all_passed else 1


if __name__ == "__main__":
    sys.exit(main())