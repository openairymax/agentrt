"""
AgentOS 测试数据管理自动化框架
Copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
Version: 1.0.0

自动生成和管理测试数据
支持多种数据类型和场景
"""

import json
import random
import string
import uuid
from datetime import datetime, timedelta
from typing import Any, Dict, List, Optional, Type, Union
from dataclasses import dataclass, field, asdict
from pathlib import Path
from enum import Enum


class DataType(Enum):
    """数据类型枚举"""
    STRING = "string"
    INTEGER = "integer"
    FLOAT = "float"
    BOOLEAN = "boolean"
    DATETIME = "datetime"
    UUID = "uuid"
    EMAIL = "email"
    URL = "url"
    JSON = "json"
    LIST = "list"


@dataclass
class FieldSpec:
    """字段规格"""
    name: str
    data_type: DataType
    required: bool = True
    min_value: Optional[Union[int, float]] = None
    max_value: Optional[Union[int, float]] = None
    min_length: Optional[int] = None
    max_length: Optional[int] = None
    pattern: Optional[str] = None
    enum_values: Optional[List[Any]] = None
    default: Optional[Any] = None
    description: str = ""


@dataclass
class DataSchema:
    """数据模式"""
    name: str
    fields: List[FieldSpec]
    description: str = ""


class DataGenerator:
    """
    数据生成器

    根据模式定义自动生成测试数据。
    """

    def __init__(self, seed: Optional[int] = None):
        """
        初始化数据生成器

        Args:
            seed: 随机种子（用于可重复生成）
        """
        if seed is not None:
            random.seed(seed)

        self.generators = {
            DataType.STRING: self._generate_string,
            DataType.INTEGER: self._generate_integer,
            DataType.FLOAT: self._generate_float,
            DataType.BOOLEAN: self._generate_boolean,
            DataType.DATETIME: self._generate_datetime,
            DataType.UUID: self._generate_uuid,
            DataType.EMAIL: self._generate_email,
            DataType.URL: self._generate_url,
            DataType.JSON: self._generate_json,
            DataType.LIST: self._generate_list,
        }

    def generate_field(self, spec: FieldSpec) -> Any:
        """
        生成单个字段数据

        Args:
            spec: 字段规格

        Returns:
            生成的数据
        """
        if not spec.required and random.random() < 0.1:
            return spec.default

        generator = self.generators.get(spec.data_type)
        if generator:
            return generator(spec)
        return None

    def _generate_string(self, spec: FieldSpec) -> str:
        """生成字符串"""
        min_len = spec.min_length or 1
        max_len = spec.max_length or 50
        length = random.randint(min_len, max_len)

        if spec.pattern:
            return self._generate_from_pattern(spec.pattern)

        if spec.enum_values:
            return random.choice(spec.enum_values)

        chars = string.ascii_letters + string.digits + " "
        return ''.join(random.choice(chars) for _ in range(length))

    def _generate_integer(self, spec: FieldSpec) -> int:
        """生成整数"""
        min_val = spec.min_value if spec.min_value is not None else -10000
        max_val = spec.max_value if spec.max_value is not None else 10000
        return random.randint(int(min_val), int(max_val))

    def _generate_float(self, spec: FieldSpec) -> float:
        """生成浮点数"""
        min_val = spec.min_value if spec.min_value is not None else -10000.0
        max_val = spec.max_value if spec.max_value is not None else 10000.0
        return random.uniform(min_val, max_val)

    def _generate_boolean(self, spec: FieldSpec) -> bool:
        """生成布尔值"""
        return random.choice([True, False])

    def _generate_datetime(self, spec: FieldSpec) -> str:
        """生成日期时间"""
        now = datetime.now()
        delta = timedelta(days=random.randint(-365, 365))
        dt = now + delta
        return dt.isoformat()

    def _generate_uuid(self, spec: FieldSpec) -> str:
        """生成UUID"""
        return str(uuid.uuid4())

    def _generate_email(self, spec: FieldSpec) -> str:
        """生成邮箱"""
        domains = ["example.com", "test.org", "sample.net", "demo.io"]
        username = ''.join(random.choices(string.ascii_lowercase, k=random.randint(5, 10)))
        return f"{username}@{random.choice(domains)}"

    def _generate_url(self, spec: FieldSpec) -> str:
        """生成URL"""
        protocols = ["http", "https"]
        domains = ["example.com", "test.org", "sample.net"]
        paths = ["api", "v1", "users", "data", "items"]

        protocol = random.choice(protocols)
        domain = random.choice(domains)
        path = "/".join(random.sample(paths, random.randint(1, 3)))

        return f"{protocol}://{domain}/{path}"

    def _generate_json(self, spec: FieldSpec) -> Dict[str, Any]:
        """生成JSON对象"""
        depth = random.randint(1, 3)
        return self._generate_nested_dict(depth)

    def _generate_list(self, spec: FieldSpec) -> List[Any]:
        """生成列表"""
        length = random.randint(1, 10)
        return [self._generate_string(spec) for _ in range(length)]

    def _generate_nested_dict(self, depth: int) -> Dict[str, Any]:
        """生成嵌套字典"""
        if depth <= 0:
            return random.choice(["value", 123, True, None])

        result = {}
        for _ in range(random.randint(2, 5)):
            key = ''.join(random.choices(string.ascii_lowercase, k=random.randint(3, 8)))
            if random.random() < 0.3 and depth > 1:
                result[key] = self._generate_nested_dict(depth - 1)
            else:
                result[key] = random.choice(["value", 123, True, None])
        return result

    def _generate_from_pattern(self, pattern: str) -> str:
        """根据模式生成字符串"""
        result = []
        i = 0
        while i < len(pattern):
            if pattern[i] == '\\':
                i += 1
                if i < len(pattern):
                    result.append(pattern[i])
            elif pattern[i] == 'X':
                result.append(random.choice(string.ascii_uppercase))
            elif pattern[i] == 'x':
                result.append(random.choice(string.ascii_lowercase))
            elif pattern[i] == '9':
                result.append(random.choice(string.digits))
            elif pattern[i] == '*':
                result.append(random.choice(string.ascii_letters + string.digits))
            else:
                result.append(pattern[i])
            i += 1
        return ''.join(result)


