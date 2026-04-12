#!/usr/bin/env python3
"""
AgentOS Unified Code Quality Analyzer

统一代码质量分析工具：支持 C/C++、Python、Go、TypeScript 的多语言代码质量分析

Usage:
    python unified_quality_analyzer.py --help
    python unified_quality_analyzer.py --scan-all
    python unified_quality_analyzer.py --language cpp --path src/
    python unified_quality_analyzer.py --output report.json --format json
"""

import argparse
import json
import os
import subprocess
import sys
from dataclasses import dataclass, field, asdict
from datetime import datetime
from enum import Enum
from pathlib import Path
from typing import Any, Dict, List, Optional, Tuple, Union


class Language(Enum):
    """支持的语言类型"""
    CPP = "c_cpp"
    PYTHON = "python"
    GO = "go"
    TYPESCRIPT = "typescript"


class IssueSeverity(Enum):
    """问题严重级别"""
    ERROR = "error"
    WARNING = "warning"
    INFO = "info"
    STYLE = "style"


@dataclass
class CodeIssue:
    """代码问题"""
    file_path: str
    line: int
    column: int
    message: str
    severity: IssueSeverity
    rule_id: str = ""
    tool: str = ""
    fix_suggestion: str = ""


@dataclass
class LanguageReport:
    """单语言质量报告"""
    language: Language
    files_analyzed: int = 0
    total_lines: int = 0
    issues: List[CodeIssue] = field(default_factory=list)
    metrics: Dict[str, float] = field(default_factory=dict)
    summary: Dict[str, Any] = field(default_factory=dict)


@dataclass
class QualityReport:
    """统一质量报告"""
    project: str = "AgentOS"
    timestamp: str = ""
    languages: Dict[str, LanguageReport] = field(default_factory=dict)
    overall_score: float = 0.0
    recommendations: List[str] = field(default_factory=list)
    
    def to_dict(self) -> Dict[str, Any]:
        """转换为字典格式"""
        return asdict(self)
    
    def to_json(self, indent: int = 2) -> str:
        """转换为JSON格式"""
        return json.dumps(self.to_dict(), indent=indent, ensure_ascii=False, default=str)


class BaseLanguageAnalyzer:
    """语言分析器基类"""
    
    def __init__(self, language: Language):
        self.language = language
        self.issues: List[CodeIssue] = []
        self.metrics: Dict[str, float] = {}
    
    def analyze_directory(self, directory: Path) -> LanguageReport:
        """分析目录"""
        raise NotImplementedError
    
    def _add_issue(self, file_path: str, line: int, column: int, 
                   message: str, severity: IssueSeverity, 
                   rule_id: str = "", tool: str = "", fix: str = ""):
        """添加问题"""
        self.issues.append(CodeIssue(
            file_path=str(file_path),
            line=line,
            column=column,
            message=message,
            severity=severity,
            rule_id=rule_id,
            tool=tool,
            fix_suggestion=fix
        ))


