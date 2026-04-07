﻿﻿﻿"""
AgentOS 契约测试用例自动生成器
Copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
Version: 1.0.0

基于契约定义自动生成测试用例
支持多种契约格式和测试框架
"""

import json
import os
from datetime import datetime
from typing import Any, Dict, List, Optional, Set, Tuple
from dataclasses import dataclass, field
from pathlib import Path
from enum import Enum
import re


class FieldType(Enum):
    """字段类型枚举"""
    STRING = "string"
    INTEGER = "integer"
    NUMBER = "number"
    BOOLEAN = "boolean"
    ARRAY = "array"
    OBJECT = "object"
    NULL = "null"


class TestType(Enum):
    """测试类型枚举"""
    POSITIVE = "positive"
    NEGATIVE = "negative"
    BOUNDARY = "boundary"
    SECURITY = "security"


@dataclass
class ContractField:
    """契约字段"""
    name: str
    field_type: FieldType
    required: bool = True
    description: str = ""
    default: Optional[Any] = None
    min_value: Optional[float] = None
    max_value: Optional[float] = None
    min_length: Optional[int] = None
    max_length: Optional[int] = None
    pattern: Optional[str] = None
    enum_values: Optional[List[Any]] = None
    items: Optional['ContractField'] = None
    properties: Optional[Dict[str, 'ContractField']] = None


@dataclass
class Contract:
    """契约定义"""
    name: str
    version: str
    description: str
    fields: List[ContractField]
    metadata: Dict[str, Any] = field(default_factory=dict)


@dataclass
class TestCase:
    """测试用例"""
    name: str
    description: str
    test_type: TestType
    input_data: Dict[str, Any]
    expected_result: Dict[str, Any]
    contract_name: str
    priority: int = 1


class ContractParser:
    """
    契约解析器

    解析JSON Schema或其他契约格式。
    """

    def parse_json_schema(self, schema: Dict[str, Any]) -> Contract:
        """
        解析JSON Schema

        Args:
            schema: JSON Schema字典

        Returns:
            契约定义
        """
        name = schema.get("title", "UnnamedContract")
        version = schema.get("version", "1.0.0")
        description = schema.get("description", "")

        fields = []
        properties = schema.get("properties", {})
        required = set(schema.get("required", []))

        for prop_name, prop_def in properties.items():
            field = self._parse_property(prop_name, prop_def, prop_name in required)
            fields.append(field)

        return Contract(
            name=name,
            version=version,
            description=description,
            fields=fields
        )

    def _parse_property(
        self,
        name: str,
        prop_def: Dict[str, Any],
        required: bool
    ) -> ContractField:
        """解析属性定义"""
        type_str = prop_def.get("type", "string")

        field_type = {
            "string": FieldType.STRING,
            "integer": FieldType.INTEGER,
            "number": FieldType.NUMBER,
            "boolean": FieldType.BOOLEAN,
            "array": FieldType.ARRAY,
            "object": FieldType.OBJECT,
            "null": FieldType.NULL,
        }.get(type_str, FieldType.STRING)

        return ContractField(
            name=name,
            field_type=field_type,
            required=required,
            description=prop_def.get("description", ""),
            default=prop_def.get("default"),
            min_value=prop_def.get("minimum"),
            max_value=prop_def.get("maximum"),
            min_length=prop_def.get("minLength"),
            max_length=prop_def.get("maxLength"),
            pattern=prop_def.get("pattern"),
            enum_values=prop_def.get("enum")
        )

    def parse_file(self, file_path: str) -> Contract:
        """
        从文件解析契约

        Args:
            file_path: 文件路径

        Returns:
            契约定义
        """
        with open(file_path, 'r', encoding='utf-8') as f:
            schema = json.load(f)

        return self.parse_json_schema(schema)


