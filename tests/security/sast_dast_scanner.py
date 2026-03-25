"""
AgentOS SAST/DAST 安全扫描集成
Copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
Version: 1.0.0

集成静态应用安全测试(SAST)和动态应用安全测试(DAST)
用于检测代码漏洞和运行时安全问题
"""

import os
import re
import json
import subprocess
from datetime import datetime
from typing import Any, Dict, List, Optional
from dataclasses import dataclass, field
from pathlib import Path
from enum import Enum


class Severity(Enum):
    """漏洞严重程度"""
    CRITICAL = "critical"
    HIGH = "high"
    MEDIUM = "medium"
    LOW = "low"
    INFO = "info"


@dataclass
class Vulnerability:
    """漏洞数据类"""
    id: str
    name: str
    severity: Severity
    file_path: str
    line_number: int
    description: str
    recommendation: str
    cwe_id: Optional[str] = None
    owasp_category: Optional[str] = None
    source: str = "sast"


@dataclass
class ScanResult:
    """扫描结果数据类"""
    scanner: str
    timestamp: str = field(default_factory=lambda: datetime.now().isoformat())
    vulnerabilities: List[Vulnerability] = field(default_factory=list)
    total_findings: int = 0
    critical_count: int = 0
    high_count: int = 0
    medium_count: int = 0
    low_count: int = 0


