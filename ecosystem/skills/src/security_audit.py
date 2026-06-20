"""SecurityAuditSkill — 安全审计技能

对系统配置、代码、网络资产等进行安全审计，识别潜在威胁和合规风险。
"""

from __future__ import annotations

import json
import logging
import re
from typing import Any, Dict, List, Optional, Set

import sys as _sys
from pathlib import Path as _Path

# 确保 agentos 包可导入（开发模式）
_sdk_python = _Path(__file__).resolve().parents[3] / "sdk" / "python"
if str(_sdk_python) not in _sys.path:
    _sys.path.insert(0, str(_sdk_python))

from agentos.plugin_types import (
    SkillPlugin,
    SkillDefinition,
)

logger = logging.getLogger(__name__)


class SecurityAuditSkill(SkillPlugin):
    """安全审计技能。

    对系统配置、代码、网络资产等执行安全审计，覆盖：
    1. 配置审计 (config) — 检查安全配置是否符合最佳实践
    2. 依赖审计 (dependencies) — 检查第三方依赖的已知漏洞
    3. 权限审计 (permissions) — 检查文件/目录权限设置
    4. 网络审计 (network) — 检查开放端口、TLS 配置等
    5. 合规审计 (compliance) — 检查是否符合 OWASP/NIST/等保等标准
    """

    PLUGIN_TYPE = "skill"

    # ── Skill Definition ──────────────────────────────────

    def get_definition(self) -> SkillDefinition:
        return SkillDefinition(
            name="security_audit",
            description="对系统配置、代码、网络资产进行安全审计，识别潜在威胁和合规风险",
            version="1.0.0",
            category="security",
            tags=["security", "audit", "compliance", "vulnerability"],
            input_schema={
                "type": "object",
                "properties": {
                    "target": {
                        "type": "string",
                        "description": "审计目标内容（代码、配置、日志等）",
                    },
                    "audit_type": {
                        "type": "string",
                        "description": "审计类型 (config/dependencies/permissions/network/compliance/auto)",
                        "default": "auto",
                    },
                    "framework": {
                        "type": "string",
                        "description": "合规框架 (owasp/nist/iso27001/dengbao/gdpr/custom)",
                        "default": "owasp",
                    },
                    "severity_threshold": {
                        "type": "string",
                        "description": "最低报告严重级别 (info/low/medium/high/critical)",
                        "default": "low",
                    },
                },
                "required": ["target"],
            },
            output_schema={
                "type": "object",
                "properties": {
                    "audit_type": {"type": "string"},
                    "framework": {"type": "string"},
                    "risk_score": {"type": "number"},
                    "risk_level": {"type": "string"},
                    "findings": {"type": "array"},
                    "summary": {"type": "string"},
                    "recommendations": {"type": "array"},
                },
            },
            examples=[
                {
                    "input": "审计 Docker 容器的安全配置",
                    "output": "发现 3 个高危：root 运行、特权模式、未限制资源",
                },
                {
                    "input": "审计 Python 项目的依赖漏洞",
                    "output": "发现 2 个已知 CVE，建议升级 requests 到 2.32+",
                },
            ],
            requires=["llm"],
        )

    def get_prompt_template(self) -> Optional[str]:
        return (
            "Perform a {audit_type} security audit on the following target.\n\n"
            "Compliance framework: {framework}\n"
            "Target:\n{target}\n\n"
            "Provide findings in JSON format with fields: "
            "id, severity (critical/high/medium/low/info), "
            "category, title, description, remediation, cve_ref (if applicable)."
        )

    def get_system_instructions(self) -> Optional[str]:
        return (
            "You are a senior security auditor. "
            "Identify real security risks, not theoretical ones. "
            "For each finding, provide a clear description and actionable remediation. "
            "Reference CVE numbers when applicable. "
            "Prioritize findings that pose immediate risk. "
            "Rate severity based on exploitability and impact: "
            "critical = remote code execution / data breach, "
            "high = privilege escalation / auth bypass, "
            "medium = information disclosure / DoS, "
            "low = configuration hardening, "
            "info = best practice recommendation."
        )

    # ── Validation ────────────────────────────────────────

    def validate_input(self, context: Dict[str, Any]) -> bool:
        target = context.get("target", "")
        if not target or not target.strip():
            logger.warning("SecurityAuditSkill: empty target input")
            return False
        audit_type = context.get("audit_type", "auto")
        valid_types = {"config", "dependencies", "permissions", "network", "compliance", "auto"}
        if audit_type not in valid_types:
            logger.warning("SecurityAuditSkill: invalid audit_type '%s'", audit_type)
            return False
        return True

    # ── Execution ─────────────────────────────────────────

    async def pre_execute(self, context: Dict[str, Any]) -> Dict[str, Any]:
        context.setdefault("audit_type", "auto")
        context.setdefault("framework", "owasp")
        context.setdefault("severity_threshold", "low")

        target = context.get("target", "")

        # 自动检测审计类型
        if context["audit_type"] == "auto":
            context["audit_type"] = self._detect_audit_type(target)

        # 快速预扫描
        context["quick_scan_results"] = self._quick_scan(target, context["audit_type"])

        return context

    async def execute(self, context: Dict[str, Any]) -> Any:
        target = context.get("target", "")
        audit_type = context.get("audit_type", "auto")
        framework = context.get("framework", "owasp")
        quick_scan = context.get("quick_scan_results", [])

        # 本地静态审计
        local_findings = self._static_audit(target, audit_type)

        # 合并快速扫描和静态审计结果
        all_findings = quick_scan + local_findings

        # 去重
        seen: Set[tuple] = set()
        unique = []
        for f in all_findings:
            key = (f.get("category", ""), f.get("title", ""))
            if key not in seen:
                seen.add(key)
                unique.append(f)

        # 按严重级别排序
        severity_order = {"critical": 0, "high": 1, "medium": 2, "low": 3, "info": 4}
        unique.sort(key=lambda f: severity_order.get(f.get("severity", "info"), 5))

        # 计算风险评分
        risk_score = self._compute_risk_score(unique)

        return {
            "audit_type": audit_type,
            "framework": framework,
            "risk_score": risk_score,
            "risk_level": self._risk_level(risk_score),
            "findings": unique,
            "summary": self._generate_summary(unique, risk_score, audit_type),
            "recommendations": self._generate_recommendations(unique),
        }

    async def post_execute(self, context: Dict[str, Any], result: Any) -> Any:
        # 过滤低于阈值的发现
        threshold = context.get("severity_threshold", "low")
        severity_order = {"info": 0, "low": 1, "medium": 2, "high": 3, "critical": 4}
        min_level = severity_order.get(threshold, 1)

        if isinstance(result, dict) and "findings" in result:
            result["findings"] = [
                f for f in result["findings"]
                if severity_order.get(f.get("severity", "info"), 0) >= min_level
            ]
        return result

    # ── Audit Type Detection ──────────────────────────────

    def _detect_audit_type(self, target: str) -> str:
        """自动检测审计类型。"""
        target_lower = target.lower()

        if any(kw in target_lower for kw in ["dockerfile", "docker-compose", "nginx", "apache", "toml", "yaml", "json", "ini", "conf"]):
            return "config"
        if any(kw in target_lower for kw in ["requirements.txt", "package.json", "cargo.toml", "go.mod", "pom.xml", "gemfile", "dependency"]):
            return "dependencies"
        if any(kw in target_lower for kw in ["chmod", "permission", "rwx", "owner", "acl", "sudo", "setuid"]):
            return "permissions"
        if any(kw in target_lower for kw in ["port", "firewall", "tls", "ssl", "certificate", "dns", "subnet", "open port"]):
            return "network"
        if any(kw in target_lower for kw in ["owasp", "nist", "iso", "gdpr", "hipaa", "pci", "soc2", "compliance"]):
            return "compliance"

        return "config"

    # ── Quick Scan ────────────────────────────────────────

    def _quick_scan(self, target: str, audit_type: str) -> List[Dict[str, str]]:
        """快速正则扫描。"""
        findings = []

        patterns = [
            # 配置类
            (r'password\s*[=:]\s*["\'][^"\']{1,30}["\']', "critical", "config",
             "Hardcoded password in configuration"),
            (r'api[_-]?key\s*[=:]\s*["\'][A-Za-z0-9_-]{10,}["\']', "high", "config",
             "Hardcoded API key detected"),
            (r'secret\s*[=:]\s*["\'][^"\']+["\']', "high", "config",
             "Hardcoded secret in configuration"),
            (r'debug\s*[=:]\s*true', "medium", "config",
             "Debug mode enabled in production config"),
            (r'ALLOWED_HOSTS\s*=\s*\[\s*["\']\*["\']', "high", "config",
             "Wildcard ALLOWED_HOSTS — potential host header injection"),
            # 依赖类
            (r'requests\s*[=<>]=?\s*["\']?2\.[0-9]', "high", "dependencies",
             "Outdated requests library — potential CVE"),
            (r'django\s*[=<>]=?\s*["\']?[12]\.[0-9]', "medium", "dependencies",
             "Older Django version — check for security patches"),
            # 权限类
            (r'chmod\s+777', "high", "permissions",
             "World-writable permissions (777) detected"),
            (r'chmod\s+o\+w', "medium", "permissions",
             "World-writable permission added"),
            (r'RUN\s+useradd|USER\s+root', "high", "permissions",
             "Container running as root"),
            # 网络类
            (r'0\.0\.0\.0', "medium", "network",
             "Binding to all interfaces (0.0.0.0)"),
            (r'SSLv[23]|TLSv1[^.]', "high", "network",
             "Deprecated SSL/TLS version detected"),
        ]

        for pattern, severity, category, title in patterns:
            if audit_type in ("auto", category):
                if re.search(pattern, target, re.IGNORECASE):
                    findings.append({
                        "id": f"qs-{len(findings)+1}",
                        "severity": severity,
                        "category": category,
                        "title": title,
                        "description": f"Pattern detected: {title}",
                        "remediation": "Review and remediate the identified issue",
                    })

        return findings

    # ── Static Audit ──────────────────────────────────────

    def _static_audit(self, target: str, audit_type: str) -> List[Dict[str, str]]:
        """静态安全审计。"""
        findings = []

        if audit_type in ("auto", "config"):
            findings.extend(self._audit_config(target))

        if audit_type in ("auto", "dependencies"):
            findings.extend(self._audit_dependencies(target))

        if audit_type in ("auto", "permissions"):
            findings.extend(self._audit_permissions(target))

        if audit_type in ("auto", "network"):
            findings.extend(self._audit_network(target))

        return findings

    def _audit_config(self, target: str) -> List[Dict[str, str]]:
        """审计配置安全。"""
        findings = []
        lines = target.splitlines()

        for i, line in enumerate(lines, 1):
            stripped = line.strip()

            # 检查不安全的 HTTP 配置
            if re.search(r'(?i)\bhttp://', stripped) and not re.search(r'(?i)\blocalhost|127\.0\.0\.1', stripped):
                findings.append({
                    "id": f"cfg-{len(findings)+1}",
                    "severity": "medium",
                    "category": "config",
                    "title": "HTTP used instead of HTTPS",
                    "description": f"Line {i}: Non-localhost HTTP URL detected",
                    "remediation": "Use HTTPS for all production endpoints",
                })

            # 检查过期加密算法
            if re.search(r'(?i)\bmd5\b|sha1\b', stripped):
                findings.append({
                    "id": f"cfg-{len(findings)+1}",
                    "severity": "high",
                    "category": "config",
                    "title": "Deprecated hash algorithm",
                    "description": f"Line {i}: MD5 or SHA-1 detected",
                    "remediation": "Use SHA-256 or stronger hashing algorithm",
                })

                # 检查 CORS 配置
                if re.search(r'(?i)access-control-allow-origin\s*:\s*\*', stripped):
                    findings.append({
                        "id": f"cfg-{len(findings)+1}",
                        "severity": "medium",
                        "category": "config",
                        "title": "Overly permissive CORS policy",
                        "description": "Access-Control-Allow-Origin set to wildcard",
                        "remediation": "Restrict CORS to specific trusted origins",
                    })

        return findings

    def _audit_dependencies(self, target: str) -> List[Dict[str, str]]:
        """审计依赖安全。"""
        findings = []

        # 已知漏洞库映射
        known_vulnerable: Dict[str, tuple] = {
            "requests": ("2.31.0", "CVE-2024-35195", "medium",
                         "Upgrade requests to >= 2.32.0"),
            "urllib3": ("2.0.7", "CVE-2024-37891", "medium",
                        "Upgrade urllib3 to >= 2.2.0"),
            "certifi": ("2023.7.22", "CVE-2024-39689", "low",
                        "Upgrade certifi to >= 2024.7.4"),
            "cryptography": ("42.0.0", "CVE-2024-26130", "high",
                             "Upgrade cryptography to >= 42.0.4"),
            "jinja2": ("3.1.2", "CVE-2024-34064", "medium",
                       "Upgrade Jinja2 to >= 3.1.4"),
            "pyyaml": ("6.0", "CVE-2020-14343", "high",
                       "Upgrade PyYAML to >= 6.0.1"),
        }

        for package, (version, cve, severity, remediation) in known_vulnerable.items():
            pattern = re.compile(
                rf'{package}\s*[=<>]=?\s*["\']?([\d.]+)',
                re.IGNORECASE,
            )
            for match in pattern.finditer(target):
                found_version = match.group(1)
                if self._version_le(found_version, version):
                    findings.append({
                        "id": f"dep-{len(findings)+1}",
                        "severity": severity,
                        "category": "dependencies",
                        "title": f"Vulnerable dependency: {package} {found_version}",
                        "description": f"{package} {found_version} is affected by {cve}",
                        "remediation": remediation,
                        "cve_ref": cve,
                    })

        return findings

    def _audit_permissions(self, target: str) -> List[Dict[str, str]]:
        """审计权限安全。"""
        findings = []
        lines = target.splitlines()

        for i, line in enumerate(lines, 1):
            stripped = line.strip()

            # 检查过于宽松的文件权限
            if re.search(r'chmod\s+777', stripped):
                findings.append({
                    "id": f"perm-{len(findings)+1}",
                    "severity": "high",
                    "category": "permissions",
                    "title": "World-writable file permissions (777)",
                    "description": f"Line {i}: chmod 777 detected",
                    "remediation": "Use restrictive permissions (e.g., 644 for files, 755 for dirs)",
                })

            if re.search(r'chmod\s+o\+w', stripped):
                findings.append({
                    "id": f"perm-{len(findings)+1}",
                    "severity": "medium",
                    "category": "permissions",
                    "title": "World-writable permission added",
                    "description": f"Line {i}: chmod o+w detected",
                    "remediation": "Consider using group permissions instead",
                })

            # Docker 特定检查
            if re.search(r'(?i)USER\s+root', stripped):
                findings.append({
                    "id": f"perm-{len(findings)+1}",
                    "severity": "high",
                    "category": "permissions",
                    "title": "Container running as root",
                    "description": f"Line {i}: USER root in Dockerfile",
                    "remediation": "Create and use a non-root user for the container",
                })

            if re.search(r'(?i)--privileged', stripped):
                findings.append({
                    "id": f"perm-{len(findings)+1}",
                    "severity": "critical",
                    "category": "permissions",
                    "title": "Privileged container mode",
                    "description": f"Line {i}: --privileged flag detected",
                    "remediation": "Remove --privileged; use --cap-add for specific capabilities",
                })

        return findings

    def _audit_network(self, target: str) -> List[Dict[str, str]]:
        """审计网络安全。"""
        findings = []
        lines = target.splitlines()

        for i, line in enumerate(lines, 1):
            stripped = line.strip()

            if re.search(r'(?i)SSLv[23]|TLSv1\b(?!\.\d)', stripped):
                findings.append({
                    "id": f"net-{len(findings)+1}",
                    "severity": "high",
                    "category": "network",
                    "title": "Deprecated SSL/TLS protocol",
                    "description": f"Line {i}: SSLv3 or TLS 1.0/1.1 detected",
                    "remediation": "Use TLS 1.2 or TLS 1.3 minimum",
                })

            if re.search(r'(?i)telnet', stripped):
                findings.append({
                    "id": f"net-{len(findings)+1}",
                    "severity": "critical",
                    "category": "network",
                    "title": "Telnet protocol detected",
                    "description": "Telnet transmits data in plaintext",
                    "remediation": "Replace telnet with SSH",
                })

            if re.search(r'(?i)ftp\b(?!s)', stripped):
                findings.append({
                    "id": f"net-{len(findings)+1}",
                    "severity": "high",
                    "category": "network",
                    "title": "Unencrypted FTP detected",
                    "description": "FTP transmits credentials in plaintext",
                    "remediation": "Use SFTP or FTPS instead",
                })

        return findings

    # ── Scoring ───────────────────────────────────────────

    def _compute_risk_score(self, findings: List[Dict[str, str]]) -> float:
        """计算风险评分 (0-100, 越高越危险)。"""
        if not findings:
            return 0.0

        weights = {"critical": 10, "high": 6, "medium": 3, "low": 1, "info": 0.5}
        score = sum(weights.get(f.get("severity", "info"), 0.5) for f in findings)
        # 归一化到 0-100
        return round(min(100.0, score * 2), 1)

    def _risk_level(self, score: float) -> str:
        """风险等级判定。"""
        if score >= 80:
            return "critical"
        if score >= 50:
            return "high"
        if score >= 25:
            return "medium"
        if score >= 10:
            return "low"
        return "info"

    def _generate_summary(
        self, findings: List[Dict[str, str]], risk_score: float, audit_type: str
    ) -> str:
        """生成审计摘要。"""
        severity_counts: Dict[str, int] = {}
        for f in findings:
            sev = f.get("severity", "info")
            severity_counts[sev] = severity_counts.get(sev, 0) + 1

        level = self._risk_level(risk_score)
        parts = [
            f"Security audit ({audit_type}): {level.upper()} risk (score {risk_score}/100)"
        ]
        for sev in ("critical", "high", "medium", "low", "info"):
            if sev in severity_counts:
                parts.append(f"{severity_counts[sev]} {sev}")

        return ", ".join(parts)

    def _generate_recommendations(
        self, findings: List[Dict[str, str]]
    ) -> List[str]:
        """生成修复建议列表。"""
        recs = []
        for f in findings:
            remediation = f.get("remediation", "")
            if remediation and remediation not in recs:
                recs.append(remediation)
        return recs

    # ── Helpers ───────────────────────────────────────────

    @staticmethod
    def _version_le(v1: str, v2: str) -> bool:
        """版本比较：v1 <= v2。"""
        try:
            parts1 = [int(x) for x in v1.split(".")]
            parts2 = [int(x) for x in v2.split(".")]
            max_len = max(len(parts1), len(parts2))
            parts1.extend([0] * (max_len - len(parts1)))
            parts2.extend([0] * (max_len - len(parts2)))
            return parts1 <= parts2
        except (ValueError, AttributeError):
            return False