class TestDataFactory:
    """
    测试数据工厂

    管理数据模式并批量生成测试数据。
    """

    def __init__(self, output_dir: Optional[str] = None):
        """
        初始化测试数据工厂

        Args:
            output_dir: 输出目录
        """
        self.output_dir = Path(output_dir or "tests/fixtures/data")
        self.output_dir.mkdir(parents=True, exist_ok=True)
        self.schemas: Dict[str, DataSchema] = {}
        self.generator = DataGenerator()

        self._init_default_schemas()

    def _init_default_schemas(self):
        """初始化默认数据模式"""
        self.register_schema(DataSchema(
            name="agent_contract",
            description="Agent契约测试数据模式",
            fields=[
                FieldSpec("schema_version", DataType.STRING, default="1.0.0"),
                FieldSpec("agent_id", DataType.UUID),
                FieldSpec("agent_name", DataType.STRING, min_length=3, max_length=50),
                FieldSpec("version", DataType.STRING, pattern="X.X.X"),
                FieldSpec("role", DataType.STRING, enum_values=["assistant", "planner", "executor", "monitor"]),
                FieldSpec("description", DataType.STRING, max_length=500),
                FieldSpec("capabilities", DataType.LIST),
                FieldSpec("models", DataType.JSON),
                FieldSpec("required_permissions", DataType.LIST),
                FieldSpec("cost_profile", DataType.JSON),
                FieldSpec("trust_metrics", DataType.JSON),
            ]
        ))

        self.register_schema(DataSchema(
            name="memory_entry",
            description="记忆条目测试数据模式",
            fields=[
                FieldSpec("entry_id", DataType.UUID),
                FieldSpec("content", DataType.STRING, min_length=10, max_length=1000),
                FieldSpec("layer", DataType.INTEGER, min_value=1, max_value=4),
                FieldSpec("importance", DataType.FLOAT, min_value=0.0, max_value=1.0),
                FieldSpec("created_at", DataType.DATETIME),
                FieldSpec("last_accessed", DataType.DATETIME),
                FieldSpec("access_count", DataType.INTEGER, min_value=0, max_value=1000),
                FieldSpec("tags", DataType.LIST),
                FieldSpec("metadata", DataType.JSON),
            ]
        ))

        self.register_schema(DataSchema(
            name="task",
            description="任务测试数据模式",
            fields=[
                FieldSpec("task_id", DataType.UUID),
                FieldSpec("name", DataType.STRING, min_length=3, max_length=100),
                FieldSpec("description", DataType.STRING, max_length=500),
                FieldSpec("status", DataType.STRING, enum_values=["pending", "running", "completed", "failed"]),
                FieldSpec("priority", DataType.INTEGER, min_value=1, max_value=10),
                FieldSpec("created_at", DataType.DATETIME),
                FieldSpec("updated_at", DataType.DATETIME),
                FieldSpec("assigned_agent", DataType.UUID),
                FieldSpec("dependencies", DataType.LIST),
                FieldSpec("result", DataType.JSON),
            ]
        ))

        self.register_schema(DataSchema(
            name="user_session",
            description="用户会话测试数据模式",
            fields=[
                FieldSpec("session_id", DataType.UUID),
                FieldSpec("user_id", DataType.UUID),
                FieldSpec("started_at", DataType.DATETIME),
                FieldSpec("last_activity", DataType.DATETIME),
                FieldSpec("status", DataType.STRING, enum_values=["active", "idle", "ended"]),
                FieldSpec("ip_address", DataType.STRING, pattern="999.999.999.999"),
                FieldSpec("user_agent", DataType.STRING),
                FieldSpec("metadata", DataType.JSON),
            ]
        ))

    def register_schema(self, schema: DataSchema):
        """
        注册数据模式

        Args:
            schema: 数据模式
        """
        self.schemas[schema.name] = schema

    def generate_record(self, schema_name: str) -> Dict[str, Any]:
        """
        生成单条记录

        Args:
            schema_name: 模式名称

        Returns:
            生成的记录
        """
        schema = self.schemas.get(schema_name)
        if not schema:
            raise ValueError(f"Unknown schema: {schema_name}")

        record = {}
        for field in schema.fields:
            record[field.name] = self.generator.generate_field(field)

        return record

    def generate_batch(
        self,
        schema_name: str,
        count: int = 10
    ) -> List[Dict[str, Any]]:
        """
        批量生成记录

        Args:
            schema_name: 模式名称
            count: 生成数量

        Returns:
            生成的记录列表
        """
        return [self.generate_record(schema_name) for _ in range(count)]

    def save_to_file(
        self,
        schema_name: str,
        records: List[Dict[str, Any]],
        filename: Optional[str] = None
    ) -> Path:
        """
        保存到文件

        Args:
            schema_name: 模式名称
            records: 记录列表
            filename: 文件名

        Returns:
            文件路径
        """
        if filename is None:
            filename = f"{schema_name}.json"

        file_path = self.output_dir / filename

        with open(file_path, 'w', encoding='utf-8') as f:
            json.dump(records, f, indent=2, ensure_ascii=False, default=str)

        return file_path

    def generate_and_save(
        self,
        schema_name: str,
        count: int = 10,
        filename: Optional[str] = None
    ) -> Dict[str, Any]:
        """
        生成并保存数据

        Args:
            schema_name: 模式名称
            count: 生成数量
            filename: 文件名

        Returns:
            操作结果
        """
        records = self.generate_batch(schema_name, count)
        file_path = self.save_to_file(schema_name, records, filename)

        return {
            "schema": schema_name,
            "count": len(records),
            "file": str(file_path),
            "records": records
        }


