#!/usr/bin/env python3
"""
AgentOS 测试运行脚本
提供统一的测试运行接口和错误诊断
"""

import os
import sys
import subprocess
import json
import time
from pathlib import Path
from typing import Dict, List, Optional


class SyntaxChecker:
    """Python 语法检查器"""

    def __init__(self, test_dir: Path):
        self.test_dir = test_dir

    def check_file(self, file_path: Path) -> tuple[bool, Optional[str]]:
        """
        检查单个文件的语法

        Returns:
            (是否通过, 错误信息)
        """
        try:
            with open(file_path, 'r', encoding='utf-8') as f:
                content = f.read()
            compile(content, str(file_path), 'exec')
            return True, None
        except SyntaxError as e:
            return False, f"语法错误 at line {e.lineno}: {e.msg}"
        except Exception as e:
            return False, str(e)

    def run(self) -> bool:
        """运行语法检查"""
        print("🔍 检查Python文件语法...")

        try:
            python_files = list(self.test_dir.rglob("*.py"))
            python_files = [f for f in python_files if f.name != "run_tests.py"]

            for file_path in python_files:
                success, error = self.check_file(file_path)
                if success:
                    print(f"  ✅ {file_path.relative_to(self.test_dir)}")
                else:
                    print(f"  ❌ {file_path.relative_to(self.test_dir)}: {error}")
                    return False

            return True
        except Exception as e:
            print(f"  ❌ 语法检查失败: {e}")
            return False


class DependencyChecker:
    """依赖项检查器"""

    REQUIRED_MODULES = [
        'pytest',
        'requests',
        'aiohttp',
        'pathlib',
        'json',
        'time',
        'unittest.mock'
    ]

    def check_module(self, module: str) -> bool:
        """检查单个模块是否可用"""
        try:
            if '.' in module:
                parts = module.split('.')
                parent = __import__(parts[0])
                for part in parts[1:]:
                    getattr(parent, part)
            else:
                __import__(module)
            return True
        except ImportError:
            return False

    def run(self) -> bool:
        """运行依赖检查"""
        print("🔍 检查Python依赖项...")

        missing_modules = []
        for module in self.REQUIRED_MODULES:
            if self.check_module(module):
                print(f"  ✅ {module}")
            else:
                print(f"  ❌ {module} - 缺失")
                missing_modules.append(module)

        return len(missing_modules) == 0


class TestDataChecker:
    """测试数据文件检查器"""

    REQUIRED_FILES = [
        "tasks/sample_tasks.json",
        "memories/sample_memories.json",
        "sessions/sample_sessions.json",
        "skills/sample_skills.json"
    ]

    def __init__(self, test_dir: Path):
        self.test_dir = test_dir
        self.data_dir = test_dir / "fixtures" / "data"

    def check_file(self, file_path: Path) -> tuple[bool, Optional[str]]:
        """
        检查单个数据文件

        Returns:
            (是否通过, 错误信息)
        """
        if not file_path.exists():
            return False, "文件不存在"

        try:
            with open(file_path, 'r', encoding='utf-8') as f:
                json.load(f)
            return True, None
        except json.JSONDecodeError as e:
            return False, f"JSON格式错误 - {e}"
        except Exception as e:
            return False, str(e)

    def run(self) -> bool:
        """运行测试数据检查"""
        print("🔍 检查测试数据文件...")

        if not self.data_dir.exists():
            print(f"  ❌ 测试数据目录不存在: {self.data_dir}")
            return False

        all_valid = True
        for file_path in self.REQUIRED_FILES:
            full_path = self.data_dir / file_path
            success, error = self.check_file(full_path)

            if success:
                print(f"  ✅ {file_path}")
            else:
                print(f"  ❌ {file_path}: {error}")
                all_valid = False

        return all_valid


class CMakeChecker:
    """CMake 配置文件检查器"""

    def __init__(self, test_dir: Path):
        self.test_dir = test_dir

    def check_file(self, cmake_file: Path) -> tuple[bool, Optional[str]]:
        """
        检查单个 CMake 文件

        Returns:
            (是否通过, 错误信息)
        """
        try:
            with open(cmake_file, 'r', encoding='utf-8') as f:
                content = f.read()

            if 'cmake_minimum_required' not in content:
                return False, "缺少cmake_minimum_required"

            return True, None
        except Exception as e:
            return False, str(e)

    def run(self) -> bool:
        """运行 CMake 配置检查"""
        print("🔍 检查CMake配置...")

        cmake_files = list(self.test_dir.rglob("CMakeLists.txt"))

        for cmake_file in cmake_files:
            success, error = self.check_file(cmake_file)

            if success:
                print(f"  ✅ {cmake_file.relative_to(self.test_dir)}")
            else:
                print(f"  ⚠️  {cmake_file.relative_to(self.test_dir)}: {error}")

        return True


