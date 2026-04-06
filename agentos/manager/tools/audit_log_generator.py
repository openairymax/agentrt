#!/usr/bin/env python3
"""
AgentOS Config Audit Log Generator

用于生成测试用的配置变更审计日志，符合config-audit-log.schema.json规范

Usage:
    python audit_log_generator.py --config-dir ../ --output sample_audit.json --count 10
    python audit_log_generator.py --action CHANGE --file kernel/settings.yaml --reason "测试"
"""

import argparse
import hashlib
import json
import random
import uuid
from dataclasses import dataclass, field, asdict
from datetime import datetime, timezone, timedelta
from enum import Enum
from pathlib import Path
from typing import Any, Dict, List, Optional


class ActionType(Enum):
    """审计动作类型"""
    LOAD = "LOAD"
    RELOAD = "RELOAD"
    CHANGE = "CHANGE"
    ROLLBACK = "ROLLBACK"
    VALIDATE = "VALIDATE"
    EXPORT = "EXPORT"
    IMPORT = "IMPORT"


class OperatorType(Enum):
    """操作者类型"""
    USER = "user"
    SYSTEM = "system"
    CI_CD = "ci_cd"


@dataclass
class Operator:
    """操作者信息"""
    type: str
    identity: str
    ip_address: Optional[str] = None
    session_id: Optional[str] = None
    
    def to_dict(self) -> Dict[str, Any]:
        result = {
            "type": self.type,
            "identity": self.identity
        }
        if self.ip_address:
            result["ip_address"] = self.ip_address
        if self.session_id:
            result["session_id"] = self.session_id
        return result


@dataclass
class ChangeItem:
    """变更项"""
    path: str
    old_value: Any
    new_value: Any
    field_type: Optional[str] = None
    
    def to_dict(self) -> Dict[str, Any]:
        result = {
            "path": self.path,
            "old_value": self.old_value,
            "new_value": self.new_value
        }
        if self.field_type:
            result["field_type"] = self.field_type
        return result


@dataclass
class Checksum:
    """校验和"""
    algorithm: str = "sha256"
    before: str = ""
    after: str = ""
    
    def to_dict(self) -> Dict[str, Any]:
        return {
            "algorithm": self.algorithm,
            "before": self.before,
            "after": self.after
        }


@dataclass
class Metadata:
    """元数据"""
    environment: str = "production"
    version: str = "1.0.0.14"
    correlation_id: Optional[str] = None
    source: Optional[str] = None
    reason: Optional[str] = None
    approved_by: Optional[str] = None
    ticket_id: Optional[str] = None
    
    def to_dict(self) -> Dict[str, Any]:
        result = {
            "environment": self.environment,
            "version": self.version
        }
        if self.correlation_id:
            result["correlation_id"] = self.correlation_id
        if self.source:
            result["source"] = self.source
        if self.reason:
            result["reason"] = self.reason
        if self.approved_by:
            result["approved_by"] = self.approved_by
        if self.ticket_id:
            result["ticket_id"] = self.ticket_id
        return result


@dataclass
class Result:
    """操作结果"""
    success: bool = True
    error_code: Optional[str] = None
    error_message: Optional[str] = None
    duration_ms: int = 0
    
    def to_dict(self) -> Dict[str, Any]:
        result = {
            "success": self.success,
            "duration_ms": self.duration_ms
        }
        if not self.success:
            if self.error_code:
                result["error_code"] = self.error_code
            if self.error_message:
                result["error_message"] = self.error_message
        return result


@dataclass
class AuditLogEntry:
    """审计日志条目"""
    timestamp: str
    action: str
    config_file: str
    operator: Operator
    checksum: Checksum
    changes: List[ChangeItem] = field(default_factory=list)
    metadata: Optional[Metadata] = None
    result: Optional[Result] = None
    
    def to_dict(self) -> Dict[str, Any]:
        entry = {
            "timestamp": self.timestamp,
            "action": self.action,
            "config_file": self.config_file,
            "operator": self.operator.to_dict(),
            "changes": [c.to_dict() for c in self.changes],
            "checksum": self.checksum.to_dict()
        }
        if self.metadata:
            entry["metadata"] = self.metadata.to_dict()
        if self.result:
            entry["result"] = self.result.to_dict()
        return entry


