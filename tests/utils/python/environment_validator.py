#!/usr/bin/env python3
"""
AgentOS 测试环境验证器

验证测试环境是否满足运行测试的所有要求。

Version: 2.0.0
Last updated: 2026-04-23
"""

import os
import sys
import platform
import subprocess
import shutil
from pathlib import Path
from typing import Dict, List, Tuple, Optional
from dataclasses import dataclass
from datetime import datetime


@dataclass
class CheckResult:
    """检查结果"""
    name: str
    passed: bool
    message: str
    details: Optional[str] = None


class EnvironmentValidator:
    """环境验证器"""

    def __init__(self, project_root: Path = None):
        self.project_root = project_root or Path(__file__).parent.parent
        self.results: List[CheckResult] = []

    def check_python_version(self, min_version: Tuple[int, int] = (3, 8)) -> CheckResult:
        """检查 Python 版本"""
        current = sys.version_info[:2]
        passed = current >= min_version
        return CheckResult(
            name="Python 版本",
            passed=passed,
            message=f"当前版本: {current[0]}.{current[1]}, 最低要求: {min_version[0]}.{min_version[1]}",
            details=sys.version
        )

    def check_module(self, module_name: str) -> CheckResult:
        """检查模块是否可用"""
        try:
            __import__(module_name)
            return CheckResult(
                name=f"模块 {module_name}",
                passed=True,
                message="已安装"
            )
        except ImportError:
            return CheckResult(
                name=f"模块 {module_name}",
                passed=False,
                message="未安装"
            )

    def check_command(self, command: str) -> CheckResult:
        """检查命令是否可用"""
        result = shutil.which(command)
        if result:
            return CheckResult(
                name=f"命令 {command}",
                passed=True,
                message=f"可用: {result}"
            )
        else:
            return CheckResult(
                name=f"命令 {command}",
                passed=False,
                message="不可用"
            )

    def check_directory(self, path: Path, must_exist: bool = True) -> CheckResult:
        """检查目录"""
        exists = path.exists()
        if must_exist:
            return CheckResult(
                name=f"目录 {path.name}",
                passed=exists,
                message="存在" if exists else "不存在",
                details=str(path)
            )
        else:
            return CheckResult(
                name=f"目录 {path.name}",
                passed=True,
                message="可选目录",
                details=str(path)
            )

    def check_file(self, path: Path, must_exist: bool = True) -> CheckResult:
        """检查文件"""
        exists = path.exists() and path.is_file()
        if must_exist:
            return CheckResult(
                name=f"文件 {path.name}",
                passed=exists,
                message="存在" if exists else "不存在",
                details=str(path)
            )
        else:
            return CheckResult(
                name=f"文件 {path.name}",
                passed=True,
                message="可选文件",
                details=str(path)
            )

    def check_write_permission(self, path: Path) -> CheckResult:
        """检查写入权限"""
        try:
            test_file = path / ".write_test"
            test_file.write_text("test")
            test_file.unlink()
            return CheckResult(
                name=f"写入权限 {path.name}",
                passed=True,
                message="可写入"
            )
        except Exception as e:
            return CheckResult(
                name=f"写入权限 {path.name}",
                passed=False,
                message=f"无写入权限: {e}"
            )

    def validate_all(self) -> List[CheckResult]:
        """执行所有验证"""
        self.results = []

        # Python 环境
        self.results.append(self.check_python_version())

        # 核心模块
        core_modules = [
            'pytest', 'json', 'pathlib', 'unittest.mock',
            'asyncio', 'threading', 'multiprocessing',
            'tempfile', 'shutil', 'os', 'sys'
        ]
        for module in core_modules:
            self.results.append(self.check_module(module))

        # 可选模块
        optional_modules = [
            'pytest_cov', 'pytest_asyncio', 'pytest_xdist',
            'pytest_timeout', 'pytest_benchmark', 'aiohttp',
            'httpx', 'requests', 'faker', 'hypothesis'
        ]
        for module in optional_modules:
            result = self.check_module(module)
            result.name = f"可选模块 {module}"
            self.results.append(result)

        # 命令
        commands = ['python', 'pip']
        if platform.system() != 'Windows':
            commands.extend(['gcc', 'make', 'cmake'])
        for cmd in commands:
            self.results.append(self.check_command(cmd))

        # 目录
        required_dirs = [
            self.project_root / "tests",
            self.project_root / "tests" / "unit",
            self.project_root / "tests" / "utils",
            self.project_root / "tests" / "fixtures",
        ]
        for dir_path in required_dirs:
            self.results.append(self.check_directory(dir_path))

        # 可选目录
        optional_dirs = [
            self.project_root / "tests" / "integration",
            self.project_root / "tests" / "security",
            self.project_root / "tests" / "benchmarks",
        ]
        for dir_path in optional_dirs:
            self.results.append(self.check_directory(dir_path, must_exist=False))

        # 配置文件
        config_files = [
            self.project_root / "tests" / "pytest.ini",
            self.project_root / "tests" / "conftest.py",
            self.project_root / "tests" / "requirements.txt",
        ]
        for file_path in config_files:
            self.results.append(self.check_file(file_path))

        # 写入权限
        self.results.append(self.check_write_permission(self.project_root / "tests"))

        return self.results

    def generate_report(self) -> str:
        """生成报告"""
        lines = []
        lines.append("=" * 60)
        lines.append("🔍 AgentOS 测试环境验证报告")
        lines.append("=" * 60)
        lines.append(f"时间: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}")
        lines.append(f"平台: {platform.system()} {platform.release()}")
        lines.append(f"Python: {sys.version.split()[0]}")
        lines.append("")

        # 按类别分组
        passed = sum(1 for r in self.results if r.passed)
        total = len(self.results)

        lines.append("📊 总体状态:")
        lines.append(f"  通过: {passed}/{total} ({passed/total*100:.1f}%)")
        lines.append("")

        lines.append("📋 详细结果:")
        for result in self.results:
            status = "✅" if result.passed else "❌"
            lines.append(f"  {status} {result.name}: {result.message}")

        lines.append("")
        lines.append("=" * 60)

        if passed == total:
            lines.append("✅ 环境验证通过，可以运行测试")
        else:
            lines.append("⚠️ 环境验证发现问题，请检查上述失败项")

        return '\n'.join(lines)

    def save_report(self, output_file: Path = None):
        """保存报告"""
        if output_file is None:
            output_file = self.project_root / "tests" / "environment_report.txt"

        report = self.generate_report()
        with open(output_file, 'w', encoding='utf-8') as f:
            f.write(report)

        return output_file


def main():
    """主函数"""
    print("🔍 正在验证测试环境...\n")

    validator = EnvironmentValidator()
    results = validator.validate_all()

    report = validator.generate_report()
    print(report)

    report_file = validator.save_report()
    print(f"\n📄 报告已保存到: {report_file}")

    passed = sum(1 for r in results if r.passed)
    total = len(results)

    return 0 if passed == total else 1


if __name__ == "__main__":
    sys.exit(main())
