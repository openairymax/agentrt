#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# SPDX-FileCopyrightText: 2026 SPHARX Ltd.
# SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0
"""
P3.20 生产数据库迁移策略 — 测试套件

测试覆盖:
  - P3.20.1: Schema 版本化 (version file read/write)
  - P3.20.2: 前向兼容迁移 (v1 → v2 非破坏性新增字段)
  - P3.20.3: 后向兼容回滚 (v2 → v1 丢弃新字段，保留核心数据)
  - P3.20.4: CLI 命令集成 (agentrt db migrate)
"""

import json
import os
import shutil
import sys
import tempfile
import unittest
from pathlib import Path

# 添加 toolkit src 到路径
sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "..", "..",
                                "scripts", "toolkit", "src"))

from heapstore_migrate import (
    FORWARD_DEFAULTS,
    SCHEMA_DEFINITIONS,
    MigrationEngine,
)


class TestP3_20_1_SchemaVersioning(unittest.TestCase):
    """P3.20.1: Schema 版本化测试"""

    def setUp(self):
        self.temp_dir = tempfile.mkdtemp()
        self.engine = MigrationEngine(self.temp_dir)

    def tearDown(self):
        shutil.rmtree(self.temp_dir, ignore_errors=True)

    def test_version_file_not_exists_returns_0(self):
        """版本文件不存在时返回 0（未初始化）"""
        version = self.engine.get_current_version()
        self.assertEqual(version, 0)

    def test_set_and_get_version(self):
        """设置和读取版本号"""
        self.engine.set_version(10000)
        self.assertEqual(self.engine.get_current_version(), 10000)

        self.engine.set_version(20000)
        self.assertEqual(self.engine.get_current_version(), 20000)

    def test_version_file_content(self):
        """版本文件内容格式正确"""
        self.engine.set_version(10000)
        content = (Path(self.temp_dir) / ".schema_version").read_text()
        self.assertIn("10000", content)

    def test_version_invalid_file_content(self):
        """版本文件内容无效时返回 0"""
        version_file = Path(self.temp_dir) / ".schema_version"
        version_file.write_text("invalid")
        version = self.engine.get_current_version()
        self.assertEqual(version, 0)

    def test_status_uninitialized(self):
        """未初始化时状态正确"""
        status = self.engine.get_status()
        self.assertEqual(status["current_version"], 0)
        self.assertEqual(status["current_version_name"], "uninitialized")
        self.assertTrue(status["needs_migration"])

    def test_status_up_to_date(self):
        """已是最新版本时状态正确"""
        self.engine.set_version(20000)
        status = self.engine.get_status()
        self.assertEqual(status["current_version"], 20000)
        self.assertFalse(status["needs_migration"])

    def test_status_needs_migration(self):
        """版本落后时需要迁移"""
        self.engine.set_version(10000)
        status = self.engine.get_status()
        self.assertEqual(status["current_version"], 10000)
        self.assertTrue(status["needs_migration"])


