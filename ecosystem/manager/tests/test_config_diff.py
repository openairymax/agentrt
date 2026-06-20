# Copyright (c) 2026 SPHARX. All Rights Reserved.
"""Unit tests for ecosystem/manager/tools/config_diff.py"""

import pytest
import sys
from pathlib import Path

# Add manager tools to path
sys.path.insert(0, str(Path(__file__).parent.parent))

from tools.config_diff import (
    DiffType,
    ValueType,
    DiffEntry,
    DiffResult,
    compare_configs,
    deep_diff,
    normalize_value,
    detect_value_type,
    load_config_file,
    format_diff_entry,
)


class TestValueType:
    """Tests for value type detection."""

    def test_detect_null(self):
        assert detect_value_type(None) == ValueType.NULL

    def test_detect_bool(self):
        assert detect_value_type(True) == ValueType.BOOLEAN
        assert detect_value_type(False) == ValueType.BOOLEAN

    def test_detect_int(self):
        assert detect_value_type(42) == ValueType.NUMBER

    def test_detect_float(self):
        assert detect_value_type(3.14) == ValueType.NUMBER

    def test_detect_string(self):
        assert detect_value_type("hello") == ValueType.STRING

    def test_detect_dict(self):
        assert detect_value_type({"key": "val"}) == ValueType.OBJECT

    def test_detect_list(self):
        assert detect_value_type([1, 2, 3]) == ValueType.ARRAY


class TestNormalizeValue:
    """Tests for value normalization."""

    def test_normalize_int_string(self):
        assert normalize_value("42") == 42

    def test_normalize_float_string(self):
        assert normalize_value("3.14") == 3.14

    def test_normalize_true_string(self):
        assert normalize_value("true") is True
        assert normalize_value("True") is True

    def test_normalize_false_string(self):
        assert normalize_value("false") is False
        assert normalize_value("False") is False

    def test_normalize_plain_string(self):
        assert normalize_value("hello") == "hello"

    def test_normalize_dict_sorted_keys(self):
        result = normalize_value({"b": 2, "a": 1})
        assert list(result.keys()) == ["a", "b"]

    def test_normalize_nested_list(self):
        result = normalize_value([[3, 1], [2]])
        assert result == [[3, 1], [2]]

    def test_normalize_bool_preserved(self):
        assert normalize_value(True) is True

    def test_normalize_none(self):
        assert normalize_value(None) is None


class TestDeepDiff:
    """Tests for deep diff comparison."""

    def test_identical_values(self):
        diffs = []
        deep_diff("root", {"a": 1}, {"a": 1}, diffs)
        assert len(diffs) == 0

    def test_added_key(self):
        diffs = []
        deep_diff("", {"a": 1}, {"a": 1, "b": 2}, diffs)
        assert len(diffs) == 1
        assert diffs[0].diff_type == DiffType.ADDED
        assert diffs[0].path == "b"
        assert diffs[0].new_value == 2

    def test_removed_key(self):
        diffs = []
        deep_diff("", {"a": 1, "b": 2}, {"a": 1}, diffs)
        assert len(diffs) == 1
        assert diffs[0].diff_type == DiffType.REMOVED
        assert diffs[0].path == "b"
        assert diffs[0].old_value == 2

    def test_modified_value(self):
        diffs = []
        deep_diff("", {"a": 1}, {"a": 2}, diffs)
        assert len(diffs) == 1
        assert diffs[0].diff_type == DiffType.MODIFIED
        assert diffs[0].old_value == 1
        assert diffs[0].new_value == 2

    def test_type_change(self):
        diffs = []
        deep_diff("", {"a": 1}, {"a": "string"}, diffs)
        assert len(diffs) == 1
        assert diffs[0].diff_type == DiffType.MODIFIED

    def test_nested_diff(self):
        diffs = []
        deep_diff("", {"a": {"b": 1}}, {"a": {"b": 2}}, diffs)
        assert len(diffs) == 1
        assert diffs[0].path == "a.b"

    def test_array_added(self):
        diffs = []
        deep_diff("", {"arr": [1]}, {"arr": [1, 2]}, diffs)
        added = [d for d in diffs if d.diff_type == DiffType.ADDED]
        assert len(added) == 1
        assert added[0].path == "arr[1]"

    def test_array_removed(self):
        diffs = []
        deep_diff("", {"arr": [1, 2]}, {"arr": [1]}, diffs)
        removed = [d for d in diffs if d.diff_type == DiffType.REMOVED]
        assert len(removed) == 1

    def test_array_modified(self):
        diffs = []
        deep_diff("", {"arr": [1, 2]}, {"arr": [1, 3]}, diffs)
        modified = [d for d in diffs if d.diff_type == DiffType.MODIFIED]
        assert len(modified) == 1

    def test_string_value_comparison_uses_normalization(self):
        """Strings that normalize to same value should not diff."""
        diffs = []
        deep_diff("", {"a": "42"}, {"a": 42}, diffs)
        assert len(diffs) == 0  # "42" normalizes to 42


class TestDiffEntry:
    """Tests for DiffEntry dataclass."""

    def test_to_dict(self):
        entry = DiffEntry(
            path="a.b",
            diff_type=DiffType.MODIFIED,
            old_value=1,
            new_value=2,
            value_type=ValueType.NUMBER,
        )
        d = entry.to_dict()
        assert d["path"] == "a.b"
        assert d["type"] == "modified"
        assert d["old_value"] == 1
        assert d["new_value"] == 2
        assert d["value_type"] == "number"


