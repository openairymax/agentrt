#!/usr/bin/env python3
"""
AgentOS Configuration Drift Detector

检测配置文件是否偏离基线版本，支持创建基线、检测漂移、生成报告等功能。

Usage:
    # 创建基线
    python drift_detector.py --action create-baseline
    
    # 检测漂移
    python drift_detector.py --action detect
    
    # 一键执行（先建基线再检测）
    python drift_detector.py --action both --output drift_report.json
    
    # 详细模式
    python drift_detector.py --action detect --verbose
"""

import argparse
import hashlib
import json
import os
import sys
from dataclasses import dataclass, field, asdict
from datetime import datetime, timezone
from enum import Enum
from pathlib import Path
from typing import Dict, List, Optional, Set, Tuple
import shutil


class DriftSeverity(Enum):
    """漂移严重程度"""
    INFO = "info"
    WARNING = "warning"
    CRITICAL = "critical"


class DriftType(Enum):
    """漂移类型"""
    MODIFIED = "modified"
    DELETED = "deleted"
    ADDED = "added"
    UNCHANGED = "unchanged"


@dataclass
class DriftItem:
    """单个漂移项"""
    file_path: str
    severity: str
    drift_type: str
    baseline_hash: str
    current_hash: str
    detected_at: str
    details: str = ""
    file_size: Optional[int] = None
    last_modified: Optional[str] = None
    
    def to_dict(self) -> Dict:
        return {
            "file_path": self.file_path,
            "severity": self.severity,
            "drift_type": self.drift_type,
            "baseline_hash": self.baseline_hash,
            "current_hash": self.current_hash,
            "detected_at": self.detected_at,
            "details": self.details,
            "file_size": self.file_size,
            "last_modified": self.last_modified
        }


@dataclass
class DriftReport:
    """漂移检测报告"""
    scan_time: str
    config_dir: str
    baseline_created: str
    total_files_scanned: int
    drifted_files: int
    unchanged_files: int
    drift_items: List[DriftItem] = field(default_factory=list)
    
    @property
    def has_drift(self) -> bool:
        return len(self.drift_items) > 0
    
    @property
    def drift_rate(self) -> float:
        if self.total_files_scanned == 0:
            return 0.0
        return (self.drifted_files / self.total_files_scanned) * 100
    
    def summary(self) -> str:
        critical = sum(1 for d in self.drift_items if d.severity == DriftSeverity.CRITICAL.value)
        warning = sum(1 for d in self.drift_items if d.severity == DriftSeverity.WARNING.value)
        info = sum(1 for d in self.drift_items if d.severity == DriftSeverity.INFO.value)
        
        return (
            f"Drift Report Summary:\n"
            f"  Scan Time: {self.scan_time}\n"
            f"  Config Directory: {self.config_dir}\n"
            f"  Total Files Scanned: {self.total_files_scanned}\n"
            f"  Drifted Files: {self.drifted_files} ({self.drift_rate:.1f}%)\n"
            f"  Unchanged Files: {self.unchanged_files}\n"
            f"  Severity Breakdown:\n"
            f"    - Critical: {critical}\n"
            f"    - Warning: {warning}\n"
            f"    - Info: {info}"
        )