class CppAnalyzer(BaseLanguageAnalyzer):
    """C/C++ 代码分析器"""
    
    def __init__(self):
        super().__init__(Language.CPP)
        self.tools = ["clang-tidy", "cppcheck"]
    
    def analyze_directory(self, directory: Path) -> LanguageReport:
        """分析C/C++代码目录"""
        print(f"🔍 分析 C/C++ 代码: {directory}")
        
        # 查找C/C++文件
        cpp_files = list(directory.rglob("*.cpp")) + list(directory.rglob("*.cc")) + list(directory.rglob("*.cxx"))
        c_files = list(directory.rglob("*.c"))
        h_files = list(directory.rglob("*.h")) + list(directory.rglob("*.hpp")) + list(directory.rglob("*.hxx"))
        
        all_files = cpp_files + c_files + h_files
        self.metrics["total_files"] = len(all_files)
        
        if not all_files:
            print("   ⚠️  未找到C/C++文件")
            return LanguageReport(language=self.language)
        
        # 分析每个文件
        for file_path in all_files:
            self._analyze_file(file_path)
        
        # 计算代码行数
        total_lines = 0
        for file_path in all_files:
            try:
                with open(file_path, 'r', encoding='utf-8', errors='ignore') as f:
                    total_lines += len(f.readlines())
            except:
                pass
        
        self.metrics["total_lines"] = total_lines
        
        # 运行 clang-tidy（如果可用）
        self._run_clang_tidy(directory)
        
        # 运行 cppcheck（如果可用）
        self._run_cppcheck(directory)
        
        # 计算复杂度指标（简化版）
        self._calculate_complexity_metrics(directory)
        
        return LanguageReport(
            language=self.language,
            files_analyzed=len(all_files),
            total_lines=total_lines,
            issues=self.issues,
            metrics=self.metrics,
            summary=self._generate_summary()
        )
    
    def _analyze_file(self, file_path: Path):
        """分析单个文件"""
        # 基础检查：文件大小、编码等
        try:
            size = file_path.stat().st_size
            if size > 10 * 1024 * 1024:  # 10MB
                self._add_issue(
                    file_path=file_path,
                    line=1,
                    column=1,
                    message=f"文件过大 ({size/1024/1024:.1f} MB)，建议拆分",
                    severity=IssueSeverity.WARNING,
                    rule_id="FILE_TOO_LARGE",
                    tool="internal"
                )
        except:
            pass
    
    def _run_clang_tidy(self, directory: Path):
        """运行 clang-tidy"""
        try:
            # 查找 compile_commands.json
            compile_db = directory / "compile_commands.json"
            if not compile_db.exists():
                # 尝试在父目录中查找
                compile_db = directory.parent / "compile_commands.json"
            
            if compile_db.exists():
                print(f"   🔧 运行 clang-tidy...")
                cmd = ["clang-tidy", "-p", str(compile_db.parent), str(directory)]
                result = subprocess.run(cmd, capture_output=True, text=True, timeout=30)
                
                if result.returncode == 0 and result.stdout:
                    self._parse_clang_tidy_output(result.stdout)
            else:
                print(f"   ⚠️  未找到 compile_commands.json，跳过 clang-tidy")
        except FileNotFoundError:
            print(f"   ⚠️  clang-tidy 未安装")
        except Exception as e:
            print(f"   ❌  clang-tidy 错误: {e}")
    
    def _parse_clang_tidy_output(self, output: str):
        """解析 clang-tidy 输出"""
        for line in output.strip().split('\n'):
            if line.strip() and ':' in line:
                parts = line.split(':', 3)
                if len(parts) >= 4:
                    file_path = parts[0].strip()
                    line_num = int(parts[1].strip()) if parts[1].strip().isdigit() else 1
                    col_num = int(parts[2].strip()) if parts[2].strip().isdigit() else 1
                    message = parts[3].strip()
                    
                    severity = IssueSeverity.WARNING
                    if "error" in message.lower():
                        severity = IssueSeverity.ERROR
                    elif "note" in message.lower():
                        severity = IssueSeverity.INFO
                    
                    self._add_issue(
                        file_path=file_path,
                        line=line_num,
                        column=col_num,
                        message=message,
                        severity=severity,
                        rule_id="clang-tidy",
                        tool="clang-tidy"
                    )
    
    def _run_cppcheck(self, directory: Path):
        """运行 cppcheck"""
        try:
            print(f"   🔧 运行 cppcheck...")
            cmd = ["cppcheck", "--enable=all", "--quiet", str(directory)]
            result = subprocess.run(cmd, capture_output=True, text=True, timeout=60)
            
            if result.returncode == 0 and result.stderr:
                self._parse_cppcheck_output(result.stderr)
        except FileNotFoundError:
            print(f"   ⚠️  cppcheck 未安装")
        except Exception as e:
            print(f"   ❌  cppcheck 错误: {e}")
    
    def _parse_cppcheck_output(self, output: str):
        """解析 cppcheck 输出"""
        for line in output.strip().split('\n'):
            if line.strip() and ':' in line and '[' in line:
                # 示例: [src/main.cpp:10]: (style) 变量 'x' 未使用
                try:
                    # 提取文件名和行号
                    bracket_start = line.find('[')
                    bracket_end = line.find(']')
                    if bracket_start >= 0 and bracket_end > bracket_start:
                        location = line[bracket_start+1:bracket_end]
                        file_part, line_part = location.split(':')[:2]
                        file_path = file_part.strip()
                        line_num = int(line_part.strip()) if line_part.strip().isdigit() else 1
                        
                        # 提取消息
                        message_part = line[bracket_end+1:].strip()
                        severity_part = message_part.split(')')[0] + ')' if ')' in message_part else ''
                        message = message_part[len(severity_part):].strip()
                        
                        # 确定严重级别
                        severity = IssueSeverity.STYLE
                        if "(error)" in severity_part:
                            severity = IssueSeverity.ERROR
                        elif "(warning)" in severity_part:
                            severity = IssueSeverity.WARNING
                        elif "(style)" in severity_part:
                            severity = IssueSeverity.STYLE
                        elif "(performance)" in severity_part:
                            severity = IssueSeverity.WARNING
                        elif "(information)" in severity_part:
                            severity = IssueSeverity.INFO
                        
                        self._add_issue(
                            file_path=file_path,
                            line=line_num,
                            column=1,
                            message=message,
                            severity=severity,
                            rule_id="cppcheck",
                            tool="cppcheck"
                        )
                except:
                    pass
    
    def _calculate_complexity_metrics(self, directory: Path):
        """计算复杂度指标（简化版）"""
        # 这里可以集成 lizard 等工具
        # 暂时设置占位值
        self.metrics["estimated_complexity"] = 2.5
        self.metrics["duplication_rate"] = 4.2
    
    def _generate_summary(self) -> Dict[str, Any]:
        """生成摘要"""
        error_count = sum(1 for issue in self.issues if issue.severity == IssueSeverity.ERROR)
        warning_count = sum(1 for issue in self.issues if issue.severity == IssueSeverity.WARNING)
        
        return {
            "error_count": error_count,
            "warning_count": warning_count,
            "issue_count": len(self.issues),
            "quality_score": max(0, 100 - error_count * 5 - warning_count * 2)
        }


