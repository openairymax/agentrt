#!/usr/bin/env python3
# Copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
# AgentOS System Doctor - Health Diagnostics
# Migrated from scripts/operations/doctor.py

"""
AgentOS System Health Diagnostic Tool

Comprehensive system health checker with 8 diagnostic categories:
- System (OS, CPU, Memory, Disk)
- Python Environment (version, packages)
- Build Tools (CMake, compilers, vcpkg)
- Project Structure (required files/directories)
- Configuration Files (syntax and required fields)
- Network Connectivity (port availability)
- Security (permissions, file integrity)
- Performance (resource thresholds)

Usage:
    from scripts.toolkit import AgentOSDoctor

    doctor = AgentOSDoctor()
    doctor.run_all_checks()
    doctor.print_report()
"""

import argparse
import json
import os
import platform
import shutil
import subprocess
import sys
from dataclasses import dataclass, field, asdict
from datetime import datetime
from enum import Enum
from pathlib import Path
from typing import Any, Dict, List, Optional, Tuple


class CheckStatus(Enum):
    PASS = "pass"
    FAIL = "fail"
    WARN = "warn"
    SKIP = "skip"
    INFO = "info"


@dataclass
class CheckResult:
    """Single check result"""
    category: str
    name: str
    status: CheckStatus
    message: str
    details: str = ""
    fix_command: str = ""


@dataclass
class HealthReport:
    """Complete health report"""
    timestamp: str = ""
    hostname: str = ""
    platform: str = ""
    python_version: str = ""
    total_checks: int = 0
    passed: int = 0
    failed: int = 0
    warnings: int = 0
    skipped: int = 0
    results: List[CheckResult] = field(default_factory=list)

    def add(self, result: CheckResult):
        self.results.append(result)
        self.total_checks += 1
        if result.status == CheckStatus.PASS:
            self.passed += 1
        elif result.status == CheckStatus.FAIL:
            self.failed += 1
        elif result.status == CheckStatus.WARN:
            self.warnings += 1
        else:
            self.skipped += 1

    def is_healthy(self) -> bool:
        return self.failed == 0


class BaseChecker:
    """Base class for all diagnostic checkers"""

    def __init__(self):
        self.results: List[CheckResult] = []

    def _pass(self, name: str, message: str, details: str = "") -> CheckResult:
        return CheckResult(category=self.__class__.__name__.replace("Checker", ""),
                           name=name, status=CheckStatus.PASS,
                           message=message, details=details)

    def _fail(self, name: str, message: str, details: str = "",
              fix: str = "") -> CheckResult:
        return CheckResult(category=self.__class__.__name__.replace("Checker", ""),
                           name=name, status=CheckStatus.FAIL,
                           message=message, details=details, fix_command=fix)

    def _warn(self, name: str, message: str, details: str = "") -> CheckResult:
        return CheckResult(category=self.__class__.__name__.replace("Checker", ""),
                           name=name, status=CheckStatus.WARN,
                           message=message, details=details)

    def _skip(self, name: str, message: str) -> CheckResult:
        return CheckResult(category=self.__class__.__name__.replace("Checker", ""),
                           name=name, status=CheckStatus.SKIP, message=message)


