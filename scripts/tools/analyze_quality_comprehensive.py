#!/usr/bin/env python3
"""
AgentOS Comprehensive Code Quality Analyzer

全面的代码质量分析工具，支持多种编程语言：
- Python: 圈复杂度、重复代码检测
- C/C++: 基本复杂度分析
- Go: 基本复杂度分析  
- TypeScript: 基本复杂度分析

Usage:
    python analyze_quality_comprehensive.py --help
    python analyze_quality_comprehensive.py --full-scan
    python analyze_quality_comprehensive.py --output report.json
"""

import argparse
import ast
import os
import re
import sys
import json
from collections import defaultdict
from dataclasses import dataclass, field, asdict
from pathlib import Path
from typing import Dict, List, Optional, Set, Tuple, Any

# 生产级质量阈值
PRODUCTION_THRESHOLDS = {
    "python": {
        "duplicate_rate": 5.0,  # 重复率阈值 <5%
        "average_complexity": 5.0,  # 平均圈复杂度 <5
        "max_complexity": 15,  # 最大圈复杂度 <15
        "high_complexity_count": 10,  # 高复杂度函数数 <10
    },
    "c_cpp": {
        "duplicate_rate": 7.0,
        "average_complexity": 7.0,
        "max_complexity": 20,
        "high_complexity_count": 20,
    },
    "go": {
        "duplicate_rate": 5.0,
        "average_complexity": 5.0,
        "max_complexity": 15,
        "high_complexity_count": 15,
    },
    "typescript": {
        "duplicate_rate": 5.0,
        "average_complexity": 5.0,
        "max_complexity": 15,
        "high_complexity_count": 15,
    }
}

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
    language: str = "python"
    
    def to_dict(self):
        return {
            "name": self.name,
            "file": self.file_path,
            "line": self.start_line,
            "end_line": self.end_line,
            "complexity": self.cyclomatic_complexity,
            "line_count": self.line_count,
            "parameter_count": self.parameter_count,
            "language": self.language
        }

@dataclass
class CodeBlock:
    """重复代码块"""
    file_path: str
    start_line: int
    end_line: int
    code_hash: str
    content: str = ""

@dataclass
class QualityReport:
    """质量报告"""
    language: str
    total_files: int = 0
    total_lines: int = 0
    duplicate_lines: int = 0
    duplicate_rate: float = 0.0
    high_complexity_functions: List[FunctionInfo] = field(default_factory=list)
    average_complexity: float = 0.0
    max_complexity: int = 0
    total_functions: int = 0
    duplicate_blocks: Dict[str, List[CodeBlock]] = field(default_factory=dict)
    
    def to_dict(self):
        return {
            "language": self.language,
            "total_files": self.total_files,
            "total_lines": self.total_lines,
            "duplicate_lines": self.duplicate_lines,
            "duplicate_rate": round(self.duplicate_rate, 2),
            "average_complexity": round(self.average_complexity, 2),
            "max_complexity": self.max_complexity,
            "total_functions": self.total_functions,
            "high_complexity_functions": [f.to_dict() for f in self.high_complexity_functions],
            "duplicate_blocks_count": len(self.duplicate_blocks)
        }

class PythonComplexityAnalyzer(ast.NodeVisitor):
    """Python圈复杂度分析器"""
    
    def __init__(self):
        self.complexity = 1
        self.functions: List[FunctionInfo] = []
        self.current_file = ""
        self.current_lines = ""
        
    def analyze_file(self, file_path: Path) -> List[FunctionInfo]:
        """分析单个Python文件的复杂度"""
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
            print(f"⚠️  Error analyzing Python {file_path}: {e}")
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
            parameter_count=len(node.args.args),
            language="python"
        )
        
        self.functions.append(func_info)