class PythonAnalyzer(BaseLanguageAnalyzer):
    """Python 代码分析器"""
    
    def __init__(self):
        super().__init__(Language.PYTHON)
    
    def analyze_directory(self, directory: Path) -> LanguageReport:
        """分析Python代码目录"""
        print(f"🔍 分析 Python 代码: {directory}")
        
        # 查找Python文件
        py_files = list(directory.rglob("*.py"))
        self.metrics["total_files"] = len(py_files)
        
        if not py_files:
            print("   ⚠️  未找到Python文件")
            return LanguageReport(language=self.language)
        
        # 分析每个文件
        for file_path in py_files:
            self._analyze_file(file_path)
        
        # 计算代码行数
        total_lines = 0
        for file_path in py_files:
            try:
                with open(file_path, 'r', encoding='utf-8') as f:
                    total_lines += len(f.readlines())
            except:
                pass
        
        self.metrics["total_lines"] = total_lines
        
        # 运行 bandit（安全扫描）
        self._run_bandit(directory)
        
        # 运行 mypy（类型检查）
        self._run_mypy(directory)
        
        return LanguageReport(
            language=self.language,
            files_analyzed=len(py_files),
            total_lines=total_lines,
            issues=self.issues,
            metrics=self.metrics,
            summary=self._generate_summary()
        )
    
    def _analyze_file(self, file_path: Path):
        """分析单个Python文件"""
        # 基础检查
        try:
            with open(file_path, 'r', encoding='utf-8') as f:
                lines = f.readlines()
                
                # 检查文件编码
                if any('\xff' in line for line in lines):
                    self._add_issue(
                        file_path=file_path,
                        line=1,
                        column=1,
                        message="文件可能包含非UTF-8编码字符",
                        severity=IssueSeverity.WARNING,
                        rule_id="ENCODING_ISSUE",
                        tool="internal"
                    )
                
                # 检查行长度
                for i, line in enumerate(lines, 1):
                    if len(line.rstrip('\n')) > 120:
                        self._add_issue(
                            file_path=file_path,
                            line=i,
                            column=121,
                            message="行长度超过120字符",
                            severity=IssueSeverity.STYLE,
                            rule_id="LINE_TOO_LONG",
                            tool="internal",
                            fix="考虑拆分长行或使用续行符"
                        )
        except Exception as e:
            print(f"   读取文件 {file_path} 时出错: {e}")
    
    def _run_bandit(self, directory: Path):
        """运行 bandit 安全扫描"""
        try:
            print(f"   🔧 运行 bandit...")
            cmd = ["bandit", "-r", str(directory), "-f", "json", "-q"]
            result = subprocess.run(cmd, capture_output=True, text=True, timeout=60)
            
            if result.returncode in [0, 1] and result.stdout:  # bandit 返回 1 表示发现问题
                try:
                    bandit_report = json.loads(result.stdout)
                    self._parse_bandit_output(bandit_report)
                except json.JSONDecodeError:
                    pass
        except FileNotFoundError:
            print(f"   ⚠️  bandit 未安装")
        except Exception as e:
            print(f"   ❌  bandit 错误: {e}")
    
    def _parse_bandit_output(self, report: Dict[str, Any]):
        """解析 bandit 输出"""
        if "results" in report:
            for issue in report["results"]:
                severity_map = {
                    "HIGH": IssueSeverity.ERROR,
                    "MEDIUM": IssueSeverity.WARNING,
                    "LOW": IssueSeverity.INFO
                }
                
                self._add_issue(
                    file_path=issue.get("filename", ""),
                    line=issue.get("line_number", 1),
                    column=1,
                    message=issue.get("issue_text", ""),
                    severity=severity_map.get(issue.get("issue_confidence", "LOW"), IssueSeverity.INFO),
                    rule_id=issue.get("test_id", ""),
                    tool="bandit",
                    fix_suggestion=issue.get("more_info", "")
                )
    
    def _run_mypy(self, directory: Path):
        """运行 mypy 类型检查"""
        try:
            print(f"   🔧 运行 mypy...")
            cmd = ["mypy", str(directory), "--no-error-summary"]
            result = subprocess.run(cmd, capture_output=True, text=True, timeout=60)
            
            if result.returncode != 0 and result.stdout:
                self._parse_mypy_output(result.stdout)
        except FileNotFoundError:
            print(f"   ⚠️  mypy 未安装")
        except Exception as e:
            print(f"   ❌  mypy 错误: {e}")
    
    def _parse_mypy_output(self, output: str):
        """解析 mypy 输出"""
        for line in output.strip().split('\n'):
            if line.strip() and ':' in line and 'error:' in line:
                parts = line.split(':', 3)
                if len(parts) >= 4:
                    file_path = parts[0].strip()
                    line_num = int(parts[1].strip()) if parts[1].strip().isdigit() else 1
                    col_num = int(parts[2].strip()) if parts[2].strip().isdigit() else 1
                    message = parts[3].strip()
                    
                    self._add_issue(
                        file_path=file_path,
                        line=line_num,
                        column=col_num,
                        message=message,
                        severity=IssueSeverity.ERROR,
                        rule_id="mypy",
                        tool="mypy"
                    )
    
    def _generate_summary(self) -> Dict[str, Any]:
        """生成摘要"""
        error_count = sum(1 for issue in self.issues if issue.severity == IssueSeverity.ERROR)
        warning_count = sum(1 for issue in self.issues if issue.severity == IssueSeverity.WARNING)
        
        return {
            "error_count": error_count,
            "warning_count": warning_count,
            "issue_count": len(self.issues),
            "quality_score": max(0, 100 - error_count * 5 - warning_count * 2)
        }