class ConfigDriftDetector:
    """配置漂移检测器"""
    
    # 忽略的文件模式
    IGNORED_PATTERNS = [
        "*.pyc", "__pycache__/", ".git/", ".gitignore",
        "*.log", "node_modules/", ".env*", "*.tmp",
        "*.swp", "*.bak", ".DS_Store", "Thumbs.db"
    ]
    
    # 敏感文件（变更时为CRITICAL级别）
    SENSITIVE_FILES = [
        "security/policy.yaml",
        "security/permission_rules.yaml",
        "model/model.yaml",
        "kernel/settings.yaml",
        "manager_management.yaml"
    ]
    
    # 重要文件（变更时为WARNING级别）
    IMPORTANT_FILES = [
        "agent/registry.yaml",
        "skill/registry.yaml",
        "logging/manager.yaml",
        "sanitizer/sanitizer_rules.json"
    ]
    
    def __init__(
        self,
        config_dir: Path,
        baseline_dir: Optional[Path] = None,
        verbose: bool = False
    ):
        self.config_dir = config_dir.resolve()
        self.baseline_dir = baseline_dir or self.config_dir / ".baseline"
        self.baseline_manifest = self.baseline_dir / "manifest.json"
        self.verbose = verbose
        
        # 确保目录存在
        self.config_dir.mkdir(parents=True, exist_ok=True)
        
    def create_baseline(self) -> Path:
        """
        创建配置基线快照
        
        将当前所有配置文件的哈希值保存到基线清单
        
        Returns:
            Path: 基线清单文件路径
        """
        if self.verbose:
            print(f"📝 Creating baseline for {self.config_dir}...")
        
        manifest = {
            "created_at": datetime.now(timezone.utc).isoformat(),
            "version": "1.0.0",
            "config_dir": str(self.config_dir),
            "files": {}
        }
        
        config_files = self._get_config_files()
        
        if self.verbose:
            print(f"   Found {len(config_files)} configuration files")
        
        for file_path in config_files:
            rel_path = file_path.relative_to(self.config_dir)
            file_hash = self._calculate_file_hash(file_path)
            stat_info = file_path.stat()
            
            manifest["files"][str(rel_path)] = {
                "sha256": file_hash,
                "size": stat_info.st_size,
                "last_modified": datetime.fromtimestamp(
                    stat_info.st_mtime, tz=timezone.utc
                ).isoformat()
            }
            
            if self.verbose and len(manifest["files"]) % 10 == 0:
                print(f"   Processed {len(manifest['files'])} files...")
        
        self.baseline_dir.mkdir(parents=True, exist_ok=True)
        self.baseline_manifest.write_text(
            json.dumps(manifest, indent=2, ensure_ascii=False),
            encoding='utf-8'
        )
        
        if self.verbose:
            print(f"✅ Baseline created with {len(config_files)} files")
            print(f"   Saved to: {self.baseline_manifest}")
        
        return self.baseline_manifest
    
    def detect_drift(self) -> DriftReport:
        """
        检测配置漂移
        
        Returns:
            DriftReport: 漂移检测报告
        """
        if not self.baseline_manifest.exists():
            raise RuntimeError(
                f"Baseline not found at {self.baseline_manifest}. "
                "Run create_baseline() first."
            )
        
        if self.verbose:
            print(f"🔍 Detecting configuration drift...")
        
        # 加载基线清单
        manifest = json.loads(self.baseline_manifest.read_text(encoding='utf-8'))
        baseline_files = manifest["files"]
        baseline_created = manifest["created_at"]
        
        if self.verbose:
            print(f"   Baseline created: {baseline_created}")
            print(f"   Baseline files: {len(baseline_files)}")
        
        # 获取当前配置文件
        current_files = {
            str(p.relative_to(self.config_dir)): p 
            for p in self._get_config_files()
        }
        
        if self.verbose:
            print(f"   Current files: {len(current_files)}")
        
        # 创建报告
        report = DriftReport(
            scan_time=datetime.now(timezone.utc).isoformat(),
            config_dir=str(self.config_dir),
            baseline_created=baseline_created,
            total_files_scanned=len(baseline_files),
            drifted_files=0,
            unchanged_files=0,
            drift_items=[]
        )
        
        # 检查文件修改和删除
        for rel_path, baseline_info in baseline_files.items():
            current_path = self.config_dir / rel_path
            
            if rel_path not in current_files:
                # 文件被删除
                drift_item = DriftItem(
                    file_path=rel_path,
                    severity=self._get_severity_for_file(rel_path),
                    drift_type=DriftType.DELETED.value,
                    baseline_hash=baseline_info["sha256"],
                    current_hash="[DELETED]",
                    detected_at=datetime.now(timezone.utc).isoformat(),
                    details="File has been deleted since baseline"
                )
                report.drift_items.append(drift_item)
                report.drifted_files += 1
                
            else:
                current_hash = self._calculate_file_hash(current_path)
                
                if current_hash != baseline_info["sha256"]:
                    # 文件已被修改
                    stat_info = current_path.stat()
                    drift_item = DriftItem(
                        file_path=rel_path,
                        severity=self._get_severity_for_file(rel_path),
                        drift_type=DriftType.MODIFIED.value,
                        baseline_hash=baseline_info["sha256"],
                        current_hash=current_hash,
                        detected_at=datetime.now(timezone.utc).isoformat(),
                        details="File content has been modified",
                        file_size=stat_info.st_size,
                        last_modified=datetime.fromtimestamp(
                            stat_info.st_mtime, tz=timezone.utc
                        ).isoformat()
                    )
                    report.drift_items.append(drift_item)
                    report.drifted_files += 1
                else:
                    report.unchanged_files += 1
        
        # 检查新增文件
        for rel_path in current_files:
            if rel_path not in baseline_files:
                current_path = self.config_dir / rel_path
                stat_info = current_path.stat()
                
                drift_item = DriftItem(
                    file_path=rel_path,
                    severity=DriftSeverity.INFO.value,
                    drift_type=DriftType.ADDED.value,
                    baseline_hash="[NEW FILE]",
                    current_hash=self._calculate_file_hash(current_path),
                    detected_at=datetime.now(timezone.utc).isoformat(),
                    details="New file added since baseline",
                    file_size=stat_info.st_size,
                    last_modified=datetime.fromtimestamp(
                        stat_info.st_mtime, tz=timezone.utc
                    ).isoformat()
                )
                report.drift_items.append(drift_item)
                report.drifted_files += 1
                report.total_files_scanned += 1
        
        if self.verbose:
            print(f"\n{report.summary()}")
        
        return report
    
    def _get_severity_for_file(self, rel_path: str) -> str:
        """根据文件确定漂移严重程度"""
        if rel_path in self.SENSITIVE_FILES:
            return DriftSeverity.CRITICAL.value
        if any(important in rel_path for important in self.IMPORTANT_FILES):
            return DriftSeverity.WARNING.value
        if any(sec in rel_path for sec in ['security', 'kernel', 'model']):
            return DriftSeverity.WARNING.value
        return DriftSeverity.INFO.value
    
    def _get_config_files(self) -> List[Path]:
        """获取所有配置文件"""
        patterns = ['*.yaml', '*.yml', '*.json', '*.schema.json']
        files = []
        
        for pattern in patterns:
            for file_path in self.config_dir.rglob(pattern):
                # 检查是否应该忽略
                if self._should_ignore(file_path):
                    continue
                files.append(file_path)
        
        return sorted(set(files))
    
    def _should_ignore(self, file_path: Path) -> bool:
        """检查文件是否应该被忽略"""
        # 检查基线目录
        if ".baseline" in str(file_path):
            return True
        
        # 检查忽略模式
        for pattern in self.IGNORED_PATTERNS:
            if pattern.endswith('/'):
                if pattern[:-1] in file_path.parts:
                    return True
            elif pattern.startswith('*'):
                if file_path.name.endswith(pattern[1:]):
                    return True
            elif file_path.match(pattern):
                return True
        
        return False
    
    def _calculate_file_hash(self, file_path: Path) -> str:
        """计算文件SHA256哈希"""
        if not file_path.exists():
            return ""
        return hashlib.sha256(file_path.read_bytes()).hexdigest()
    
    def export_report_json(self, report: DriftReport, output_path: Path) -> None:
        """导出报告为JSON"""
        output_path.parent.mkdir(parents=True, exist_ok=True)
        
        data = {
            "scan_time": report.scan_time,
            "config_dir": report.config_dir,
            "baseline_created": report.baseline_created,
            "total_files_scanned": report.total_files_scanned,
            "drifted_files": report.drifted_files,
            "unchanged_files": report.unchanged_files,
            "has_drift": report.has_drift,
            "drift_rate": report.drift_rate,
            "drift_items": [item.to_dict() for item in report.drift_items]
        }
        
        output_path.write_text(
            json.dumps(data, indent=2, ensure_ascii=False),
            encoding='utf-8'
        )
        
        if self.verbose:
            print(f"📄 Full report saved to {output_path}")
    
    def export_report_markdown(self, report: DriftReport, output_path: Path) -> None:
        """导出报告为Markdown格式"""
        output_path.parent.mkdir(parents=True, exist_ok=True)
        
        md_content = f"""# Configuration Drift Report

**Scan Time**: {report.scan_time}  
**Config Directory**: {report.config_dir}  
**Baseline Created**: {report.baseline_created}  

## Summary

| Metric | Value |
|--------|-------|
| Total Files Scanned | {report.total_files_scanned} |
| Drifted Files | {report.drifted_files} |
| Unchanged Files | {report.unchanged_files} |
| Drift Rate | {report.drift_rate:.1f}% |
| Has Drift | {'Yes ⚠️' if report.has_drift else 'No ✅'} |

## Severity Breakdown

| Severity | Count |
|----------|-------|
| 🔴 Critical | {sum(1 for d in report.drift_items if d.severity == DriftSeverity.CRITICAL.value)} |
| 🟡 Warning | {sum(1 for d in report.drift_items if d.severity == DriftSeverity.WARNING.value)} |
| 🔵 Info | {sum(1 for d in report.drift_items if d.severity == DriftSeverity.INFO.value)} |

## Drift Details

"""
        
        if report.has_drift:
            # 按严重程度分组
            for severity in [DriftSeverity.CRITICAL, DriftSeverity.WARNING, DriftSeverity.INFO]:
                items = [i for i in report.drift_items if i.severity == severity.value]
                if items:
                    md_content += f"### {severity.value.upper()}\n\n"
                    for item in items:
                        icon = {"critical": "🔴", "warning": "🟡", "info": "🔵"}[severity.value]
                        md_content += f"- {icon} **{item.file_path}** ({item.drift_type})\n"
                        md_content += f"  - Details: {item.details}\n"
                        if item.drift_type != DriftType.ADDED.value:
                            md_content += f"  - Baseline Hash: `{item.baseline_hash[:16]}...`\n"
                        if item.drift_type != DriftType.DELETED.value:
                            md_content += f"  - Current Hash: `{item.current_hash[:16]}...`\n"
                        md_content += "\n"
        else:
            md_content += "**No configuration drift detected.** ✅\n"
        
        md_content += f"\n---\n*Generated by AgentOS Config Drift Detector v1.0*\n"
        
        output_path.write_text(md_content, encoding='utf-8')
        
        if self.verbose:
            print(f"📄 Markdown report saved to {output_path}")


