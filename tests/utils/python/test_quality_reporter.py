#!/usr/bin/env python3
"""
AgentOS 测试质量报告工具

生成详细的测试质量报告，包括：
- 测试覆盖率统计
- 测试文件分布
- 测试用例数量
- 测试健康度评估
- 趋势分析
- 多格式输出 (JSON, HTML, Markdown)

Version: 2.0.0
Last updated: 2026-04-23
"""

import os
import sys
import json
import re
from pathlib import Path
from typing import Dict, List, Optional, Any
from datetime import datetime, timedelta
from dataclasses import dataclass, field, asdict
from collections import defaultdict


class TestQualityReporter:
    """测试质量报告生成器"""

    def __init__(self, tests_dir: Path):
        self.tests_dir = tests_dir
        self.stats = {
            'total_files': 0,
            'total_test_cases': 0,
            'c_tests': 0,
            'python_tests': 0,
            'unit_tests': 0,
            'integration_tests': 0,
            'security_tests': 0,
            'benchmark_tests': 0,
        }

    def count_test_files(self) -> Dict:
        """统计测试文件数量"""
        for file_path in self.tests_dir.rglob('test_*'):
            if file_path.is_file():
                self.stats['total_files'] += 1
                if file_path.suffix == '.c':
                    self.stats['c_tests'] += 1
                elif file_path.suffix == '.py':
                    self.stats['python_tests'] += 1

        # 按类型统计
        unit_dir = self.tests_dir / 'unit'
        if unit_dir.exists():
            self.stats['unit_tests'] = len(list(unit_dir.rglob('test_*')))

        integration_dir = self.tests_dir / 'integration'
        if integration_dir.exists():
            self.stats['integration_tests'] = len(list(integration_dir.rglob('test_*')))

        security_dir = self.tests_dir / 'security'
        if security_dir.exists():
            self.stats['security_tests'] = len(list(security_dir.rglob('test_*')))

        benchmark_dir = self.tests_dir / 'benchmarks'
        if benchmark_dir.exists():
            self.stats['benchmark_tests'] = len(list(benchmark_dir.rglob('*benchmark*')))

        return self.stats

    def estimate_test_cases(self) -> int:
        """估算测试用例数量"""
        count = 0

        for file_path in self.tests_dir.rglob('test_*'):
            if not file_path.is_file():
                continue

            try:
                with open(file_path, 'r', encoding='utf-8') as f:
                    content = f.read()

                # 统计测试函数
                if file_path.suffix == '.py':
                    # Python: 统计 def test_ 开头的方法
                    count += content.count('def test_')
                elif file_path.suffix == '.c':
                    # C: 统计 TEST 宏
                    count += content.count('TEST(')
                    count += content.count('TEST_F(')

            except Exception:
                pass

        self.stats['total_test_cases'] = count
        return count

    def calculate_health_score(self) -> float:
        """计算测试健康度评分 (0-100)"""
        score = 0.0

        # 文件覆盖度 (40分)
        if self.stats['total_files'] >= 100:
            score += 40
        elif self.stats['total_files'] >= 50:
            score += 30
        elif self.stats['total_files'] >= 20:
            score += 20

        # 语言分布 (20分)
        if self.stats['c_tests'] > 0 and self.stats['python_tests'] > 0:
            score += 20
        elif self.stats['c_tests'] > 0 or self.stats['python_tests'] > 0:
            score += 10

        # 测试类型分布 (20分)
        types_with_tests = sum([
            1 if self.stats['unit_tests'] > 0 else 0,
            1 if self.stats['integration_tests'] > 0 else 0,
            1 if self.stats['security_tests'] > 0 else 0,
            1 if self.stats['benchmark_tests'] > 0 else 0,
        ])
        score += (types_with_tests / 4) * 20

        # 测试用例密度 (20分)
        if self.stats['total_test_cases'] >= 500:
            score += 20
        elif self.stats['total_test_cases'] >= 200:
            score += 15
        elif self.stats['total_test_cases'] >= 100:
            score += 10

        return min(score, 100.0)

    def generate_report(self) -> str:
        """生成完整报告"""
        self.count_test_files()
        self.estimate_test_cases()
        health_score = self.calculate_health_score()

        report = []
        report.append("=" * 60)
        report.append("📊 AgentOS 测试质量报告")
        report.append("=" * 60)
        report.append(f"生成时间: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}")
        report.append(f"测试目录: {self.tests_dir}")
        report.append("")

        report.append("📁 测试文件统计:")
        report.append(f"  总文件数: {self.stats['total_files']}")
        report.append(f"  C 测试文件: {self.stats['c_tests']}")
        report.append(f"  Python 测试文件: {self.stats['python_tests']}")
        report.append("")

        report.append("📝 测试类型分布:")
        report.append(f"  单元测试: {self.stats['unit_tests']} 文件")
        report.append(f"  集成测试: {self.stats['integration_tests']} 文件")
        report.append(f"  安全测试: {self.stats['security_tests']} 文件")
        report.append(f"  基准测试: {self.stats['benchmark_tests']} 文件")
        report.append("")

        report.append("🎯 测试用例统计:")
        report.append(f"  估算测试用例数: {self.stats['total_test_cases']}")
        report.append("")

        report.append("💚 测试健康度评分:")
        report.append(f"  得分: {health_score:.1f}/100")

        if health_score >= 90:
            report.append("  评级: ⭐⭐⭐⭐⭐ 优秀")
        elif health_score >= 80:
            report.append("  评级: ⭐⭐⭐⭐ 良好")
        elif health_score >= 70:
            report.append("  评级: ⭐⭐⭐ 合格")
        elif health_score >= 60:
            report.append("  评级: ⭐⭐ 需改进")
        else:
            report.append("  评级: ⭐ 待加强")

        report.append("")
        report.append("=" * 60)

        return '\n'.join(report)