class TestDiffResult:
    """Tests for DiffResult dataclass."""

    def test_has_changes_with_added(self):
        result = DiffResult(file1="a.yaml", file2="b.yaml", added_count=1)
        assert result.has_changes is True

    def test_has_changes_with_removed(self):
        result = DiffResult(file1="a.yaml", file2="b.yaml", removed_count=1)
        assert result.has_changes is True

    def test_has_changes_with_modified(self):
        result = DiffResult(file1="a.yaml", file2="b.yaml", modified_count=1)
        assert result.has_changes is True

    def test_has_changes_empty(self):
        result = DiffResult(file1="a.yaml", file2="b.yaml")
        assert result.has_changes is False

    def test_to_dict(self):
        result = DiffResult(
            file1="a.yaml",
            file2="b.yaml",
            added_count=1,
            modified_count=2,
        )
        d = result.to_dict()
        assert d["file1"] == "a.yaml"
        assert d["file2"] == "b.yaml"
        assert d["summary"]["added"] == 1
        assert d["summary"]["modified"] == 2
        assert d["summary"]["removed"] == 0
        assert d["summary"]["unchanged"] == 0

    def test_errors_included(self):
        result = DiffResult(
            file1="a.yaml",
            file2="b.yaml",
            errors=["file not found"],
        )
        d = result.to_dict()
        assert len(d["errors"]) == 1


class TestFormatDiffEntry:
    """Tests for diff entry formatting."""

    def test_format_added(self):
        entry = DiffEntry(
            path="new_field",
            diff_type=DiffType.ADDED,
            new_value="hello",
        )
        output = format_diff_entry(entry, color=False)
        assert "+ new_field" in output
        assert "hello" in output

    def test_format_removed(self):
        entry = DiffEntry(
            path="old_field",
            diff_type=DiffType.REMOVED,
            old_value="bye",
        )
        output = format_diff_entry(entry, color=False)
        assert "- old_field" in output
        assert "bye" in output

    def test_format_modified(self):
        entry = DiffEntry(
            path="changed_field",
            diff_type=DiffType.MODIFIED,
            old_value=1,
            new_value=2,
        )
        output = format_diff_entry(entry, color=False)
        assert "~ changed_field" in output
        assert "1" in output
        assert "2" in output


class TestCompareConfigs:
    """Tests for compare_configs function."""

    def test_identical_yaml(self, tmp_path):
        f1 = tmp_path / "a.yaml"
        f2 = tmp_path / "b.yaml"
        content = "key: value\nnum: 42\n"
        f1.write_text(content)
        f2.write_text(content)

        result = compare_configs(str(f1), str(f2))
        assert not result.has_changes
        assert len(result.errors) == 0

    def test_different_yaml(self, tmp_path):
        f1 = tmp_path / "a.yaml"
        f2 = tmp_path / "b.yaml"
        f1.write_text("key: value1\n")
        f2.write_text("key: value2\n")

        result = compare_configs(str(f1), str(f2))
        assert result.has_changes
        assert result.modified_count == 1

    def test_file_not_found(self, tmp_path):
        result = compare_configs(
            str(tmp_path / "nonexistent.yaml"),
            str(tmp_path / "also_nonexistent.yaml"),
        )
        assert len(result.errors) > 0

    def test_json_files(self, tmp_path):
        f1 = tmp_path / "a.json"
        f2 = tmp_path / "b.json"
        f1.write_text('{"a": 1, "b": 2}')
        f2.write_text('{"a": 1, "b": 3}')

        result = compare_configs(str(f1), str(f2))
        assert result.has_changes
        assert result.modified_count == 1

    def test_added_fields(self, tmp_path):
        f1 = tmp_path / "a.yaml"
        f2 = tmp_path / "b.yaml"
        f1.write_text("a: 1\n")
        f2.write_text("a: 1\nb: 2\n")

        result = compare_configs(str(f1), str(f2))
        assert result.added_count == 1

    def test_removed_fields(self, tmp_path):
        f1 = tmp_path / "a.yaml"
        f2 = tmp_path / "b.yaml"
        f1.write_text("a: 1\nb: 2\n")
        f2.write_text("a: 1\n")

        result = compare_configs(str(f1), str(f2))
        assert result.removed_count == 1

    def test_metadata_fields_ignored(self, tmp_path):
        f1 = tmp_path / "a.yaml"
        f2 = tmp_path / "b.yaml"
        f1.write_text("_version: 1.0\nkey: value\n")
        f2.write_text("_version: 2.0\nkey: value\n")

        result = compare_configs(str(f1), str(f2))
        assert not result.has_changes  # metadata starts with _


class TestLoadConfigFile:
    """Tests for load_config_file."""

    def test_load_yaml(self, tmp_path):
        f = tmp_path / "test.yaml"
        f.write_text("key: value\n")
        data, error = load_config_file(str(f))
        assert error is None
        assert data == {"key": "value"}

    def test_load_json(self, tmp_path):
        f = tmp_path / "test.json"
        f.write_text('{"key": "value"}')
        data, error = load_config_file(str(f))
        assert error is None
        assert data == {"key": "value"}

    def test_load_not_found(self, tmp_path):
        data, error = load_config_file(str(tmp_path / "nonexistent.yaml"))
        assert data is None
        assert error is not None

    def test_load_unsupported_extension(self, tmp_path):
        f = tmp_path / "test.txt"
        f.write_text("hello")
        data, error = load_config_file(str(f))
        assert data is None
        assert error is not None