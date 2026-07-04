#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# SPDX-FileCopyrightText: 2026 SPHARX Ltd.
# SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0
"""
AgentRT Heapstore 数据格式迁移脚本

P3.20.2: 前向兼容迁移 (v1 → v2 非破坏性新增字段)
P3.20.3: 后向兼容回滚 (v2 → v1 丢弃新字段 → 保留核心数据)

用法:
    python3 heapstore_migrate.py forward --target v2
    python3 heapstore_migrate.py rollback --target v1 --force
    python3 heapstore_migrate.py status
"""

import json
import os
import shutil
import sys
import time
from datetime import datetime
from pathlib import Path
from typing import Any, Callable, Dict, List, Optional, Tuple

# ============================================================================
# Schema 定义
# ============================================================================

# 各 record 类型的字段定义（按版本）
SCHEMA_DEFINITIONS = {
    "agent": {
        "v1": ["id", "name", "type", "version", "status", "config_path",
                "created_at", "updated_at"],
        "v2": ["id", "name", "type", "version", "status", "config_path",
                "created_at", "updated_at", "priority", "tags"],
    },
    "session": {
        "v1": ["id", "user_id", "created_at", "last_active_at", "ttl_seconds", "status"],
        "v2": ["id", "user_id", "created_at", "last_active_at", "ttl_seconds", "status",
                "metadata"],
    },
    "skill": {
        "v1": ["id", "name", "version", "library_path", "manifest_path", "installed_at"],
        "v2": ["id", "name", "version", "library_path", "manifest_path", "installed_at",
                "checksum", "dependencies"],
    },
    "memory_pool": {
        "v1": ["pool_id", "name", "total_size", "used_size", "block_size",
                "block_count", "free_block_count", "created_at", "status"],
        "v2": ["pool_id", "name", "total_size", "used_size", "block_size",
                "block_count", "free_block_count", "created_at", "status",
                "max_size", "allocation_policy"],
    },
    "memory_allocation": {
        "v1": ["allocation_id", "pool_id", "size", "address",
                "allocated_at", "freed_at", "status"],
        "v2": ["allocation_id", "pool_id", "size", "address",
                "allocated_at", "freed_at", "status", "request_id", "trace_id"],
    },
    "ipc_channel": {
        "v1": ["channel_id", "name", "type", "status",
                "created_at", "last_activity_at", "buffer_size", "current_usage"],
        "v2": ["channel_id", "name", "type", "status",
                "created_at", "last_activity_at", "buffer_size", "current_usage",
                "backpressure_enabled", "max_message_size"],
    },
    "ipc_buffer": {
        "v1": ["buffer_id", "channel_id", "size", "used", "created_at", "status"],
        "v2": ["buffer_id", "channel_id", "size", "used", "created_at", "status",
                "priority", "expires_at"],
    },
    "span": {
        "v1": ["trace_id", "span_id", "parent_span_id", "name", "kind",
                "start_time_ns", "end_time_ns", "service_name", "status"],
        "v2": ["trace_id", "span_id", "parent_span_id", "name", "kind",
                "start_time_ns", "end_time_ns", "service_name", "status",
                "error_message", "resource_attributes"],
    },
}

# 默认值映射（v1 → v2 新增字段的默认值）
FORWARD_DEFAULTS = {
    "agent_priority": 0,
    "agent_tags": "",
    "session_metadata": "",
    "skill_checksum": "",
    "skill_dependencies": "",
    "memory_pool_max_size": 0,
    "memory_pool_allocation_policy": "best_fit",
    "memory_allocation_request_id": "",
    "memory_allocation_trace_id": "",
    "ipc_channel_backpressure_enabled": False,
    "ipc_channel_max_message_size": 65536,
    "ipc_buffer_priority": 0,
    "ipc_buffer_expires_at": 0,
    "span_error_message": "",
    "span_resource_attributes": "",
}


# ============================================================================
# 迁移引擎
# ============================================================================