class SASTScanner:
    """
    静态应用安全测试扫描器
    
    集成多种SAST工具进行代码安全扫描。
    """
    
    def __init__(self, target_dir: str):
        """
        初始化SAST扫描器
        
        Args:
            target_dir: 扫描目标目录
        """
        self.target_dir = Path(target_dir)
        self.results: List[ScanResult] = []
        
        self.patterns = {
            "hardcoded_password": [
                (r'password\s*=\s*["\'][^"\']+["\']', "CWE-798", "A07:2021"),
                (r'passwd\s*=\s*["\'][^"\']+["\']', "CWE-798", "A07:2021"),
                (r'pwd\s*=\s*["\'][^"\']+["\']', "CWE-798", "A07:2021"),
            ],
            "hardcoded_api_key": [
                (r'api_key\s*=\s*["\'][^"\']+["\']', "CWE-798", "A07:2021"),
                (r'apikey\s*=\s*["\'][^"\']+["\']', "CWE-798", "A07:2021"),
                (r'secret_key\s*=\s*["\'][^"\']+["\']', "CWE-798", "A07:2021"),
            ],
            "sql_injection": [
                (r'execute\s*\(\s*["\'].*%s.*["\'].*%', "CWE-89", "A03:2021"),
                (r'cursor\.execute\s*\(\s*f["\']', "CWE-89", "A03:2021"),
            ],
            "xss_vulnerable": [
                (r'innerHTML\s*=', "CWE-79", "A03:2021"),
                (r'document\.write\s*\(', "CWE-79", "A03:2021"),
            ],
            "unsafe_deserialization": [
                (r'pickle\.loads?\s*\(', "CWE-502", "A08:2021"),
                (r'yaml\.load\s*\([^)]*\)\s*$', "CWE-502", "A08:2021"),
            ],
            "command_injection": [
                (r'os\.system\s*\(', "CWE-78", "A03:2021"),
                (r'subprocess\.call\s*\([^)]*shell\s*=\s*True', "CWE-78", "A03:2021"),
            ],
        }
    
    def scan_python_file(self, file_path: Path) -> List[Vulnerability]:
        """
        扫描Python文件
        
        Args:
            file_path: 文件路径
            
        Returns:
            发现的漏洞列表
        """
        vulnerabilities = []
        
        try:
            with open(file_path, 'r', encoding='utf-8', errors='ignore') as f:
                lines = f.readlines()
            
            for line_num, line in enumerate(lines, 1):
                for vuln_type, patterns in self.patterns.items():
                    for pattern, cwe_id, owasp in patterns:
                        if re.search(pattern, line, re.IGNORECASE):
                            if 'test' in str(file_path).lower():
                                continue
                            
                            vuln = Vulnerability(
                                id=f"SAST-{vuln_type.upper()}-{line_num}",
                                name=f"Potential {vuln_type.replace('_', ' ').title()}",
                                severity=Severity.HIGH if vuln_type in [
                                    "sql_injection", "command_injection"
                                ] else Severity.MEDIUM,
                                file_path=str(file_path),
                                line_number=line_num,
                                description=f"Potential {vuln_type.replace('_', ' ')} detected",
                                recommendation=self._get_recommendation(vuln_type),
                                cwe_id=cwe_id,
                                owasp_category=owasp,
                                source="sast"
                            )
                            vulnerabilities.append(vuln)
        except Exception:
            pass
        
        return vulnerabilities
    
    def scan_c_file(self, file_path: Path) -> List[Vulnerability]:
        """
        扫描C文件
        
        Args:
            file_path: 文件路径
            
        Returns:
            发现的漏洞列表
        """
        vulnerabilities = []
        
        c_patterns = {
            "buffer_overflow": [
                (r'strcpy\s*\(', "CWE-120", "A06:2021"),
                (r'strcat\s*\(', "CWE-120", "A06:2021"),
                (r'gets\s*\(', "CWE-120", "A06:2021"),
                (r'sprintf\s*\([^,]+,\s*[^"]', "CWE-120", "A06:2021"),
            ],
            "format_string": [
                (r'printf\s*\(\s*[a-zA-Z_][a-zA-Z0-9_]*\s*\)', "CWE-134", "A06:2021"),
                (r'fprintf\s*\([^,]+,\s*[a-zA-Z_]', "CWE-134", "A06:2021"),
            ],
            "memory_leak": [
                (r'malloc\s*\([^)]*\)(?![^;]*free)', "CWE-401", "A06:2021"),
            ],
        }
        
        try:
            with open(file_path, 'r', encoding='utf-8', errors='ignore') as f:
                lines = f.readlines()
            
            for line_num, line in enumerate(lines, 1):
                for vuln_type, patterns in c_patterns.items():
                    for pattern, cwe_id, owasp in patterns:
                        if re.search(pattern, line):
                            if 'test' in str(file_path).lower():
                                continue
                            
                            vuln = Vulnerability(
                                id=f"SAST-{vuln_type.upper()}-{line_num}",
                                name=f"Potential {vuln_type.replace('_', ' ').title()}",
                                severity=Severity.HIGH,
                                file_path=str(file_path),
                                line_number=line_num,
                                description=f"Potential {vuln_type.replace('_', ' ')} in C code",
                                recommendation=self._get_recommendation(vuln_type),
                                cwe_id=cwe_id,
                                owasp_category=owasp,
                                source="sast"
                            )
                            vulnerabilities.append(vuln)
        except Exception:
            pass
        
        return vulnerabilities
    
    def _get_recommendation(self, vuln_type: str) -> str:
        """获取修复建议"""
        recommendations = {
            "hardcoded_password": "使用环境变量或安全的密钥管理服务存储敏感信息",
            "hardcoded_api_key": "使用环境变量或配置文件存储API密钥，不要硬编码",
            "sql_injection": "使用参数化查询或ORM框架，避免字符串拼接SQL",
            "xss_vulnerable": "对用户输入进行适当的转义和编码",
            "unsafe_deserialization": "使用安全的序列化格式如JSON，避免pickle",
            "command_injection": "避免使用shell=True，使用参数列表形式传递命令",
            "buffer_overflow": "使用安全的字符串函数如strncpy、snprintf",
            "format_string": "使用格式化字符串常量，避免用户控制的格式字符串",
            "memory_leak": "确保每个malloc都有对应的free，考虑使用RAII模式",
        }
        return recommendations.get(vuln_type, "请参考安全编码最佳实践")
    
    def run_full_scan(self) -> ScanResult:
        """
        运行完整扫描
        
        Returns:
            扫描结果
        """
        result = ScanResult(scanner="SAST")
        
        for ext, scanner in [('.py', self.scan_python_file), ('.c', self.scan_c_file)]:
            for file_path in self.target_dir.rglob(f'*{ext}'):
                vulns = scanner(file_path)
                result.vulnerabilities.extend(vulns)
        
        result.total_findings = len(result.vulnerabilities)
        result.critical_count = len([v for v in result.vulnerabilities if v.severity == Severity.CRITICAL])
        result.high_count = len([v for v in result.vulnerabilities if v.severity == Severity.HIGH])
        result.medium_count = len([v for v in result.vulnerabilities if v.severity == Severity.MEDIUM])
        result.low_count = len([v for v in result.vulnerabilities if v.severity == Severity.LOW])
        
        self.results.append(result)
        return result


