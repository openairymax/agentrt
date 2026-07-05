#!/usr/bin/env python3
# Copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
# config_validator.py — AgentRT agentrt.yaml Schema 校验器 (ACC-C19)
#
# 用法:
#   python3 scripts/ci/pipeline/validate/config_validator.py [--config agentrt.yaml] [--strict]
#
# 校验项:
#   1. YAML 语法合法性
#   2. 顶层必填字段存在性
#   3. 各节必填字段完整性
#   4. 字段类型正确性
#   5. 值范围合理性
#   6. 引用完整性（如 provider 名称引用）

import os
import sys
import re
from typing import Any, Dict, List, Optional, Tuple

try:
    import yaml
except ImportError:
    print("[FATAL] PyYAML is required. Install with: pip install pyyaml")
    sys.exit(1)


# ============================================================================
# Schema 定义 — agentrt.yaml v0.1.1
# ============================================================================

# 必填字段定义: (key, type, validator_fn, description)
# validator_fn 返回 (ok: bool, message: str)

def _is_nonempty_string(v: Any) -> Tuple[bool, str]:
    if not isinstance(v, str) or not v.strip():
        return False, "must be a non-empty string"
    return True, ""

def _is_string(v: Any) -> Tuple[bool, str]:
    if not isinstance(v, str):
        return False, f"expected string, got {type(v).__name__}"
    return True, ""

def _is_int_positive(v: Any) -> Tuple[bool, str]:
    if not isinstance(v, int) or v <= 0:
        return False, "must be a positive integer"
    return True, ""

def _is_int_nonneg(v: Any) -> Tuple[bool, str]:
    if not isinstance(v, int) or v < 0:
        return False, "must be a non-negative integer"
    return True, ""

def _is_int_range(low: int, high: int):
    def _check(v: Any) -> Tuple[bool, str]:
        if not isinstance(v, int):
            return False, f"expected integer, got {type(v).__name__}"
        if v < low or v > high:
            return False, f"must be between {low} and {high}"
        return True, ""
    return _check

def _is_bool(v: Any) -> Tuple[bool, str]:
    if not isinstance(v, bool):
        return False, f"expected boolean, got {type(v).__name__}"
    return True, ""

def _is_float_positive(v: Any) -> Tuple[bool, str]:
    if not isinstance(v, (int, float)) or v <= 0:
        return False, "must be a positive number"
    return True, ""

def _is_list_of_strings(v: Any) -> Tuple[bool, str]:
    if not isinstance(v, list):
        return False, f"expected list, got {type(v).__name__}"
    for i, item in enumerate(v):
        if not isinstance(item, str):
            return False, f"item[{i}] must be a string"
    return True, ""

def _is_list_of_dicts(v: Any) -> Tuple[bool, str]:
    if not isinstance(v, list):
        return False, f"expected list, got {type(v).__name__}"
    for i, item in enumerate(v):
        if not isinstance(item, dict):
            return False, f"item[{i}] must be a dict"
    return True, ""

def _is_port(v: Any) -> Tuple[bool, str]:
    if not isinstance(v, int):
        return False, f"expected integer, got {type(v).__name__}"
    if v < 1 or v > 65535:
        return False, "must be a valid port (1-65535)"
    return True, ""