class MigrationEngine:
    """Heapstore 数据格式迁移引擎"""

    def __init__(self, heapstore_root: str):
        self.root = Path(heapstore_root)
        self.backup_dir = self.root / ".migration_backups"
        self.version_file = self.root / ".schema_version"

    def get_current_version(self) -> int:
        """读取当前 Schema 版本"""
        if not self.version_file.exists():
            return 0
        try:
            return int(self.version_file.read_text().strip())
        except (ValueError, OSError):
            return 0

    def set_version(self, version: int) -> None:
        """写入 Schema 版本"""
        self.version_file.write_text(f"{version}\n")

    def create_backup(self) -> str:
        """创建数据备份"""
        timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
        backup_path = self.backup_dir / f"backup_{timestamp}"
        backup_path.mkdir(parents=True, exist_ok=True)

        # 备份 registry 数据
        registry_dir = self.root / "registry"
        if registry_dir.exists():
            shutil.copytree(registry_dir, backup_path / "registry",
                            dirs_exist_ok=True)

        # 备份版本文件
        if self.version_file.exists():
            shutil.copy2(self.version_file, backup_path / ".schema_version")

        return str(backup_path)

    def restore_backup(self, backup_path: str) -> None:
        """从备份恢复数据"""
        backup = Path(backup_path)
        if not backup.exists():
            raise FileNotFoundError(f"Backup not found: {backup_path}")

        registry_dir = self.root / "registry"
        backup_registry = backup / "registry"
        if backup_registry.exists():
            if registry_dir.exists():
                shutil.rmtree(registry_dir)
            shutil.copytree(backup_registry, registry_dir)

        backup_version = backup / ".schema_version"
        if backup_version.exists():
            shutil.copy2(backup_version, self.version_file)

    def migrate_record(self, record: Dict[str, Any], record_type: str,
                        source_version: str, target_version: str,
                        direction: str) -> Dict[str, Any]:
        """迁移单条记录"""
        schema = SCHEMA_DEFINITIONS.get(record_type)
        if not schema:
            return record  # 未知类型，保持不变

        source_fields = set(schema.get(source_version, []))
        target_fields = set(schema.get(target_version, []))

        if direction == "forward":
            # 新增字段用默认值填充
            new_fields = target_fields - source_fields
            for field in new_fields:
                default_key = f"{record_type}_{field}"
                record[field] = FORWARD_DEFAULTS.get(default_key, "")
        elif direction == "rollback":
            # 移除新增字段，保留核心字段
            removed_fields = source_fields - target_fields
            for field in removed_fields:
                record.pop(field, None)

        return record

    def migrate_data_file(self, file_path: Path, record_type: str,
                           source_version: str, target_version: str,
                           direction: str) -> Tuple[int, int]:
        """迁移数据文件（JSON Lines 格式）"""
        if not file_path.exists():
            return 0, 0

        records_migrated = 0
        records_skipped = 0

        # 读取所有记录
        lines = []
        try:
            with open(file_path, "r", encoding="utf-8") as f:
                for line in f:
                    line = line.strip()
                    if line and not line.startswith("#"):
                        try:
                            lines.append(json.loads(line))
                        except json.JSONDecodeError:
                            records_skipped += 1
        except Exception:
            return 0, 0

        # 迁移每条记录
        migrated = []
        for record in lines:
            try:
                new_record = self.migrate_record(
                    record, record_type, source_version, target_version, direction
                )
                migrated.append(new_record)
                records_migrated += 1
            except Exception:
                records_skipped += 1

        # 写回文件
        if migrated:
            with open(file_path, "w", encoding="utf-8") as f:
                for record in migrated:
                    f.write(json.dumps(record, ensure_ascii=False) + "\n")

        return records_migrated, records_skipped

    def run_forward(self, target_version: str = "v2") -> Dict[str, Any]:
        """P3.20.2: 执行前向兼容迁移 (v1 → v2)"""
        start_time = time.time()

        version_map = {"v1": 10000, "v2": 20000}
        target_ver = version_map.get(target_version, 20000)
        current_ver = self.get_current_version()

        report = {
            "direction": "forward",
            "from_version": "v1",
            "to_version": target_version,
            "steps": [],
            "success": True,
            "total_records_migrated": 0,
            "total_records_skipped": 0,
        }

        # 幂等性检查：如果当前版本已 >= 目标版本，跳过
        if current_ver >= target_ver:
            report["total_records_migrated"] = 0
            report["total_duration_ms"] = 0
            return report

        # 创建备份
        backup_path = self.create_backup()
        report["backup_path"] = backup_path

        try:
            # 遍历所有 record 类型
            registry_dir = self.root / "registry"
            for record_type in SCHEMA_DEFINITIONS:
                data_file = registry_dir / f"{record_type}s.dat"

                step_start = time.time()
                migrated, skipped = self.migrate_data_file(
                    data_file, record_type, "v1", target_version, "forward"
                )
                step_duration = (time.time() - step_start) * 1000

                step = {
                    "name": f"{record_type}: v1 → {target_version}",
                    "records_migrated": migrated,
                    "records_skipped": skipped,
                    "duration_ms": round(step_duration, 1),
                    "status": "OK",
                }
                report["steps"].append(step)
                report["total_records_migrated"] += migrated
                report["total_records_skipped"] += skipped

            # 更新版本号
            new_version = target_ver
            self.set_version(new_version)
            report["new_version"] = new_version

        except Exception as e:
            report["success"] = False
            report["error"] = str(e)
            # 恢复备份
            try:
                self.restore_backup(backup_path)
                report["rollback"] = "backup_restored"
            except Exception:
                report["rollback"] = "failed"

        report["total_duration_ms"] = round((time.time() - start_time) * 1000, 1)
        return report

    def run_rollback(self, target_version: str = "v1") -> Dict[str, Any]:
        """P3.20.3: 执行后向兼容回滚 (v2 → v1)"""
        start_time = time.time()

        version_map = {"v1": 10000, "v2": 20000}
        target_ver = version_map.get(target_version, 10000)
        current_ver = self.get_current_version()

        report = {
            "direction": "rollback",
            "from_version": "v2",
            "to_version": target_version,
            "steps": [],
            "success": True,
            "total_records_affected": 0,
            "total_fields_removed": 0,
        }

        # 幂等性检查：如果当前版本已 <= 目标版本，跳过
        if current_ver <= target_ver:
            report["total_records_affected"] = 0
            report["total_duration_ms"] = 0
            return report

        backup_path = self.create_backup()
        report["backup_path"] = backup_path

        try:
            registry_dir = self.root / "registry"
            for record_type in SCHEMA_DEFINITIONS:
                data_file = registry_dir / f"{record_type}s.dat"

                step_start = time.time()
                migrated, skipped = self.migrate_data_file(
                    data_file, record_type, "v2", target_version, "rollback"
                )
                step_duration = (time.time() - step_start) * 1000

                # 计算移除的字段数
                v2_fields = set(SCHEMA_DEFINITIONS[record_type]["v2"])
                v1_fields = set(SCHEMA_DEFINITIONS[record_type]["v1"])
                fields_removed = len(v2_fields - v1_fields)

                step = {
                    "name": f"{record_type}: v2 → {target_version}",
                    "records_affected": migrated + skipped,
                    "fields_removed": fields_removed,
                    "duration_ms": round(step_duration, 1),
                    "status": "OK",
                    "note": "New fields discarded, core data preserved",
                }
                report["steps"].append(step)
                report["total_records_affected"] += migrated + skipped
                report["total_fields_removed"] += fields_removed

            new_version = target_ver
            self.set_version(new_version)
            report["new_version"] = new_version

        except Exception as e:
            report["success"] = False
            report["error"] = str(e)
            try:
                self.restore_backup(backup_path)
                report["rollback"] = "backup_restored"
            except Exception:
                report["rollback"] = "failed"

        report["total_duration_ms"] = round((time.time() - start_time) * 1000, 1)
        return report

    def get_status(self) -> Dict[str, Any]:
        """获取当前迁移状态"""
        current_version = self.get_current_version()
        version_name = {0: "uninitialized", 10000: "v1", 20000: "v2"}

        status = {
            "current_version": current_version,
            "current_version_name": version_name.get(current_version, f"v{current_version}"),
            "latest_version": 20000,
            "latest_version_name": "v2",
            "needs_migration": current_version < 20000,
            "backups": [],
        }

        # 列出备份
        if self.backup_dir.exists():
            for backup in sorted(self.backup_dir.iterdir(), reverse=True):
                if backup.is_dir():
                    status["backups"].append({
                        "name": backup.name,
                        "created": datetime.fromtimestamp(
                            backup.stat().st_mtime
                        ).isoformat(),
                    })

        return status