class TestCaseGenerator:
    """
    测试用例生成器

    根据契约定义自动生成测试用例。
    """

    def __init__(self):
        """初始化测试用例生成器"""
        self.test_cases: List[TestCase] = []
        self.case_counter = 0

    def generate_for_contract(self, contract: Contract) -> List[TestCase]:
        """
        为契约生成测试用例

        Args:
            contract: 契约定义

        Returns:
            测试用例列表
        """
        self.test_cases = []

        self._generate_positive_tests(contract)
        self._generate_negative_tests(contract)
        self._generate_boundary_tests(contract)
        self._generate_security_tests(contract)

        return self.test_cases

    def _generate_positive_tests(self, contract: Contract):
        """生成正向测试用例"""
        self.case_counter += 1

        valid_data = {}
        for field in contract.fields:
            valid_data[field.name] = self._generate_valid_value(field)

        test_case = TestCase(
            name=f"test_{contract.name}_valid_input_{self.case_counter}",
            description=f"测试{contract.name}契约有效输入",
            test_type=TestType.POSITIVE,
            input_data=valid_data,
            expected_result={"valid": True, "errors": []},
            contract_name=contract.name,
            priority=1
        )
        self.test_cases.append(test_case)

        for field in contract.fields:
            if field.enum_values:
                for value in field.enum_values:
                    self.case_counter += 1
                    data = valid_data.copy()
                    data[field.name] = value

                    test_case = TestCase(
                        name=f"test_{contract.name}_{field.name}_enum_{self.case_counter}",
                        description=f"测试{contract.name}契约{field.name}字段枚举值",
                        test_type=TestType.POSITIVE,
                        input_data=data,
                        expected_result={"valid": True, "errors": []},
                        contract_name=contract.name,
                        priority=2
                    )
                    self.test_cases.append(test_case)

    def _generate_negative_tests(self, contract: Contract):
        """生成负向测试用例"""
        for field in contract.fields:
            if field.required:
                self.case_counter += 1
                data = {}
                for f in contract.fields:
                    if f.name != field.name:
                        data[f.name] = self._generate_valid_value(f)

                test_case = TestCase(
                    name=f"test_{contract.name}_{field.name}_missing",
                    description=f"测试{contract.name}契约缺少必需字段{field.name}",
                    test_type=TestType.NEGATIVE,
                    input_data=data,
                    expected_result={
                        "valid": False,
                        "errors": [f"Missing required field: {field.name}"]
                    },
                    contract_name=contract.name,
                    priority=1
                )
                self.test_cases.append(test_case)

            self.case_counter += 1
            data = {}
            for f in contract.fields:
                data[f.name] = self._generate_valid_value(f)

            data[field.name] = self._generate_invalid_type_value(field)

            test_case = TestCase(
                name=f"test_{contract.name}_{field.name}_invalid_type",
                description=f"测试{contract.name}契约{field.name}字段类型错误",
                test_type=TestType.NEGATIVE,
                input_data=data,
                expected_result={
                    "valid": False,
                    "errors": [f"Invalid type for field: {field.name}"]
                },
                contract_name=contract.name,
                priority=2
            )
            self.test_cases.append(test_case)

    def _generate_boundary_tests(self, contract: Contract):
        """生成边界测试用例"""
        for field in contract.fields:
            if field.field_type in [FieldType.INTEGER, FieldType.NUMBER]:
                if field.min_value is not None:
                    self._add_boundary_test(contract, field, field.min_value, "min")
                    self._add_boundary_test(contract, field, field.min_value - 1, "below_min", should_fail=True)

                if field.max_value is not None:
                    self._add_boundary_test(contract, field, field.max_value, "max")
                    self._add_boundary_test(contract, field, field.max_value + 1, "above_max", should_fail=True)

            elif field.field_type == FieldType.STRING:
                if field.min_length is not None:
                    min_str = "a" * field.min_length
                    self._add_boundary_test(contract, field, min_str, "min_length")

                    if field.min_length > 0:
                        short_str = "a" * (field.min_length - 1)
                        self._add_boundary_test(contract, field, short_str, "below_min_length", should_fail=True)

                if field.max_length is not None:
                    max_str = "a" * field.max_length
                    self._add_boundary_test(contract, field, max_str, "max_length")

                    long_str = "a" * (field.max_length + 1)
                    self._add_boundary_test(contract, field, long_str, "above_max_length", should_fail=True)

    def _generate_security_tests(self, contract: Contract):
        """生成安全测试用例"""
        security_payloads = {
            "sql_injection": [
                "' OR '1'='1",
                "'; DROP TABLE users; --",
                "1; SELECT * FROM users",
            ],
            "xss": [
                "<script>alert('XSS')</script>",
                "<img src=x onerror=alert('XSS')>",
                "javascript:alert('XSS')",
            ],
            "path_traversal": [
                "../../../etc/passwd",
                "..\\..\\..\\windows\\system32\\manager\\sam",
            ],
            "command_injection": [
                "; ls -la",
                "| cat /etc/passwd",
                "$(whoami)",
            ],
        }

        for field in contract.fields:
            if field.field_type == FieldType.STRING:
                for attack_type, payloads in security_payloads.items():
                    for payload in payloads:
                        self.case_counter += 1
                        data = {}
                        for f in contract.fields:
                            data[f.name] = self._generate_valid_value(f)

                        data[field.name] = payload

                        test_case = TestCase(
                            name=f"test_{contract.name}_{field.name}_{attack_type}_{self.case_counter}",
                            description=f"测试{contract.name}契约{field.name}字段{attack_type}攻击",
                            test_type=TestType.SECURITY,
                            input_data=data,
                            expected_result={
                                "valid": True,
                                "sanitized": True,
                                "attack_detected": attack_type
                            },
                            contract_name=contract.name,
                            priority=1
                        )
                        self.test_cases.append(test_case)

    def _add_boundary_test(
        self,
        contract: Contract,
        field: ContractField,
        value: Any,
        boundary_type: str,
        should_fail: bool = False
    ):
        """添加边界测试用例"""
        self.case_counter += 1
        data = {}
        for f in contract.fields:
            data[f.name] = self._generate_valid_value(f)

        data[field.name] = value

        test_case = TestCase(
            name=f"test_{contract.name}_{field.name}_{boundary_type}_{self.case_counter}",
            description=f"测试{contract.name}契约{field.name}字段边界值{boundary_type}",
            test_type=TestType.BOUNDARY,
            input_data=data,
            expected_result={
                "valid": not should_fail,
                "errors": [] if not should_fail else [f"Boundary violation for field: {field.name}"]
            },
            contract_name=contract.name,
            priority=2
        )
        self.test_cases.append(test_case)

    def _generate_valid_value(self, field: ContractField) -> Any:
        """生成有效值"""
        if field.default is not None:
            return field.default

        if field.enum_values:
            return field.enum_values[0]

        if field.field_type == FieldType.STRING:
            length = field.min_length or 10
            return "test_" + "a" * (length - 5) if length > 5 else "test"

        elif field.field_type == FieldType.INTEGER:
            return field.min_value if field.min_value is not None else 42

        elif field.field_type == FieldType.NUMBER:
            return field.min_value if field.min_value is not None else 3.14

        elif field.field_type == FieldType.BOOLEAN:
            return True

        elif field.field_type == FieldType.ARRAY:
            return []

        elif field.field_type == FieldType.OBJECT:
            return {}

        return None

    def _generate_invalid_type_value(self, field: ContractField) -> Any:
        """生成无效类型值"""
        invalid_values = {
            FieldType.STRING: 12345,
            FieldType.INTEGER: "not_a_number",
            FieldType.NUMBER: "not_a_number",
            FieldType.BOOLEAN: "not_a_boolean",
            FieldType.ARRAY: "not_an_array",
            FieldType.OBJECT: "not_an_object",
        }

        return invalid_values.get(field.field_type, None)