class TestP3_20_2_ForwardMigration(unittest.TestCase):
    """P3.20.2: 前向兼容迁移测试"""

    def setUp(self):
        self.temp_dir = tempfile.mkdtemp()
        self.engine = MigrationEngine(self.temp_dir)
        # 创建 v1 格式的测试数据
        self._create_v1_test_data()

    def tearDown(self):
        shutil.rmtree(self.temp_dir, ignore_errors=True)

    def _create_v1_test_data(self):
        """创建 v1 格式的测试数据"""
        registry_dir = Path(self.temp_dir) / "registry"
        registry_dir.mkdir(parents=True, exist_ok=True)

        # Agent v1 数据
        agents = [
            {
                "id": "agent-001",
                "name": "TestAgent",
                "type": "llm",
                "version": "1.0.0",
                "status": "active",
                "config_path": "/etc/agentrt/agents/001.yaml",
                "created_at": 1700000000,
                "updated_at": 1700000000,
            }
        ]
        with open(registry_dir / "agents.dat", "w") as f:
            for agent in agents:
                f.write(json.dumps(agent) + "\n")

        # Session v1 数据
        sessions = [
            {
                "id": "session-001",
                "user_id": "user-001",
                "created_at": 1700000000,
                "last_active_at": 1700000100,
                "ttl_seconds": 3600,
                "status": "active",
            }
        ]
        with open(registry_dir / "sessions.dat", "w") as f:
            for session in sessions:
                f.write(json.dumps(session) + "\n")

        # 设置版本为 v1
        self.engine.set_version(10000)

    def test_forward_migration_adds_new_fields(self):
        """前向迁移：v1 → v2 添加新字段"""
        report = self.engine.run_forward("v2")

        self.assertTrue(report["success"], f"Migration failed: {report.get('error')}")
        self.assertEqual(report["from_version"], "v1")
        self.assertEqual(report["to_version"], "v2")

        # 验证版本号已更新
        self.assertEqual(self.engine.get_current_version(), 20000)

        # 验证 agent 数据包含新字段
        registry_dir = Path(self.temp_dir) / "registry"
        with open(registry_dir / "agents.dat", "r") as f:
            for line in f:
                line = line.strip()
                if line and not line.startswith("#"):
                    agent = json.loads(line)
                    # v1 核心字段保留
                    self.assertIn("id", agent)
                    self.assertIn("name", agent)
                    self.assertIn("type", agent)
                    # v2 新增字段存在
                    self.assertIn("priority", agent)
                    self.assertIn("tags", agent)
                    # 新字段有默认值
                    self.assertEqual(agent["priority"], 0)
                    self.assertEqual(agent["tags"], "")

        # 验证 session 数据包含新字段
        with open(registry_dir / "sessions.dat", "r") as f:
            for line in f:
                line = line.strip()
                if line and not line.startswith("#"):
                    session = json.loads(line)
                    self.assertIn("id", session)
                    self.assertIn("user_id", session)
                    # v2 新增字段
                    self.assertIn("metadata", session)
                    self.assertEqual(session["metadata"], "")

    def test_forward_migration_creates_backup(self):
        """前向迁移创建备份"""
        report = self.engine.run_forward("v2")
        self.assertTrue(report["success"])
        self.assertIn("backup_path", report)
        self.assertTrue(os.path.exists(report["backup_path"]))

    def test_forward_migration_preserves_core_data(self):
        """前向迁移保留核心数据"""
        original_version = self.engine.get_current_version()
        self.assertEqual(original_version, 10000)

        report = self.engine.run_forward("v2")
        self.assertTrue(report["success"])

        # 验证原始数据仍在
        registry_dir = Path(self.temp_dir) / "registry"
        with open(registry_dir / "agents.dat", "r") as f:
            content = f.read()
            self.assertIn("agent-001", content)
            self.assertIn("TestAgent", content)

    def test_forward_migration_idempotent(self):
        """前向迁移幂等：多次执行不损坏数据"""
        # 第一次迁移
        report1 = self.engine.run_forward("v2")
        self.assertTrue(report1["success"])

        # 第二次迁移（应跳过）
        report2 = self.engine.run_forward("v2")
        self.assertTrue(report2["success"])
        # 第二次无实际操作
        self.assertEqual(report2["total_records_migrated"], 0)

    def test_forward_migration_all_record_types(self):
        """前向迁移覆盖所有 record 类型"""
        # 为所有类型创建测试数据
        registry_dir = Path(self.temp_dir) / "registry"
        for record_type in SCHEMA_DEFINITIONS:
            if record_type in ("agent", "session"):
                continue  # 已创建
            data_file = registry_dir / f"{record_type}s.dat"
            v1_fields = SCHEMA_DEFINITIONS[record_type]["v1"]
            record = {f: f"test_{f}" for f in v1_fields}
            with open(data_file, "w") as f:
                f.write(json.dumps(record) + "\n")

        report = self.engine.run_forward("v2")
        self.assertTrue(report["success"])

        # 验证每类 record 都有对应的迁移步骤
        step_names = [s["name"] for s in report["steps"]]
        for record_type in SCHEMA_DEFINITIONS:
            self.assertTrue(
                any(record_type in name for name in step_names),
                f"Missing migration step for {record_type}"
            )


