# Copyright (c) 2026 SPHARX. All Rights Reserved.
"""Tests for ecosystem/manager/tools/ — audit_log_generator, drift_detector, config_version_cleanup."""

import json
import os
import tempfile
from datetime import datetime, timedelta
from pathlib import Path

import pytest

from ecosystem.manager.tools.audit_log_generator import (
    ActionType, OperatorType, Operator, ChangeItem, Checksum,
    Metadata, Result, AuditLogEntry, AuditLogGenerator,
)
from ecosystem.manager.tools.drift_detector import (
    DriftSeverity, DriftType, DriftItem, DriftReport, ConfigDriftDetector,
)
from ecosystem.manager.tools.config_version_cleanup import (
    VersionInfo, CleanupResult, ConfigVersionCleanup, format_bytes,
)


# ============================================================
# AuditLogGenerator
# ============================================================

class TestActionType:
    def test_values(self):
        assert ActionType.LOAD.value == "LOAD"
        assert ActionType.RELOAD.value == "RELOAD"
        assert ActionType.CHANGE.value == "CHANGE"
        assert ActionType.ROLLBACK.value == "ROLLBACK"
        assert ActionType.VALIDATE.value == "VALIDATE"
        assert ActionType.EXPORT.value == "EXPORT"
        assert ActionType.IMPORT.value == "IMPORT"


class TestOperatorType:
    def test_values(self):
        assert OperatorType.USER.value == "user"
        assert OperatorType.SYSTEM.value == "system"
        assert OperatorType.CI_CD.value == "ci_cd"


class TestOperator:
    def test_to_dict_basic(self):
        op = Operator(type="user", identity="admin")
        d = op.to_dict()
        assert d["type"] == "user"
        assert d["identity"] == "admin"

    def test_to_dict_full(self):
        op = Operator(type="user", identity="admin", ip_address="192.168.1.1", session_id="abc")
        d = op.to_dict()
        assert d["ip_address"] == "192.168.1.1"
        assert d["session_id"] == "abc"


class TestChangeItem:
    def test_to_dict_basic(self):
        ci = ChangeItem(path="a.b", old_value=1, new_value=2)
        d = ci.to_dict()
        assert d["path"] == "a.b"
        assert d["old_value"] == 1
        assert d["new_value"] == 2

    def test_to_dict_with_type(self):
        ci = ChangeItem(path="a.b", old_value=1, new_value=2, field_type="integer")
        d = ci.to_dict()
        assert d["field_type"] == "integer"


class TestChecksum:
    def test_to_dict(self):
        cs = Checksum(algorithm="sha256", before="abc", after="def")
        d = cs.to_dict()
        assert d["algorithm"] == "sha256"
        assert d["before"] == "abc"
        assert d["after"] == "def"


class TestMetadata:
    def test_to_dict_basic(self):
        m = Metadata(environment="staging")
        d = m.to_dict()
        assert d["environment"] == "staging"
        assert d["version"] == "0.1.0"

    def test_to_dict_full(self):
        m = Metadata(
            environment="production",
            correlation_id="corr-1",
            source="manual",
            reason="test",
            approved_by="admin",
            ticket_id="OPS-2026-1234",
        )
        d = m.to_dict()
        assert d["correlation_id"] == "corr-1"
        assert d["source"] == "manual"
        assert d["reason"] == "test"
        assert d["approved_by"] == "admin"
        assert d["ticket_id"] == "OPS-2026-1234"


class TestResult:
    def test_to_dict_success(self):
        r = Result(success=True, duration_ms=50)
        d = r.to_dict()
        assert d["success"] is True
        assert d["duration_ms"] == 50
        assert "error_code" not in d

    def test_to_dict_failure(self):
        r = Result(success=False, error_code="SCHEMA_VIOLATION", error_message="invalid", duration_ms=10)
        d = r.to_dict()
        assert d["success"] is False
        assert d["error_code"] == "SCHEMA_VIOLATION"
        assert d["error_message"] == "invalid"