class AuditLogGenerator:
    """审计日志生成器"""
    
    CONFIG_FILES = [
        "kernel/settings.yaml",
        "model/model.yaml",
        "agent/registry.yaml",
        "skill/registry.yaml",
        "security/policy.yaml",
        "security/permission_rules.yaml",
        "logging/manager.yaml",
        "environment/production.yaml",
        "environment/staging.yaml",
        "environment/development.yaml"
    ]
    
    USER_NAMES = [
        "admin", "devops_engineer", "security_admin",
        "agent_developer", "system_architect", "tech_lead"
    ]
    
    SYSTEM_COMPONENTS = [
        "config-manager", "file-watcher", "schema-validator",
        "auto-rollback", "periodic-check"
    ]
    
    CI_CD_SYSTEMS = [
        "github-actions", "jenkins", "gitlab-ci", "circleci"
    ]
    
    CHANGE_REASONS = [
        "调试生产环境问题",
        "提升系统性能",
        "增强安全配置",
        "更新模型配置",
        "调整并发参数",
        "优化日志级别",
        "修复配置错误",
        "部署新版本"
    ]
    
    def __init__(self, config_dir: Path):
        self.config_dir = config_dir
        self._file_hashes: Dict[str, str] = {}
        
    def generate_entry(
        self,
        action: Optional[str] = None,
        config_file: Optional[str] = None,
        operator_type: Optional[str] = None,
        environment: str = "production",
        reason: Optional[str] = None,
        include_changes: bool = True
    ) -> AuditLogEntry:
        """
        生成单个审计日志条目
        
        Args:
            action: 动作类型，None则随机选择
            config_file: 配置文件路径，None则随机选择
            operator_type: 操作者类型，None则随机选择
            environment: 环境名称
            reason: 变更原因
            include_changes: 是否包含变更详情
            
        Returns:
            AuditLogEntry: 审计日志条目
        """
        if action is None:
            action = random.choice(list(ActionType)).value
        if config_file is None:
            config_file = random.choice(self.CONFIG_FILES)
        if operator_type is None:
            operator_type = random.choice(list(OperatorType)).value
            
        timestamp = datetime.now(timezone.utc).isoformat()
        
        operator = self._generate_operator(operator_type)
        
        checksum = self._generate_checksum(config_file)
        
        changes = []
        if include_changes and action in [ActionType.CHANGE.value, ActionType.ROLLBACK.value, ActionType.IMPORT.value]:
            changes = self._generate_changes(config_file)
            
        metadata = Metadata(
            environment=environment,
            correlation_id=str(uuid.uuid4()),
            source=self._get_source_for_action(action),
            reason=reason or (random.choice(self.CHANGE_REASONS) if action == ActionType.CHANGE.value else None),
            approved_by=random.choice(["tech_lead", "architect", "ciso"]) if action == ActionType.CHANGE.value else None,
            ticket_id=f"{random.choice(['OPS', 'SEC', 'DEPLOY'])}-2026-{random.randint(1000, 9999)}" if random.random() > 0.5 else None
        )
        
        success = random.random() > 0.1
        result = Result(
            success=success,
            duration_ms=random.randint(10, 300),
            error_code="SCHEMA_VIOLATION" if not success else None,
            error_message="Field validation failed" if not success else None
        )
        
        return AuditLogEntry(
            timestamp=timestamp,
            action=action,
            config_file=config_file,
            operator=operator,
            checksum=checksum,
            changes=changes,
            metadata=metadata,
            result=result
        )
    
    def generate_batch(
        self,
        count: int = 10,
        environment: str = "production"
    ) -> List[AuditLogEntry]:
        """
        批量生成审计日志
        
        Args:
            count: 生成数量
            environment: 环境名称
            
        Returns:
            List[AuditLogEntry]: 审计日志列表
        """
        entries = []
        
        for i in range(count):
            if i == 0:
                action = ActionType.LOAD.value
            elif i % 5 == 0:
                action = ActionType.VALIDATE.value
            elif i % 7 == 0:
                action = ActionType.RELOAD.value
            elif i % 10 == 0:
                action = ActionType.ROLLBACK.value
            else:
                action = ActionType.CHANGE.value
                
            entry = self.generate_entry(
                action=action,
                environment=environment
            )
            entries.append(entry)
            
        return entries
    
    def _generate_operator(self, operator_type: str) -> Operator:
        """生成操作者信息"""
        if operator_type == OperatorType.USER.value:
            return Operator(
                type=operator_type,
                identity=random.choice(self.USER_NAMES),
                ip_address=f"192.168.{random.randint(1, 255)}.{random.randint(1, 255)}",
                session_id=str(uuid.uuid4())
            )
        elif operator_type == OperatorType.SYSTEM.value:
            return Operator(
                type=operator_type,
                identity=random.choice(self.SYSTEM_COMPONENTS)
            )
        else:
            return Operator(
                type=operator_type,
                identity=random.choice(self.CI_CD_SYSTEMS)
            )
    
    def _generate_checksum(self, config_file: str) -> Checksum:
        """生成校验和"""
        before_hash = self._file_hashes.get(config_file, self._random_hash())
        after_hash = self._random_hash()
        
        self._file_hashes[config_file] = after_hash
        
        return Checksum(
            algorithm="sha256",
            before=before_hash,
            after=after_hash
        )
    
    def _generate_changes(self, config_file: str) -> List[ChangeItem]:
        """生成变更项"""
        changes = []
        
        if "kernel" in config_file:
            changes.append(ChangeItem(
                path="kernel.log_level",
                old_value=random.choice(["info", "warning", "error"]),
                new_value=random.choice(["debug", "info", "warning"]),
                field_type="string"
            ))
        elif "model" in config_file:
            changes.append(ChangeItem(
                path="model.primary.provider",
                old_value=random.choice(["openai", "anthropic"]),
                new_value=random.choice(["openai", "anthropic", "google"]),
                field_type="string"
            ))
        elif "security" in config_file:
            changes.append(ChangeItem(
                path="security.sandbox.default_isolation",
                old_value=random.choice(["none", "process"]),
                new_value=random.choice(["process", "container"]),
                field_type="string"
            ))
        elif "agent" in config_file:
            changes.append(ChangeItem(
                path="agents[0].max_concurrent_tasks",
                old_value=random.randint(1, 10),
                new_value=random.randint(5, 20),
                field_type="integer"
            ))
        else:
            changes.append(ChangeItem(
                path="config.updated_at",
                old_value=datetime.now(timezone.utc).isoformat(),
                new_value=datetime.now(timezone.utc).isoformat(),
                field_type="string"
            ))
            
        return changes
    
    def _get_source_for_action(self, action: str) -> str:
        """根据动作获取来源"""
        sources = {
            ActionType.LOAD.value: "system_startup",
            ActionType.RELOAD.value: "file_watcher",
            ActionType.CHANGE.value: "manual",
            ActionType.ROLLBACK.value: "validation_failure",
            ActionType.VALIDATE.value: "periodic_check",
            ActionType.EXPORT.value: "manual",
            ActionType.IMPORT.value: "deployment_pipeline"
        }
        return sources.get(action, "unknown")
    
    def _random_hash(self) -> str:
        """生成随机SHA256哈希"""
        return hashlib.sha256(str(random.random()).encode()).hexdigest()
    
    def export_to_json(
        self,
        entries: List[AuditLogEntry],
        output_path: Path
    ) -> None:
        """导出为JSON文件"""
        data = [entry.to_dict() for entry in entries]
        
        output_path.parent.mkdir(parents=True, exist_ok=True)
        output_path.write_text(
            json.dumps(data, indent=2, ensure_ascii=False),
            encoding='utf-8'
        )
        
        print(f"✅ Generated {len(entries)} audit log entries to {output_path}")