class CComplexityAnalyzer:
    """C/C++圈复杂度分析器（基于简单启发式）"""
    
    def analyze_file(self, file_path: Path) -> List[FunctionInfo]:
        """分析C/C++文件的复杂度"""
        try:
            source = file_path.read_text(encoding='utf-8')
            lines = source.split('\n')
            
            functions = []
            in_function = False
            func_name = ""
            func_start = 0
            brace_count = 0
            complexity = 1
            
            for i, line in enumerate(lines, 1):
                line_stripped = line.strip()
                
                # 检测函数定义
                if not in_function and re.match(r'^\w+[\s\*]+\w+\s*\([^)]*\)\s*\{?', line_stripped):
                    if '{' in line_stripped:
                        in_function = True
                        brace_count = 1
                    else:
                        in_function = True
                        brace_count = 0
                    
                    # 提取函数名
                    match = re.search(r'\b(\w+)\s*\([^)]*\)', line_stripped)
                    if match:
                        func_name = match.group(1)
                    else:
                        func_name = f"anonymous_func_{i}"
                    
                    func_start = i
                    complexity = 1
                
                elif in_function:
                    if '{' in line_stripped:
                        brace_count += line_stripped.count('{')
                    if '}' in line_stripped:
                        brace_count -= line_stripped.count('}')
                    
                    # 计算复杂度增量
                    if re.search(r'\bif\s*\(|else\s*\{|while\s*\(|for\s*\(|case\s+.*:|default\s*:', line_stripped):
                        complexity += 1
                    elif '&&' in line_stripped or '||' in line_stripped:
                        complexity += 1
                    
                    # 函数结束
                    if brace_count <= 0:
                        func_end = i
                        func_lines = func_end - func_start + 1
                        
                        func_info = FunctionInfo(
                            name=func_name,
                            file_path=str(file_path),
                            start_line=func_start,
                            end_line=func_end,
                            cyclomatic_complexity=complexity,
                            line_count=func_lines,
                            parameter_count=0,  # 简化处理
                            language="c_cpp"
                        )
                        functions.append(func_info)
                        
                        in_function = False
                        func_name = ""
            
            return functions
            
        except Exception as e:
            print(f"⚠️  Error analyzing C/C++ {file_path}: {e}")
            return []