class TestAuditLogEntry:
    def test_to_dict(self):
        op = Operator(type="user", identity="admin")
        cs = Checksum(before="abc", after="def")
        entry = AuditLogEntry(
            timestamp="2026-01-01T00:00:00Z",
            action="CHANGE",
            config_file="kernel/settings.yaml",
            operator=op,
            checksum=cs,
        )
        d = entry.to_dict()
        assert d["timestamp"] == "2026-01-01T00:00:00Z"
        assert d["action"] == "CHANGE"
        assert d["config_file"] == "kernel/settings.yaml"
        assert d["operator"]["type"] == "user"
        assert d["checksum"]["algorithm"] == "sha256"
        assert d["changes"] == []

    def test_to_dict_with_metadata_and_result(self):
        op = Operator(type="system", identity="config-manager")
        cs = Checksum(before="x", after="y")
        ci = ChangeItem(path="a", old_value=1, new_value=2)
        m = Metadata(environment="production")
        r = Result(success=True, duration_ms=100)
        entry = AuditLogEntry(
            timestamp="2026-01-01T00:00:00Z",
            action="CHANGE",
            config_file="model/model.yaml",
            operator=op,
            checksum=cs,
            changes=[ci],
            metadata=m,
            result=r,
        )
        d = entry.to_dict()
        assert len(d["changes"]) == 1
        assert d["metadata"]["environment"] == "production"
        assert d["result"]["success"] is True


class TestAuditLogGenerator:
    def test_init(self):
        gen = AuditLogGenerator(Path("/tmp/test"))
        assert gen.config_dir == Path("/tmp/test")

    def test_generate_entry_with_all_params(self):
        gen = AuditLogGenerator(Path("/tmp/test"))
        entry = gen.generate_entry(
            action="CHANGE",
            config_file="kernel/settings.yaml",
            operator_type="user",
            environment="staging",
            reason="test change",
            include_changes=True,
        )
        assert entry.action == "CHANGE"
        assert entry.config_file == "kernel/settings.yaml"
        assert entry.operator.type == "user"
        assert len(entry.changes) > 0
        assert entry.metadata.environment == "staging"
        assert entry.metadata.reason == "test change"

    def test_generate_entry_random(self):
        gen = AuditLogGenerator(Path("/tmp/test"))
        entry = gen.generate_entry()
        assert entry.action in [a.value for a in ActionType]
        assert entry.config_file in AuditLogGenerator.CONFIG_FILES
        assert entry.operator.type in [o.value for o in OperatorType]

    def test_generate_entry_no_changes(self):
        gen = AuditLogGenerator(Path("/tmp/test"))
        entry = gen.generate_entry(action="LOAD", include_changes=False)
        assert len(entry.changes) == 0

    def test_generate_batch(self):
        gen = AuditLogGenerator(Path("/tmp/test"))
        entries = gen.generate_batch(count=5, environment="development")
        assert len(entries) == 5
        for e in entries:
            assert e.metadata.environment == "development"

    def test_generate_batch_first_is_load(self):
        gen = AuditLogGenerator(Path("/tmp/test"))
        entries = gen.generate_batch(count=3)
        assert entries[0].action == ActionType.LOAD.value

    def test_generate_operator_user(self):
        gen = AuditLogGenerator(Path("/tmp/test"))
        op = gen._generate_operator("user")
        assert op.type == "user"
        assert op.identity in AuditLogGenerator.USER_NAMES
        assert op.ip_address is not None
        assert op.session_id is not None

    def test_generate_operator_system(self):
        gen = AuditLogGenerator(Path("/tmp/test"))
        op = gen._generate_operator("system")
        assert op.type == "system"
        assert op.identity in AuditLogGenerator.SYSTEM_COMPONENTS

    def test_generate_operator_ci_cd(self):
        gen = AuditLogGenerator(Path("/tmp/test"))
        op = gen._generate_operator("ci_cd")
        assert op.type == "ci_cd"
        assert op.identity in AuditLogGenerator.CI_CD_SYSTEMS

    def test_generate_checksum_stateful(self):
        gen = AuditLogGenerator(Path("/tmp/test"))
        cs1 = gen._generate_checksum("kernel/settings.yaml")
        cs2 = gen._generate_checksum("kernel/settings.yaml")
        # after of first becomes before of second
        assert cs2.before == cs1.after

    def test_generate_changes_kernel(self):
        gen = AuditLogGenerator(Path("/tmp/test"))
        changes = gen._generate_changes("kernel/settings.yaml")
        assert len(changes) == 1
        assert "kernel" in changes[0].path

    def test_generate_changes_model(self):
        gen = AuditLogGenerator(Path("/tmp/test"))
        changes = gen._generate_changes("model/model.yaml")
        assert len(changes) == 1
        assert "model" in changes[0].path

    def test_generate_changes_security(self):
        gen = AuditLogGenerator(Path("/tmp/test"))
        changes = gen._generate_changes("security/policy.yaml")
        assert len(changes) == 1
        assert "security" in changes[0].path

    def test_generate_changes_agent(self):
        gen = AuditLogGenerator(Path("/tmp/test"))
        changes = gen._generate_changes("agent/registry.yaml")
        assert len(changes) == 1
        assert "agents" in changes[0].path

    def test_generate_changes_other(self):
        gen = AuditLogGenerator(Path("/tmp/test"))
        changes = gen._generate_changes("logging/manager.yaml")
        assert len(changes) == 1
        assert "config" in changes[0].path

    def test_get_source_for_action(self):
        gen = AuditLogGenerator(Path("/tmp/test"))
        assert gen._get_source_for_action("LOAD") == "system_startup"
        assert gen._get_source_for_action("RELOAD") == "file_watcher"
        assert gen._get_source_for_action("CHANGE") == "manual"
        assert gen._get_source_for_action("ROLLBACK") == "validation_failure"
        assert gen._get_source_for_action("VALIDATE") == "periodic_check"
        assert gen._get_source_for_action("EXPORT") == "manual"
        assert gen._get_source_for_action("IMPORT") == "deployment_pipeline"
        assert gen._get_source_for_action("UNKNOWN") == "unknown"

    def test_export_to_json(self):
        gen = AuditLogGenerator(Path("/tmp/test"))
        entry = gen.generate_entry(action="LOAD", config_file="kernel/settings.yaml", include_changes=False)
        with tempfile.NamedTemporaryFile(suffix=".json", delete=False, mode="w") as f:
            output_path = Path(f.name)
        try:
            gen.export_to_json([entry], output_path)
            data = json.loads(output_path.read_text())
            assert len(data) == 1
            assert data[0]["action"] == "LOAD"
        finally:
            output_path.unlink(missing_ok=True)