def main():
    """CLI入口"""
    parser = argparse.ArgumentParser(
        description="Generate AgentOS config audit logs",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  # 生成10条随机审计日志
  python audit_log_generator.py --count 10 --output audit_log.json

  # 生成特定动作的审计日志
  python audit_log_generator.py --action CHANGE --file kernel/settings.yaml --reason "测试变更"

  # 批量生成不同环境的日志
  python audit_log_generator.py --count 20 --environment staging --output staging_audit.json
        """
    )
    
    parser.add_argument(
        "--config-dir",
        type=Path,
        default=Path(__file__).parent.parent,
        help="Configuration directory path (default: ../)"
    )
    parser.add_argument(
        "--output",
        type=Path,
        default=Path("sample_audit_log.json"),
        help="Output audit log file path (default: sample_audit_log.json)"
    )
    parser.add_argument(
        "--count",
        type=int,
        default=10,
        help="Number of log entries to generate (default: 10)"
    )
    parser.add_argument(
        "--action",
        choices=[a.value for a in ActionType],
        help="Specific action type (default: random)"
    )
    parser.add_argument(
        "--file",
        help="Specific config file path (default: random)"
    )
    parser.add_argument(
        "--operator-type",
        choices=[o.value for o in OperatorType],
        help="Specific operator type (default: random)"
    )
    parser.add_argument(
        "--environment",
        choices=["development", "staging", "production"],
        default="production",
        help="Environment name (default: production)"
    )
    parser.add_argument(
        "--reason",
        help="Change reason description"
    )
    parser.add_argument(
        "--no-changes",
        action="store_true",
        help="Do not include change details"
    )
    
    args = parser.parse_args()
    
    generator = AuditLogGenerator(args.config_dir)
    
    if args.action or args.file:
        entry = generator.generate_entry(
            action=args.action,
            config_file=args.file,
            operator_type=args.operator_type,
            environment=args.environment,
            reason=args.reason,
            include_changes=not args.no_changes
        )
        entries = [entry]
    else:
        entries = generator.generate_batch(
            count=args.count,
            environment=args.environment
        )
    
    generator.export_to_json(entries, args.output)
    
    print(f"\n📊 Summary:")
    print(f"  - Total entries: {len(entries)}")
    print(f"  - Environment: {args.environment}")
    print(f"  - Actions: {', '.join(set(e.action for e in entries))}")
    print(f"  - Output: {args.output}")


if __name__ == "__main__":
    main()
