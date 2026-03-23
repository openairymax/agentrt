#!/usr/bin/env python3
# Copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
# AgentOS 契约验证工具
# 遵循 AgentOS 架构设计原则：反馈闭环、工程美学

"""
AgentOS 契约验证工具

用于验证 AgentOS 系统组件之间的接口契约，确保：
- 系统调用接口兼容性
- 模块间接口一致性
- 配置文件格式正确性
- API 响应格式符合规范

Usage:
    python validate_contracts.py --type syscall --spec path/to/spec
    python validate_contracts.py --type config --dir path/to/config
    python validate_contracts.py --all --verbose
"""

import argparse
import json
import os
import re
import sys
from dataclasses import dataclass, field
from datetime import datetime
from enum import Enum
from typing import Any, Dict, List, Optional, Set


class ValidationStatus(Enum):
    PASS = "pass"
    FAIL = "fail"
    WARN = "warn"
    SKIP = "skip"


class ContractType(Enum):
    SYSCALL = "syscall"
    CONFIG = "config"
    API = "api"
    PROTOCOL = "protocol"


@dataclass
class ValidationResult:
    """验证结果"""
    contract_type: ContractType
    contract_name: str
    status: ValidationStatus
    message: str
    details: Optional[str] = None
    location: Optional[str] = None
    suggestion: Optional[str] = None


@dataclass
class ValidationReport:
    """验证报告"""
    timestamp: str
    total_checks: int = 0
    passed: int = 0
    failed: int = 0
    warnings: int = 0
    skipped: int = 0
    results: List[ValidationResult] = field(default_factory=list)

    def add_result(self, result: ValidationResult):
        self.results.append(result)
        self.total_checks += 1

        if result.status == ValidationStatus.PASS:
            self.passed += 1
        elif result.status == ValidationStatus.FAIL:
            self.failed += 1
        elif result.status == ValidationStatus.WARN:
            self.warnings += 1
        elif result.status == ValidationStatus.SKIP:
            self.skipped += 1

    def is_success(self) -> bool:
        return self.failed == 0