# ============================================================
# ConfigDriftDetector
# ============================================================

class TestDriftSeverity:
    def test_values(self):
        assert DriftSeverity.INFO.value == "info"
        assert DriftSeverity.WARNING.value == "warning"
        assert DriftSeverity.CRITICAL.value == "critical"


class TestDriftType:
    def test_values(self):
        assert DriftType.MODIFIED.value == "modified"
        assert DriftType.DELETED.value == "deleted"
        assert DriftType.ADDED.value == "added"
        assert DriftType.UNCHANGED.value == "unchanged"


class TestDriftItem:
    def test_to_dict(self):
        item = DriftItem(
            file_path="test.yaml",
            severity="warning",
            drift_type="modified",
            baseline_hash="abc123",
            current_hash="def456",
            detected_at="2026-01-01T00:00:00Z",
            details="modified",
            file_size=1024,
            last_modified="2026-01-01T00:00:00Z",
        )
        d = item.to_dict()
        assert d["file_path"] == "test.yaml"
        assert d["severity"] == "warning"
        assert d["drift_type"] == "modified"
        assert d["baseline_hash"] == "abc123"
        assert d["current_hash"] == "def456"
        assert d["file_size"] == 1024


class TestDriftReport:
    def test_has_drift_true(self):
        item = DriftItem(
            file_path="test.yaml", severity="info", drift_type="added",
            baseline_hash="", current_hash="abc", detected_at="2026-01-01T00:00:00Z",
        )
        report = DriftReport(
            scan_time="2026-01-01T00:00:00Z",
            config_dir="/tmp",
            baseline_created="2026-01-01T00:00:00Z",
            total_files_scanned=1,
            drifted_files=1,
            unchanged_files=0,
            drift_items=[item],
        )
        assert report.has_drift is True

    def test_has_drift_false(self):
        report = DriftReport(
            scan_time="2026-01-01T00:00:00Z",
            config_dir="/tmp",
            baseline_created="2026-01-01T00:00:00Z",
            total_files_scanned=1,
            drifted_files=0,
            unchanged_files=1,
        )
        assert report.has_drift is False

    def test_drift_rate(self):
        report = DriftReport(
            scan_time="2026-01-01T00:00:00Z",
            config_dir="/tmp",
            baseline_created="2026-01-01T00:00:00Z",
            total_files_scanned=10,
            drifted_files=3,
            unchanged_files=7,
        )
        assert report.drift_rate == 30.0

    def test_drift_rate_zero_total(self):
        report = DriftReport(
            scan_time="2026-01-01T00:00:00Z",
            config_dir="/tmp",
            baseline_created="2026-01-01T00:00:00Z",
            total_files_scanned=0,
            drifted_files=0,
            unchanged_files=0,
        )
        assert report.drift_rate == 0.0

    def test_summary(self):
        items = [
            DriftItem(file_path="a.yaml", severity="critical", drift_type="modified",
                       baseline_hash="x", current_hash="y", detected_at="2026-01-01T00:00:00Z"),
            DriftItem(file_path="b.yaml", severity="warning", drift_type="deleted",
                       baseline_hash="x", current_hash="", detected_at="2026-01-01T00:00:00Z"),
            DriftItem(file_path="c.yaml", severity="info", drift_type="added",
                       baseline_hash="", current_hash="z", detected_at="2026-01-01T00:00:00Z"),
        ]
        report = DriftReport(
            scan_time="2026-01-01T00:00:00Z",
            config_dir="/tmp",
            baseline_created="2026-01-01T00:00:00Z",
            total_files_scanned=3,
            drifted_files=3,
            unchanged_files=0,
            drift_items=items,
        )
        summary = report.summary()
        assert "Critical: 1" in summary
        assert "Warning: 1" in summary
        assert "Info: 1" in summary


