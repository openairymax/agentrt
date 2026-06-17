# Copyright (c) 2026 SPHARX. All Rights Reserved.
"""
AgentOS OpenLab: Skills Contract Validator

Validates skill contract specifications for marketplace deployment.
Ensures compliance with AgentOS security, capability, and interface standards.

Usage:
    python -m agentos.openlab.markets.skills.contracts.validator --skill <skill_path>
"""

from __future__ import annotations

import json
import sys
from pathlib import Path
from typing import Any, Dict, List, Optional


class ContractValidationError(Exception):
    """Raised when a skill contract fails validation."""
    pass


class SkillContractValidator:
    """Validates skill contracts for marketplace deployment."""

    REQUIRED_FIELDS = [
        "name",
        "version",
        "description",
        "capabilities",
        "interface",
        "permissions",
    ]

    ALLOWED_PERMISSION_SCOPES = {
        "filesystem:read",
        "filesystem:write",
        "network:http",
        "network:ws",
        "process:spawn",
        "memory:read",
        "memory:write",
        "storage:local",
        "storage:cache",
        "system:info",
        "system:metrics",
    }

    ALLOWED_INTERFACE_TYPES = {
        "stdio",
        "http_rest",
        "websocket",
        "grpc",
        "message_queue",
        "function_call",
    }

    def __init__(self, contract_path: Optional[Path] = None) -> None:
        self.contract_path = contract_path
        self._contract: Dict[str, Any] = {}

    def _load_yaml(self, file_path: Path) -> Dict[str, Any]:
        try:
            import yaml
            with open(file_path, 'r', encoding='utf-8') as f:
                return yaml.safe_load(f) or {}
        except ImportError:
            raise ContractValidationError(
                "PyYAML is required for YAML contract files. "
                "Install with: pip install pyyaml"
            )

    def _load_json(self, file_path: Path) -> Dict[str, Any]:
        with open(file_path, 'r', encoding='utf-8') as f:
            return json.load(f)

    def load(self, contract_path: Path) -> Dict[str, Any]:
        """Load and parse a skill contract file (YAML or JSON)."""
        if not contract_path.exists():
            raise ContractValidationError(f"Contract file not found: {contract_path}")

        suffix = contract_path.suffix.lower()
        if suffix in ('.yaml', '.yml'):
            self._contract = self._load_yaml(contract_path)
        elif suffix == '.json':
            self._contract = self._load_json(contract_path)
        else:
            raise ContractValidationError(
                f"Unsupported contract format: {suffix}. "
                "Supported formats: .yaml, .yml, .json"
            )

        self.contract_path = contract_path
        return self._contract

    def validate(self) -> List[str]:
        """Validate the loaded contract and return list of errors."""
        errors: List[str] = []

        if not self._contract:
            errors.append("No contract loaded. Call load() first.")
            return errors

        for field in self.REQUIRED_FIELDS:
            if field not in self._contract:
                errors.append(f"Missing required field: {field}")

        if 'name' in self._contract:
            name = self._contract['name']
            if not isinstance(name, str) or not name.strip():
                errors.append("Field 'name' must be a non-empty string")

        if 'version' in self._contract:
            version = self._contract['version']
            if not isinstance(version, str):
                errors.append("Field 'version' must be a string")

        if 'capabilities' in self._contract:
            capabilities = self._contract['capabilities']
            if not isinstance(capabilities, list) or len(capabilities) == 0:
                errors.append("Field 'capabilities' must be a non-empty list")

        if 'interface' in self._contract:
            interface = self._contract['interface']
            if isinstance(interface, dict) and 'type' in interface:
                if interface['type'] not in self.ALLOWED_INTERFACE_TYPES:
                    errors.append(
                        f"Interface type '{interface['type']}' is not allowed. "
                        f"Allowed types: {', '.join(sorted(self.ALLOWED_INTERFACE_TYPES))}"
                    )
            else:
                errors.append("Field 'interface' must be a dict with a 'type' field")

        if 'permissions' in self._contract:
            perms_errors = self.validate_permissions()
            errors.extend(perms_errors)

        return errors

    def validate_permissions(self) -> List[str]:
        """Validate permission declarations are within allowed scope."""
        errors: List[str] = []

        permissions = self._contract.get('permissions', [])
        if not isinstance(permissions, list):
            return ["Field 'permissions' must be a list"]

        for perm in permissions:
            if not isinstance(perm, str):
                errors.append(f"Invalid permission entry: {perm} (must be a string)")
                continue

            scope = perm.split(':')[0] + ':' + (perm.split(':')[1] if ':' in perm else '')

            if '*' in perm:
                is_allowed = False
                for allowed in self.ALLOWED_PERMISSION_SCOPES:
                    if perm.startswith(allowed.split(':')[0] + ':'):
                        is_allowed = True
                        break
                if not is_allowed:
                    errors.append(f"Permission scope not recognized: {perm}")
            elif perm not in self.ALLOWED_PERMISSION_SCOPES:
                closest = None
                for allowed in self.ALLOWED_PERMISSION_SCOPES:
                    if allowed.startswith(scope.split(':')[0] + ':'):
                        if closest is None:
                            closest = allowed
                if closest:
                    errors.append(
                        f"Permission '{perm}' not allowed. Did you mean '{closest}'?"
                    )
                else:
                    errors.append(f"Permission '{perm}' not in allowed scopes")

        return errors


def main() -> None:
    """CLI entry point."""
    import argparse

    parser = argparse.ArgumentParser(
        prog="agentos-skill-validator",
        description="AgentOS Skill Contract Validator",
    )
    parser.add_argument(
        "--skill", type=Path, required=True,
        help="Path to skill contract file (YAML or JSON)"
    )
    args = parser.parse_args()

    validator = SkillContractValidator()

    try:
        contract = validator.load(args.skill)
        print(f"[AgentOS] Loaded contract: {contract.get('name', 'unknown')}")

        errors = validator.validate()
        if errors:
            print(f"[AgentOS] Validation failed with {len(errors)} error(s):")
            for err in errors:
                print(f"  [ERROR] {err}")
            sys.exit(1)
        else:
            print("[AgentOS] Contract validation passed.")
            sys.exit(0)
    except ContractValidationError as e:
        print(f"[AgentOS] {e}", file=sys.stderr)
        sys.exit(2)
    except Exception as e:
        print(f"[AgentOS] Unexpected error: {e}", file=sys.stderr)
        sys.exit(3)


if __name__ == "__main__":
    main()