class CodeDuplicationDetector:
    """代码重复检测器（支持多种语言）"""
    
    def __init__(self, min_lines: int = 4):
        self.min_lines = min_lines
        self.code_blocks: Dict[str, List[CodeBlock]] = defaultdict(list)
    
    def scan_files(self, files: List[Path]) -> Dict[str, List[CodeBlock]]:
        """扫描文件列表检测重复代码"""
        for file_path in files:
            if self._should_ignore(file_path):
                continue
            
            self._analyze_file(file_path)
        
        return self.code_blocks
    
    def _should_ignore(self, file_path: Path) -> bool:
        """检查文件是否应该被忽略"""
        ignore_patterns = [
            '__pycache__', '.git', '.venv', 'node_modules',
            '.pyc', '.pyo', '.egg-info', 'dist', 'build',
            'test', 'tests', '__tests__'  # 可配置是否忽略测试文件
        ]
        return any(pattern in str(file_path).lower() for pattern in ignore_patterns)
    
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
            
            # 跳过空行
            if not line:
                continue
            
            # 跳过单行注释
            if line.startswith('//') or line.startswith('#'):
                continue
            
            # 跳过多行注释的开始/结束
            if line.startswith('/*') or line.startswith('*/'):
                continue
            
            # 去除行内注释
            if '//' in line:
                line = line.split('//')[0].strip()
            if '#' in line:
                line = line.split('#')[0].strip()
            if '/*' in line:
                line = line.split('/*')[0].strip()
            
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
    """综合代码质量分析器"""
    
    def __init__(self, project_root: Path):
        self.project_root = project_root
        self.python_analyzer = PythonComplexityAnalyzer()
        self.c_analyzer = CComplexityAnalyzer()
        self.duplication_detector = CodeDuplicationDetector(min_lines=5)
    
    def analyze_python(self) -> QualityReport:
        """分析Python代码质量"""
        print("🔍 分析Python代码...")
        
        # 查找Python文件
        python_files = list(self.project_root.rglob("*.py"))
        
        # 排除测试文件
        python_files = [f for f in python_files 
                       if not any(pattern in str(f) for pattern in ['/test', '/tests', '__test__'])]
        
        report = QualityReport(language="python")
        report.total_files = len(python_files)
        
        all_functions: List[FunctionInfo] = []
        total_lines = 0
        
        for py_file in python_files:
            try:
                lines = py_file.read_text(encoding='utf-8').split('\n')
                total_lines += len(lines)
                
                functions = self.python_analyzer.analyze_file(py_file)
                all_functions.extend(functions)
            except Exception as e:
                print(f"⚠️  跳过文件 {py_file}: {e}")
        
        report.total_lines = total_lines
        
        # 分析重复代码
        duplicates = self.duplication_detector.scan_files(python_files)
        dup_blocks = self.duplication_detector.get_duplicates()
        
        duplicate_lines = sum(
            (block.end_line - block.start_line + 1) * (len(blocks) - 1)
            for blocks in dup_blocks.values()
            for block in blocks
        )
        
        report.duplicate_lines = duplicate_lines
        report.duplicate_rate = (duplicate_lines / total_lines * 100) if total_lines > 0 else 0
        report.duplicate_blocks = dup_blocks
        
        # 计算复杂度统计
        if all_functions:
            report.total_functions = len(all_functions)
            report.average_complexity = sum(f.cyclomatic_complexity for f in all_functions) / len(all_functions)
            report.max_complexity = max(f.cyclomatic_complexity for f in all_functions)
            
            # 高复杂度函数（复杂度 >= 10）
            threshold = PRODUCTION_THRESHOLDS["python"]["max_complexity"]
            report.high_complexity_functions = [
                func for func in all_functions
                if func.cyclomatic_complexity >= threshold
            ]
        
        return report
    
    def analyze_c_cpp(self) -> QualityReport:
        """分析C/C++代码质量"""
        print("🔍 分析C/C++代码...")
        
        # 查找C/C++文件
        c_extensions = ['.c', '.h', '.cpp', '.hpp', '.cc', '.cxx']
        c_files = []
        for ext in c_extensions:
            c_files.extend(list(self.project_root.rglob(f"*{ext}")))
        
        # 排除测试文件
        c_files = [f for f in c_files 
                  if not any(pattern in str(f) for pattern in ['/test', '/tests', '__test__'])]
        
        report = QualityReport(language="c_cpp")
        report.total_files = len(c_files)
        
        all_functions: List[FunctionInfo] = []
        total_lines = 0
        
        for c_file in c_files:
            try:
                lines = c_file.read_text(encoding='utf-8').split('\n')
                total_lines += len(lines)
                
                functions = self.c_analyzer.analyze_file(c_file)
                all_functions.extend(functions)
            except Exception as e:
                print(f"⚠️  跳过文件 {c_file}: {e}")
        
        report.total_lines = total_lines
        
        # 分析重复代码
        duplicates = self.duplication_detector.scan_files(c_files)
        dup_blocks = self.duplication_detector.get_duplicates()
        
        duplicate_lines = sum(
            (block.end_line - block.start_line + 1) * (len(blocks) - 1)
            for blocks in dup_blocks.values()
            for block in blocks
        )
        
        report.duplicate_lines = duplicate_lines
        report.duplicate_rate = (duplicate_lines / total_lines * 100) if total_lines > 0 else 0
        report.duplicate_blocks = dup_blocks
        
        # 计算复杂度统计
        if all_functions:
            report.total_functions = len(all_functions)
            report.average_complexity = sum(f.cyclomatic_complexity for f in all_functions) / len(all_functions)
            report.max_complexity = max(f.cyclomatic_complexity for f in all_functions)
            
            # 高复杂度函数（复杂度 >= 15）
            threshold = PRODUCTION_THRESHOLDS["c_cpp"]["max_complexity"]
            report.high_complexity_functions = [
                func for func in all_functions
                if func.cyclomatic_complexity >= threshold
            ]
        
        return report
    
    def analyze_all(self) -> Dict[str, QualityReport]:
        """分析所有语言"""
        print("📊 开始全面代码质量分析...")
        print("="*70)
        
        reports = {}
        
        # Python分析
        python_report = self.analyze_python()
        reports["python"] = python_report
        
        print()
        
        # C/C++分析
        c_report = self.analyze_c_cpp()
        reports["c_cpp"] = c_report
        
        print()
        
        return reports
    
    def print_comprehensive_report(self, reports: Dict[str, QualityReport]):
        """打印综合报告"""
        print("\n" + "="*70)
        print("📊 AgentOS 综合代码质量分析报告")
        print("="*70)
        
        total_duplicate_lines = 0
        total_lines = 0
        all_high_complexity = []
        
        for lang, report in reports.items():
            print(f"\n🔤 {lang.upper()} 代码质量:")
            print(f"   文件数: {report.total_files}")
            print(f"   代码行数: {report.total_lines:,}")
            print(f"   重复率: {report.duplicate_rate:.2f}%")
            print(f"   平均复杂度: {report.average_complexity:.2f}")
            print(f"   最大复杂度: {report.max_complexity}")
            print(f"   高复杂度函数数: {len(report.high_complexity_functions)}")
            
            # 质量评估
            thresholds = PRODUCTION_THRESHOLDS.get(lang, PRODUCTION_THRESHOLDS["python"])
            
            if report.duplicate_rate <= thresholds["duplicate_rate"]:
                print(f"   重复率评估: ✅ 优秀 (≤{thresholds['duplicate_rate']}%)")
            else:
                print(f"   重复率评估: ❌ 需改进 (> {thresholds['duplicate_rate']}%)")
            
            if report.average_complexity <= thresholds["average_complexity"]:
                print(f"   平均复杂度评估: ✅ 优秀 (≤{thresholds['average_complexity']})")
            else:
                print(f"   平均复杂度评估: ❌ 需改进 (> {thresholds['average_complexity']})")
            
            if report.max_complexity <= thresholds["max_complexity"]:
                print(f"   最大复杂度评估: ✅ 优秀 (≤{thresholds['max_complexity']})")
            else:
                print(f"   最大复杂度评估: ❌ 需改进 (> {thresholds['max_complexity']})")
            
            if len(report.high_complexity_functions) <= thresholds["high_complexity_count"]:
                print(f"   高复杂度函数数评估: ✅ 优秀 (≤{thresholds['high_complexity_count']})")
            else:
                print(f"   高复杂度函数数评估: ❌ 需改进 (> {thresholds['high_complexity_count']})")
            
            total_duplicate_lines += report.duplicate_lines
            total_lines += report.total_lines
            all_high_complexity.extend(report.high_complexity_functions)
        
        # 总体统计
        print("\n" + "="*70)
        print("📈 总体统计:")
        print(f"   总代码行数: {total_lines:,}")
        overall_duplicate_rate = (total_duplicate_lines / total_lines * 100) if total_lines > 0 else 0
        print(f"   总体重复率: {overall_duplicate_rate:.2f}%")
        print(f"   总高复杂度函数数: {len(all_high_complexity)}")
        
        # 高复杂度函数Top 10
        if all_high_complexity:
            print(f"\n🔴 高复杂度函数 Top 10:")
            sorted_funcs = sorted(all_high_complexity, 
                                key=lambda x: x.cyclomatic_complexity, 
                                reverse=True)[:10]
            for func in sorted_funcs:
                print(f"   - {func.name} ({func.language}, 复杂度:{func.cyclomatic_complexity}, "
                      f"行数:{func.line_count}) @ {func.file_path}:{func.start_line}")
        
        print("\n" + "="*70)