class TestConfigDriftDetector:
    def test_init(self):
        detector = ConfigDriftDetector(Path("/tmp/test"))
        assert detector.verbose is False

    def test_get_severity_for_file_sensitive(self):
        detector = ConfigDriftDetector(Path("/tmp/test"))
        assert detector._get_severity_for_file("security/policy.yaml") == "critical"

    def test_get_severity_for_file_important(self):
        detector = ConfigDriftDetector(Path("/tmp/test"))
        assert detector._get_severity_for_file("agent/registry.yaml") == "warning"

    def test_get_severity_for_file_normal(self):
        detector = ConfigDriftDetector(Path("/tmp/test"))
        assert detector._get_severity_for_file("logging/manager.yaml") == "warning"

    def test_get_severity_for_file_other(self):
        detector = ConfigDriftDetector(Path("/tmp/test"))
        assert detector._get_severity_for_file("other/file.yaml") == "info"

    def test_get_severity_for_file_security_prefix(self):
        detector = ConfigDriftDetector(Path("/tmp/test"))
        assert detector._get_severity_for_file("security/other.yaml") == "warning"

    def test_should_ignore_pycache(self):
        detector = ConfigDriftDetector(Path("/tmp/test"))
        assert detector._should_ignore(Path("/tmp/test/__pycache__/module.pyc")) is True

    def test_should_ignore_baseline(self):
        detector = ConfigDriftDetector(Path("/tmp/test"))
        assert detector._should_ignore(Path("/tmp/test/.baseline/manifest.json")) is True

    def test_should_not_ignore_yaml(self):
        detector = ConfigDriftDetector(Path("/tmp/test"))
        assert detector._should_ignore(Path("/tmp/test/config.yaml")) is False

    def test_calculate_file_hash(self):
        with tempfile.NamedTemporaryFile(suffix=".yaml", delete=False) as f:
            f.write(b"hello world")
            tmp_path = Path(f.name)
        try:
            detector = ConfigDriftDetector(Path("/tmp"))
            h = detector._calculate_file_hash(tmp_path)
            assert len(h) == 64
            assert h == detector._calculate_file_hash(tmp_path)
        finally:
            tmp_path.unlink(missing_ok=True)

    def test_calculate_file_hash_missing(self):
        detector = ConfigDriftDetector(Path("/tmp"))
        assert detector._calculate_file_hash(Path("/nonexistent.yaml")) == ""

    def test_create_and_detect_no_drift(self):
        with tempfile.TemporaryDirectory() as tmpdir:
            config_dir = Path(tmpdir) / "config"
            config_dir.mkdir()
            (config_dir / "test.yaml").write_text("key: value\n")
            detector = ConfigDriftDetector(config_dir)
            baseline = detector.create_baseline()
            assert baseline.exists()
            report = detector.detect_drift()
            assert report.has_drift is False

    def test_detect_modified_file(self):
        with tempfile.TemporaryDirectory() as tmpdir:
            config_dir = Path(tmpdir) / "config"
            config_dir.mkdir()
            (config_dir / "test.yaml").write_text("key: value\n")
            detector = ConfigDriftDetector(config_dir)
            detector.create_baseline()
            (config_dir / "test.yaml").write_text("key: changed\n")
            report = detector.detect_drift()
            assert report.has_drift is True
            assert report.drifted_files == 1
            assert report.drift_items[0].drift_type == "modified"

    def test_detect_added_file(self):
        with tempfile.TemporaryDirectory() as tmpdir:
            config_dir = Path(tmpdir) / "config"
            config_dir.mkdir()
            (config_dir / "test.yaml").write_text("key: value\n")
            detector = ConfigDriftDetector(config_dir)
            detector.create_baseline()
            (config_dir / "new.yaml").write_text("new: file\n")
            report = detector.detect_drift()
            assert report.has_drift is True
            added = [d for d in report.drift_items if d.drift_type == "added"]
            assert len(added) == 1

    def test_detect_deleted_file(self):
        with tempfile.TemporaryDirectory() as tmpdir:
            config_dir = Path(tmpdir) / "config"
            config_dir.mkdir()
            (config_dir / "test.yaml").write_text("key: value\n")
            detector = ConfigDriftDetector(config_dir)
            detector.create_baseline()
            (config_dir / "test.yaml").unlink()
            report = detector.detect_drift()
            assert report.has_drift is True
            assert report.drift_items[0].drift_type == "deleted"

    def test_detect_no_baseline_raises(self):
        with tempfile.TemporaryDirectory() as tmpdir:
            config_dir = Path(tmpdir) / "config"
            config_dir.mkdir()
            detector = ConfigDriftDetector(config_dir)
            with pytest.raises(RuntimeError, match="Baseline not found"):
                detector.detect_drift()

    def test_export_report_json(self):
        with tempfile.TemporaryDirectory() as tmpdir:
            config_dir = Path(tmpdir) / "config"
            config_dir.mkdir()
            (config_dir / "test.yaml").write_text("key: value\n")
            detector = ConfigDriftDetector(config_dir)
            detector.create_baseline()
            report = detector.detect_drift()
            output = Path(tmpdir) / "report.json"
            detector.export_report_json(report, output)
            assert output.exists()
            data = json.loads(output.read_text())
            assert data["has_drift"] is False

    def test_export_report_markdown(self):
        with tempfile.TemporaryDirectory() as tmpdir:
            config_dir = Path(tmpdir) / "config"
            config_dir.mkdir()
            (config_dir / "test.yaml").write_text("key: value\n")
            detector = ConfigDriftDetector(config_dir)
            detector.create_baseline()
            report = detector.detect_drift()
            output = Path(tmpdir) / "report.md"
            detector.export_report_markdown(report, output)
            assert output.exists()
            content = output.read_text()
            assert "No configuration drift detected" in content


