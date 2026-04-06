# Copyright (c) 2026 SPHARX. All Rights Reserved.
# "From data intelligence emerges."

"""
Agent Contract Validator
========================

This module provides validation for Agent contracts in the openlab Market.
It validates agent contracts against the JSON schema defined in schema.json.

Features:
- Schema validation using jsonschema
- Semantic validation for agent capabilities
- Dependency resolution validation
- Resource requirement validation
- Error reporting with detailed messages
"""

import json
import os
import sys
from pathlib import Path
from typing import Dict, List, Any, Optional, Tuple, Union
from dataclasses import dataclass, field
from enum import Enum

try:
    import jsonschema
    from jsonschema import Draft7Validator, ValidationError
except ImportError:
    print("Error: jsonschema package is required. Install with: pip install jsonschema")
    sys.exit(1)


class ValidationSeverity(Enum):
    """Severity levels for validation issues."""
    ERROR = "error"
    WARNING = "warning"
    INFO = "info"


@dataclass
class ValidationIssue:
    """Represents a validation issue found during contract validation."""
    severity: ValidationSeverity
    message: str
    path: str = ""
    value: Any = None
    schema_path: str = ""

    def __str__(self) -> str:
        """String representation of the validation issue."""
        path_str = f" at path '{self.path}'" if self.path else ""
        return f"{self.severity.value.upper()}: {self.message}{path_str}"


@dataclass
class ValidationResult:
    """Result of contract validation."""
    is_valid: bool
    issues: List[ValidationIssue] = field(default_factory=list)
    validated_data: Optional[Dict[str, Any]] = None

    def add_issue(self, issue: ValidationIssue) -> None:
        """Add a validation issue to the result."""
        self.issues.append(issue)
        if issue.severity == ValidationSeverity.ERROR:
            self.is_valid = False

    def has_errors(self) -> bool:
        """Check if there are any errors in the validation result."""
        return any(issue.severity == ValidationSeverity.ERROR for issue in self.issues)

    def get_errors(self) -> List[ValidationIssue]:
        """Get all error issues."""
        return [issue for issue in self.issues if issue.severity == ValidationSeverity.ERROR]

    def get_warnings(self) -> List[ValidationIssue]:
        """Get all warning issues."""
        return [issue for issue in self.issues if issue.severity == ValidationSeverity.WARNING]

    def get_info(self) -> List[ValidationIssue]:
        """Get all info issues."""
        return [issue for issue in self.issues if issue.severity == ValidationSeverity.INFO]

    def to_dict(self) -> Dict[str, Any]:
        """Convert validation result to dictionary."""
        return {
            "is_valid": self.is_valid,
            "has_errors": self.has_errors(),
            "errors": [str(issue) for issue in self.get_errors()],
            "warnings": [str(issue) for issue in self.get_warnings()],
            "info": [str(issue) for issue in self.get_info()],
            "validated_data": self.validated_data,
        }


