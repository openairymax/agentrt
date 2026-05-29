"""
AgentOS 测试数据管理自动化框架
Copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
Version: 2.0.0
Last updated: 2026-04-23

自动生成和管理测试数据
支持多种数据类型和场景
增强功能：
- 关联数据生成
- 数据验证
- 批量操作优化
- 自定义生成器
"""

import json
import random
import string
import uuid
import hashlib
import copy
from datetime import datetime, timedelta
from typing import Any, Dict, List, Optional, Type, Union, Callable, Generator
from dataclasses import dataclass, field, asdict
from pathlib import Path
from enum import Enum
from abc import ABC, abstractmethod


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
    PHONE = "phone"
    NAME = "name"
    ADDRESS = "address"
    IP_ADDRESS = "ip_address"
    COLOR = "color"
    LOREM = "lorem"
    TIMESTAMP = "timestamp"
    HASH = "hash"
    REFERENCE = "reference"


class DataValidator(ABC):
    """数据验证器基类"""

    @abstractmethod
    def validate(self, value: Any, spec: 'FieldSpec') -> bool:
        """验证数据"""
        pass

    @abstractmethod
    def get_error_message(self, value: Any, spec: 'FieldSpec') -> str:
        """获取错误消息"""
        pass


class TypeValidator(DataValidator):
    """类型验证器"""

    TYPE_MAP = {
        DataType.STRING: str,
        DataType.INTEGER: int,
        DataType.FLOAT: (int, float),
        DataType.BOOLEAN: bool,
        DataType.DATETIME: str,
        DataType.UUID: str,
        DataType.EMAIL: str,
        DataType.URL: str,
        DataType.JSON: dict,
        DataType.LIST: list,
        DataType.PHONE: str,
        DataType.NAME: str,
        DataType.ADDRESS: str,
        DataType.IP_ADDRESS: str,
        DataType.COLOR: str,
        DataType.LOREM: str,
        DataType.TIMESTAMP: (int, float),
        DataType.HASH: str,
        DataType.REFERENCE: str,
    }

    def validate(self, value: Any, spec: 'FieldSpec') -> bool:
        expected_type = self.TYPE_MAP.get(spec.data_type)
        if expected_type is None:
            return True
        return isinstance(value, expected_type)

    def get_error_message(self, value: Any, spec: 'FieldSpec') -> str:
        expected_type = self.TYPE_MAP.get(spec.data_type)
        return f"类型错误: 期望 {expected_type}, 实际 {type(value)}"


class RangeValidator(DataValidator):
    """范围验证器"""

    def validate(self, value: Any, spec: 'FieldSpec') -> bool:
        if spec.min_value is not None and value < spec.min_value:
            return False
        if spec.max_value is not None and value > spec.max_value:
            return False
        return True

    def get_error_message(self, value: Any, spec: 'FieldSpec') -> str:
        return f"范围错误: {value} 不在 [{spec.min_value}, {spec.max_value}] 范围内"


class LengthValidator(DataValidator):
    """长度验证器"""

    def validate(self, value: Any, spec: 'FieldSpec') -> bool:
        if not hasattr(value, '__len__'):
            return True
        length = len(value)
        if spec.min_length is not None and length < spec.min_length:
            return False
        if spec.max_length is not None and length > spec.max_length:
            return False
        return True

    def get_error_message(self, value: Any, spec: 'FieldSpec') -> str:
        length = len(value)
        return f"长度错误: {length} 不在 [{spec.min_length}, {spec.max_length}] 范围内"


class EnumValidator(DataValidator):
    """枚举验证器"""

    def validate(self, value: Any, spec: 'FieldSpec') -> bool:
        if spec.enum_values is None:
            return True
        return value in spec.enum_values

    def get_error_message(self, value: Any, spec: 'FieldSpec') -> str:
        return f"枚举错误: {value} 不在 {spec.enum_values} 中"