# Schema 结构: 顶层 section → { required: bool, fields: [(key, validator, message)] }
SCHEMA: Dict[str, dict] = {
    "kernel": {
        "required": True,
        "fields": [
            ("name", _is_nonempty_string, "kernel.name"),
            ("version", _is_nonempty_string, "kernel.version"),
            ("log_level", _is_string, "kernel.log_level"),
            ("ipc.max_message_size", _is_int_positive, "kernel.ipc.max_message_size"),
            ("ipc.shm_pool_size_mb", _is_int_positive, "kernel.ipc.shm_pool_size_mb"),
            ("scheduler.max_tasks", _is_int_positive, "kernel.scheduler.max_tasks"),
            ("scheduler.time_slice_ms", _is_int_positive, "kernel.scheduler.time_slice_ms"),
            ("memory.max_alloc_mb", _is_int_positive, "kernel.memory.max_alloc_mb"),
            ("memory.oom_watermark_percent", _is_int_range(1, 99), "kernel.memory.oom_watermark_percent"),
            ("memory.sensitive_zero_on_free", _is_bool, "kernel.memory.sensitive_zero_on_free"),
        ],
    },
    "llm": {
        "required": True,
        "fields": [
            ("default_provider", _is_nonempty_string, "llm.default_provider"),
            ("routing.strategy", _is_nonempty_string, "llm.routing.strategy"),
            ("routing.cost_budget_daily_usd", _is_float_positive, "llm.routing.cost_budget_daily_usd"),
            ("cache.enabled", _is_bool, "llm.cache.enabled"),
            ("cache.ttl_seconds", _is_int_positive, "llm.cache.ttl_seconds"),
        ],
    },
    "memory": {
        "required": True,
        "fields": [
            ("enabled", _is_bool, "memory.enabled"),
            ("mode", _is_nonempty_string, "memory.mode"),
            ("storage_path", _is_nonempty_string, "memory.storage_path"),
        ],
    },
    "security": {
        "required": True,
        "fields": [
            ("enabled", _is_bool, "security.enabled"),
            ("default_policy", _is_string, "security.default_policy"),
            ("audit.enabled", _is_bool, "security.audit.enabled"),
        ],
    },
    "multi_agent": {
        "required": True,
        "fields": [
            ("enabled", _is_bool, "multi_agent.enabled"),
            ("max_concurrent_agents", _is_int_positive, "multi_agent.max_concurrent_agents"),
            ("communication.protocol", _is_nonempty_string, "multi_agent.communication.protocol"),
        ],
    },
    "gateway": {
        "required": True,
        "fields": [
            ("enabled", _is_bool, "gateway.enabled"),
            ("http.port", _is_port, "gateway.http.port"),
        ],
    },
    "hooks": {
        "required": True,
        "fields": [
            ("enabled", _is_bool, "hooks.enabled"),
        ],
    },
    "plugins": {
        "required": True,
        "fields": [
            ("enabled", _is_bool, "plugins.enabled"),
        ],
    },
    "observability": {
        "required": True,
        "fields": [
            ("metrics.enabled", _is_bool, "observability.metrics.enabled"),
            ("metrics.port", _is_port, "observability.metrics.port"),
            ("logging.level", _is_string, "observability.logging.level"),
            ("health.endpoint", _is_nonempty_string, "observability.health.endpoint"),
        ],
    },
}

VALID_LOG_LEVELS = {"trace", "debug", "info", "warn", "warning", "error", "fatal", "critical"}
VALID_ROUTING_STRATEGIES = {"cost_aware", "round_robin", "least_latency", "quality_first"}
VALID_MEMORY_MODES = {"full", "l1_only", "l1_l2", "off"}
VALID_SECURITY_MODES = {"standard", "strict", "permissive"}
VALID_SECURITY_POLICIES = {"allow", "deny"}
VALID_COLLAB_PATTERNS = {"orchestrator", "debate", "hierarchy", "market"}
VALID_LOG_FORMATS = {"json", "text", "console"}


def _nested_get(data: dict, dotted_key: str) -> Any:
    """Get nested dict value by dotted key, e.g. 'kernel.ipc.max_message_size'."""
    keys = dotted_key.split(".")
    current = data
    for k in keys:
        if not isinstance(current, dict):
            return None
        current = current.get(k)
        if current is None:
            return None
    return current


# ============================================================================
# 校验逻辑
# ============================================================================

class ValidationError:
    def __init__(self, section: str, field: str, message: str):
        self.section = section
        self.field = field
        self.message = message

    def __str__(self):
        return f"[{self.section}] {self.field}: {self.message}"


class ValidationWarning:
    def __init__(self, section: str, field: str, message: str):
        self.section = section
        self.field = field
        self.message = message

    def __str__(self):
        return f"[WARN] [{self.section}] {self.field}: {self.message}"


def validate_yaml_syntax(filepath: str) -> Tuple[Optional[dict], List[str]]:
    """Validate YAML syntax and parse."""
    errors = []
    try:
        with open(filepath, 'r', encoding='utf-8') as f:
            data = yaml.safe_load(f)
        if data is None:
            errors.append("YAML file is empty or contains only comments")
            return None, errors
        if not isinstance(data, dict):
            errors.append("YAML root must be a mapping (dict)")
            return None, errors
        return data, errors
    except yaml.YAMLError as e:
        errors.append(f"YAML syntax error: {e}")
        return None, errors
    except FileNotFoundError:
        errors.append(f"File not found: {filepath}")
        return None, errors


def validate_top_level(data: dict) -> List[ValidationError]:
    """Validate top-level required sections."""
    errors = []
    # Check version
    if "version" not in data:
        errors.append(ValidationError("top", "version", "missing required field"))
    elif not isinstance(data["version"], str):
        errors.append(ValidationError("top", "version", "must be a string"))

    # Check all required sections exist
    for section, spec in SCHEMA.items():
        if spec["required"] and section not in data:
            errors.append(ValidationError(section, "(section)", f"missing required section '{section}'"))
    return errors