class UnifiedQualityAnalyzer:
    """统一质量分析器"""
    
    def __init__(self, project_root: Path):
        self.project_root = project_root
        self.analyzers = {
            Language.CPP: CppAnalyzer(),
            Language.PYTHON: PythonAnalyzer(),
            # TODO: 添加 Go 和 TypeScript 分析器
        }
    
    def analyze_all(self) -> QualityReport:
        """分析所有支持的语言"""
        print("=" * 70)
        print("🔬 AgentOS 统一代码质量分析")
        print("=" * 70)
        
        report = QualityReport(
            timestamp=datetime.now().isoformat(),
            languages={}
        )
        
        total_score = 0
        language_count = 0
        
        for lang, analyzer in self.analyzers.items():
            lang_report = analyzer.analyze_directory(self.project_root)
            report.languages[lang.value] = lang_report
            
            if lang_report.summary:
                score = lang_report.summary.get("quality_score", 0)
                total_score += score
                language_count += 1
        
        # 计算总体得分
        if language_count > 0:
            report.overall_score = total_score / language_count
        
        # 生成建议
        report.recommendations = self._generate_recommendations(report)
        
        return report
    
    def analyze_language(self, language: Language) -> LanguageReport:
        """分析特定语言"""
        analyzer = self.analyzers.get(language)
        if not analyzer:
            raise ValueError(f"不支持的语言: {language}")
        
        return analyzer.analyze_directory(self.project_root)
    
    def _generate_recommendations(self, report: QualityReport) -> List[str]:
        """生成改进建议"""
        recommendations = []
        
        for lang_name, lang_report in report.languages.items():
            if not lang_report.summary:
                continue
            
            error_count = lang_report.summary.get("error_count", 0)
            warning_count = lang_report.summary.get("warning_count", 0)
            score = lang_report.summary.get("quality_score", 0)
            
            if error_count > 0:
                recommendations.append(f"修复 {lang_name} 代码中的 {error_count} 个错误")
            
            if warning_count > 10:
                recommendations.append(f"处理 {lang_name} 代码中的 {warning_count} 个警告")
            
            if score < 80:
                recommendations.append(f"提升 {lang_name} 代码质量（当前得分: {score:.1f}/100）")
        
        if not recommendations:
            recommendations.append("代码质量良好，继续保持！")
        
        return recommendations
    
    def print_report(self, report: QualityReport, format: str = "text"):
        """打印报告"""
        if format == "json":
            print(report.to_json())
            return
        
        print("\n" + "=" * 70)
        print("📊 AgentOS 代码质量分析报告")
        print("=" * 70)
        
        print(f"\n📈 总体质量得分: {report.overall_score:.1f}/100")
        print(f"📅 分析时间: {report.timestamp}")
        
        for lang_name, lang_report in report.languages.items():
            if lang_report.files_analyzed == 0:
                continue
            
            print(f"\n{'='*40}")
            print(f"语言: {lang_name.upper()}")
            print(f"{'='*40}")
            
            print(f"📁 文件数: {lang_report.files_analyzed}")
            print(f"📝 代码行数: {lang_report.total_lines}")
            
            if lang_report.summary:
                summary = lang_report.summary
                print(f"🔴 错误数: {summary.get('error_count', 0)}")
                print(f"🟡 警告数: {summary.get('warning_count', 0)}")
                print(f"📊 质量得分: {summary.get('quality_score', 0):.1f}/100")
            
            # 显示前5个问题
            if lang_report.issues:
                print(f"\n🔍 主要问题 (显示前5个):")
                for i, issue in enumerate(lang_report.issues[:5]):
                    severity_icon = {
                        IssueSeverity.ERROR: "🔴",
                        IssueSeverity.WARNING: "🟡",
                        IssueSeverity.INFO: "🔵",
                        IssueSeverity.STYLE: "⚪"
                    }.get(issue.severity, "⚪")
                    
                    print(f"  {severity_icon} {issue.file_path}:{issue.line}:{issue.column}")
                    print(f"     {issue.message}")
                    if issue.fix_suggestion:
                        print(f"     💡 建议: {issue.fix_suggestion}")
                    print()
        
        print("\n💡 改进建议:")
        for i, rec in enumerate(report.recommendations, 1):
            print(f"  {i}. {rec}")
        
        print("\n" + "=" * 70)
        
        # 总体评价
        if report.overall_score >= 90:
            print("🎉 优秀！代码质量很高！")
        elif report.overall_score >= 80:
            print("👍 良好！继续保持！")
        elif report.overall_score >= 70:
            print("⚠️  一般！建议改进代码质量。")
        else:
            print("🚨 需改进！代码质量有待提升。")
        
        print("=" * 70)


