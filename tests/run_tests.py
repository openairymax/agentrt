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

class TestRunner:
    """测试运行器"""
    
    def __init__(self):
        self.test_dir = Path(__file__).parent
        self.results = {}
        
    def run_python_syntax_check(self) -> bool:
        """运行Python语法检查"""
        print("🔍 检查Python文件语法...")
        
        try:
            # 使用内置的compile函数检查语法
            python_files = list(self.test_dir.rglob("*.py"))
            python_files = [f for f in python_files if f.name != "run_tests.py"]
            
            for file_path in python_files:
                try:
                    with open(file_path, 'r', encoding='utf-8') as f:
                        content = f.read()
                    compile(content, str(file_path), 'exec')
                    print(f"  ✅ {file_path.relative_to(self.test_dir)}")
                except SyntaxError as e:
                    print(f"  ❌ {file_path.relative_to(self.test_dir)}: 语法错误 at line {e.lineno}: {e.msg}")
                    return False
                except Exception as e:
                    print(f"  ⚠️  {file_path.relative_to(self.test_dir)}: {e}")
                    
            return True
        except Exception as e:
            print(f"  ❌ 语法检查失败: {e}")
            return False
    
    def check_dependencies(self) -> bool:
        """检查依赖项"""
        print("🔍 检查Python依赖项...")
        
        required_modules = [
            'pytest',
            'requests',
            'aiohttp',
            'pathlib',
            'json',
            'time',
            'unittest.mock'
        ]
        
        missing_modules = []
        for module in required_modules:
            try:
                if '.' in module:
                    # 处理子模块
                    parts = module.split('.')
                    parent = __import__(parts[0])
                    for part in parts[1:]:
                        getattr(parent, part)
                else:
                    __import__(module)
                print(f"  ✅ {module}")
            except ImportError:
                print(f"  ❌ {module} - 缺失")
                missing_modules.append(module)
        
        return len(missing_modules) == 0
    
    def check_test_data(self) -> bool:
        """检查测试数据文件"""
        print("🔍 检查测试数据文件...")
        
        data_dir = self.test_dir / "fixtures" / "data"
        if not data_dir.exists():
            print(f"  ❌ 测试数据目录不存在: {data_dir}")
            return False
        
        required_files = [
            "tasks/sample_tasks.json",
            "memories/sample_memories.json", 
            "sessions/sample_sessions.json",
            "skills/sample_skills.json"
        ]
        
        all_valid = True
        for file_path in required_files:
            full_path = data_dir / file_path
            if not full_path.exists():
                print(f"  ❌ 缺失文件: {file_path}")
                all_valid = False
            else:
                try:
                    with open(full_path, 'r', encoding='utf-8') as f:
                        json.load(f)
                    print(f"  ✅ {file_path}")
                except json.JSONDecodeError as e:
                    print(f"  ❌ {file_path}: JSON格式错误 - {e}")
                    all_valid = False
                except Exception as e:
                    print(f"  ❌ {file_path}: {e}")
                    all_valid = False
        
        return all_valid
    
    def check_cmake_files(self) -> bool:
        """检查CMake配置文件"""
        print("🔍 检查CMake配置...")
        
        cmake_files = list(self.test_dir.rglob("CMakeLists.txt"))
        
        for cmake_file in cmake_files:
            try:
                with open(cmake_file, 'r', encoding='utf-8') as f:
                    content = f.read()
                
                # 基本语法检查
                if 'cmake_minimum_required' not in content:
                    print(f"  ⚠️  {cmake_file.relative_to(self.test_dir)}: 缺少cmake_minimum_required")
                
                print(f"  ✅ {cmake_file.relative_to(self.test_dir)}")
            except Exception as e:
                print(f"  ❌ {cmake_file.relative_to(self.test_dir)}: {e}")
                return False
        
        return True
    
    def run_unit_tests(self) -> bool:
        """运行单元测试"""
        print("🧪 运行Python单元测试...")
        
        try:
            # 尝试运行pytest
            result = subprocess.run([
                sys.executable, '-m', 'pytest', 
                'unit/sdk/python/test_sdk.py',
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
    
    def generate_report(self) -> Dict:
        """生成测试报告"""
        report = {
            "timestamp": time.strftime("%Y-%m-%d %H:%M:%S"),
            "checks": {
                "python_syntax": self.run_python_syntax_check(),
                "dependencies": self.check_dependencies(),
                "test_data": self.check_test_data(),
                "cmake_files": self.check_cmake_files(),
                "unit_tests": self.run_unit_tests()
            }
        }
        
        # 计算总体状态
        passed = sum(1 for v in report["checks"].values() if v)
        total = len(report["checks"])
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

def main():
    """主函数"""
    runner = TestRunner()
    
    print("🚀 开始AgentOS测试模块诊断...")
    
    report = runner.generate_report()
    runner.print_report(report)
    
    # 保存报告到文件
    report_file = Path(__file__).parent / "test_report.json"
    with open(report_file, 'w', encoding='utf-8') as f:
        json.dump(report, f, indent=2, ensure_ascii=False)
    
    print(f"\n📄 详细报告已保存到: {report_file}")
    
    return 0 if report["summary"]["status"] == "PASS" else 1

if __name__ == "__main__":
    sys.exit(main())