class CompositeValidator:
    """组合验证器"""

    def __init__(self):
        self.validators: List[DataValidator] = [
            TypeValidator(),
            RangeValidator(),
            LengthValidator(),
            EnumValidator(),
        ]

    def validate(self, value: Any, spec: 'FieldSpec') -> tuple[bool, List[str]]:
        """验证数据，返回 (是否通过, 错误消息列表)"""
        errors = []
        for validator in self.validators:
            if not validator.validate(value, spec):
                errors.append(validator.get_error_message(value, spec))
        return len(errors) == 0, errors


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
            DataType.PHONE: self._generate_phone,
            DataType.NAME: self._generate_name,
            DataType.ADDRESS: self._generate_address,
            DataType.IP_ADDRESS: self._generate_ip_address,
            DataType.COLOR: self._generate_color,
            DataType.LOREM: self._generate_lorem,
            DataType.TIMESTAMP: self._generate_timestamp,
            DataType.HASH: self._generate_hash,
            DataType.REFERENCE: self._generate_reference,
        }
        self.validator = CompositeValidator()

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

    def _generate_phone(self, spec: FieldSpec) -> str:
        """生成电话号码"""
        formats = [
            "+1-XXX-XXX-XXXX",
            "+86-XXX-XXXX-XXXX",
            "XXX-XXX-XXXX",
            "(XXX) XXX-XXXX",
        ]
        return self._generate_from_pattern(random.choice(formats))

    def _generate_name(self, spec: FieldSpec) -> str:
        """生成姓名"""
        first_names = ["张", "李", "王", "刘", "陈", "杨", "赵", "黄", "周", "吴",
                       "John", "Jane", "Mike", "Emily", "David", "Sarah", "Tom", "Lisa"]
        last_names = ["伟", "芳", "娜", "敏", "静", "强", "磊", "洋", "勇", "艳",
                      "Smith", "Johnson", "Williams", "Brown", "Jones", "Miller", "Davis"]
        return f"{random.choice(first_names)}{random.choice(last_names)}"

    def _generate_address(self, spec: FieldSpec) -> str:
        """生成地址"""
        streets = ["Main St", "Oak Ave", "Park Rd", "First Blvd", "Second Lane"]
        cities = ["Beijing", "Shanghai", "Shenzhen", "Guangzhou", "Hangzhou",
                  "New York", "London", "Tokyo", "Paris", "Sydney"]
        return f"{random.randint(1, 999)} {random.choice(streets)}, {random.choice(cities)}"

    def _generate_ip_address(self, spec: FieldSpec) -> str:
        """生成IP地址"""
        return f"{random.randint(1, 255)}.{random.randint(0, 255)}.{random.randint(0, 255)}.{random.randint(1, 254)}"

    def _generate_color(self, spec: FieldSpec) -> str:
        """生成颜色"""
        return f"#{random.randint(0, 255):02x}{random.randint(0, 255):02x}{random.randint(0, 255):02x}"

    def _generate_lorem(self, spec: FieldSpec) -> str:
        """生成占位文本"""
        words = ["lorem", "ipsum", "dolor", "sit", "amet", "consectetur",
                 "adipiscing", "elit", "sed", "do", "eiusmod", "tempor"]
        length = spec.max_length or 50
        result = []
        while len(' '.join(result)) < length:
            result.append(random.choice(words))
        return ' '.join(result)[:length]

    def _generate_timestamp(self, spec: FieldSpec) -> int:
        """生成时间戳"""
        now = int(datetime.now().timestamp())
        offset = random.randint(-365 * 24 * 3600, 365 * 24 * 3600)
        return now + offset

    def _generate_hash(self, spec: FieldSpec) -> str:
        """生成哈希值"""
        data = str(uuid.uuid4()).encode()
        return hashlib.sha256(data).hexdigest()

    def _generate_reference(self, spec: FieldSpec) -> str:
        """生成引用ID"""
        return f"ref-{uuid.uuid4().hex[:8]}"


class RelationManager:
    """关联数据管理器"""

    def __init__(self):
        self._references: Dict[str, List[Any]] = {}
        self._relations: Dict[str, Dict[str, str]] = {}

    def store_reference(self, schema_name: str, record_id: str, record: Dict):
        """存储引用"""
        if schema_name not in self._references:
            self._references[schema_name] = []
        self._references[schema_name].append({'id': record_id, 'data': record})

    def get_reference(self, schema_name: str) -> Optional[str]:
        """获取随机引用ID"""
        if schema_name not in self._references or not self._references[schema_name]:
            return None
        return random.choice(self._references[schema_name])['id']

    def get_record(self, schema_name: str, record_id: str) -> Optional[Dict]:
        """获取引用记录"""
        if schema_name not in self._references:
            return None
        for ref in self._references[schema_name]:
            if ref['id'] == record_id:
                return ref['data']
        return None

    def define_relation(self, from_schema: str, to_schema: str, field_name: str):
        """定义关联关系"""
        if from_schema not in self._relations:
            self._relations[from_schema] = {}
        self._relations[from_schema][field_name] = to_schema

    def resolve_relation(self, schema_name: str, field_name: str) -> Optional[str]:
        """解析关联关系"""
        if schema_name not in self._relations:
            return None
        return self._relations[schema_name].get(field_name)


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