def main():
    """主函数"""
    tests_dir = Path(__file__).parent
    reporter = TestQualityReporter(tests_dir)
    report = reporter.generate_report()
    print(report)

    # 保存到文件
    report_file = tests_dir / 'quality_report.txt'
    with open(report_file, 'w', encoding='utf-8') as f:
        f.write(report)
    print(f"\n📄 报告已保存到: {report_file}")

    return 0


@dataclass
class TestMetrics:
    """测试指标数据类"""
    name: str
    value: float
    unit: str = ""
    threshold: Optional[float] = None
    status: str = "ok"


@dataclass
class TestFileInfo:
    """测试文件信息"""
    path: str
    language: str
    test_count: int
    size_bytes: int
    last_modified: str
    markers: List[str] = field(default_factory=list)


class DetailedTestAnalyzer:
    """详细测试分析器"""

    def __init__(self, tests_dir: Path):
        self.tests_dir = tests_dir
        self.file_infos: List[TestFileInfo] = []
        self.test_functions: Dict[str, List[str]] = defaultdict(list)

    def analyze_all_files(self) -> List[TestFileInfo]:
        """分析所有测试文件"""
        self.file_infos = []

        for file_path in self.tests_dir.rglob('test_*'):
            if not file_path.is_file():
                continue

            info = self._analyze_file(file_path)
            self.file_infos.append(info)

        return self.file_infos

    def _analyze_file(self, file_path: Path) -> TestFileInfo:
        """分析单个文件"""
        language = 'c' if file_path.suffix == '.c' else 'python' if file_path.suffix == '.py' else 'unknown'

        try:
            with open(file_path, 'r', encoding='utf-8') as f:
                content = f.read()
        except Exception:
            content = ""

        test_count = self._count_tests(content, language)
        markers = self._extract_markers(content, language)

        stat = file_path.stat()
        last_modified = datetime.fromtimestamp(stat.st_mtime).isoformat()

        return TestFileInfo(
            path=str(file_path.relative_to(self.tests_dir)),
            language=language,
            test_count=test_count,
            size_bytes=stat.st_size,
            last_modified=last_modified,
            markers=markers
        )

    def _count_tests(self, content: str, language: str) -> int:
        """统计测试数量"""
        if language == 'python':
            return len(re.findall(r'def test_\w+', content))
        elif language == 'c':
            return len(re.findall(r'TEST(?:_F)?\s*\(', content))
        return 0

    def _extract_markers(self, content: str, language: str) -> List[str]:
        """提取测试标记"""
        markers = []
        if language == 'python':
            marker_pattern = r'@pytest\.mark\.(\w+)'
            markers = re.findall(marker_pattern, content)
        return list(set(markers))

    def get_statistics(self) -> Dict[str, Any]:
        """获取统计信息"""
        if not self.file_infos:
            self.analyze_all_files()

        total_tests = sum(f.test_count for f in self.file_infos)
        by_language = defaultdict(int)
        by_marker = defaultdict(int)

        for info in self.file_infos:
            by_language[info.language] += 1
            for marker in info.markers:
                by_marker[marker] += 1

        return {
            'total_files': len(self.file_infos),
            'total_tests': total_tests,
            'by_language': dict(by_language),
            'by_marker': dict(by_marker),
            'avg_tests_per_file': total_tests / len(self.file_infos) if self.file_infos else 0,
        }