def validate_schema_fields(data: dict) -> Tuple[List[ValidationError], List[ValidationWarning]]:
    """Validate all fields against the schema."""
    errors = []
    warnings = []

    for section, spec in SCHEMA.items():
        section_data = data.get(section)
        if section_data is None:
            continue  # Already reported as missing section

        if not isinstance(section_data, dict):
            errors.append(ValidationError(section, "(section)", f"must be a mapping, got {type(section_data).__name__}"))
            continue

        for field_path, validator, label in spec["fields"]:
            value = _nested_get(section_data, field_path)
            if value is None:
                errors.append(ValidationError(section, label, "missing required field"))
                continue

            ok, msg = validator(value)
            if not ok:
                errors.append(ValidationError(section, label, f"{msg} (got: {repr(value)})"))

    return errors, warnings


def validate_semantic(data: dict) -> Tuple[List[ValidationError], List[ValidationWarning]]:
    """Validate semantic rules (cross-field consistency)."""
    errors = []
    warnings = []

    # kernel.log_level
    log_level = _nested_get(data, "kernel.log_level")
    if log_level and isinstance(log_level, str) and log_level.lower() not in VALID_LOG_LEVELS:
        warnings.append(ValidationWarning("kernel", "log_level",
            f"'{log_level}' is not a standard log level; expected one of {sorted(VALID_LOG_LEVELS)}"))

    # kernel.version should match top-level version
    kernel_version = _nested_get(data, "kernel.version")
    top_version = data.get("version")
    if kernel_version and top_version and kernel_version != top_version:
        warnings.append(ValidationWarning("kernel", "version",
            f"kernel.version ({kernel_version}) != top-level version ({top_version})"))

    # llm.routing.strategy
    strategy = _nested_get(data, "llm.routing.strategy")
    if strategy and strategy not in VALID_ROUTING_STRATEGIES:
        errors.append(ValidationError("llm", "routing.strategy",
            f"'{strategy}' is not valid; expected one of {sorted(VALID_ROUTING_STRATEGIES)}"))

    # llm.default_provider must exist in providers
    default_provider = _nested_get(data, "llm.default_provider")
    providers_data = _nested_get(data, "llm.providers")
    if default_provider and isinstance(providers_data, dict):
        if default_provider not in providers_data:
            errors.append(ValidationError("llm", "default_provider",
                f"'{default_provider}' not found in llm.providers"))

    # llm.fallback_chain providers must exist in providers
    fallback_chain = _nested_get(data, "llm.routing.fallback_chain")
    if fallback_chain and isinstance(fallback_chain, list) and isinstance(providers_data, dict):
        for fb in fallback_chain:
            if fb not in providers_data:
                errors.append(ValidationError("llm", "routing.fallback_chain",
                    f"provider '{fb}' in fallback_chain not found in llm.providers"))

    # memory.mode
    memory_mode = _nested_get(data, "memory.mode")
    if memory_mode and memory_mode not in VALID_MEMORY_MODES:
        errors.append(ValidationError("memory", "mode",
            f"'{memory_mode}' is not valid; expected one of {sorted(VALID_MEMORY_MODES)}"))

    # security.default_policy
    policy = _nested_get(data, "security.default_policy")
    if policy and policy.lower() not in VALID_SECURITY_POLICIES:
        errors.append(ValidationError("security", "default_policy",
            f"'{policy}' is not valid; expected one of {sorted(VALID_SECURITY_POLICIES)}"))

    # security.mode
    sec_mode = _nested_get(data, "security.mode")
    if sec_mode and sec_mode not in VALID_SECURITY_MODES:
        warnings.append(ValidationWarning("security", "mode",
            f"'{sec_mode}' is not a standard mode; expected one of {sorted(VALID_SECURITY_MODES)}"))

    # multi_agent.collaboration.default_pattern
    collab_pattern = _nested_get(data, "multi_agent.collaboration.default_pattern")
    if collab_pattern and collab_pattern not in VALID_COLLAB_PATTERNS:
        errors.append(ValidationError("multi_agent", "collaboration.default_pattern",
            f"'{collab_pattern}' is not valid; expected one of {sorted(VALID_COLLAB_PATTERNS)}"))

    # observability.logging.format
    log_fmt = _nested_get(data, "observability.logging.format")
    if log_fmt and log_fmt not in VALID_LOG_FORMATS:
        warnings.append(ValidationWarning("observability", "logging.format",
            f"'{log_fmt}' is not standard; expected one of {sorted(VALID_LOG_FORMATS)}"))

    return errors, warnings