# ============================================================================
# CLI 入口
# ============================================================================

def print_status(status: Dict[str, Any]) -> None:
    """打印迁移状态"""
    GREEN = "\033[32m"
    YELLOW = "\033[33m"
    RED = "\033[31m"
    CYAN = "\033[36m"
    RESET = "\033[0m"
    BOLD = "\033[1m"

    print(f"\n{BOLD}Heapstore Schema Migration Status{RESET}")
    print(f"{'=' * 50}")
    print(f"  Current version: {CYAN}{status['current_version_name']}{RESET} "
          f"({status['current_version']})")
    print(f"  Latest version:  {GREEN}{status['latest_version_name']}{RESET} "
          f"({status['latest_version']})")

    if status["needs_migration"]:
        print(f"  Status:          {YELLOW}Migration needed{RESET}")
    else:
        print(f"  Status:          {GREEN}Up to date{RESET}")

    if status["backups"]:
        print(f"\n  {BOLD}Backups:{RESET}")
        for b in status["backups"][:5]:
            print(f"    - {b['name']} ({b['created']})")
    print()


def print_report(report: Dict[str, Any]) -> None:
    """打印迁移报告"""
    GREEN = "\033[32m"
    YELLOW = "\033[33m"
    RED = "\033[31m"
    RESET = "\033[0m"
    BOLD = "\033[1m"

    direction = report["direction"]
    print(f"\n{BOLD}Migration Report ({direction}){RESET}")
    print(f"{'=' * 60}")
    print(f"  From:        v{report.get('from_version', '?')}")
    print(f"  To:          v{report.get('to_version', '?')}")
    print(f"  Duration:    {report.get('total_duration_ms', 0)}ms")
    print(f"  Backup:      {report.get('backup_path', 'N/A')}")

    if report["success"]:
        print(f"  Result:      {GREEN}SUCCESS{RESET}")
    else:
        print(f"  Result:      {RED}FAILED{RESET}")
        if "error" in report:
            print(f"  Error:       {report['error']}")

    if "steps" in report:
        print(f"\n  {BOLD}Steps:{RESET}")
        for step in report["steps"]:
            status_color = GREEN if step["status"] == "OK" else RED
            print(f"    [{status_color}{step['status']}{RESET}] {step['name']}")
            print(f"           {step.get('duration_ms', 0)}ms | "
                  f"records: {step.get('records_migrated', step.get('records_affected', 0))}")

    print(f"\n  Total records: {report.get('total_records_migrated', report.get('total_records_affected', 0))}")
    if "total_fields_removed" in report:
        print(f"  Fields removed: {report['total_fields_removed']}")
    print()