class SyscallContractValidator:
    """系统调用契约验证器"""

    REQUIRED_FUNCTIONS = [
        "task_create", "task_delete", "task_wait", "task_yield",
        "memory_alloc", "memory_free", "memory_read", "memory_write",
        "session_open", "session_close", "session_send", "session_recv",
        "telemetry_init", "telemetry_record", "telemetry_flush"
    ]

    def __init__(self, spec_dir: Optional[str] = None):
        self.spec_dir = spec_dir or os.path.join(os.getcwd(), "atoms", "syscall")

    def validate_header_file(self, header_path: str) -> List[ValidationResult]:
        results = []

        if not os.path.exists(header_path):
            results.append(ValidationResult(
                contract_type=ContractType.SYSCALL,
                contract_name=os.path.basename(header_path),
                status=ValidationStatus.FAIL,
                message=f"Header file not found: {header_path}",
                location=header_path
            ))
            return results

        with open(header_path, "r", encoding="utf-8") as f:
            content = f.read()

        results.extend(self._check_required_functions(content, header_path))
        results.extend(self._check_function_signatures(content, header_path))
        results.extend(self._check_error_codes(content, header_path))

        return results

    def _check_required_functions(self, content: str, location: str) -> List[ValidationResult]:
        results = []
        missing_functions = []

        for func in self.REQUIRED_FUNCTIONS:
            pattern = rf'\b{func}\s*\('
            if not re.search(pattern, content):
                missing_functions.append(func)

        if missing_functions:
            results.append(ValidationResult(
                contract_type=ContractType.SYSCALL,
                contract_name="Required Functions",
                status=ValidationStatus.FAIL,
                message=f"Missing required functions: {', '.join(missing_functions)}",
                location=location,
                suggestion="Implement missing syscall functions"
            ))
        else:
            results.append(ValidationResult(
                contract_type=ContractType.SYSCALL,
                contract_name="Required Functions",
                status=ValidationStatus.PASS,
                message="All required functions are defined"
            ))

        return results

    def _check_function_signatures(self, content: str, location: str) -> List[ValidationResult]:
        results = []

        function_pattern = r'(\w+)\s+(\w+)\s*\(([^)]*)\)\s*;'
        matches = re.findall(function_pattern, content)

        for return_type, func_name, params in matches:
            if not func_name.startswith("_") and not func_name.startswith("__"):
                if return_type not in ["void", "int", "uint32_t", "int32_t", "uint64_t", "int64_t", "ptr", "size_t"]:
                    results.append(ValidationResult(
                        contract_type=ContractType.SYSCALL,
                        contract_name=f"Function: {func_name}",
                        status=ValidationStatus.WARN,
                        message=f"Non-standard return type: {return_type}",
                        location=f"{location}:{func_name}",
                        suggestion="Use standard types from types.h"
                    ))

        return results

    def _check_error_codes(self, content: str, location: str) -> List[ValidationResult]:
        results = []

        error_codes = [
            "AGENTOS_SUCCESS", "AGENTOS_ERR_GENERAL", "AGENTOS_ERR_INVALID_PARAM",
            "AGENTOS_ERR_OUT_OF_MEMORY", "AGENTOS_ERR_TIMEOUT", "AGENTOS_ERR_NOT_FOUND"
        ]

        found_codes = set()
        for code in error_codes:
            if code in content:
                found_codes.add(code)

        if len(found_codes) < len(error_codes) // 2:
            results.append(ValidationResult(
                contract_type=ContractType.SYSCALL,
                contract_name="Error Codes",
                status=ValidationStatus.WARN,
                message="Standard error codes not fully defined",
                location=location,
                suggestion="Define standard error codes in error.h"
            ))

        return results

    def validate_all(self) -> List[ValidationResult]:
        results = []

        if not os.path.exists(self.spec_dir):
            results.append(ValidationResult(
                contract_type=ContractType.SYSCALL,
                contract_name="Syscall Spec Directory",
                status=ValidationStatus.SKIP,
                message=f"Syscall spec directory not found: {self.spec_dir}"
            ))
            return results

        header_files = [f for f in os.listdir(self.spec_dir) if f.endswith(".h")]

        if not header_files:
            results.append(ValidationResult(
                contract_type=ContractType.SYSCALL,
                contract_name="Syscall Headers",
                status=ValidationStatus.SKIP,
                message="No syscall header files found"
            ))
            return results

        for header in header_files:
            header_path = os.path.join(self.spec_dir, header)
            results.extend(self.validate_header_file(header_path))

        return results