class TestP3_20_3_BackwardRollback(unittest.TestCase):
    """P3.20.3: 后向兼容回滚测试"""

    def setUp(self):
        self.temp_dir = tempfile.mkdtemp()
        self.engine = MigrationEngine(self.temp_dir)
        self._create_v2_test_data()

    def tearDown(self):
        shutil.rmtree(self.temp_dir, ignore_errors=True)

    def _create_v2_test_data(self):
        """创建 v2 格式的测试数据"""
        registry_dir = Path(self.temp_dir) / "registry"
        registry_dir.mkdir(parents=True, exist_ok=True)

        # Agent v2 数据（包含新字段 priority, tags）
        agents = [
            {
                "id": "agent-001",
                "name": "TestAgent",
                "type": "llm",
                "version": "1.0.0",
                "status": "active",
                "config_path": "/etc/agentrt/agents/001.yaml",
                "created_at": 1700000000,
                "updated_at": 1700000000,
                "priority": 5,
                "tags": "production,llm",
            }
        ]
        with open(registry_dir / "agents.dat", "w") as f:
            for agent in agents:
                f.write(json.dumps(agent) + "\n")

        # Session v2 数据（包含新字段 metadata）
        sessions = [
            {
                "id": "session-001",
                "user_id": "user-001",
                "created_at": 1700000000,
                "last_active_at": 1700000100,
                "ttl_seconds": 3600,
                "status": "active",
                "metadata": '{"source": "web"}',
            }
        ]
        with open(registry_dir / "sessions.dat", "w") as f:
            for session in sessions:
                f.write(json.dumps(session) + "\n")

        self.engine.set_version(20000)

    def test_rollback_removes_new_fields(self):
        """回滚：v2 → v1 移除新字段"""
        report = self.engine.run_rollback("v1")

        self.assertTrue(report["success"], f"Rollback failed: {report.get('error')}")
        self.assertEqual(report["from_version"], "v2")
        self.assertEqual(report["to_version"], "v1")

        # 验证版本号已更新
        self.assertEqual(self.engine.get_current_version(), 10000)

        # 验证 agent 数据不包含 v2 新字段
        registry_dir = Path(self.temp_dir) / "registry"
        with open(registry_dir / "agents.dat", "r") as f:
            for line in f:
                line = line.strip()
                if line and not line.startswith("#"):
                    agent = json.loads(line)
                    # 核心字段保留
                    self.assertIn("id", agent)
                    self.assertIn("name", agent)
                    # v2 新字段已移除
                    self.assertNotIn("priority", agent)
                    self.assertNotIn("tags", agent)

        # 验证 session 数据不包含 v2 新字段
        with open(registry_dir / "sessions.dat", "r") as f:
            for line in f:
                line = line.strip()
                if line and not line.startswith("#"):
                    session = json.loads(line)
                    self.assertIn("id", session)
                    self.assertIn("user_id", session)
                    self.assertNotIn("metadata", session)

    def test_rollback_preserves_core_data(self):
        """回滚保留核心数据"""
        report = self.engine.run_rollback("v1")
        self.assertTrue(report["success"])

        registry_dir = Path(self.temp_dir) / "registry"
        with open(registry_dir / "agents.dat", "r") as f:
            content = f.read()
            self.assertIn("agent-001", content)
            self.assertIn("TestAgent", content)
            self.assertIn("llm", content)

    def test_rollback_creates_backup(self):
        """回滚创建备份"""
        report = self.engine.run_rollback("v1")
        self.assertTrue(report["success"])
        self.assertIn("backup_path", report)
        self.assertTrue(os.path.exists(report["backup_path"]))

    def test_rollback_idempotent(self):
        """回滚幂等：多次执行不损坏数据"""
        report1 = self.engine.run_rollback("v1")
        self.assertTrue(report1["success"])

        report2 = self.engine.run_rollback("v1")
        self.assertTrue(report2["success"])
        self.assertEqual(report2["total_records_affected"], 0)

    def test_rollback_removes_all_v2_fields(self):
        """回滚移除所有 v2 新增字段"""
        # 为所有类型创建 v2 数据
        registry_dir = Path(self.temp_dir) / "registry"
        for record_type in SCHEMA_DEFINITIONS:
            if record_type in ("agent", "session"):
                continue
            data_file = registry_dir / f"{record_type}s.dat"
            v2_fields = SCHEMA_DEFINITIONS[record_type]["v2"]
            record = {f: f"test_{f}" for f in v2_fields}
            with open(data_file, "w") as f:
                f.write(json.dumps(record) + "\n")

        report = self.engine.run_rollback("v1")
        self.assertTrue(report["success"])

        # 每种类型的新字段都被移除
        total_fields_removed = 0
        for step in report["steps"]:
            total_fields_removed += step.get("fields_removed", 0)
        self.assertGreater(total_fields_removed, 0)


class TestP3_20_4_BackupRestore(unittest.TestCase):
    """P3.20.4: 备份与恢复测试"""

    def setUp(self):
        self.temp_dir = tempfile.mkdtemp()
        self.engine = MigrationEngine(self.temp_dir)
        self._create_test_data()

    def tearDown(self):
        shutil.rmtree(self.temp_dir, ignore_errors=True)

    def _create_test_data(self):
        registry_dir = Path(self.temp_dir) / "registry"
        registry_dir.mkdir(parents=True, exist_ok=True)
        with open(registry_dir / "test.dat", "w") as f:
            f.write(json.dumps({"key": "value"}) + "\n")
        self.engine.set_version(10000)

    def test_backup_creation(self):
        """备份创建成功"""
        backup_path = self.engine.create_backup()
        self.assertTrue(os.path.exists(backup_path))
        self.assertTrue(os.path.isdir(backup_path))

    def test_backup_contains_registry_data(self):
        """备份包含 registry 数据"""
        backup_path = self.engine.create_backup()
        backup_registry = Path(backup_path) / "registry"
        self.assertTrue(backup_registry.exists())
        self.assertTrue((backup_registry / "test.dat").exists())

    def test_backup_contains_version_file(self):
        """备份包含版本文件"""
        backup_path = self.engine.create_backup()
        backup_version = Path(backup_path) / ".schema_version"
        self.assertTrue(backup_version.exists())

    def test_restore_from_backup(self):
        """从备份恢复数据"""
        backup_path = self.engine.create_backup()

        # 修改原始数据
        registry_dir = Path(self.temp_dir) / "registry"
        with open(registry_dir / "test.dat", "w") as f:
            f.write(json.dumps({"key": "modified"}) + "\n")
        self.engine.set_version(20000)

        # 恢复备份
        self.engine.restore_backup(backup_path)

        # 验证恢复
        with open(registry_dir / "test.dat", "r") as f:
            data = json.loads(f.readline())
            self.assertEqual(data["key"], "value")

        self.assertEqual(self.engine.get_current_version(), 10000)


