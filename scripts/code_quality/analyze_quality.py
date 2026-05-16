#!/usr/bin/env python3
"""
AgentOS Code Quality Analyzer

代码质量分析工具：检测代码重复率和圈复杂度

Usage:
    python analyze_quality.py --help
    python analyze_quality.py --scan-python
    python analyze_quality.py --scan-go
    python analyze_quality.py --full-scan
"""

import argparse
import ast
import os
import re
import sys
from collections import defaultdict
from dataclasses import dataclass, field
from pathlib import Path
from typing import Dict, List, Optional, Set, Tuple


@dataclass
class CodeBlock:
    """代码块"""
    file_path: str
    start_line: int
    end_line: int
    code_hash: str
    content: str = ""


@dataclass
class FunctionInfo:
    """函数信息"""
    name: str
    file_path: str
    start_line: int
    end_line: int = 0
    cyclomatic_complexity: int = 1
    line_count: int = 0
    parameter_count: int = 0


@dataclass
class QualityReport:
    """质量报告"""
    total_files: int = 0
    total_lines: int = 0
    duplicate_lines: int = 0
    duplicate_rate: float = 0.0
    high_complexity_functions: List[FunctionInfo] = field(default_factory=list)
    average_complexity: float = 0.0
    max_complexity: int = 0


class PythonComplexityAnalyzer(ast.NodeVisitor):
    """Python圈复杂度分析器"""
    
    def __init__(self):
        self.complexity = 1
        self.functions: List[FunctionInfo] = []
        self.current_file = ""
        self.current_lines = ""
        
    def analyze_file(self, file_path: Path) -> List[FunctionInfo]:
        """分析单个文件的复杂度"""
        try:
            source = file_path.read_text(encoding='utf-8')
            tree = ast.parse(source)
            lines = source.split('\n')
            
            self.current_file = str(file_path)
            self.current_lines = lines
            
            for node in ast.walk(tree):
                if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)):
                    self._analyze_function(node, lines)
            
            return self.functions
            
        except Exception as e:
            print(f"⚠️  Error analyzing {file_path}: {e}")
            return []
    
    def _analyze_function(self, node: ast.FunctionDef, lines: List[str]):
        """分析单个函数的复杂度"""
        self.complexity = 1
        
        # 访问函数体计算复杂度
        for child in ast.walk(node):
            if isinstance(child, (
                ast.If, ast.While, ast.For, ast.ExceptHandler,
                ast.With, ast.Assert, ast.comprehension
            )):
                self.complexity += 1
            elif isinstance(child, ast.BoolOp):
                self.complexity += len(child.values) - 1
        
        func_lines = node.end_lineno - node.lineno + 1 if hasattr(node, 'end_lineno') else 20
        
        func_info = FunctionInfo(
            name=node.name,
            file_path=self.current_file,
            start_line=node.lineno,
            end_line=getattr(node, 'end_lineno', node.lineno + func_lines - 1),
            cyclomatic_complexity=self.complexity,
            line_count=func_lines,
            parameter_count=len(node.args.args)
        )
        
        self.functions.append(func_info)