# ============================================================
# ConfigVersionCleanup
# ============================================================

class TestVersionInfo:
    def test_dataclass(self):
        vi = VersionInfo(
            version_id="v1",
            timestamp=datetime(2026, 1, 1),
            file_path="/tmp/v1.json",
            checksum="abc123",
            size_bytes=1024,
        )
        assert vi.version_id == "v1"
        assert vi.size_bytes == 1024
        assert vi.compressed is False


class TestCleanupResult:
    def test_defaults(self):
        result = CleanupResult(timestamp="2026-01-01T00:00:00Z")
        assert result.total_versions == 0
        assert result.kept_versions == 0
        assert result.deleted_versions == 0
        assert result.freed_space_bytes == 0
        assert result.errors == []


class TestConfigVersionCleanup:
    def test_init(self):
        cleaner = ConfigVersionCleanup("/tmp/test", dry_run=True, verbose=True)
        assert cleaner.dry_run is True
        assert cleaner.verbose is True

    def test_get_all_versions_empty(self):
        with tempfile.TemporaryDirectory() as tmpdir:
            cleaner = ConfigVersionCleanup(tmpdir)
            versions = cleaner.get_all_versions()
            assert versions == []

    def test_get_all_versions(self):
        with tempfile.TemporaryDirectory() as tmpdir:
            (Path(tmpdir) / "v1.json").write_text("{}")
            (Path(tmpdir) / "v2.json").write_text("{}")
            (Path(tmpdir) / ".hidden").write_text("{}")
            cleaner = ConfigVersionCleanup(tmpdir)
            versions = cleaner.get_all_versions()
            assert len(versions) == 2

    def test_cleanup_by_retention_days_dry_run(self):
        with tempfile.TemporaryDirectory() as tmpdir:
            (Path(tmpdir) / "old.json").write_text("{}")
            (Path(tmpdir) / "new.json").write_text("{}")
            cleaner = ConfigVersionCleanup(tmpdir, dry_run=True)
            versions = cleaner.get_all_versions()
            result = cleaner.cleanup_by_retention_days(versions, retention_days=0)
            assert result.deleted_versions == 2
            assert (Path(tmpdir) / "old.json").exists()

    def test_cleanup_by_retention_days_actual(self):
        with tempfile.TemporaryDirectory() as tmpdir:
            old_file = Path(tmpdir) / "old.json"
            old_file.write_text("{}")
            # Set mtime to 30 days ago
            old_mtime = (datetime.now() - timedelta(days=30)).timestamp()
            os.utime(old_file, (old_mtime, old_mtime))

            cleaner = ConfigVersionCleanup(tmpdir)
            versions = cleaner.get_all_versions()
            result = cleaner.cleanup_by_retention_days(versions, retention_days=7)
            assert result.deleted_versions == 1
            assert not old_file.exists()

    def test_cleanup_by_max_versions(self):
        with tempfile.TemporaryDirectory() as tmpdir:
            for i in range(5):
                (Path(tmpdir) / f"v{i}.json").write_text("{}")
            cleaner = ConfigVersionCleanup(tmpdir)
            versions = cleaner.get_all_versions()
            result = cleaner.cleanup_by_max_versions(versions, max_versions=2)
            assert result.kept_versions == 2
            assert result.deleted_versions == 3

    def test_cleanup_by_max_versions_dry_run(self):
        with tempfile.TemporaryDirectory() as tmpdir:
            for i in range(5):
                (Path(tmpdir) / f"v{i}.json").write_text("{}")
            cleaner = ConfigVersionCleanup(tmpdir, dry_run=True)
            versions = cleaner.get_all_versions()
            result = cleaner.cleanup_by_max_versions(versions, max_versions=2)
            assert result.kept_versions == 2
            assert result.deleted_versions == 3
            assert len(list(Path(tmpdir).glob("*.json"))) == 5

    def test_cleanup_method(self):
        with tempfile.TemporaryDirectory() as tmpdir:
            for i in range(3):
                (Path(tmpdir) / f"v{i}.json").write_text("{}")
            cleaner = ConfigVersionCleanup(tmpdir)
            result = cleaner.cleanup(max_versions=1)
            assert result.kept_versions == 1
            assert result.deleted_versions == 2

    def test_cleanup_no_condition(self):
        with tempfile.TemporaryDirectory() as tmpdir:
            cleaner = ConfigVersionCleanup(tmpdir)
            result = cleaner.cleanup()
            assert len(result.errors) == 1

    def test_get_summary(self):
        with tempfile.TemporaryDirectory() as tmpdir:
            for i in range(3):
                (Path(tmpdir) / f"v{i}.json").write_text("{}")
            cleaner = ConfigVersionCleanup(tmpdir)
            cleaner.cleanup(max_versions=1)
            summary = cleaner.get_summary()
            assert summary["total_deleted"] == 2
            assert summary["total_results"] == 1

    def test_calculate_checksum(self):
        with tempfile.TemporaryDirectory() as tmpdir:
            f = Path(tmpdir) / "test.json"
            f.write_text("hello")
            cleaner = ConfigVersionCleanup(tmpdir)
            cs = cleaner.calculate_checksum(f)
            assert len(cs) == 64


class TestFormatBytes:
    def test_bytes(self):
        assert format_bytes(0) == "0.00 B"
        assert format_bytes(500) == "500.00 B"

    def test_kb(self):
        assert "KB" in format_bytes(2048)

    def test_mb(self):
        assert "MB" in format_bytes(2 * 1024 * 1024)

    def test_gb(self):
        assert "GB" in format_bytes(2 * 1024 * 1024 * 1024)