class UnitTestRunner:
    """单元测试运行器"""

    def __init__(self, test_dir: Path):
        self.test_dir = test_dir

    def run(self) -> bool:
        """运行单元测试"""
        print("🧪 运行Python单元测试...")

        try:
            result = subprocess.run([
                sys.executable, '-m', 'pytest',
                'unit/agentos/toolkit/python/test_sdk.py',
                '-v', '--tb=short'
            ], cwd=self.test_dir, capture_output=True, text=True, timeout=60)

            if result.returncode == 0:
                print("  ✅ Python单元测试通过")
                return True
            else:
                print("  ❌ Python单元测试失败:")
                print(result.stdout)
                print(result.stderr)
                return False

        except subprocess.TimeoutExpired:
            print("  ❌ Python单元测试超时")
            return False
        except FileNotFoundError:
            print("  ❌ pytest未安装或不可用")
            return False
        except Exception as e:
            print(f"  ❌ 运行Python单元测试时出错: {e}")
            return False


class TestRunner:
    """测试运行器（主控类）"""

    def __init__(self):
        self.test_dir = Path(__file__).parent
        self.syntax_checker = SyntaxChecker(self.test_dir)
        self.dependency_checker = DependencyChecker()
        self.data_checker = TestDataChecker(self.test_dir)
        self.cmake_checker = CMakeChecker(self.test_dir)
        self.unit_runner = UnitTestRunner(self.test_dir)
        self.results = {}

    def run_all_checks(self) -> Dict:
        """
        运行所有检查（重构后的主方法）

        Returns:
            检查结果字典
        """
        checks = {
            "python_syntax": self.syntax_checker.run(),
            "dependencies": self.dependency_checker.run(),
            "test_data": self.data_checker.run(),
            "cmake_files": self.cmake_checker.run(),
            "unit_tests": self.unit_runner.run()
        }

        return checks

    def generate_report(self) -> Dict:
        """生成测试报告"""
        checks = self.run_all_checks()

        report = {
            "timestamp": time.strftime("%Y-%m-%d %H:%M:%S"),
            "checks": checks
        }

        passed = sum(1 for v in checks.values() if v)
        total = len(checks)
        report["summary"] = {
            "passed": passed,
            "total": total,
            "success_rate": passed / total * 100,
            "status": "PASS" if passed == total else "FAIL"
        }

        return report

    def print_report(self, report: Dict):
        """打印测试报告"""
        print("\n" + "="*60)
        print("📊 AgentOS 测试诊断报告")
        print("="*60)
        print(f"时间: {report['timestamp']}")
        print(f"状态: {report['summary']['status']}")
        print(f"通过率: {report['summary']['success_rate']:.1f}% ({report['summary']['passed']}/{report['summary']['total']})")

        print("\n📋 详细结果:")
        for check_name, result in report["checks"].items():
            status = "✅ 通过" if result else "❌ 失败"
            print(f"  {check_name}: {status}")

        if report["summary"]["status"] == "FAIL":
            print("\n🔧 修复建议:")
            if not report["checks"]["python_syntax"]:
                print("  - 修复Python文件中的语法错误")
            if not report["checks"]["dependencies"]:
                print("  - 安装缺失的Python依赖: pip install pytest requests aiohttp")
            if not report["checks"]["test_data"]:
                print("  - 检查并修复测试数据文件的JSON格式")
            if not report["checks"]["cmake_files"]:
                print("  - 检查CMakeLists.txt文件的语法和配置")
            if not report["checks"]["unit_tests"]:
                print("  - 修复单元测试中的错误")

        print("="*60)

    def save_report(self, report: Dict) -> Path:
        """保存报告到文件"""
        report_file = Path(__file__).parent / "test_report.json"
        with open(report_file, 'w', encoding='utf-8') as f:
            json.dump(report, f, indent=2, ensure_ascii=False)
        return report_file


def main():
    """主函数"""
    runner = TestRunner()

    print("🚀 开始AgentOS测试模块诊断...")

    report = runner.generate_report()
    runner.print_report(report)

    report_file = runner.save_report(report)
    print(f"\n📄 详细报告已保存到: {report_file}")

    return 0 if report["summary"]["status"] == "PASS" else 1


if __name__ == "__main__":
    sys.exit(main())