class TestCodeGenerator:
    """
    测试代码生成器

    将测试用例转换为可执行的测试代码。
    """

    def __init__(self, framework: str = "pytest"):
        """
        初始化测试代码生成器

        Args:
            framework: 测试框架（pytest, unittest）
        """
        self.framework = framework

    def generate_pytest_code(
        self,
        test_cases: List[TestCase],
        contract: Contract
    ) -> str:
        """
        生成pytest测试代码

        Args:
            test_cases: 测试用例列表
            contract: 契约定义

        Returns:
            Python测试代码
        """
        code_lines = [
            '"""',
            f'自动生成的契约测试 - {contract.name}',
            f'契约版本: {contract.version}',
            f'生成时间: {datetime.now().isoformat()}',
            'Copyright (c) 2026 SPHARX Ltd.',
            '"""',
            '',
            'import pytest',
            'from typing import Dict, Any',
            '',
            '',
            f'class Test{contract.name}Contract:',
            f'    """{contract.description}"""',
            '',
        ]

        for tc in test_cases:
            code_lines.extend([
                f'    def {tc.name}(self):',
                f'        """{tc.description}"""',
                f'        input_data = {json.dumps(tc.input_data, indent=12, ensure_ascii=False)}',
                f'        expected = {json.dumps(tc.expected_result, indent=12, ensure_ascii=False)}',
                '',
                f'        # @future 验证逻辑实现要点：',
                f'        # 1. 调用合约验证器：result = validate_contract(input_data)',
                f'        # 2. 验证返回值：assert result == expected',
                f'        # 3. 验证错误处理：对于预期失败的测试用例，验证异常类型和消息',
                f'        # 4. 验证边界条件：输入数据的边界值和异常情况',
                '',
                '        # 临时断言（占位符）',
                f'        assert isinstance(input_data, dict)',
                '',
                '',
            ])

        return '\n'.join(code_lines)

    def save_to_file(
        self,
        test_cases: List[TestCase],
        contract: Contract,
        output_path: str
    ) -> Path:
        """
        保存测试代码到文件

        Args:
            test_cases: 测试用例列表
            contract: 契约定义
            output_path: 输出路径

        Returns:
            文件路径
        """
        code = self.generate_pytest_code(test_cases, contract)

        path = Path(output_path)
        path.parent.mkdir(parents=True, exist_ok=True)

        with open(path, 'w', encoding='utf-8') as f:
            f.write(code)

        return path