class AgentContractValidator:
    """
    Validator for Agent contracts in the openlab Market.
    
    This class provides comprehensive validation for agent contracts,
    including schema validation, semantic validation, and custom business
    logic validation.
    """
    
    def __init__(self, schema_path: Optional[str] = None):
        """
        Initialize the validator.
        
        Args:
            schema_path: Path to the JSON schema file. If None, uses default
                        schema in the same directory.
        """
        if schema_path is None:
            # Use default schema in the same directory
            schema_path = Path(__file__).parent / "schema.json"
        
        self.schema_path = Path(schema_path)
        self.schema = self._load_schema()
        self.validator = Draft7Validator(self.schema)
        
    def _load_schema(self) -> Dict[str, Any]:
        """
        Load the JSON schema from file.
        
        Returns:
            Dictionary containing the JSON schema.
            
        Raises:
            FileNotFoundError: If schema file does not exist.
            JSONDecodeError: If schema file contains invalid JSON.
        """
        if not self.schema_path.exists():
            raise FileNotFoundError(f"Schema file not found: {self.schema_path}")
        
        with open(self.schema_path, 'r', encoding='utf-8') as f:
            return json.load(f)
    
    def validate(self, contract_data: Union[Dict[str, Any], str, Path]) -> ValidationResult:
        """
        Validate an agent contract.
        
        Args:
            contract_data: Contract data as dictionary, JSON string, or file path.
            
        Returns:
            ValidationResult object containing validation results.
        """
        result = ValidationResult(is_valid=True)
        
        try:
            # Load contract data
            contract_dict = self._load_contract_data(contract_data)
            result.validated_data = contract_dict
            
            # Perform schema validation
            schema_issues = self._validate_schema(contract_dict)
            for issue in schema_issues:
                result.add_issue(issue)
            
            # Only perform semantic validation if schema validation passes
            if not result.has_errors():
                semantic_issues = self._validate_semantics(contract_dict)
                for issue in semantic_issues:
                    result.add_issue(issue)
            
        except Exception as e:
            result.add_issue(ValidationIssue(
                severity=ValidationSeverity.ERROR,
                message=f"Failed to validate contract: {str(e)}"
            ))
        
        return result
    
    def _load_contract_data(self, contract_data: Union[Dict[str, Any], str, Path]) -> Dict[str, Any]:
        """
        Load contract data from various input formats.
        
        Args:
            contract_data: Contract data as dictionary, JSON string, or file path.
            
        Returns:
            Dictionary containing contract data.
            
        Raises:
            ValueError: If contract data format is invalid.
            FileNotFoundError: If contract file does not exist.
            JSONDecodeError: If contract data contains invalid JSON.
        """
        if isinstance(contract_data, dict):
            return contract_data
        elif isinstance(contract_data, (str, Path)):
            path = Path(contract_data)
            if path.exists():
                with open(path, 'r', encoding='utf-8') as f:
                    return json.load(f)
            else:
                # Try to parse as JSON string
                try:
                    return json.loads(str(contract_data))
                except json.JSONDecodeError:
                    raise FileNotFoundError(f"Contract file not found: {path}")
        else:
            raise ValueError(f"Unsupported contract data type: {type(contract_data)}")
    
    def _validate_schema(self, contract_dict: Dict[str, Any]) -> List[ValidationIssue]:
        """
        Validate contract against JSON schema.
        
        Args:
            contract_dict: Contract data as dictionary.
            
        Returns:
            List of validation issues found during schema validation.
        """
        issues = []
        
        try:
            # Validate against schema
            for error in self.validator.iter_errors(contract_dict):
                issue = ValidationIssue(
                    severity=ValidationSeverity.ERROR,
                    message=error.message,
                    path=".".join(str(p) for p in error.path),
                    value=error.instance,
                    schema_path=".".join(str(p) for p in error.schema_path)
                )
                issues.append(issue)
                
        except ValidationError as e:
            issues.append(ValidationIssue(
                severity=ValidationSeverity.ERROR,
                message=f"Schema validation error: {str(e)}"
            ))
        
        return issues
    
    def _validate_semantics(self, contract_dict: Dict[str, Any]) -> List[ValidationIssue]:
        """
        Perform semantic validation beyond JSON schema.
        
        Args:
            contract_dict: Contract data as dictionary.
            
        Returns:
            List of validation issues found during semantic validation.
        """
        issues = []
        
        # Validate entry point format
        entry_point = contract_dict.get("entry_point", "")
        if entry_point:
            if ":" not in entry_point:
                issues.append(ValidationIssue(
                    severity=ValidationSeverity.ERROR,
                    message="Entry point must be in format 'module:class'",
                    path="entry_point",
                    value=entry_point
                ))
            else:
                module_part, class_part = entry_point.split(":", 1)
                if not module_part or not class_part:
                    issues.append(ValidationIssue(
                        severity=ValidationSeverity.ERROR,
                        message="Entry point must have both module and class parts",
                        path="entry_point",
                        value=entry_point
                    ))
        
        # Validate capabilities
        capabilities = contract_dict.get("capabilities", [])
        if capabilities:
            # Check for duplicate capabilities
            seen = set()
            duplicates = []
            for i, cap in enumerate(capabilities):
                if cap in seen:
                    duplicates.append(cap)
                seen.add(cap)
            
            if duplicates:
                issues.append(ValidationIssue(
                    severity=ValidationSeverity.WARNING,
                    message=f"Duplicate capabilities found: {', '.join(duplicates)}",
                    path="capabilities",
                    value=duplicates
                ))
        
        # Validate dependencies
        dependencies = contract_dict.get("dependencies", [])
        if dependencies:
            for i, dep in enumerate(dependencies):
                if not isinstance(dep, dict):
                    issues.append(ValidationIssue(
                        severity=ValidationSeverity.ERROR,
                        message=f"Dependency at index {i} must be an object",
                        path=f"dependencies[{i}]",
                        value=dep
                    ))
                elif "name" not in dep:
                    issues.append(ValidationIssue(
                        severity=ValidationSeverity.ERROR,
                        message=f"Dependency at index {i} must have a 'name' field",
                        path=f"dependencies[{i}]",
                        value=dep
                    ))
        
        # Validate resource requirements
        resources = contract_dict.get("resource_requirements", {})
        if resources:
            memory_mb = resources.get("memory_mb")
            if memory_mb is not None and memory_mb < 10:
                issues.append(ValidationIssue(
                    severity=ValidationSeverity.WARNING,
                    message="Memory requirement is very low (less than 10MB)",
                    path="resource_requirements.memory_mb",
                    value=memory_mb
                ))
            
            cpu_cores = resources.get("cpu_cores")
            if cpu_cores is not None and cpu_cores < 0.1:
                issues.append(ValidationIssue(
                    severity=ValidationSeverity.WARNING,
                    message="CPU requirement is very low (less than 0.1 cores)",
                    path="resource_requirements.cpu_cores",
                    value=cpu_cores
                ))
        
        # Validate manager schema
        config_schema = contract_dict.get("config_schema", {})
        if config_schema:
            if config_schema.get("type") != "object":
                issues.append(ValidationIssue(
                    severity=ValidationSeverity.WARNING,
                    message="manager schema type should be 'object' for better compatibility",
                    path="config_schema.type",
                    value=config_schema.get("type")
                ))
        
        return issues
    
    def validate_file(self, file_path: Union[str, Path]) -> ValidationResult:
        """
        Validate an agent contract file.
        
        Args:
            file_path: Path to the contract file.
            
        Returns:
            ValidationResult object containing validation results.
        """
        return self.validate(file_path)
    
    def validate_string(self, json_string: str) -> ValidationResult:
        """
        Validate an agent contract from JSON string.
        
        Args:
            json_string: JSON string containing contract data.
            
        Returns:
            ValidationResult object containing validation results.
        """
        return self.validate(json_string)


