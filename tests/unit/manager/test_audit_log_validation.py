#!/usr/bin/env python3
"""
AgentOS Config Audit Log Validation Tests

测试审计日志是否符合config-audit-log.schema.json规范

Usage:
    python test_audit_log_validation.py
    pytest test_audit_log_validation.py -v
"""

import json
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import List, Dict, Any

try:
    import jsonschema
    from jsonschema import validate, ValidationError, Draft7Validator
    HAS_JSONSCHEMA = True
except ImportError:
    HAS_JSONSCHEMA = False
    print("⚠️  Warning: jsonschema not installed. Install with: pip install jsonschema")


@dataclass
class ValidationResult:
    """验证结果"""
    test_name: str
    passed: bool
    message: str = ""
    details: str = ""
    
    def __str__(self) -> str:
        status = "✅ PASS" if self.passed else "❌ FAIL"
        result = f"{status}: {self.test_name}"
        if self.message:
            result += f"\n  {self.message}"
        if self.details:
            result += f"\n  {self.details}"
        return result


class AuditLogValidator:
    """审计日志验证器"""
    
    def __init__(self, manager_dir: Path):
        self.manager_dir = manager_dir
        self.schema_file = manager_dir / "schema" / "config-audit-log.schema.json"
        self.sample_log_file = manager_dir / "audit" / "sample_audit_log.json"
        
        self._schema = None
        self._sample_logs = None
        
    @property
    def schema(self) -> Dict[str, Any]:
        """加载Schema"""
        if self._schema is None:
            if not self.schema_file.exists():
                raise FileNotFoundError(f"Schema file not found: {self.schema_file}")
            self._schema = json.loads(self.schema_file.read_text(encoding='utf-8'))
        return self._schema
    
    @property
    def sample_logs(self) -> List[Dict[str, Any]]:
        """加载示例日志"""
        if self._sample_logs is None:
            if not self.sample_log_file.exists():
                raise FileNotFoundError(f"Sample log file not found: {self.sample_log_file}")
            self._sample_logs = json.loads(self.sample_log_file.read_text(encoding='utf-8'))
        return self._sample_logs
    
    def validate_schema_structure(self) -> ValidationResult:
        """测试1: 验证Schema文件结构"""
        try:
            schema = self.schema
            
            assert "$schema" in schema, "Missing $schema field"
            assert schema["$schema"] == "http://json-schema.org/draft-07/schema#", \
                f"Invalid $schema version: {schema.get('$schema')}"
            
            assert "title" in schema, "Missing title field"
            assert "type" in schema, "Missing type field"
            assert schema["type"] == "object", "Schema type must be 'object'"
            
            assert "required" in schema, "Missing required fields"
            required_fields = ["timestamp", "action", "config_file", "operator", "checksum"]
            for field in required_fields:
                assert field in schema["required"], f"Missing required field: {field}"
            
            assert "properties" in schema, "Missing properties definition"
            
            return ValidationResult(
                test_name="Schema Structure Validation",
                passed=True,
                message="Schema structure is valid and complete"
            )
            
        except Exception as e:
            return ValidationResult(
                test_name="Schema Structure Validation",
                passed=False,
                message=f"Schema structure validation failed: {str(e)}"
            )
    
    def validate_schema_fields(self) -> ValidationResult:
        """测试2: 验证Schema字段定义"""
        try:
            schema = self.schema
            props = schema.get("properties", {})
            
            assert "timestamp" in props, "Missing timestamp property"
            assert props["timestamp"]["type"] == "string", "timestamp must be string"
            assert props["timestamp"]["format"] == "date-time", "timestamp must be date-time format"
            
            assert "action" in props, "Missing action property"
            assert "enum" in props["action"], "action must have enum constraint"
            valid_actions = ["LOAD", "RELOAD", "CHANGE", "ROLLBACK", "VALIDATE", "EXPORT", "IMPORT"]
            assert set(props["action"]["enum"]) == set(valid_actions), \
                f"action enum values mismatch: {props['action']['enum']}"
            
            assert "config_file" in props, "Missing config_file property"
            assert "pattern" in props["config_file"], "config_file must have pattern constraint"
            
            assert "operator" in props, "Missing operator property"
            assert props["operator"]["type"] == "object", "operator must be object"
            
            assert "checksum" in props, "Missing checksum property"
            assert props["checksum"]["type"] == "object", "checksum must be object"
            
            return ValidationResult(
                test_name="Schema Fields Validation",
                passed=True,
                message="All required fields are properly defined"
            )
            
        except Exception as e:
            return ValidationResult(
                test_name="Schema Fields Validation",
                passed=False,
                message=f"Schema fields validation failed: {str(e)}"
            )
    
    def validate_sample_log_exists(self) -> ValidationResult:
        """测试3: 验证示例日志文件存在"""
        try:
            if not self.sample_log_file.exists():
                return ValidationResult(
                    test_name="Sample Log File Existence",
                    passed=False,
                    message=f"Sample log file not found: {self.sample_log_file}"
                )
            
            logs = self.sample_logs
            assert isinstance(logs, list), "Sample log must be a JSON array"
            assert len(logs) > 0, "Sample log must not be empty"
            
            return ValidationResult(
                test_name="Sample Log File Existence",
                passed=True,
                message=f"Sample log file exists with {len(logs)} entries"
            )
            
        except Exception as e:
            return ValidationResult(
                test_name="Sample Log File Existence",
                passed=False,
                message=f"Sample log validation failed: {str(e)}"
            )
    
    def validate_sample_log_entries(self) -> ValidationResult:
        """测试4: 验证示例日志条目格式"""
        try:
            if not HAS_JSONSCHEMA:
                return ValidationResult(
                    test_name="Sample Log Entries Validation",
                    passed=False,
                    message="jsonschema package not installed"
                )
            
            logs = self.sample_logs
            schema = self.schema
            
            errors = []
            for i, entry in enumerate(logs):
                try:
                    validate(instance=entry, schema=schema)
                except ValidationError as e:
                    errors.append(f"Entry {i}: {e.message}")
            
            if errors:
                return ValidationResult(
                    test_name="Sample Log Entries Validation",
                    passed=False,
                    message=f"Found {len(errors)} validation errors",
                    details="\n  ".join(errors[:5])
                )
            
            return ValidationResult(
                test_name="Sample Log Entries Validation",
                passed=True,
                message=f"All {len(logs)} entries are valid"
            )
            
        except Exception as e:
            return ValidationResult(
                test_name="Sample Log Entries Validation",
                passed=False,
                message=f"Sample log entries validation failed: {str(e)}"
            )
    
    def validate_action_types(self) -> ValidationResult:
        """测试5: 验证所有动作类型都有示例"""
        try:
            logs = self.sample_logs
            actions_found = set(entry.get("action") for entry in logs)
            
            required_actions = {"LOAD", "RELOAD", "CHANGE", "ROLLBACK", "VALIDATE", "EXPORT", "IMPORT"}
            missing_actions = required_actions - actions_found
            
            if missing_actions:
                return ValidationResult(
                    test_name="Action Types Coverage",
                    passed=False,
                    message=f"Missing action types in sample: {missing_actions}"
                )
            
            return ValidationResult(
                test_name="Action Types Coverage",
                passed=True,
                message=f"All 7 action types are covered: {actions_found}"
            )
            
        except Exception as e:
            return ValidationResult(
                test_name="Action Types Coverage",
                passed=False,
                message=f"Action types validation failed: {str(e)}"
            )
    
    def validate_operator_types(self) -> ValidationResult:
        """测试6: 验证操作者类型覆盖"""
        try:
            logs = self.sample_logs
            operator_types = set(entry.get("operator", {}).get("type") for entry in logs)
            
            required_types = {"user", "system", "ci_cd"}
            missing_types = required_types - operator_types
            
            if missing_types:
                return ValidationResult(
                    test_name="Operator Types Coverage",
                    passed=False,
                    message=f"Missing operator types in sample: {missing_types}"
                )
            
            return ValidationResult(
                test_name="Operator Types Coverage",
                passed=True,
                message=f"All 3 operator types are covered: {operator_types}"
            )
            
        except Exception as e:
            return ValidationResult(
                test_name="Operator Types Coverage",
                passed=False,
                message=f"Operator types validation failed: {str(e)}"
            )
    
    def validate_checksum_format(self) -> ValidationResult:
        """测试7: 验证校验和格式"""
        try:
            logs = self.sample_logs
            
            for i, entry in enumerate(logs):
                checksum = entry.get("checksum", {})
                
                assert checksum.get("algorithm") == "sha256", \
                    f"Entry {i}: algorithm must be 'sha256'"
                
                before = checksum.get("before", "")
                after = checksum.get("after", "")
                
                if before:
                    assert len(before) == 64, \
                        f"Entry {i}: before hash must be 64 chars (SHA-256)"
                    assert all(c in "0123456789abcdef" for c in before), \
                        f"Entry {i}: before hash must be hexadecimal"
                
                if after:
                    assert len(after) == 64, \
                        f"Entry {i}: after hash must be 64 chars (SHA-256)"
                    assert all(c in "0123456789abcdef" for c in after), \
                        f"Entry {i}: after hash must be hexadecimal"
            
            return ValidationResult(
                test_name="Checksum Format Validation",
                passed=True,
                message="All checksums are properly formatted SHA-256 hashes"
            )
            
        except Exception as e:
            return ValidationResult(
                test_name="Checksum Format Validation",
                passed=False,
                message=f"Checksum format validation failed: {str(e)}"
            )
    
    def validate_timestamp_format(self) -> ValidationResult:
        """测试8: 验证时间戳格式"""
        try:
            from datetime import datetime
            
            logs = self.sample_logs
            
            for i, entry in enumerate(logs):
                timestamp_str = entry.get("timestamp")
                
                try:
                    datetime.fromisoformat(timestamp_str.replace('Z', '+00:00'))
                except ValueError:
                    raise AssertionError(
                        f"Entry {i}: Invalid ISO 8601 timestamp: {timestamp_str}"
                    )
            
            return ValidationResult(
                test_name="Timestamp Format Validation",
                passed=True,
                message="All timestamps are valid ISO 8601 format"
            )
            
        except Exception as e:
            return ValidationResult(
                test_name="Timestamp Format Validation",
                passed=False,
                message=f"Timestamp format validation failed: {str(e)}"
            )
    
    def validate_changes_structure(self) -> ValidationResult:
        """测试9: 验证变更详情结构"""
        try:
            logs = self.sample_logs
            
            for i, entry in enumerate(logs):
                changes = entry.get("changes", [])
                
                for j, change in enumerate(changes):
                    assert "path" in change, \
                        f"Entry {i}, Change {j}: Missing 'path' field"
                    assert "old_value" in change, \
                        f"Entry {i}, Change {j}: Missing 'old_value' field"
                    assert "new_value" in change, \
                        f"Entry {i}, Change {j}: Missing 'new_value' field"
            
            return ValidationResult(
                test_name="Changes Structure Validation",
                passed=True,
                message="All change items have required fields"
            )
            
        except Exception as e:
            return ValidationResult(
                test_name="Changes Structure Validation",
                passed=False,
                message=f"Changes structure validation failed: {str(e)}"
            )
    
    def validate_metadata_fields(self) -> ValidationResult:
        """测试10: 验证元数据字段"""
        try:
            logs = self.sample_logs
            
            environments = set()
            versions = set()
            
            for entry in logs:
                metadata = entry.get("metadata", {})
                
                if "environment" in metadata:
                    environments.add(metadata["environment"])
                
                if "version" in metadata:
                    versions.add(metadata["version"])
            
            return ValidationResult(
                test_name="Metadata Fields Validation",
                passed=True,
                message=f"Environments: {environments}, Versions: {versions}"
            )
            
        except Exception as e:
            return ValidationResult(
                test_name="Metadata Fields Validation",
                passed=False,
                message=f"Metadata fields validation failed: {str(e)}"
            )
    
    def run_all_tests(self) -> List[ValidationResult]:
        """运行所有测试"""
        tests = [
            self.validate_schema_structure,
            self.validate_schema_fields,
            self.validate_sample_log_exists,
            self.validate_sample_log_entries,
            self.validate_action_types,
            self.validate_operator_types,
            self.validate_checksum_format,
            self.validate_timestamp_format,
            self.validate_changes_structure,
            self.validate_metadata_fields,
        ]
        
        results = []
        for test in tests:
            try:
                result = test()
                results.append(result)
            except Exception as e:
                results.append(ValidationResult(
                    test_name=test.__name__,
                    passed=False,
                    message=f"Test execution failed: {str(e)}"
                ))
        
        return results


def main():
    """主函数"""
    print("=" * 70)
    print("AgentOS Config Audit Log Validation Tests")
    print("=" * 70)
    print()
    
    manager_dir = Path(__file__).parent.parent
    
    if not HAS_JSONSCHEMA:
        print("⚠️  Warning: jsonschema package not installed")
        print("   Install with: pip install jsonschema")
        print()
    
    try:
        validator = AuditLogValidator(manager_dir)
        results = validator.run_all_tests()
        
        passed = sum(1 for r in results if r.passed)
        failed = sum(1 for r in results if not r.passed)
        
        for result in results:
            print(result)
            print()
        
        print("=" * 70)
        print(f"Test Summary: {passed} passed, {failed} failed")
        print("=" * 70)
        
        if failed > 0:
            sys.exit(1)
        else:
            print("\n✅ All tests passed!")
            sys.exit(0)
            
    except FileNotFoundError as e:
        print(f"❌ Error: {e}")
        sys.exit(1)
    except Exception as e:
        print(f"❌ Unexpected error: {e}")
        import traceback
        traceback.print_exc()
        sys.exit(1)


if __name__ == "__main__":
    main()
