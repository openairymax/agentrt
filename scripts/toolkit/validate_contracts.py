#!/usr/bin/env python3
# Copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
# AgentOS Contract Validator
# Migrated from scripts/operations/validate_contracts.py

"""
AgentOS Contract Validation Tool

Validates interface contracts between system components:
- Syscall header completeness and signatures
- Configuration file format and required fields
- API response format compliance

Usage:
    from scripts.toolkit import ContractValidator
    
    validator = ContractValidator()
    validator.validate_all()
    validator.print_report()
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
    contract_type: ContractType
    contract_name: str
    status: ValidationStatus
    message: str
    details: Optional[str] = None
    location: Optional[str] = None
    suggestion: Optional[str] = None


@dataclass
class ValidationReport:
    timestamp: str
    total_checks: int = 0
    passed: int = 0
    failed: int = 0
    warnings: int = 0
    skipped: int = 0
    results: List[ValidationResult] = field(default_factory=list)

    def add(self, r: ValidationResult):
        self.results.append(r)
        self.total_checks += 1
        {ValidationStatus.PASS: lambda s: setattr(s, 's', s.passed + 1),
         ValidationStatus.FAIL: lambda s: setattr(s, 's', s.failed + 1),
         ValidationStatus.WARN: lambda s: setattr(s, 's', s.warnings + 1),
         ValidationStatus.SKIP: lambda s: setattr(s, 's', s.skipped + 1)}[r.status](self)

    def is_success(self) -> bool:
        return self.failed == 0


class SyscallContractValidator:
    REQUIRED_FUNCTIONS = [
        "task_create", "task_delete", "task_wait", "task_yield",
        "memory_alloc", "memory_free", "memory_read", "memory_write",
        "session_open", "session_close", "session_send", "session_recv",
        "telemetry_init", "telemetry_record", "telemetry_flush"
    ]

    def __init__(self, spec_dir: Optional[str] = None):
        self.spec_dir = spec_dir or os.path.join(os.getcwd(), "atoms", "syscall")

    def validate_header_file(self, path: str) -> List[ValidationResult]:
        results = []
        if not os.path.exists(path):
            return [ValidationResult(ContractType.SYSCALL, os.path.basename(path),
                                     ValidationStatus.FAIL, f"File not found: {path}", location=path)]
        
        with open(path, "r", encoding="utf-8") as f:
            content = f.read()
        
        missing = [fn for fn in self.REQUIRED_FUNCTIONS if not re.search(rf'\b{fn}\s*\(', content)]
        if missing:
            results.append(ValidationResult(
                ContractType.SYSCALL, "Required Functions",
                ValidationStatus.FAIL, f"Missing: {', '.join(missing)}",
                location=path, suggestion="Implement all required syscalls"))
        else:
            results.append(ValidationResult(ContractType.SYSCALL, "Required Functions",
                                             ValidationStatus.PASS, "All functions defined"))
        return results

    def validate_all(self) -> List[ValidationResult]:
        if not os.path.exists(self.spec_dir):
            return [ValidationResult(ContractType.SYSCALL, "Directory",
                                     ValidationStatus.SKIP, f"Not found: {self.spec_dir}")]
        headers = [f for f in os.listdir(self.spec_dir) if f.endswith(".h")]
        if not headers:
            return [ValidationResult(ContractType.SYSCALL, "Headers",
                                     ValidationStatus.SKIP, "No .h files found")]
        results = []
        for h in headers:
            results.extend(self.validate_header_file(os.path.join(self.spec_dir, h)))
        return results


class ConfigContractValidator:
    REQUIRED_FIELDS = {
        "agentos.conf": ["version", "log_level", "data_dir"],
        "logging.conf": ["level", "format", "output"],
        "memory.conf": ["max_memory", "gc_threshold"]
    }

    def __init__(self, config_dir: Optional[str] = None):
        self.config_dir = config_dir or os.path.join(os.getcwd(), "manager")

    def validate_config_file(self, path: str) -> List[ValidationResult]:
        name = os.path.basename(path)
        results = []
        if not os.path.exists(path):
            return [ValidationResult(ContractType.CONFIG, name,
                                     ValidationStatus.FAIL, f"File not found: {path}", location=path)]
        
        with open(path, "r", encoding="utf-8") as f:
            content = f.read()
        
        req_fields = self.REQUIRED_FIELDS.get(name, [])
        missing = [f for f in req_fields if not re.search(rf'^{f}\s*=', content, re.MULTILINE)]
        if missing:
            results.append(ValidationResult(ContractType.CONFIG, f"Fields: {name}",
                                             ValidationStatus.FAIL, f"Missing: {', '.join(missing)}",
                                             location=path))
        else:
            results.append(ValidationResult(ContractType.CONFIG, f"Fields: {name}",
                                             ValidationStatus.PASS, "All fields present"))
        return results

    def validate_all(self) -> List[ValidationResult]:
        if not os.path.exists(self.config_dir):
            return [ValidationResult(ContractType.CONFIG, "Directory",
                                     ValidationStatus.SKIP, f"Not found: {self.config_dir}")]
        configs = [f for f in os.listdir(self.config_dir) if f.endswith((".conf", ".json"))]
        if not configs:
            return [ValidationResult(ContractType.CONFIG, "Files",
                                     ValidationStatus.SKIP, "No config files found")]
        results = []
        for c in configs:
            results.extend(self.validate_config_file(os.path.join(self.config_dir, c)))
        return results


class APIContractValidator:
    def validate_response_format(self, resp: Dict[str, Any], expected_type: str) -> List[ValidationResult]:
        required = {"agent": ["id","name","status","created_at"],
                     "task": ["id","type","priority","state"]}.get(expected_type, [])
        missing = [f for f in required if f not in resp]
        if missing:
            return [ValidationResult(ContractType.API, f"Response: {expected_type}",
                                       ValidationStatus.FAIL, f"Missing: {', '.join(missing)}")]
        return [ValidationResult(ContractType.API, f"Response: {expected_type}",
                                   ValidationStatus.PASS, "Format valid")]


class ContractValidator:
    """Main validation orchestrator"""

    def __init__(self, verbose: bool = False):
        self.verbose = verbose
        self.report = ValidationReport(timestamp=datetime.now().isoformat())

    def validate_syscall(self, spec_dir=None):
        for r in SyscallContractValidator(spec_dir).validate_all():
            self.report.add(r)

    def validate_config(self, config_dir=None):
        for r in ConfigContractValidator(config_dir).validate_all():
            self.report.add(r)

    def validate_api(self, api_spec_path=None):
        v = APIContractValidator(api_spec_path)
        test_responses = [
            {"id":"a1","name":"Test","status":"active","created_at":"2026-01-01"},
            {"id":"t1","type":"compute","priority":1,"state":"pending"}
        ]
        for resp in test_responses:
            etype = "agent" if "agent" in resp.get("id","") else "task"
            for r in v.validate_response_format(resp, etype):
                self.report.add(r)

    def validate_all(self, **kwargs):
        self.validate_syscall(kwargs.get("syscall_dir"))
        self.validate_config(kwargs.get("config_dir"))
        self.validate_api(kwargs.get("api_spec"))

    def print_report(self, fmt: str = "text"):
        if fmt == "json":
            print(json.dumps(asdict(self.report), indent=2, default=str))
            return
        
        icons = {ValidationStatus.PASS: "\033[0;32m✓\033[0m",
                 ValidationStatus.FAIL: "\033[0;31m✗\033[0m",
                 ValidationStatus.WARN: "\033[1;33m!\033[0m",
                 ValidationStatus.SKIP: "\033[0;36m-\033[0m"}
        
        print("=" * 70)
        print("AgentOS Contract Validation Report")
        print("=" * 70)
        print(f"Timestamp: {self.report.timestamp}\n")
        print(f"{'Contract':<25} {'Status':<8} {'Message'}")
        print("-" * 70)
        
        for r in self.report.results:
            icon = icons.get(r.status, "?")
            print(f"{r.contract_name:<25} {icon} {r.status.value:<6} {r.message}")
            if self.verbose and r.location:
                print(f"{'':>25}   Location: {r.location}")
            if self.verbose and r.suggestion:
                print(f"{'':>25}   Suggestion: {r.suggestion}")
        
        print("-" * 70)
        print(f"\n  \033[0;32mPassed: {self.report.passed}\033[0m  "
              f"\033[0;31mFailed: {self.report.failed}\033[0m  "
              f"\033[1;33mWarnings: {self.report.warnings}\033[0m  "
              f"\033[0;36mSkipped: {self.report.skipped}\033[0m  Total: {self.report.total_checks}")
        print(f"\n{'✓ All passed!' if self.report.is_success() else '✗ Some failed.'}")
        print("=" * 70)


def main():
    parser = argparse.ArgumentParser(description="AgentOS Contract Validation Tool")
    parser.add_argument("-t", "--type", choices=["syscall","config","api","all"], default="all")
    parser.add_argument("--spec", type=str)
    parser.add_argument("--dir", type=str)
    parser.add_argument("-v", "--verbose", action="store_true")
    parser.add_argument("-o", "--output", choices=["text","json"], default="text")
    args = parser.parse_args()
    
    validator = ContractValidator(verbose=args.verbose)
    
    if args.type in ("syscall", "all"): validator.validate_syscall(args.spec)
    if args.type in ("config", "all"): validator.validate_config(args.dir)
    if args.type in ("api", "all"): validator.validate_api(args.spec)
    
    validator.print_report(output_format=args.output)
    return 0 if validator.report.is_success() else 1


if __name__ == "__main__":
    sys.exit(main())