class CodeDuplicationDetector:
    """代码重复检测器"""
    
    def __init__(self, min_lines: int = 4):
        self.min_lines = min_lines
        self.code_blocks: Dict[str, List[CodeBlock]] = defaultdict(list)
        
    def scan_directory(self, directory: Path, pattern: str = "*.py") -> Dict[str, List[CodeBlock]]:
        """扫描目录检测重复代码"""
        for file_path in directory.rglob(pattern):
            if self._should_ignore(file_path):
                continue
            
            self._analyze_file(file_path)
        
        return self.code_blocks
    
    def _should_ignore(self, file_path: Path) -> bool:
        """检查文件是否应该被忽略"""
        ignore_patterns = [
            '__pycache__', '.git', '.venv', 'node_modules',
            '.pyc', '.pyo', '.egg-info', 'dist', 'build'
        ]
        return any(pattern in str(file_path) for pattern in ignore_patterns)
    
    def _analyze_file(self, file_path: Path):
        """分析单个文件"""
        try:
            source = file_path.read_text(encoding='utf-8')
            lines = source.split('\n')
            
            # 提取代码块（忽略空白和注释）
            code_blocks = self._extract_code_blocks(lines, str(file_path))
            
            # 按哈希分组
            for block in code_blocks:
                self.code_blocks[block.code_hash].append(block)
                
        except Exception as e:
            print(f"⚠️  Error analyzing {file_path}: {e}")
    
    def _extract_code_blocks(self, lines: List[str], file_path: str) -> List[CodeBlock]:
        """提取代码块"""
        blocks = []
        
        # 滑动窗口提取代码块
        for i in range(len(lines) - self.min_lines + 1):
            block_lines = lines[i:i + self.min_lines]
            
            # 清理代码（去除空白和注释）
            cleaned = self._clean_code(block_lines)
            
            if len(cleaned) >= self.min_lines:
                code_hash = hash('\n'.join(cleaned))
                block = CodeBlock(
                    file_path=file_path,
                    start_line=i + 1,
                    end_line=i + self.min_lines,
                    code_hash=str(code_hash),
                    content='\n'.join(cleaned)
                )
                blocks.append(block)
        
        return blocks
    
    def _clean_code(self, lines: List[str]) -> List[str]:
        """清理代码（去除空白和注释）"""
        cleaned = []
        for line in lines:
            # 去除首尾空白
            line = line.strip()
            
            # 跳过空行和注释
            if not line or line.startswith('#'):
                continue
            
            # 去除行内注释
            if '#' in line:
                line = line.split('#')[0].strip()
            
            if line:
                cleaned.append(line)
        
        return cleaned
    
    def get_duplicates(self) -> Dict[str, List[CodeBlock]]:
        """获取重复代码块"""
        return {
            hash_code: blocks
            for hash_code, blocks in self.code_blocks.items()
            if len(blocks) > 1
        }