def validate_contract(contract_data: Union[Dict[str, Any], str, Path], 
                     schema_path: Optional[str] = None) -> ValidationResult:
    """
    Convenience function to validate an agent contract.
    
    Args:
        contract_data: Contract data as dictionary, JSON string, or file path.
        schema_path: Path to the JSON schema file. If None, uses default schema.
        
    Returns:
        ValidationResult object containing validation results.
    """
    validator = AgentContractValidator(schema_path)
    return validator.validate(contract_data)


def main():
    """Command-line interface for contract validation."""
    import argparse
    
    parser = argparse.ArgumentParser(
        description="Validate Agent contracts for openlab Market"
    )
    parser.add_argument(
        "contract_file",
        help="Path to the agent contract file (JSON)"
    )
    parser.add_argument(
        "--schema",
        help="Path to custom schema file (default: schema.json in same directory)"
    )
    parser.add_argument(
        "--verbose", "-v",
        action="store_true",
        help="Show detailed validation results"
    )
    parser.add_argument(
        "--strict",
        action="store_true",
        help="Treat warnings as errors"
    )
    
    args = parser.parse_args()
    
    try:
        validator = AgentContractValidator(args.schema)
        result = validator.validate_file(args.contract_file)
        
        if args.strict:
            # Treat warnings as errors in strict mode
            for warning in result.get_warnings():
                result.add_issue(ValidationIssue(
                    severity=ValidationSeverity.ERROR,
                    message=f"Warning treated as error: {warning.message}",
                    path=warning.path,
                    value=warning.value
                ))
        
        if args.verbose:
            print(f"Validation Results for: {args.contract_file}")
            print(f"Valid: {result.is_valid}")
            print(f"Has Errors: {result.has_errors()}")
            print()
            
            if result.get_errors():
                print("Errors:")
                for error in result.get_errors():
                    print(f"  - {error}")
                print()
            
            if result.get_warnings():
                print("Warnings:")
                for warning in result.get_warnings():
                    print(f"  - {warning}")
                print()
            
            if result.get_info():
                print("Info:")
                for info in result.get_info():
                    print(f"  - {info}")
                print()
        
        else:
            # Simple output
            if result.is_valid and not result.has_errors():
                print(f"✓ Contract is valid: {args.contract_file}")
                if result.get_warnings():
                    print(f"  Warnings: {len(result.get_warnings())}")
            else:
                print(f"✗ Contract is invalid: {args.contract_file}")
                for error in result.get_errors():
                    print(f"  - {error}")
        
        # Exit code based on validation result
        sys.exit(0 if result.is_valid and not result.has_errors() else 1)
        
    except Exception as e:
        print(f"Error: {str(e)}", file=sys.stderr)
        sys.exit(1)


if __name__ == "__main__":
    main()