def main():
    """主函数"""
    parser = argparse.ArgumentParser(
        description="AgentOS Comprehensive Code Quality Analyzer",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  # 全面分析
  python analyze_quality_comprehensive.py --full-scan
  
  # 输出JSON报告
  python analyze_quality_comprehensive.py --output report.json
  
  # 仅分析Python
  python analyze_quality_comprehensive.py --python-only
  
  # 仅分析C/C++
  python analyze_quality_comprehensive.py --c-only
        """
    )
    
    parser.add_argument(
        "--full-scan",
        action="store_true",
        help="全面分析所有语言"
    )
    parser.add_argument(
        "--python-only",
        action="store_true",
        help="仅分析Python代码"
    )
    parser.add_argument(
        "--c-only",
        action="store_true",
        help="仅分析C/C++代码"
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
    
    if not any([args.full_scan, args.python_only, args.c_only]):
        parser.print_help()
        sys.exit(1)
    
    project_root = Path(__file__).parent.parent
    
    analyzer = QualityAnalyzer(project_root)
    reports = {}
    
    if args.python_only:
        reports["python"] = analyzer.analyze_python()
    elif args.c_only:
        reports["c_cpp"] = analyzer.analyze_c_cpp()
    else:  # full-scan
        reports = analyzer.analyze_all()
    
    # 打印报告
    analyzer.print_comprehensive_report(reports)
    
    # 输出JSON报告
    if args.output:
        output_data = {
            "timestamp": datetime.datetime.now().isoformat(),
            "project_root": str(project_root),
            "reports": {lang: report.to_dict() for lang, report in reports.items()},
            "production_thresholds": PRODUCTION_THRESHOLDS
        }
        
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(
            json.dumps(output_data, indent=2, ensure_ascii=False),
            encoding='utf-8'
        )
        print(f"📄 报告已保存至 {args.output}")

if __name__ == "__main__":
    import datetime
    main()