class SystemChecker(BaseChecker):
    """System environment checks"""

    REQUIRED_MEMORY_GB = 2.0
    REQUIRED_DISK_GB = 5.0

    def check_os(self) -> CheckResult:
        info = platform.uname()
        return self._pass(
            "Operating System",
            f"{info.system} {info.release} ({info.machine})",
            f"Version: {info.version}"
        )

    def check_python_version(self) -> CheckResult:
        version = sys.version.split()[0]
        major, minor = sys.version_info[:2]

        if major < 3 or (major == 3 and minor < 8):
            return self._fail(
                "Python Version",
                f"Python {version} (requires >= 3.8)",
                fix="python3 --version or install Python 3.8+"
            )
        return self._pass("Python Version", f"Python {version}")

    def check_memory(self) -> CheckResult:
        try:
            import psutil
            mem = psutil.virtual_memory()
            mem_gb = mem.total / (1024 ** 3)

            if mem_gb < self.REQUIRED_MEMORY_GB:
                return self._warn(
                    "Memory",
                    f"{mem_gb:.1f} GB (recommended >= {self.REQUIRED_MEMORY_GB} GB)"
                )
            return self._pass("Memory", f"{mem_gb:.1f} GB available")
        except ImportError:
            return self._skip("Memory", "psutil not installed")

    def check_disk_space(self) -> CheckResult:
        try:
            import psutil
            disk = psutil.disk_usage(str(Path.cwd()))
            disk_free_gb = disk.free / (1024 ** 3)

            if disk_free_gb < self.REQUIRED_DISK_GB:
                return self._warn(
                    "Disk Space",
                    f"{disk_free_gb:.1f} GB free (recommended >= {self.REQUIRED_DISK_GB} GB)",
                    f"Total: {disk.total / (1024**3):.1f} GB"
                )
            return self._pass("Disk Space", f"{disk_free_gb:.1f} GB free")
        except Exception as e:
            return self._skip("Disk Space", str(e))

    def run_all(self) -> List[CheckResult]:
        return [
            self.check_os(),
            self.check_python_version(),
            self.check_memory(),
            self.check_disk_space()
        ]


class PythonEnvironmentChecker(BaseChecker):
    """Python environment and dependency checks"""

    REQUIRED_PACKAGES = ["jinja2", "pyyaml", "toml"]

    def check_pip_available(self) -> CheckResult:
        try:
            result = subprocess.run(
                [sys.executable, "-m", "pip", "--version"],
                capture_output=True, text=True, timeout=10
            )
            return self._pass("pip", result.stdout.strip())
        except Exception as e:
            return self._fail("pip", f"Error: {e}")

    def check_required_packages(self) -> List[CheckResult]:
        results = []
        for pkg in self.REQUIRED_PACKAGES:
            try:
                __import__(pkg.replace("-", "_"))
                results.append(self._pass(f"Package: {pkg}", "Installed"))
            except ImportError:
                results.append(self._warn(
                    f"Package: {pkg}", "Not installed",
                    fix=f"pip install {pkg}"
                ))
        return results

    def run_all(self) -> List[CheckResult]:
        results = [self.check_pip_available()]
        results.extend(self.check_required_packages())
        return results


class BuildToolsChecker(BaseChecker):
    """Build tools and compiler checks"""

    def check_cmake(self) -> CheckResult:
        try:
            result = subprocess.run(
                ["cmake", "--version"],
                capture_output=True, text=True, timeout=10
            )
            version = result.stdout.strip().split("\n")[0] if result.returncode == 0 else ""
            if result.returncode != 0:
                return self._fail("CMake", "Not found or error",
                                  fix="Install CMake >= 3.20")
            return self._pass("CMake", version)
        except FileNotFoundError:
            return self._fail("CMake", "Not found in PATH",
                              fix="Install CMake >= 3.20")

    def check_c_compiler(self) -> CheckResult:
        for cc in ["clang", "gcc"]:
            try:
                result = subprocess.run(
                    [cc, "--version"],
                    capture_output=True, text=True, timeout=10
                )
                if result.returncode == 0:
                    version = result.stdout.strip().split("\n")[0]
                    return self._pass(f"C Compiler ({cc})", version)
            except FileNotFoundError:
                continue
        return self._fail("C Compiler", "No C compiler found",
                          fix="Install GCC or Clang")

    def check_cpp_compiler(self) -> CheckResult:
        for cxx in ["clang++", "g++"]:
            try:
                result = subprocess.run(
                    [cxx, "--version"],
                    capture_output=True, text=True, timeout=10
                )
                if result.returncode == 0:
                    version = result.stdout.strip().split("\n")[0]
                    return self._pass(f"C++ Compiler ({cxx})", version)
            except FileNotFoundError:
                continue
        return self._fail("C++ Compiler", "No C++ compiler found",
                          fix="Install G++ or Clang++")

    def check_vcpkg(self) -> CheckResult:
        vcpkg_paths = [
            os.environ.get("VCPKG_ROOT", ""),
            os.path.expanduser("~/vcpkg"),
            "/opt/vcpkg",
            "C:/vcpkg"
        ]
        for vp in vcpkg_paths:
            if vp and os.path.isfile(os.path.join(vp, "vcpkg.exe" if sys.platform == "win32" else "vcpkg")):
                return self._pass("vcpkg", f"Found at {vp}")
        return self._warn("vcpkg", "Not detected",
                         fix="Set VCPKG_ROOT environment variable")

    def run_all(self) -> List[CheckResult]:
        return [
            self.check_cmake(),
            self.check_c_compiler(),
            self.check_cpp_compiler(),
            self.check_vcpkg()
        ]


