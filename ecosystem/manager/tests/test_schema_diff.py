# Copyright (c) 2026 SPHARX. All Rights Reserved.
"""Unit tests for ecosystem/manager/tools/schema_diff.py"""

import pytest
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent.parent / "tools"))

from schema_diff import (
    DiffSeverity,
    DiffEntry,
    DiffReport,
    SchemaDiffer,
)


class TestDiffSeverity:
    """Tests for severity constants."""

    def test_severity_values(self):
        assert DiffSeverity.ERROR == "error"
        assert DiffSeverity.WARNING == "warning"
        assert DiffSeverity.INFO == "info"


class TestDiffEntry:
    """Tests for DiffEntry dataclass."""

    def test_creation(self):
        entry = DiffEntry(
            severity=DiffSeverity.ERROR,
            schema_file="test.schema.json",
            yaml_path="kernel.ipc",
            message="Field missing",
            schema_value="max_connections",
            yaml_value="max_message_size",
        )
        assert entry.severity == "error"
        assert entry.schema_file == "test.schema.json"
        assert entry.yaml_path == "kernel.ipc"
        assert entry.message == "Field missing"

    def test_str_error(self):
        entry = DiffEntry(
            severity=DiffSeverity.ERROR,
            schema_file="test.schema.json",
            yaml_path="kernel.ipc",
            message="Something wrong",
        )
        s = str(entry)
        assert "[X]" in s
        assert "test.schema.json" in s
        assert "kernel.ipc" in s

    def test_str_warning(self):
        entry = DiffEntry(
            severity=DiffSeverity.WARNING,
            schema_file="test.schema.json",
            yaml_path="kernel.memory",
            message="Field mismatch",
        )
        s = str(entry)
        assert "[!]" in s

    def test_str_info(self):
        entry = DiffEntry(
            severity=DiffSeverity.INFO,
            schema_file="test.schema.json",
            yaml_path="llm.routing",
            message="Info message",
        )
        s = str(entry)
        assert "[i]" in s


class TestDiffReport:
    """Tests for DiffReport dataclass."""

    def test_empty_report(self):
        report = DiffReport()
        assert report.has_errors() is False
        assert report.has_warnings() is False
        assert report.summary["error"] == 0
        assert report.summary["warning"] == 0
        assert report.summary["info"] == 0
        assert report.summary["ok"] == 0

    def test_add_entry(self):
        report = DiffReport()
        entry = DiffEntry(
            severity=DiffSeverity.ERROR,
            schema_file="test.schema.json",
            yaml_path="kernel",
            message="Missing field",
        )
        report.add(entry)
        assert report.has_errors() is True
        assert report.summary["error"] == 1

    def test_add_ok(self):
        report = DiffReport()
        report.add_ok()
        report.add_ok()
        assert report.summary["ok"] == 2

    def test_add_warning(self):
        report = DiffReport()
        entry = DiffEntry(
            severity=DiffSeverity.WARNING,
            schema_file="test.schema.json",
            yaml_path="kernel",
            message="Mismatch",
        )
        report.add(entry)
        assert report.has_warnings() is True
        assert report.has_errors() is False

    def test_format_text_consistent(self):
        report = DiffReport()
        report.add_ok()
        text = report.format_text()
        assert "CONSISTENT" in text

    def test_format_text_with_errors(self):
        report = DiffReport()
        report.add(DiffEntry(
            severity=DiffSeverity.ERROR,
            schema_file="test.schema.json",
            yaml_path="kernel",
            message="error",
        ))
        text = report.format_text()
        assert "DRIFT DETECTED" in text

    def test_format_text_with_warnings(self):
        report = DiffReport()
        report.add(DiffEntry(
            severity=DiffSeverity.WARNING,
            schema_file="test.schema.json",
            yaml_path="kernel",
            message="warning",
        ))
        text = report.format_text()
        assert "WARNINGS" in text

    def test_to_json(self):
        report = DiffReport()
        report.add(DiffEntry(
            severity=DiffSeverity.ERROR,
            schema_file="test.schema.json",
            yaml_path="kernel.ipc",
            message="Missing",
            schema_value="v1",
            yaml_value="v2",
        ))
        report.add_ok()
        j = report.to_json()
        assert j["drift_detected"] is True
        assert j["summary"]["error"] == 1
        assert j["summary"]["ok"] == 1
        assert len(j["entries"]) == 1
        assert j["entries"][0]["severity"] == "error"
        assert j["entries"][0]["schema_file"] == "test.schema.json"

    def test_multiple_entries(self):
        report = DiffReport()
        for i in range(3):
            report.add(DiffEntry(
                severity=DiffSeverity.WARNING,
                schema_file=f"test{i}.schema.json",
                yaml_path=f"path.{i}",
                message=f"issue {i}",
            ))
        assert report.summary["warning"] == 3
        assert len(report.entries) == 3