class DataCleaner:
    """
    数据清理器

    清理和脱敏测试数据。
    """

    def __init__(self):
        """初始化数据清理器"""
        self.sensitive_patterns = [
            (r'\b\d{16,19}\b', '****CARD****'),
            (r'\b\d{3}-\d{2}-\d{4}\b', '***-**-****'),
            (r'\b[A-Za-z0-9._%+-]+@[A-Za-z0-9.-]+\.[A-Z|a-z]{2,}\b', 'user@example.com'),
            (r'\b(?:password|passwd|pwd|secret|token|key)\s*[=:]\s*\S+\b', 'REDACTED'),
        ]

    def clean_string(self, text: str) -> str:
        """
        清理字符串中的敏感信息

        Args:
            text: 原始文本

        Returns:
            清理后的文本
        """
        import re
        result = text
        for pattern, replacement in self.sensitive_patterns:
            result = re.sub(pattern, replacement, result, flags=re.IGNORECASE)
        return result

    def clean_dict(self, data: Dict[str, Any]) -> Dict[str, Any]:
        """
        清理字典中的敏感信息

        Args:
            data: 原始字典

        Returns:
            清理后的字典
        """
        result = {}
        for key, value in data.items():
            if isinstance(value, str):
                result[key] = self.clean_string(value)
            elif isinstance(value, dict):
                result[key] = self.clean_dict(value)
            elif isinstance(value, list):
                result[key] = [
                    self.clean_string(item) if isinstance(item, str) else item
                    for item in value
                ]
            else:
                result[key] = value
        return result


def generate_all_test_data(output_dir: str = "tests/fixtures/data") -> Dict[str, Any]:
    """
    生成所有测试数据

    Args:
        output_dir: 输出目录

    Returns:
        生成结果
    """
    factory = TestDataFactory(output_dir)

    results = {}

    results["agents"] = factory.generate_and_save("agent_contract", 20)
    results["memories"] = factory.generate_and_save("memory_entry", 50, "sample_memories.json")
    results["tasks"] = factory.generate_and_save("task", 30, "sample_tasks.json")
    results["sessions"] = factory.generate_and_save("user_session", 15, "sample_sessions.json")

    return {
        "status": "completed",
        "output_dir": output_dir,
        "schemas_generated": list(results.keys()),
        "total_records": sum(r["count"] for r in results.values())
    }


if __name__ == "__main__":
    print("=" * 60)
    print("AgentOS 测试数据管理框架")
    print("Copyright (c) 2026 SPHARX Ltd.")
    print("=" * 60)

    result = generate_all_test_data()

    print(f"\n✅ 数据生成完成!")
    print(f"输出目录: {result['output_dir']}")
    print(f"生成模式: {', '.join(result['schemas_generated'])}")
    print(f"总记录数: {result['total_records']}")