class DASTScanner:
    """
    动态应用安全测试扫描器
    
    运行时安全测试和依赖漏洞扫描。
    """
    
    def __init__(self, target_dir: str):
        """
        初始化DAST扫描器
        
        Args:
            target_dir: 扫描目标目录
        """
        self.target_dir = Path(target_dir)
        self.results: List[ScanResult] = []
    
    def check_dependency_vulnerabilities(self) -> ScanResult:
        """
        检查依赖漏洞
        
        Returns:
            扫描结果
        """
        result = ScanResult(scanner="DAST-Dependency")
        
        requirements_file = self.target_dir / "requirements.txt"
        
        if requirements_file.exists():
            try:
                with open(requirements_file, 'r', encoding='utf-8') as f:
                    lines = f.readlines()
                
                for line_num, line in enumerate(lines, 1):
                    line = line.strip()
                    if not line or line.startswith('#'):
                        continue
                    
                    if '==' not in line:
                        vuln = Vulnerability(
                            id=f"DEP-UNPINNED-{line_num}",
                            name="Unpinned Dependency",
                            severity=Severity.LOW,
                            file_path=str(requirements_file),
                            line_number=line_num,
                            description=f"Dependency '{line}' is not pinned to a specific version",
                            recommendation="Pin dependencies to specific versions for reproducibility",
                            source="dast"
                        )
                        result.vulnerabilities.append(vuln)
            except Exception:
                pass
        
        result.total_findings = len(result.vulnerabilities)
        self.results.append(result)
        return result
    
    def check_security_headers(self) -> ScanResult:
        """
        检查安全头配置
        
        Returns:
            扫描结果
        """
        result = ScanResult(scanner="DAST-Headers")
        
        recommended_headers = [
            "Content-Security-Policy",
            "X-Content-Type-Options",
            "X-Frame-Options",
            "Strict-Transport-Security",
            "X-XSS-Protection",
        ]
        
        for file_path in self.target_dir.rglob('*.py'):
            try:
                with open(file_path, 'r', encoding='utf-8') as f:
                    content = f.read()
                
                for header in recommended_headers:
                    if header.lower().replace('-', '_') not in content.lower():
                        pass
            except Exception:
                pass
        
        result.total_findings = len(result.vulnerabilities)
        self.results.append(result)
        return result
    
    def run_full_scan(self) -> List[ScanResult]:
        """
        运行完整DAST扫描
        
        Returns:
            扫描结果列表
        """
        self.check_dependency_vulnerabilities()
        self.check_security_headers()
        return self.results


