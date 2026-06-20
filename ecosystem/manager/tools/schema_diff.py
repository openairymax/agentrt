# Copyright (c) 2026 SPHARX. All Rights Reserved.
"""
AgentRT: Schema Diff Tool

Compares Manager 11 Schema files against agentos.yaml to detect
field inconsistencies, schema drift, and missing configurations.

Part of P1.15: Manager Schema ↔ agentos.yaml bidirectional sync.

Usage:
    python3 -m ecosystem.manager.schema_diff [--report] [--check]
    python3 -m ecosystem.manager.schema_diff --json   # CI-compatible JSON output

Exit codes:
    0 = all schemas consistent
    1 = drift detected
    2 = runtime error
"""

from __future__ import annotations

import json
import sys
from dataclasses import dataclass, field
from pathlib import Path
from typing import Any, Dict, List, Optional, Set, Tuple

import yaml


class DiffSeverity:
    ERROR = "error"
    WARNING = "warning"
    INFO = "info"


@dataclass
class DiffEntry:
    """A single schema diff finding."""
    severity: str
    schema_file: str
    yaml_path: str
    message: str
    schema_value: str = ""
    yaml_value: str = ""

    def __str__(self):
        icon = {"error": "X", "warning": "!", "info": "i"}[self.severity]
        return f"  [{icon}] {self.schema_file}: {self.yaml_path} — {self.message}"


@dataclass
class DiffReport:
    """Complete schema diff report."""
    entries: List[DiffEntry] = field(default_factory=list)
    summary: Dict[str, int] = field(default_factory=lambda: {
        "error": 0, "warning": 0, "info": 0, "ok": 0,
    })

    def add(self, entry: DiffEntry):
        self.entries.append(entry)
        self.summary[entry.severity] += 1

    def add_ok(self):
        self.summary["ok"] += 1

    def has_errors(self) -> bool:
        return self.summary["error"] > 0

    def has_warnings(self) -> bool:
        return self.summary["warning"] > 0

    def format_text(self) -> str:
        lines = [
            "=" * 60,
            "  Schema Diff Report: Manager Schemas ↔ agentos.yaml",
            "=" * 60,
            "",
            f"  Errors:   {self.summary['error']}",
            f"  Warnings: {self.summary['warning']}",
            f"  Info:     {self.summary['info']}",
            f"  OK:       {self.summary['ok']}",
            "",
        ]
        if self.entries:
            lines.append("-" * 60)
            for entry in self.entries:
                lines.append(str(entry))
            lines.append("-" * 60)
            lines.append("")

        if self.has_errors():
            lines.append("  RESULT: DRIFT DETECTED — fix required")
        elif self.has_warnings():
            lines.append("  RESULT: WARNINGS — review recommended")
        else:
            lines.append("  RESULT: CONSISTENT")
        return "\n".join(lines)

    def to_json(self) -> Dict[str, Any]:
        return {
            "summary": self.summary,
            "drift_detected": self.has_errors(),
            "entries": [
                {
                    "severity": e.severity,
                    "schema_file": e.schema_file,
                    "yaml_path": e.yaml_path,
                    "message": e.message,
                    "schema_value": e.schema_value,
                    "yaml_value": e.yaml_value,
                }
                for e in self.entries
            ],
        }