def main():
    """CLI入口"""
    parser = argparse.ArgumentParser(
        description="AgentOS Configuration Drift Detector",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  # 创建基线
  python drift_detector.py --action create-baseline
  
  # 检测漂移
  python drift_detector.py --action detect
  
  # 一键执行（先建基线再检测）
  python drift_detector.py --action both --output drift_report.json
  
  # 详细模式
  python drift_detector.py --action detect --verbose --output report.json
        """
    )
    
    parser.add_argument(
        "--config-dir",
        type=Path,
        default=Path(__file__).parent.parent / "manager",
        help="Configuration directory path (default: ../manager)"
    )
    parser.add_argument(
        "--action",
        choices=["create-baseline", "detect", "both"],
        default="both",
        help="Action to perform (default: both)"
    )
    parser.add_argument(
        "--output",
        type=Path,
        default=Path("drift_report.json"),
        help="Output report path (default: drift_report.json)"
    )
    parser.add_argument(
        "--output-md",
        type=Path,
        help="Output Markdown report path (optional)"
    )
    parser.add_argument(
        "--verbose", "-v",
        action="store_true",
        help="Enable verbose output"
    )
    parser.add_argument(
        "--fail-on-drift",
        action="store_true",
        help="Exit with error code if drift is detected"
    )
    
    args = parser.parse_args()
    
    try:
        detector = ConfigDriftDetector(
            args.config_dir,
            verbose=args.verbose
        )
        
        if args.action in ["create-baseline", "both"]:
            detector.create_baseline()
        
        if args.action in ["detect", "both"]:
            report = detector.detect_drift()
            
            # 导出JSON报告
            detector.export_report_json(report, args.output)
            
            # 导出Markdown报告
            if args.output_md:
                detector.export_report_markdown(report, args.output_md)
            else:
                md_output = args.output.with_suffix('.md')
                detector.export_report_markdown(report, md_output)
            
            # 打印摘要
            print(f"\n{'='*70}")
            print(report.summary())
            print(f"{'='*70}\n")
            
            # 如果有漂移，列出详细信息
            if report.has_drift:
                print("Drift Details:\n")
                for item in report.drift_items:
                    icon = {"critical": "🔴", "warning": "🟡", "info": "🔵"}[item.severity]
                    print(f"{icon} [{item.severity.upper()}] {item.file_path} ({item.drift_type})")
                    print(f"   {item.details}")
                    if item.drift_type != DriftType.ADDED.value:
                        print(f"   Baseline: {item.baseline_hash[:16]}...")
                    if item.drift_type != DriftType.DELETED.value:
                        print(f"   Current:  {item.current_hash[:16]}...")
                    print()
            
            # 如果需要，在检测到漂移时返回错误码
            if args.fail_on_drift and report.has_drift:
                print("❌ Configuration drift detected!")
                sys.exit(1)
        
        print(f"\n✅ Drift detection completed successfully")
        
    except Exception as e:
        print(f"❌ Error: {e}")
        if args.verbose:
            import traceback
            traceback.print_exc()
        sys.exit(1)


if __name__ == "__main__":
    main()