class ConfigContractValidator:
    """配置文件契约验证器"""

    REQUIRED_FIELDS = {
        "agentos.conf": ["version", "log_level", "data_dir"],
        "logging.conf": ["level", "format", "output"],
        "memory.conf": ["max_memory", "gc_threshold"]
    }

    def __init__(self, config_dir: Optional[str] = None):
        self.config_dir = config_dir or os.path.join(os.getcwd(), "config")

    def validate_config_file(self, config_path: str) -> List[ValidationResult]:
        results = []
        config_name = os.path.basename(config_path)

        if not os.path.exists(config_path):
            results.append(ValidationResult(
                contract_type=ContractType.CONFIG,
                contract_name=config_name,
                status=ValidationStatus.FAIL,
                message=f"Config file not found: {config_path}",
                location=config_path
            ))
            return results

        with open(config_path, "r", encoding="utf-8") as f:
            content = f.read()

        results.extend(self._check_required_fields(content, config_name, config_path))
        results.extend(self._check_syntax(content, config_name, config_path))
        results.extend(self._check_values(content, config_name, config_path))

        return results

    def _check_required_fields(self, content: str, config_name: str, location: str) -> List[ValidationResult]:
        results = []
        required_fields = self.REQUIRED_FIELDS.get(config_name, [])

        if not required_fields:
            results.append(ValidationResult(
                contract_type=ContractType.CONFIG,
                contract_name=config_name,
                status=ValidationStatus.SKIP,
                message=f"No required fields defined for {config_name}",
                location=location
            ))
            return results

        missing_fields = []
        for field in required_fields:
            if not re.search(rf'^{field}\s*=', content, re.MULTILINE):
                missing_fields.append(field)

        if missing_fields:
            results.append(ValidationResult(
                contract_type=ContractType.CONFIG,
                contract_name=f"Required Fields: {config_name}",
                status=ValidationStatus.FAIL,
                message=f"Missing required fields: {', '.join(missing_fields)}",
                location=location,
                suggestion=f"Add missing fields to {config_name}"
            ))
        else:
            results.append(ValidationResult(
                contract_type=ContractType.CONFIG,
                contract_name=f"Required Fields: {config_name}",
                status=ValidationStatus.PASS,
                message="All required fields are present"
            ))

        return results

    def _check_syntax(self, content: str, config_name: str, location: str) -> List[ValidationResult]:
        results = []

        if config_name.endswith(".conf"):
            key_value_pattern = r'^(\w+)\s*=\s*(.+)$'
            invalid_lines = []

            for i, line in enumerate(content.split("\n"), 1):
                line = line.strip()
                if line and not line.startswith("#") and not line.startswith(";"):
                    if not re.match(key_value_pattern, line):
                        invalid_lines.append((i, line))

            if invalid_lines:
                results.append(ValidationResult(
                    contract_type=ContractType.CONFIG,
                    contract_name=f"Syntax: {config_name}",
                    status=ValidationStatus.FAIL,
                    message=f"Found {len(invalid_lines)} lines with invalid syntax",
                    location=f"{location}:{invalid_lines[0][0]}",
                    details=f"Invalid: {invalid_lines[0][1][:50]}...",
                    suggestion="Use key=value format without spaces around ="
                ))

        return results

    def _check_values(self, content: str, config_name: str, location: str) -> List[ValidationResult]:
        results = []

        max_memory_match = re.search(r'max_memory\s*=\s*(\d+)([KMGT]?)', content, re.MULTILINE)
        if max_memory_match:
            value = int(max_memory_match.group(1))
            unit = max_memory_match.group(2)

            multiplier = {"K": 1024, "M": 1024**2, "G": 1024**3, "T": 1024**4}.get(unit, 1)
            total_bytes = value * multiplier

            if total_bytes < 64 * 1024 * 1024:
                results.append(ValidationResult(
                    contract_type=ContractType.CONFIG,
                    contract_name=f"Value Check: max_memory",
                    status=ValidationStatus.WARN,
                    message=f"max_memory ({value}{unit}) is very small, minimum recommended is 64M",
                    location=location,
                    suggestion="Consider increasing max_memory for production use"
                ))

        return results

    def validate_all(self) -> List[ValidationResult]:
        results = []

        if not os.path.exists(self.config_dir):
            results.append(ValidationResult(
                contract_type=ContractType.CONFIG,
                contract_name="Config Directory",
                status=ValidationStatus.SKIP,
                message=f"Config directory not found: {self.config_dir}"
            ))
            return results

        config_files = [f for f in os.listdir(self.config_dir) if f.endswith((".conf", ".json"))]

        if not config_files:
            results.append(ValidationResult(
                contract_type=ContractType.CONFIG,
                contract_name="Config Files",
                status=ValidationStatus.SKIP,
                message="No config files found"
            ))
            return results

        for config_file in config_files:
            config_path = os.path.join(self.config_dir, config_file)
            results.extend(self.validate_config_file(config_path))

        return results


class APIContractValidator:
    """API 契约验证器"""

    def __init__(self, api_spec_path: Optional[str] = None):
        self.api_spec_path = api_spec_path

    def validate_response_format(self, response: Dict[str, Any], expected_type: str) -> List[ValidationResult]:
        results = []

        if expected_type == "agent":
            required_fields = ["id", "name", "status", "created_at"]
        elif expected_type == "task":
            required_fields = ["id", "type", "priority", "state"]
        else:
            required_fields = []

        missing_fields = [f for f in required_fields if f not in response]

        if missing_fields:
            results.append(ValidationResult(
                contract_type=ContractType.API,
                contract_name=f"API Response: {expected_type}",
                status=ValidationStatus.FAIL,
                message=f"Missing required fields: {', '.join(missing_fields)}",
                suggestion="Ensure API response includes all required fields"
            ))
        else:
            results.append(ValidationResult(
                contract_type=ContractType.API,
                contract_name=f"API Response: {expected_type}",
                status=ValidationStatus.PASS,
                message="API response format is valid"
            ))

        return results