class SchemaDiffer:
    """Compares Manager JSON Schemas against agentos.yaml for consistency.

    Mappings between Manager schemas and agentos.yaml sections:
      kernel-settings.schema.json → kernel
      model.schema.json           → llm
      security-policy.schema.json → security
      logging.schema.json         → observability.logging
      agent-registry.schema.json  → (not in agentos.yaml)
      skill-registry.schema.json  → (not in agentos.yaml)
      tool-service.schema.json    → (not in agentos.yaml)
      config-management.schema.json → (not in agentos.yaml)
      config-audit-log.schema.json  → (not in agentos.yaml)
      sanitizer-rules.schema.json   → security (indirect)
      _metadata.schema.json         → (not in agentos.yaml)
    """

    def __init__(self, agentos_root: Optional[str] = None):
        if agentos_root:
            self._root = Path(agentos_root)
        else:
            self._root = Path(__file__).parent.parent.parent
        self._yaml_path = self._root / "agentos.yaml"
        self._schema_dir = self._root / "ecosystem" / "manager" / "schema"

    def run(self) -> DiffReport:
        """Run full schema diff and return report."""
        report = DiffReport()

        yaml_data = self._load_yaml()
        if yaml_data is None:
            report.add(DiffEntry(
                DiffSeverity.ERROR, "agentos.yaml", "",
                "Cannot load agentos.yaml",
            ))
            return report

        # Run individual checks
        report = self._merge_reports(report, self._check_kernel_schema(yaml_data))
        report = self._merge_reports(report, self._check_model_schema(yaml_data))
        report = self._merge_reports(report, self._check_security_schema(yaml_data))
        report = self._merge_reports(report, self._check_schema_files_exist())

        return report

    def _load_yaml(self) -> Optional[Dict[str, Any]]:
        """Load agentos.yaml."""
        try:
            with open(self._yaml_path, 'r', encoding='utf-8') as f:
                return yaml.safe_load(f)
        except Exception as e:
            return None

    def _load_schema(self, name: str) -> Optional[Dict[str, Any]]:
        """Load a Manager schema file."""
        path = self._schema_dir / name
        try:
            with open(path, 'r', encoding='utf-8') as f:
                return json.load(f)
        except Exception:
            return None

    def _get_schema_properties(self, schema: Dict, *path: str) -> Dict[str, Any]:
        """Navigate a JSON Schema to get properties at a given path."""
        obj = schema
        for key in path:
            obj = obj.get("properties", {}).get(key, {})
        return obj.get("properties", {})

    def _merge_reports(self, a: DiffReport, b: DiffReport) -> DiffReport:
        a.entries.extend(b.entries)
        for k in a.summary:
            a.summary[k] += b.summary.get(k, 0)
        return a

    # ─── Kernel Schema Check ───

    def _check_kernel_schema(self, yaml_data: Dict) -> DiffReport:
        """Check kernel-settings.schema.json against agentos.yaml kernel section."""
        report = DiffReport()
        schema = self._load_schema("kernel-settings.schema.json")
        yaml_kernel = yaml_data.get("kernel", {})

        if not schema or not yaml_kernel:
            if not schema:
                report.add(DiffEntry(
                    DiffSeverity.ERROR, "kernel-settings.schema.json", "kernel",
                    "Schema file not found or invalid",
                ))
            return report

        schema_props = schema.get("properties", {}).get("kernel", {}).get("properties", {})

        # Check kernel.ipc section
        yaml_ipc = yaml_kernel.get("ipc", {})
        schema_ipc = (
            schema_props.get("ipc", {}).get("properties", {})
        )

        # Field mapping: agentos.yaml → schema
        ipc_mappings = {
            "max_message_size": "max_connections",  # Different names, similar purpose
            # agentos.yaml has shm_pool_size_mb, schema has buffer_size_kb
        }

        # Check that yaml fields exist in schema
        for yaml_key in yaml_ipc:
            if yaml_key not in schema_ipc:
                report.add(DiffEntry(
                    DiffSeverity.WARNING, "kernel-settings.schema.json",
                    f"kernel.ipc.{yaml_key}",
                    f"Field in agentos.yaml but not in Manager schema",
                    yaml_value=str(yaml_ipc[yaml_key]),
                ))

        # Check kernel.scheduler section
        yaml_scheduler = yaml_kernel.get("scheduler", {})
        schema_scheduler = (
            schema_props.get("scheduler", {}).get("properties", {})
        )

        # Field mapping: agentos.yaml → schema
        scheduler_mappings = {
            "max_tasks": "max_concurrency",  # Different names
            "default_priority": None,  # Only in agentos.yaml
        }

        for yaml_key in yaml_scheduler:
            if yaml_key not in schema_scheduler:
                report.add(DiffEntry(
                    DiffSeverity.WARNING, "kernel-settings.schema.json",
                    f"kernel.scheduler.{yaml_key}",
                    f"Field in agentos.yaml but not in Manager schema",
                    yaml_value=str(yaml_scheduler[yaml_key]),
                ))
            else:
                report.add_ok()

        # Check kernel.memory section
        yaml_memory = yaml_kernel.get("memory", {})
        schema_memory = (
            schema_props.get("memory", {}).get("properties", {})
        )

        memory_field_mapping = {
            "max_alloc_mb": "max_allocation_mb",  # Different names, same concept
        }

        for yaml_key in yaml_memory:
            if yaml_key not in schema_memory:
                alt_name = memory_field_mapping.get(yaml_key)
                msg = "Field in agentos.yaml but not in Manager schema"
                if alt_name and alt_name in schema_memory:
                    msg = (
                        f"Field name mismatch: agentos.yaml uses "
                        f"'{yaml_key}', Manager schema uses '{alt_name}'"
                    )
                    report.add(DiffEntry(
                        DiffSeverity.WARNING, "kernel-settings.schema.json",
                        f"kernel.memory.{yaml_key}",
                        msg,
                        yaml_value=str(yaml_memory[yaml_key]),
                        schema_value=alt_name,
                    ))
                else:
                    report.add(DiffEntry(
                        DiffSeverity.WARNING, "kernel-settings.schema.json",
                        f"kernel.memory.{yaml_key}",
                        msg,
                        yaml_value=str(yaml_memory[yaml_key]),
                    ))
            else:
                report.add_ok()

        # Check required schema fields present in yaml
        schema_required = schema.get("properties", {}).get("kernel", {}).get("required", [])
        for req in schema_required:
            if req not in yaml_kernel:
                report.add(DiffEntry(
                    DiffSeverity.WARNING, "kernel-settings.schema.json",
                    f"kernel.{req}",
                    "Field required by Manager schema but missing from agentos.yaml",
                    schema_value=req,
                ))

        return report

    # ─── Model Schema Check ───

    def _check_model_schema(self, yaml_data: Dict) -> DiffReport:
        """Check model.schema.json against agentos.yaml llm section."""
        report = DiffReport()
        schema = self._load_schema("model.schema.json")
        yaml_llm = yaml_data.get("llm", {})

        if not schema:
            report.add(DiffEntry(
                DiffSeverity.ERROR, "model.schema.json", "llm",
                "Schema file not found or invalid",
            ))
            return report

        schema_props = schema.get("properties", {})

        # Check global section vs agentos.yaml llm top-level
        yaml_llm_keys = set(yaml_llm.keys())
        schema_global = schema_props.get("global", {}).get("properties", {})

        for yaml_key in yaml_llm_keys:
            if yaml_key in ("providers", "routing", "cache"):
                continue  # These are in different schema sections
            if yaml_key not in schema_global:
                report.add(DiffEntry(
                    DiffSeverity.WARNING, "model.schema.json",
                    f"llm.{yaml_key}",
                    "Field in agentos.yaml but not in model schema global section",
                    yaml_value=str(yaml_llm[yaml_key]),
                ))
            else:
                report.add_ok()

        # Check providers section
        yaml_providers = yaml_llm.get("providers", {})
        schema_models = schema_props.get("models", {})

        if yaml_providers and schema_models:
            report.add_ok()  # Both exist

        # Check routing
        yaml_routing = yaml_llm.get("routing", {})
        schema_routing = schema_props.get("routing", {}).get("properties", {})

        routing_mapping = {
            "strategy": "cost_optimization",  # Different structure
            "fallback_chain": None,
            "cost_budget_daily_usd": None,
        }

        for yaml_key in yaml_routing:
            if yaml_key not in schema_routing:
                alt = routing_mapping.get(yaml_key)
                if alt and alt in schema_routing:
                    report.add(DiffEntry(
                        DiffSeverity.INFO, "model.schema.json",
                        f"llm.routing.{yaml_key}",
                        f"Field name differs: yaml='{yaml_key}', schema='{alt}'",
                        yaml_value=str(yaml_routing[yaml_key]),
                    ))
                else:
                    report.add(DiffEntry(
                        DiffSeverity.INFO, "model.schema.json",
                        f"llm.routing.{yaml_key}",
                        "Field in agentos.yaml not in model schema routing",
                        yaml_value=str(yaml_routing[yaml_key]),
                    ))
            else:
                report.add_ok()

        return report

    # ─── Security Schema Check ───

    def _check_security_schema(self, yaml_data: Dict) -> DiffReport:
        """Check security-policy.schema.json against agentos.yaml security section."""
        report = DiffReport()
        schema = self._load_schema("security-policy.schema.json")
        yaml_security = yaml_data.get("security", {})

        if not schema or not yaml_security:
            if not schema:
                report.add(DiffEntry(
                    DiffSeverity.ERROR, "security-policy.schema.json", "security",
                    "Schema file not found or invalid",
                ))
            return report

        schema_security = schema.get("properties", {}).get("security", {}).get("properties", {})

        # Check agentos.yaml security fields exist in schema
        for yaml_key in yaml_security:
            if yaml_key not in schema_security:
                report.add(DiffEntry(
                    DiffSeverity.WARNING, "security-policy.schema.json",
                    f"security.{yaml_key}",
                    "Field in agentos.yaml but not in Manager security schema",
                    yaml_value=str(yaml_security[yaml_key]),
                ))
            else:
                report.add_ok()

        # Check schema required fields exist in yaml
        schema_required = schema.get("properties", {}).get("security", {}).get("required", [])
        for req in schema_required:
            if req not in yaml_security:
                report.add(DiffEntry(
                    DiffSeverity.WARNING, "security-policy.schema.json",
                    f"security.{req}",
                    "Field required by Manager schema but missing from agentos.yaml",
                    schema_value=req,
                ))

        return report

    # ─── Schema File Existence Check ───

    def _check_schema_files_exist(self) -> DiffReport:
        """Check that all expected Manager schema files exist."""
        report = DiffReport()

        expected_schemas = [
            ("_metadata.schema.json", "metadata"),
            ("kernel-settings.schema.json", "kernel"),
            ("model.schema.json", "llm"),
            ("security-policy.schema.json", "security"),
            ("logging.schema.json", "observability.logging"),
            ("agent-registry.schema.json", "agent registry"),
            ("skill-registry.schema.json", "skill registry"),
            ("tool-service.schema.json", "tool service"),
            ("config-management.schema.json", "config management"),
            ("config-audit-log.schema.json", "audit log"),
            ("sanitizer-rules.schema.json", "sanitizer"),
        ]

        for filename, yaml_section in expected_schemas:
            schema_path = self._schema_dir / filename
            if not schema_path.exists():
                report.add(DiffEntry(
                    DiffSeverity.ERROR, filename, yaml_section,
                    f"Manager schema file '{filename}' not found",
                ))
            else:
                report.add_ok()

        return report


def main():
    """CLI entry point."""
    import argparse

    parser = argparse.ArgumentParser(
        description="Schema Diff: Manager Schemas ↔ agentos.yaml",
    )
    parser.add_argument(
        "--json", action="store_true",
        help="Output in JSON format (for CI)",
    )
    parser.add_argument(
        "--check", action="store_true",
        help="Exit with non-zero code if drift detected",
    )
    args = parser.parse_args()

    differ = SchemaDiffer()
    report = differ.run()

    if args.json:
        print(json.dumps(report.to_json(), indent=2))
    else:
        print(report.format_text())

    if args.check and report.has_errors():
        sys.exit(1)
    elif report.has_errors():
        sys.exit(1)

    sys.exit(0)


if __name__ == "__main__":
    main()