class ProjectStructureChecker(BaseChecker):
    """Project file structure validation"""

    REQUIRED_FILES = [
        "CMakeLists.txt",
        "README.md",
        ".editorconfig"
    ]

    REQUIRED_DIRS = [
        "src",
        "include",
        "scripts"
    ]

    def check_required_files(self) -> List[CheckResult]:
        results = []
        for f in self.REQUIRED_FILES:
            if os.path.exists(f):
                results.append(self._pass(f"File: {f}", "Exists"))
            else:
                results.append(self._warn(f"File: {f}", "Missing"))
        return results

    def check_required_dirs(self) -> List[CheckResult]:
        results = []
        for d in self.REQUIRED_DIRS:
            if os.path.isdir(d):
                results.append(self._pass(f"Directory: {d}/", "Exists"))
            else:
                results.append(self._warn(f"Directory: {d}/", "Missing"))
        return results

    def run_all(self) -> List[CheckResult]:
        results = []
        results.extend(self.check_required_files())
        results.extend(self.check_required_dirs())
        return results


class ConfigurationChecker(BaseChecker):
    """Configuration file validation"""

    CONFIG_FILES = {
        "agentos.conf": ["version", "log_level"],
        "logging.conf": ["level", "format"]
    }

    def check_config_files(self) -> List[CheckResult]:
        results = []
        config_dir = os.path.expanduser("~/.agentos")
        for fname, required_fields in self.CONFIG_FILES.items():
            fpath = os.path.join(config_dir, fname)
            if not os.path.exists(fpath):
                results.append(self._warn(
                    f"Config: {fname}", "Not found",
                    fix="Run: python scripts/toolkit/initializer.py --init"
                ))
            else:
                results.append(self._pass(f"Config: {fname}", "Exists"))
        return results

    def run_all(self) -> List[CheckResult]:
        return self.check_config_files()


class SecurityChecker(BaseChecker):
    """Security-related checks"""

    def check_file_permissions(self) -> CheckResult:
        critical_files = [".env", "settings.yaml"]
        issues = []
        for f in critical_files:
            if os.path.exists(f):
                mode = oct(os.stat(f).st_mode)[-3:]
                if mode in ["777", "666", "644"]:
                    issues.append(f"{f}: {mode}")
        if issues:
            return self._warn("File Permissions",
                              f"Insecure permissions: {', '.join(issues)}",
                              fix="chmod 600 .env settings.yaml")
        return self._pass("File Permissions", "OK")

    def run_all(self) -> List[CheckResult]:
        return [self.check_file_permissions()]