class QualityAnalyzer:
    """代码质量分析器"""
    
    def __init__(self, project_root: Path):
        self.project_root = project_root
        self.complexity_analyzer = PythonComplexityAnalyzer()
        self.duplication_detector = CodeDuplicationDetector(min_lines=5)
        
    def analyze_python(self) -> QualityReport:
        """分析Python代码质量"""
        report = QualityReport()
        all_functions: List[FunctionInfo] = []
        
        # 扫描Python文件
        python_files = list(self.project_root.rglob("*.py"))
        report.total_files = len(python_files)
        
        print(f"📊 分析 {len(python_files)} 个Python文件...")
        
        total_lines = 0
        for py_file in python_files:
            # 计算行数
            try:
                lines = py_file.read_text(encoding='utf-8').split('\n')
                total_lines += len(lines)
            except:
                pass
            
            # 分析复杂度
            functions = self.complexity_analyzer.analyze_file(py_file)
            all_functions.extend(functions)
        
        report.total_lines = total_lines
        
        # 分析重复代码
        print("🔍 检测重复代码...")
        duplicates = self.duplication_detector.scan_directory(self.project_root)
        dup_blocks = self.duplication_detector.get_duplicates()
        
        # 计算重复行数
        duplicate_lines = sum(
            (block.end_line - block.start_line + 1) * (len(blocks) - 1)
            for blocks in dup_blocks.values()
            for block in blocks
        )
        
        report.duplicate_lines = duplicate_lines
        report.duplicate_rate = (duplicate_lines / total_lines * 100) if total_lines > 0 else 0
        
        # 筛选高复杂度函数
        report.high_complexity_functions = [
            func for func in all_functions
            if func.cyclomatic_complexity >= 3
        ]
        
        if all_functions:
            report.average_complexity = sum(f.cyclomatic_complexity for f in all_functions) / len(all_functions)
            report.max_complexity = max(f.cyclomatic_complexity for f in all_functions)
        
        return report
    
    def print_report(self, report: QualityReport):
        """打印质量报告"""
        print("\n" + "="*70)
        print("📊 AgentOS 代码质量分析报告")
        print("="*70)
        
        print(f"\n📁 文件统计:")
        print(f"   总文件数: {report.total_files}")
        print(f"   总代码行数: {report.total_lines:,}")
        
        print(f"\n🔄 代码重复率:")
        print(f"   重复行数: {report.duplicate_lines:,}")
        print(f"   重复率: {report.duplicate_rate:.2f}%")
        
        if report.duplicate_rate < 5:
            print(f"   ✅ 优秀 (<5%)")
        elif report.duplicate_rate < 10:
            print(f"   ⚠️  可接受 (5-10%)")
        else:
            print(f"   ❌ 需改进 (>10%)")
        
        print(f"\n🔀 圈复杂度:")
        print(f"   平均复杂度: {report.average_complexity:.2f}")
        print(f"   最大复杂度: {report.max_complexity}")
        print(f"   高复杂度函数数: {len(report.high_complexity_functions)}")
        
        if report.average_complexity < 3:
            print(f"   ✅ 优秀 (<3)")
        elif report.average_complexity < 5:
            print(f"   ⚠️  可接受 (3-5)")
        else:
            print(f"   ❌ 需改进 (>5)")
        
        if report.high_complexity_functions:
            print(f"\n🔴 高复杂度函数 Top 10:")
            sorted_funcs = sorted(report.high_complexity_functions, 
                                key=lambda x: x.cyclomatic_complexity, 
                                reverse=True)[:10]
            for func in sorted_funcs:
                print(f"   - {func.name} (复杂度:{func.cyclomatic_complexity}, "
                      f"行数:{func.line_count}) @ {func.file_path}:{func.start_line}")
        
        print("\n" + "="*70)


def main():
    """主函数"""
    parser = argparse.ArgumentParser(
        description="AgentOS Code Quality Analyzer",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  # 分析Python代码
  python analyze_quality.py --scan-python
  
  # 完整分析
  python analyze_quality.py --full-scan
  
  # 输出JSON报告
  python analyze_quality.py --output report.json
        """
    )
    
    parser.add_argument(
        "--scan-python",
        action="store_true",
        help="分析Python代码质量"
    )
    parser.add_argument(
        "--full-scan",
        action="store_true",
        help="完整分析（Python + Go + C/C++）"
    )
    parser.add_argument(
        "--output",
        type=Path,
        help="输出报告路径"
    )
    parser.add_argument(
        "--verbose", "-v",
        action="store_true",
        help="详细模式"
    )
    
    args = parser.parse_args()
    
    if not any([args.scan_python, args.full_scan]):
        parser.print_help()
        sys.exit(1)
    
    project_root = Path(__file__).parent.parent
    
    analyzer = QualityAnalyzer(project_root)
    
    if args.scan_python or args.full_scan:
        report = analyzer.analyze_python()
        analyzer.print_report(report)
        
        if args.output:
            import json
            output_data = {
                "total_files": report.total_files,
                "total_lines": report.total_lines,
                "duplicate_lines": report.duplicate_lines,
                "duplicate_rate": report.duplicate_rate,
                "average_complexity": report.average_complexity,
                "max_complexity": report.max_complexity,
                "high_complexity_functions": [
                    {
                        "name": f.name,
                        "file": f.file_path,
                        "line": f.start_line,
                        "complexity": f.cyclomatic_complexity
                    }
                    for f in report.high_complexity_functions[:20]
                ]
            }
            
            args.output.parent.mkdir(parents=True, exist_ok=True)
            args.output.write_text(
                json.dumps(output_data, indent=2, ensure_ascii=False),
                encoding='utf-8'
            )
            print(f"📄 报告已保存至 {args.output}")


if __name__ == "__main__":
    main()