def validate_env_security(data: dict) -> List[ValidationWarning]:
    """Check that no API keys are hardcoded in the config."""
    warnings = []
    providers_data = _nested_get(data, "llm.providers")
    if isinstance(providers_data, dict):
        for name, provider in providers_data.items():
            if isinstance(provider, dict):
                api_key = provider.get("api_key")
                if api_key and isinstance(api_key, str) and len(api_key) > 10:
                    warnings.append(ValidationWarning("llm", f"providers.{name}.api_key",
                        "API key appears hardcoded in config. Use api_key_env instead."))
    return warnings


# ============================================================================
# 主函数
# ============================================================================

def main():
    import argparse
    parser = argparse.ArgumentParser(
        description="AgentRT agentrt.yaml Schema Validator (ACC-C19)")
    parser.add_argument("--config", default="configs/agentrt.yaml",
                        help="Path to agentrt.yaml (default: configs/agentrt.yaml)")
    parser.add_argument("--strict", action="store_true",
                        help="Treat warnings as errors")
    parser.add_argument("--json", action="store_true",
                        help="Output results as JSON")
    args = parser.parse_args()

    config_path = args.config
    if not os.path.isabs(config_path):
        # Try relative to project root
        script_dir = os.path.dirname(os.path.abspath(__file__))
        project_root = os.path.abspath(os.path.join(script_dir, "..", "..", "..", ".."))
        config_path = os.path.join(project_root, config_path)

    all_errors = []
    all_warnings = []

    # Step 1: YAML Syntax
    print("=" * 60)
    print("  AgentRT Config Validator (ACC-C19)")
    print(f"  Config: {config_path}")
    print("=" * 60)
    print()

    data, syntax_errors = validate_yaml_syntax(config_path)
    if syntax_errors:
        for e in syntax_errors:
            print(f"  [ERROR] {e}")
            all_errors.append(ValidationError("syntax", "", e))
    if data is None:
        print(f"\n  Result: FAILED ({len(all_errors)} syntax error(s))")
        sys.exit(1 if not args.json else 0)

    print("  [OK] YAML syntax valid")
    print()

    # Step 2: Top-level sections
    print("--- Top-Level Validation ---")
    top_errors = validate_top_level(data)
    for e in top_errors:
        print(f"  [ERROR] {e}")
    all_errors.extend(top_errors)
    if not top_errors:
        print("  [OK] All required top-level sections present")
    print()

    # Step 3: Schema field validation
    print("--- Schema Field Validation ---")
    field_errors, field_warnings = validate_schema_fields(data)
    for e in field_errors:
        print(f"  [ERROR] {e}")
    for w in field_warnings:
        print(f"  [WARN]  {w}")
    all_errors.extend(field_errors)
    all_warnings.extend(field_warnings)
    if not field_errors:
        print("  [OK] All required fields present and valid")
    print()

    # Step 4: Semantic validation
    print("--- Semantic Validation ---")
    sem_errors, sem_warnings = validate_semantic(data)
    for e in sem_errors:
        print(f"  [ERROR] {e}")
    for w in sem_warnings:
        print(f"  [WARN]  {w}")
    all_errors.extend(sem_errors)
    all_warnings.extend(sem_warnings)
    if not sem_errors:
        print("  [OK] Semantic rules passed")
    print()

    # Step 5: Security check
    print("--- Security Check ---")
    env_warnings = validate_env_security(data)
    for w in env_warnings:
        print(f"  [WARN]  {w}")
    all_warnings.extend(env_warnings)
    if not env_warnings:
        print("  [OK] No hardcoded secrets detected")
    print()

    # Summary
    print("=" * 60)
    print(f"  Errors:   {len(all_errors)}")
    print(f"  Warnings: {len(all_warnings)}")
    print("=" * 60)

    if args.strict and all_warnings:
        print("  Result: FAILED (strict mode: warnings treated as errors)")
        print(f"  {len(all_warnings)} warning(s) found")
        sys.exit(1)
    elif all_errors:
        print(f"  Result: FAILED ({len(all_errors)} error(s))")
        sys.exit(1)
    else:
        print("  Result: PASSED")
        if all_warnings:
            print(f"  ({len(all_warnings)} non-blocking warning(s))")
        sys.exit(0)


if __name__ == "__main__":
    main()