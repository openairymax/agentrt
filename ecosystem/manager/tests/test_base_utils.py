# Copyright (c) 2026 SPHARX. All Rights Reserved.
"""Unit tests for ecosystem/manager/tools/base/utils.py"""

import json
import pytest
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent.parent))

from tools.base.utils import ConfigLoader, ReportExporter, FileHelper


class TestConfigLoader:
    """Tests for ConfigLoader."""

    def test_load_yaml(self, tmp_path):
        f = tmp_path / "test.yaml"
        f.write_text("key: value\nlist:\n  - a\n  - b\n")
        data, error = ConfigLoader.load(str(f))
        assert error is None
        assert data == {"key": "value", "list": ["a", "b"]}

    def test_load_json(self, tmp_path):
        f = tmp_path / "test.json"
        f.write_text('{"key": "value", "num": 42}')
        data, error = ConfigLoader.load(str(f))
        assert error is None
        assert data == {"key": "value", "num": 42}

    def test_load_bom_yaml(self, tmp_path):
        f = tmp_path / "test.yaml"
        f.write_text("\ufeffkey: value\n", encoding="utf-8")
        data, error = ConfigLoader.load(str(f))
        assert error is None
        assert data == {"key": "value"}

    def test_load_file_not_found(self, tmp_path):
        data, error = ConfigLoader.load(str(tmp_path / "nonexistent.yaml"))
        assert data is None
        assert error is not None
        assert "not found" in error.lower()

    def test_load_unsupported_extension(self, tmp_path):
        f = tmp_path / "test.txt"
        f.write_text("hello")
        data, error = ConfigLoader.load(str(f))
        assert data is None
        assert error is not None
        assert "unsupported" in error.lower()

    def test_load_yaml_method(self, tmp_path):
        f = tmp_path / "test.yaml"
        f.write_text("key: value\n")
        data = ConfigLoader.load_yaml(str(f))
        assert data == {"key": "value"}

    def test_load_yaml_raises_on_error(self, tmp_path):
        with pytest.raises(ValueError):
            ConfigLoader.load_yaml(str(tmp_path / "nonexistent.yaml"))


class TestReportExporter:
    """Tests for ReportExporter."""

    def test_export_json(self, tmp_path):
        output = tmp_path / "subdir" / "report.json"
        data = {"key": "value", "list": [1, 2, 3]}
        ReportExporter.export_json(data, output)
        assert output.exists()
        loaded = json.loads(output.read_text())
        assert loaded == data

    def test_export_json_indent(self, tmp_path):
        output = tmp_path / "report.json"
        data = {"a": 1}
        ReportExporter.export_json(data, output, indent=4)
        content = output.read_text()
        assert "    " in content  # 4-space indent

    def test_export_json_ensure_ascii(self, tmp_path):
        output = tmp_path / "report.json"
        data = {"key": "中文"}
        ReportExporter.export_json(data, output, ensure_ascii=True)
        content = output.read_text()
        assert "\\u" in content  # Unicode escaped

    def test_export_markdown(self, tmp_path):
        output = tmp_path / "subdir" / "report.md"
        content = "# Title\n\nContent here."
        ReportExporter.export_markdown(content, output)
        assert output.exists()
        assert output.read_text() == content

    def test_generate_timestamp(self):
        ts = ReportExporter.generate_timestamp()
        assert "T" in ts  # ISO format
        assert "+" in ts or "Z" in ts


class TestFileHelper:
    """Tests for FileHelper."""

    def test_calculate_sha256(self, tmp_path):
        f = tmp_path / "test.txt"
        f.write_text("hello world")
        sha = FileHelper.calculate_sha256(f)
        assert len(sha) == 64
        assert all(c in "0123456789abcdef" for c in sha)

    def test_calculate_sha256_file_not_found(self, tmp_path):
        sha = FileHelper.calculate_sha256(tmp_path / "nonexistent.txt")
        assert sha == ""

    def test_calculate_sha256_deterministic(self, tmp_path):
        f = tmp_path / "test.txt"
        f.write_text("same content")
        sha1 = FileHelper.calculate_sha256(f)
        sha2 = FileHelper.calculate_sha256(f)
        assert sha1 == sha2

    def test_ensure_directory(self, tmp_path):
        new_dir = tmp_path / "a" / "b" / "c"
        FileHelper.ensure_directory(new_dir)
        assert new_dir.exists()
        assert new_dir.is_dir()

    def test_ensure_directory_existing(self, tmp_path):
        FileHelper.ensure_directory(tmp_path)
        assert tmp_path.exists()

    def test_is_ignored_pyc(self, tmp_path):
        assert FileHelper.is_ignored_file(Path("test.pyc")) is True

    def test_is_ignored_pycache_dir(self, tmp_path):
        result = FileHelper.is_ignored_file(Path("__pycache__/module.pyc"))
        assert result is True

    def test_is_ignored_git(self, tmp_path):
        assert FileHelper.is_ignored_file(Path(".git/config")) is True

    def test_is_ignored_env(self, tmp_path):
        assert FileHelper.is_ignored_file(Path(".env.production")) is True

    def test_is_ignored_log(self, tmp_path):
        assert FileHelper.is_ignored_file(Path("app.log")) is True
        assert FileHelper.is_ignored_file(Path("error.log")) is True

    def test_not_ignored_python(self, tmp_path):
        assert FileHelper.is_ignored_file(Path("main.py")) is False

    def test_not_ignored_yaml(self, tmp_path):
        assert FileHelper.is_ignored_file(Path("config.yaml")) is False

    def test_custom_ignore_patterns(self, tmp_path):
        custom = ["*.custom", "secret_dir/"]
        assert FileHelper.is_ignored_file(Path("data.custom"), custom) is True
        assert FileHelper.is_ignored_file(Path("data.yaml"), custom) is False