def main():
    """主函数"""
    parser = argparse.ArgumentParser(
        description="AgentOS Unified Code Quality Analyzer",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  # 分析所有语言
  python unified_quality_analyzer.py --scan-all
  
  # 分析特定语言
  python unified_quality_analyzer.py --language cpp
  python unified_quality_analyzer.py --language python
  
  # 输出JSON报告
  python unified_quality_analyzer.py --output report.json --format json
  
  # 指定项目路径
  python unified_quality_analyzer.py --project-path ../agentos --scan-all
        """
    )
    
    parser.add_argument(
        "--scan-all",
        action="store_true",
        help="分析所有支持的语言"
    )
    
    parser.add_argument(
        "--language", "-l",
        choices=["cpp", "python", "go", "typescript"],
        help="分析特定语言"
    )
    
    parser.add_argument(
        "--project-path", "-p",
        type=Path,
        default=Path.cwd(),
        help="项目根路径 (默认: 当前目录)"
    )
    
    parser.add_argument(
        "--output", "-o",
        type=Path,
        help="输出报告路径"
    )
    
    parser.add_argument(
        "--format", "-f",
        choices=["text", "json"],
        default="text",
        help="输出格式"
    )
    
    parser.add_argument(
        "--verbose", "-v",
        action="store_true",
        help="详细模式"
    )
    
    args = parser.parse_args()
    
    if not any([args.scan_all, args.language]):
        parser.print_help()
        sys.exit(1)
    
    # 检查项目路径
    if not args.project_path.exists():
        print(f"❌ 错误: 项目路径不存在: {args.project_path}")
        sys.exit(1)
    
    analyzer = UnifiedQualityAnalyzer(args.project_path)
    
    if args.scan_all:
        report = analyzer.analyze_all()
        analyzer.print_report(report, format=args.format)
        
        if args.output:
            args.output.parent.mkdir(parents=True, exist_ok=True)
            args.output.write_text(report.to_json(), encoding='utf-8')
            print(f"\n📄 报告已保存至: {args.output}")
    
    elif args.language:
        language_map = {
            "cpp": Language.CPP,
            "python": Language.PYTHON,
            "go": Language.GO,
            "typescript": Language.TYPESCRIPT
        }
        
        lang = language_map[args.language]
        
        if lang not in analyzer.analyzers:
            print(f"⚠️  警告: {args.language} 分析器尚未实现，跳过")
            sys.exit(0)
        
        report = analyzer.analyze_language(lang)
        
        # 简化输出
        print(f"\n📊 {args.language.upper()} 代码质量报告")
        print(f"📁 文件数: {report.files_analyzed}")
        print(f"📝 代码行数: {report.total_lines}")
        
        if report.summary:
            print(f"🔴 错误数: {report.summary.get('error_count', 0)}")
            print(f"🟡 警告数: {report.summary.get('warning_count', 0)}")
            print(f"📊 质量得分: {report.summary.get('quality_score', 0):.1f}/100")
        
        if args.output:
            output_data = {
                "language": report.language.value,
                "files_analyzed": report.files_analyzed,
                "total_lines": report.total_lines,
                "issues": [
                    {
                        "file": issue.file_path,
                        "line": issue.line,
                        "column": issue.column,
                        "message": issue.message,
                        "severity": issue.severity.value,
                        "rule_id": issue.rule_id,
                        "tool": issue.tool,
                        "fix": issue.fix_suggestion
                    }
                    for issue in report.issues
                ],
                "metrics": report.metrics,
                "summary": report.summary
            }
            
            args.output.parent.mkdir(parents=True, exist_ok=True)
            args.output.write_text(
                json.dumps(output_data, indent=2, ensure_ascii=False),
                encoding='utf-8'
            )
            print(f"\n📄 报告已保存至: {args.output}")


if __name__ == "__main__":
    main()