class TestMigrationEdgeCases(unittest.TestCase):
    """边界条件测试"""

    def setUp(self):
        self.temp_dir = tempfile.mkdtemp()
        self.engine = MigrationEngine(self.temp_dir)

    def tearDown(self):
        shutil.rmtree(self.temp_dir, ignore_errors=True)

    def test_empty_registry_no_error(self):
        """空 registry 目录迁移不报错"""
        registry_dir = Path(self.temp_dir) / "registry"
        registry_dir.mkdir(parents=True, exist_ok=True)
        self.engine.set_version(10000)

        report = self.engine.run_forward("v2")
        self.assertTrue(report["success"])

    def test_no_registry_dir_no_error(self):
        """无 registry 目录迁移不报错"""
        self.engine.set_version(10000)

        report = self.engine.run_forward("v2")
        self.assertTrue(report["success"])

    def test_forward_migration_with_invalid_json(self):
        """包含无效 JSON 行时迁移继续"""
        registry_dir = Path(self.temp_dir) / "registry"
        registry_dir.mkdir(parents=True, exist_ok=True)

        with open(registry_dir / "agents.dat", "w") as f:
            f.write(json.dumps({"id": "agent-001", "name": "Test", "type": "llm",
                                "version": "1.0", "status": "active",
                                "config_path": "/etc", "created_at": 1,
                                "updated_at": 1}) + "\n")
            f.write("this is not valid json\n")
            f.write(json.dumps({"id": "agent-002", "name": "Test2", "type": "tool",
                                "version": "1.0", "status": "active",
                                "config_path": "/etc", "created_at": 2,
                                "updated_at": 2}) + "\n")

        self.engine.set_version(10000)
        report = self.engine.run_forward("v2")
        self.assertTrue(report["success"])

        # 有效记录被迁移
        with open(registry_dir / "agents.dat", "r") as f:
            lines = [l.strip() for l in f if l.strip() and not l.startswith("#")]
            self.assertEqual(len(lines), 2)
            for line in lines:
                record = json.loads(line)
                self.assertIn("priority", record)

    def test_rollback_with_invalid_json(self):
        """回滚时包含无效 JSON 行继续"""
        registry_dir = Path(self.temp_dir) / "registry"
        registry_dir.mkdir(parents=True, exist_ok=True)

        with open(registry_dir / "agents.dat", "w") as f:
            f.write(json.dumps({"id": "agent-001", "name": "Test", "type": "llm",
                                "version": "1.0", "status": "active",
                                "config_path": "/etc", "created_at": 1,
                                "updated_at": 1, "priority": 5, "tags": "prod"}) + "\n")
            f.write("invalid json\n")

        self.engine.set_version(20000)
        report = self.engine.run_rollback("v1")
        self.assertTrue(report["success"])

        with open(registry_dir / "agents.dat", "r") as f:
            for line in f:
                line = line.strip()
                if line and not line.startswith("#"):
                    try:
                        record = json.loads(line)
                        self.assertNotIn("priority", record)
                    except json.JSONDecodeError:
                        pass

    def test_migrate_unknown_record_type(self):
        """未知 record type 的迁移返回原样"""
        record = {"custom_field": "value"}
        result = self.engine.migrate_record(record, "unknown_type", "v1", "v2", "forward")
        self.assertEqual(result, record)

    def test_forward_defaults_complete(self):
        """所有 v2 新增字段都有默认值"""
        for record_type, versions in SCHEMA_DEFINITIONS.items():
            v1_fields = set(versions["v1"])
            v2_fields = set(versions["v2"])
            new_fields = v2_fields - v1_fields

            for field in new_fields:
                default_key = f"{record_type}_{field}"
                self.assertIn(default_key, FORWARD_DEFAULTS,
                              f"Missing default for {default_key}")


if __name__ == "__main__":
    unittest.main(verbosity=2)