class TestSchemaDiffer:
    """Tests for SchemaDiffer class."""

    def test_init_with_root(self, tmp_path):
        differ = SchemaDiffer(agentos_root=str(tmp_path))
        assert differ._root == tmp_path

    def test_init_default_root(self):
        differ = SchemaDiffer()
        assert differ._root is not None
        assert differ._root.exists()

    def test_load_yaml_not_found(self, tmp_path):
        differ = SchemaDiffer(agentos_root=str(tmp_path))
        result = differ._load_yaml()
        assert result is None

    def test_load_yaml_valid(self, tmp_path):
        yaml_path = tmp_path / "agentos.yaml"
        yaml_path.write_text("kernel:\n  ipc:\n    max_message_size: 4096\n")
        differ = SchemaDiffer(agentos_root=str(tmp_path))
        result = differ._load_yaml()
        assert result is not None
        assert result["kernel"]["ipc"]["max_message_size"] == 4096

    def test_load_schema_not_found(self, tmp_path):
        differ = SchemaDiffer(agentos_root=str(tmp_path))
        result = differ._load_schema("nonexistent.json")
        assert result is None

    def test_load_schema_valid(self, tmp_path):
        schema_dir = tmp_path / "ecosystem" / "manager" / "schema"
        schema_dir.mkdir(parents=True)
        schema_file = schema_dir / "kernel-settings.schema.json"
        schema_file.write_text('{"type": "object", "properties": {}}')

        differ = SchemaDiffer(agentos_root=str(tmp_path))
        result = differ._load_schema("kernel-settings.schema.json")
        assert result is not None
        assert result["type"] == "object"

    def test_get_schema_properties(self):
        differ = SchemaDiffer()
        schema = {
            "properties": {
                "kernel": {
                    "properties": {
                        "ipc": {
                            "properties": {
                                "max_connections": {"type": "integer"}
                            }
                        }
                    }
                }
            }
        }
        props = differ._get_schema_properties(schema, "kernel", "ipc")
        assert "max_connections" in props

    def test_merge_reports(self):
        differ = SchemaDiffer()
        a = DiffReport()
        a.add(DiffEntry(DiffSeverity.ERROR, "a.json", "path", "err"))
        a.add_ok()

        b = DiffReport()
        b.add(DiffEntry(DiffSeverity.WARNING, "b.json", "path", "warn"))
        b.add_ok()

        merged = differ._merge_reports(a, b)
        assert merged.summary["error"] == 1
        assert merged.summary["warning"] == 1
        assert merged.summary["ok"] == 2

    def test_run_with_missing_yaml(self, tmp_path):
        differ = SchemaDiffer(agentos_root=str(tmp_path))
        report = differ.run()
        assert report.has_errors() is True

    def test_check_schema_files_exist(self, tmp_path):
        schema_dir = tmp_path / "ecosystem" / "manager" / "schema"
        schema_dir.mkdir(parents=True)

        differ = SchemaDiffer(agentos_root=str(tmp_path))
        report = differ._check_schema_files_exist()
        # All 11 expected schema files are missing
        assert report.summary["error"] == 11

    def test_check_schema_files_exist_all_present(self, tmp_path):
        schema_dir = tmp_path / "ecosystem" / "manager" / "schema"
        schema_dir.mkdir(parents=True)

        expected = [
            "_metadata.schema.json",
            "kernel-settings.schema.json",
            "model.schema.json",
            "security-policy.schema.json",
            "logging.schema.json",
            "agent-registry.schema.json",
            "skill-registry.schema.json",
            "tool-service.schema.json",
            "config-management.schema.json",
            "config-audit-log.schema.json",
            "sanitizer-rules.schema.json",
        ]
        for name in expected:
            (schema_dir / name).write_text("{}")

        differ = SchemaDiffer(agentos_root=str(tmp_path))
        report = differ._check_schema_files_exist()
        assert report.summary["error"] == 0
        assert report.summary["ok"] == 11