class ReportFormatter:
    """报告格式化器"""

    @staticmethod
    def to_json(data: Dict[str, Any], indent: int = 2) -> str:
        """转换为JSON格式"""
        return json.dumps(data, indent=indent, ensure_ascii=False, default=str)

    @staticmethod
    def to_markdown(data: Dict[str, Any]) -> str:
        """转换为Markdown格式"""
        lines = []
        lines.append("# AgentOS 测试质量报告")
        lines.append("")
        lines.append(f"**生成时间**: {data.get('timestamp', 'N/A')}")
        lines.append("")

        if 'stats' in data:
            lines.append("## 📊 统计概览")
            lines.append("")
            stats = data['stats']
            lines.append(f"- 总文件数: {stats.get('total_files', 0)}")
            lines.append(f"- 总测试数: {stats.get('total_test_cases', 0)}")
            lines.append(f"- C 测试文件: {stats.get('c_tests', 0)}")
            lines.append(f"- Python 测试文件: {stats.get('python_tests', 0)}")
            lines.append("")

        if 'health_score' in data:
            lines.append("## 💚 健康度评分")
            lines.append("")
            lines.append(f"**得分**: {data['health_score']:.1f}/100")
            lines.append("")

        return '\n'.join(lines)

    @staticmethod
    def to_html(data: Dict[str, Any]) -> str:
        """转换为HTML格式"""
        html = ['<!DOCTYPE html>', '<html lang="zh-CN">', '<head>',
                '<meta charset="UTF-8">', '<title>AgentOS 测试质量报告</title>',
                '<style>',
                'body { font-family: Arial, sans-serif; margin: 20px; }',
                'h1 { color: #333; }',
                '.stat-card { background: #f5f5f5; padding: 15px; margin: 10px 0; border-radius: 5px; }',
                '.score { font-size: 24px; font-weight: bold; }',
                '.good { color: #4caf50; }',
                '.warning { color: #ff9800; }',
                '.bad { color: #f44336; }',
                '</style>', '</head>', '<body>']

        html.append('<h1>📊 AgentOS 测试质量报告</h1>')
        html.append(f'<p>生成时间: {data.get("timestamp", "N/A")}</p>')

        if 'stats' in data:
            stats = data['stats']
            html.append('<div class="stat-card">')
            html.append('<h2>统计概览</h2>')
            html.append(f'<p>总文件数: {stats.get("total_files", 0)}</p>')
            html.append(f'<p>总测试数: {stats.get("total_test_cases", 0)}</p>')
            html.append(f'<p>C 测试文件: {stats.get("c_tests", 0)}</p>')
            html.append(f'<p>Python 测试文件: {stats.get("python_tests", 0)}</p>')
            html.append('</div>')

        if 'health_score' in data:
            score = data['health_score']
            score_class = 'good' if score >= 80 else 'warning' if score >= 60 else 'bad'
            html.append('<div class="stat-card">')
            html.append('<h2>健康度评分</h2>')
            html.append(f'<p class="score {score_class}">{score:.1f}/100</p>')
            html.append('</div>')

        html.append('</body></html>')
        return '\n'.join(html)