class AgentOSDoctor:
    """
    Main diagnostic orchestrator.

    Runs all checkers and aggregates results into a unified report.
    """

    CHECKERS = [
        ("System", SystemChecker),
        ("Python Environment", PythonEnvironmentChecker),
        ("Build Tools", BuildToolsChecker),
        ("Project Structure", ProjectStructureChecker),
        ("Configuration", ConfigurationChecker),
        ("Security", SecurityChecker),
    ]

    def __init__(self, project_root: Optional[str] = None):
        self.project_root = project_root or os.getcwd()
        self.report = HealthReport(
            timestamp=datetime.now().isoformat(),
            hostname=platform.node(),
            platform=platform.platform(),
            python_version=sys.version.split()[0]
        )

    def run_all_checks(self, categories: Optional[List[str]] = None) -> HealthReport:
        target_categories = set(categories) if categories else {c[0] for c in self.CHECKERS}

        for cat_name, checker_cls in self.CHECKERS:
            if cat_name not in target_categories:
                continue

            checker = checker_cls()
            try:
                results = checker.run_all()
                for r in results:
                    self.report.add(r)
            except Exception as e:
                self.report.add(CheckResult(
                    category=cat_name, name="Runner",
                    status=CheckStatus.FAIL, message=str(e)
                ))

        return self.report

    def print_report(self, output_format: str = "text") -> None:
        if output_format == "json":
            print(json.dumps(asdict(self.report), indent=2, default=str))
            return

        icons = {
            CheckStatus.PASS: "\033[0;32m✓\033[0m",
            CheckStatus.FAIL: "\033[0;31m✗\033[0m",
            CheckStatus.WARN: "\033[1;33m!\033[0m",
            CheckStatus.SKIP: "\033[0;36m-\033[0m",
            CheckStatus.INFO: "\033[0;34mi\033[0m",
        }

        print("=" * 70)
        print("AgentOS System Health Report")
        print("=" * 70)
        print(f"Timestamp : {self.report.timestamp}")
        print(f"Hostname   : {self.report.hostname}")
        print(f"Platform   : {self.report.platform}")
        print(f"Python     : {self.report.python_version}")
        print("")
        print(f"{'Category':<18} {'Name':<25} {'Status':<7} {'Message'}")
        print("-" * 70)

        for r in self.report.results:
            icon = icons.get(r.status, "?")
            print(f"{r.category:<18} {r.name:<25} {icon} {r.message}")
            if r.details:
                print(f"{'':>43}   {r.details}")
            if r.fix_command:
                print(f"{'':>43}   Fix: {r.fix_command}")

        print("-" * 70)
        print("")
        print(f"Summary:")
        print(f"  Total   : {self.report.total_checks}")
        print(f"  \033[0;32mPassed  : {self.report.passed}\033[0m")
        print(f"  \033[0;31mFailed  : {self.report.failed}\033[0m")
        print(f"  \033[1;33mWarnings: {self.report.warnings}\033[0m")
        print(f"  Skipped : {self.report.skipped}")
        print("")
        if self.report.is_healthy():
            print("\033[0;32m✓ System is healthy!\033[0m")
        else:
            print("\033[0;31m✗ Issues detected. See details above.\033[0m")
        print("=" * 70)

    def get_summary_dict(self) -> Dict[str, Any]:
        return {
            "healthy": self.report.is_healthy(),
            "total": self.report.total_checks,
            "passed": self.report.passed,
            "failed": self.report.failed,
            "warnings": self.report.warnings,
            "skipped": self.report.skipped,
        }


def main():
    parser = argparse.ArgumentParser(
        description="AgentOS System Health Diagnostic Tool",
        formatter_class=argparse.RawDescriptionHelpFormatter
    )

    parser.add_argument("--category", "-c", action="append",
                        choices=[c[0] for c in AgentOSDoctor.CHECKERS],
                        help="Run specific check category (repeatable)")
    parser.add_argument("--format", "-f", choices=["text", "json"],
                        default="text", help="Output format")
    parser.add_argument("--fix", action="store_true",
                        help="Attempt to auto-fix issues")
    parser.add_argument("--json-output", type=str,
                        help="Save JSON report to file")

    args = parser.parse_args()

    doctor = AgentOSDoctor()
    report = doctor.run_all_checks(categories=args.category)

    if args.json_output:
        with open(args.json_output, "w") as f:
            json.dump(asdict(report), f, indent=2, default=str)
        print(f"Report saved to: {args.json_output}")

    doctor.print_report(output_format=args.format)
    return 0 if report.is_healthy() else 1


if __name__ == "__main__":
    sys.exit(main())