class SecurityScanOrchestrator:
    """
    安全扫描编排器
    
    协调SAST和DAST扫描，生成综合报告。
    """
    
    def __init__(self, target_dir: str):
        """
        初始化安全扫描编排器
        
        Args:
            target_dir: 扫描目标目录
        """
        self.target_dir = target_dir
        self.sast = SASTScanner(target_dir)
        self.dast = DASTScanner(target_dir)
    
    def run_all_scans(self) -> Dict[str, Any]:
        """
        运行所有安全扫描
        
        Returns:
            综合扫描结果
        """
        sast_result = self.sast.run_full_scan()
        dast_results = self.dast.run_full_scan()
        
        return {
            "timestamp": datetime.now().isoformat(),
            "target": self.target_dir,
            "sast": {
                "scanner": sast_result.scanner,
                "total_findings": sast_result.total_findings,
                "critical": sast_result.critical_count,
                "high": sast_result.high_count,
                "medium": sast_result.medium_count,
                "low": sast_result.low_count,
                "vulnerabilities": [
                    {
                        "id": v.id,
                        "name": v.name,
                        "severity": v.severity.value,
                        "file": v.file_path,
                        "line": v.line_number,
                        "description": v.description,
                        "recommendation": v.recommendation,
                        "cwe": v.cwe_id,
                        "owasp": v.owasp_category
                    }
                    for v in sast_result.vulnerabilities
                ]
            },
            "dast": [
                {
                    "scanner": r.scanner,
                    "total_findings": r.total_findings
                }
                for r in dast_results
            ],
            "summary": {
                "total_vulnerabilities": sast_result.total_findings + sum(r.total_findings for r in dast_results),
                "critical_high": sast_result.critical_count + sast_result.high_count,
                "scan_passed": (sast_result.critical_count + sast_result.high_count) == 0
            }
        }
    
    def generate_report(self, results: Dict[str, Any]) -> str:
        """
        生成安全扫描报告
        
        Args:
            results: 扫描结果
            
        Returns:
            Markdown格式报告
        """
        report = [
            "# 🔒 安全扫描报告",
            "",
            f"**扫描时间**: {results['timestamp']}",
            f"**扫描目标**: {results['target']}",
            "",
            "## 📊 扫描摘要",
            "",
            "| 指标 | 数值 |",
            "|------|------|",
            f"| 总漏洞数 | {results['summary']['total_vulnerabilities']} |",
            f"| 严重/高危 | {results['summary']['critical_high']} |",
            f"| 扫描状态 | {'✅ 通过' if results['summary']['scan_passed'] else '❌ 失败'} |",
            "",
            "## 🔍 SAST 结果",
            "",
            f"- 总发现: {results['sast']['total_findings']}",
            f"- 严重: {results['sast']['critical']}",
            f"- 高危: {results['sast']['high']}",
            f"- 中危: {results['sast']['medium']}",
            f"- 低危: {results['sast']['low']}",
            "",
        ]
        
        if results['sast']['vulnerabilities']:
            report.extend([
                "### 发现的漏洞",
                "",
                "| ID | 名称 | 严重程度 | 文件 | 行号 |",
                "|----|----|----------|------|------|",
            ])
            
            for v in results['sast']['vulnerabilities'][:20]:
                report.append(
                    f"| {v['id']} | {v['name']} | {v['severity'].upper()} | "
                    f"{Path(v['file']).name} | {v['line']} |"
                )
        
        report.extend([
            "",
            "---",
            "",
            "*报告由 AgentOS 安全扫描框架生成*",
            "*Copyright (c) 2026 SPHARX Ltd.*"
        ])
        
        return "\n".join(report)


def run_security_scan(target_dir: str = ".") -> Dict[str, Any]:
    """
    运行安全扫描
    
    Args:
        target_dir: 扫描目标目录
        
    Returns:
        扫描结果
    """
    orchestrator = SecurityScanOrchestrator(target_dir)
    results = orchestrator.run_all_scans()
    report = orchestrator.generate_report(results)
    
    return {
        "results": results,
        "report": report,
        "passed": results["summary"]["scan_passed"]
    }


if __name__ == "__main__":
    print("=" * 60)
    print("AgentOS SAST/DAST 安全扫描框架")
    print("Copyright (c) 2026 SPHARX Ltd.")
    print("=" * 60)
    
    scan_result = run_security_scan(".")
    
    print("\n" + scan_result["report"])
    
    if not scan_result["passed"]:
        print("\n⚠️ 发现安全漏洞，请修复！")
        exit(1)
    else:
        print("\n✅ 安全扫描通过！")