class TrendAnalyzer:
    """趋势分析器"""

    def __init__(self, history_file: Path = None):
        self.history_file = history_file or Path("test_history.json")
        self.history: List[Dict] = []

    def load_history(self):
        """加载历史数据"""
        if self.history_file.exists():
            try:
                with open(self.history_file, 'r', encoding='utf-8') as f:
                    self.history = json.load(f)
            except Exception:
                self.history = []

    def save_history(self):
        """保存历史数据"""
        with open(self.history_file, 'w', encoding='utf-8') as f:
            json.dump(self.history, f, indent=2, ensure_ascii=False)

    def add_record(self, data: Dict):
        """添加记录"""
        record = {
            'timestamp': datetime.now().isoformat(),
            'data': data
        }
        self.history.append(record)

        if len(self.history) > 100:
            self.history = self.history[-100:]

        self.save_history()

    def get_trend(self, days: int = 7) -> Dict[str, Any]:
        """获取趋势"""
        if not self.history:
            return {'trend': 'no_data'}

        cutoff = datetime.now() - timedelta(days=days)
        recent = [
            r for r in self.history
            if datetime.fromisoformat(r['timestamp']) > cutoff
        ]

        if len(recent) < 2:
            return {'trend': 'insufficient_data', 'records': len(recent)}

        scores = [r['data'].get('health_score', 0) for r in recent]
        avg_score = sum(scores) / len(scores)

        if scores[-1] > scores[0]:
            trend = 'improving'
        elif scores[-1] < scores[0]:
            trend = 'declining'
        else:
            trend = 'stable'

        return {
            'trend': trend,
            'avg_score': avg_score,
            'records': len(recent),
            'score_change': scores[-1] - scores[0]
        }


class EnhancedTestReporter:
    """增强版测试报告器"""

    def __init__(self, tests_dir: Path):
        self.tests_dir = tests_dir
        self.basic_reporter = TestQualityReporter(tests_dir)
        self.analyzer = DetailedTestAnalyzer(tests_dir)
        self.trend_analyzer = TrendAnalyzer(tests_dir / "test_history.json")

    def generate_full_report(self) -> Dict[str, Any]:
        """生成完整报告"""
        self.basic_reporter.count_test_files()
        self.basic_reporter.estimate_test_cases()
        health_score = self.basic_reporter.calculate_health_score()
        detailed_stats = self.analyzer.get_statistics()

        report = {
            'timestamp': datetime.now().isoformat(),
            'tests_dir': str(self.tests_dir),
            'stats': self.basic_reporter.stats,
            'health_score': health_score,
            'detailed_stats': detailed_stats,
            'files': [asdict(f) for f in self.analyzer.file_infos[:50]]
        }

        self.trend_analyzer.load_history()
        self.trend_analyzer.add_record(report)
        report['trend'] = self.trend_analyzer.get_trend()

        return report

    def save_report(self, report: Dict[str, Any], output_dir: Path = None):
        """保存报告"""
        output_dir = output_dir or self.tests_dir / "reports"
        output_dir.mkdir(parents=True, exist_ok=True)

        timestamp = datetime.now().strftime('%Y%m%d_%H%M%S')

        with open(output_dir / f'report_{timestamp}.json', 'w', encoding='utf-8') as f:
            f.write(ReportFormatter.to_json(report))

        with open(output_dir / f'report_{timestamp}.md', 'w', encoding='utf-8') as f:
            f.write(ReportFormatter.to_markdown(report))

        with open(output_dir / f'report_{timestamp}.html', 'w', encoding='utf-8') as f:
            f.write(ReportFormatter.to_html(report))

        return output_dir


if __name__ == '__main__':
    tests_dir = Path(__file__).parent
    reporter = EnhancedTestReporter(tests_dir)
    report = reporter.generate_full_report()

    print(ReportFormatter.to_markdown(report))

    output_dir = reporter.save_report(report)
    print(f"\n📄 报告已保存到: {output_dir}")