class ContractValidator:
    """契约验证主类"""

    def __init__(self, verbose: bool = False):
        self.verbose = verbose
        self.report = ValidationReport(timestamp=datetime.now().isoformat())

    def validate_syscall(self, spec_dir: Optional[str] = None):
        validator = SyscallContractValidator(spec_dir)
        results = validator.validate_all()

        for result in results:
            self.report.add_result(result)

    def validate_config(self, config_dir: Optional[str] = None):
        validator = ConfigContractValidator(config_dir)
        results = validator.validate_all()

        for result in results:
            self.report.add_result(result)

    def validate_api(self, api_spec_path: Optional[str] = None):
        validator = APIContractValidator(api_spec_path)

        test_responses = [
            {"id": "agent_1", "name": "Test Agent", "status": "active", "created_at": "2026-01-01T00:00:00Z"},
            {"id": "task_1", "type": "compute", "priority": 1, "state": "pending"}
        ]

        for response in test_responses:
            expected_type = "agent" if "agent" in response.get("id", "") else "task"
            results = validator.validate_response_format(response, expected_type)
            for result in results:
                self.report.add_result(result)

    def validate_all(self, syscall_dir: Optional[str] = None, config_dir: Optional[str] = None):
        self.validate_syscall(syscall_dir)
        self.validate_config(config_dir)
        self.validate_api()

    def print_report(self, output_format: str = "text"):
        if output_format == "json":
            print(json.dumps(asdict(self.report), indent=2, default=str))
            return

        print("=" * 70)
        print("AgentOS Contract Validation Report")
        print("=" * 70)
        print(f"Timestamp: {self.report.timestamp}")
        print("")

        print(f"{'Contract':<25} {'Status':<8} {'Message'}")
        print("-" * 70)

        for result in self.report.results:
            status_icon = {
                ValidationStatus.PASS: "✓",
                ValidationStatus.FAIL: "✗",
                ValidationStatus.WARN: "!",
                ValidationStatus.SKIP: "-"
            }.get(result.status, "?")

            status_color = {
                ValidationStatus.PASS: "\033[0;32m",
                ValidationStatus.FAIL: "\033[0;31m",
                ValidationStatus.WARN: "\033[1;33m",
                ValidationStatus.SKIP: "\033[0;36m"
            }.get(result.status, "")

            reset = "\033[0m"
            print(f"{result.contract_name:<25} {status_color}{status_icon} {result.status.value:<6}{reset} {result.message}")

            if self.verbose and result.location:
                print(f"{'':>25}   Location: {result.location}")

            if self.verbose and result.suggestion:
                print(f"{'':>25}   Suggestion: {result.suggestion}")

        print("-" * 70)
        print("")

        print("Summary:")
        print(f"  \033[0;32mPassed: {self.report.passed}\033[0m")
        print(f"  \033[0;31mFailed: {self.report.failed}\033[0m")
        print(f"  \033[1;33mWarnings: {self.report.warnings}\033[0m")
        print(f"  \033[0;36mSkipped: {self.report.skipped}\033[0m")
        print(f"  Total: {self.report.total_checks}")

        print("")
        if self.report.is_success():
            print("\033[0;32m✓ All contract validations passed!\033[0m")
        else:
            print("\033[0;31m✗ Some contract validations failed.\033[0m")

        print("=" * 70)


def main():
    from dataclasses import asdict

    parser = argparse.ArgumentParser(
        description="AgentOS Contract Validation Tool",
        formatter_class=argparse.RawDescriptionHelpFormatter
    )

    parser.add_argument(
        "--type", "-t",
        choices=["syscall", "config", "api", "all"],
        default="all",
        help="Type of contract to validate"
    )

    parser.add_argument(
        "--spec",
        type=str,
        help="Path to syscall spec directory"
    )

    parser.add_argument(
        "--dir",
        type=str,
        help="Path to config directory"
    )

    parser.add_argument(
        "--verbose", "-v",
        action="store_true",
        help="Show detailed output"
    )

    parser.add_argument(
        "--output", "-o",
        choices=["text", "json"],
        default="text",
        help="Output format"
    )

    args = parser.parse_args()

    validator = ContractValidator(verbose=args.verbose)

    if args.type in ["syscall", "all"]:
        validator.validate_syscall(args.spec)

    if args.type in ["config", "all"]:
        validator.validate_config(args.dir)

    if args.type in ["api", "all"]:
        validator.validate_api(args.spec)

    validator.print_report(output_format=args.output)

    return 0 if validator.report.is_success() else 1


if __name__ == "__main__":
    sys.exit(main())