def main():
    import argparse

    parser = argparse.ArgumentParser(
        prog="heapstore_migrate",
        description="AgentRT Heapstore 数据格式迁移工具"
    )
    subparsers = parser.add_subparsers(dest="command", help="Commands")

    # status
    subparsers.add_parser("status", help="查看迁移状态")

    # forward
    forward_parser = subparsers.add_parser("forward", help="前向兼容迁移 (v1 → v2)")
    forward_parser.add_argument("--target", default="v2", choices=["v2"],
                                help="目标版本")
    forward_parser.add_argument("--root", default=None,
                                help="Heapstore 数据根目录")

    # rollback
    rollback_parser = subparsers.add_parser("rollback", help="后向兼容回滚 (v2 → v1)")
    rollback_parser.add_argument("--target", default="v1", choices=["v1"],
                                 help="目标版本")
    rollback_parser.add_argument("--force", action="store_true",
                                 help="确认执行回滚")
    rollback_parser.add_argument("--root", default=None,
                                 help="Heapstore 数据根目录")

    args = parser.parse_args()

    if not args.command:
        parser.print_help()
        return 0

    # 确定 heapstore 根目录
    root = getattr(args, "root", None)
    if not root:
        root = os.environ.get(
            "AGENTOS_HEAPSTORE_ROOT",
            os.path.expanduser("~/.agentrt/heapstore")
        )

    engine = MigrationEngine(root)

    if args.command == "status":
        status = engine.get_status()
        print_status(status)
        return 0

    elif args.command == "forward":
        print(f"Running forward migration to {args.target}...")
        print(f"Data directory: {root}")
        report = engine.run_forward(args.target)
        print_report(report)
        return 0 if report["success"] else 1

    elif args.command == "rollback":
        if not args.force:
            print("ERROR: Rollback requires --force flag for safety.")
            print("This will discard new fields added in v2.")
            print("Usage: python3 heapstore_migrate.py rollback --force")
            return 1

        print(f"Running rollback to {args.target}...")
        print(f"Data directory: {root}")
        report = engine.run_rollback(args.target)
        print_report(report)
        return 0 if report["success"] else 1

    return 0


if __name__ == "__main__":
    sys.exit(main())