def generate_tests_from_contract(contract_path: str, output_dir: str) -> Dict[str, Any]:
    """
    从契约文件生成测试用例

    Args:
        contract_path: 契约文件路径
        output_dir: 输出目录

    Returns:
        生成结果
    """
    parser = ContractParser()
    contract = parser.parse_file(contract_path)

    generator = TestCaseGenerator()
    test_cases = generator.generate_for_contract(contract)

    code_generator = TestCodeGenerator()
    output_path = os.path.join(output_dir, f"test_{contract.name.lower()}_generated.py")

    code_generator.save_to_file(test_cases, contract, output_path)

    return {
        "status": "completed",
        "contract": contract.name,
        "test_cases_generated": len(test_cases),
        "output_file": output_path,
        "test_types": {
            "positive": len([tc for tc in test_cases if tc.test_type == TestType.POSITIVE]),
            "negative": len([tc for tc in test_cases if tc.test_type == TestType.NEGATIVE]),
            "boundary": len([tc for tc in test_cases if tc.test_type == TestType.BOUNDARY]),
            "security": len([tc for tc in test_cases if tc.test_type == TestType.SECURITY]),
        }
    }


if __name__ == "__main__":
    print("=" * 60)
    print("AgentOS 契约测试用例自动生成器")
    print("Copyright (c) 2026 SPHARX Ltd.")
    print("=" * 60)

    sample_schema = {
        "title": "AgentContract",
        "version": "1.0.0",
        "description": "Agent契约定义",
        "type": "object",
        "required": ["agent_id", "name", "version"],
        "properties": {
            "agent_id": {
                "type": "string",
                "description": "Agent唯一标识",
                "pattern": "^[a-zA-Z0-9-]+$"
            },
            "name": {
                "type": "string",
                "description": "Agent名称",
                "minLength": 3,
                "maxLength": 50
            },
            "version": {
                "type": "string",
                "description": "版本号",
                "pattern": "^\\d+\\.\\d+\\.\\d+$"
            },
            "enabled": {
                "type": "boolean",
                "description": "是否启用",
                "default": true
            },
            "priority": {
                "type": "integer",
                "description": "优先级",
                "minimum": 1,
                "maximum": 10
            }
        }
    }

    import tempfile
    with tempfile.NamedTemporaryFile(mode='w', suffix='.json', delete=False) as f:
        json.dump(sample_schema, f)
        temp_path = f.name

    parser = ContractParser()
    contract = parser.parse_json_schema(sample_schema)

    generator = TestCaseGenerator()
    test_cases = generator.generate_for_contract(contract)

    print(f"\n契约名称: {contract.name}")
    print(f"生成测试用例数: {len(test_cases)}")

    for tc in test_cases[:5]:
        print(f"  - {tc.name}: {tc.description}")

    print(f"  ... 共 {len(test_cases)} 个测试用例")

    os.unlink(temp_path)
