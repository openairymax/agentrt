# AgentOS 测试数据管理器
# Version: 1.0.0.6
# Last updated: 2026-03-22

"""
测试数据管理模块。

提供统一的测试数据加载、生成、验证和管理功能。
"""

import os
import json
import time
import random
import hashlib
from pathlib import Path
from typing import Dict, Any, List, Optional, Union, Callable
from dataclasses import dataclass, field, asdict
from datetime import datetime, timezone
import faker

# 添加项目根目录到路径
PROJECT_ROOT = Path(__file__).parent.parent.parent


@dataclass
class TestDataTemplate:
    """测试数据模板"""
    name: str
    description: str
    schema: Dict[str, Any]
    generators: Dict[str, Callable] = field(default_factory=dict)
    validators: List[Callable] = field(default_factory=list)


@dataclass
class TestDataSet:
    """测试数据集"""
    name: str
    version: str
    description: str
    data: List[Dict[str, Any]] = field(default_factory=list)
    metadata: Dict[str, Any] = field(default_factory=dict)
    created_at: str = field(default_factory=lambda: datetime.now(timezone.utc).isoformat())
    updated_at: str = field(default_factory=lambda: datetime.now(timezone.utc).isoformat())


class TestDataManager:
    """测试数据管理器"""

    def __init__(self, data_dir: Optional[Path] = None):
        """
        初始化测试数据管理器。

        Args:
            data_dir: 测试数据目录
        """
        self.data_dir = data_dir or PROJECT_ROOT / "tests" / "fixtures" / "data"
        self.cache = {}
        self.fake = faker.Faker('zh_CN')
        self.templates = self._load_templates()

        # 确保数据目录存在
        self.data_dir.mkdir(parents=True, exist_ok=True)

    def _load_templates(self) -> Dict[str, TestDataTemplate]:
        """加载测试数据模板"""
        templates = {}

        # 任务模板
        templates['task'] = TestDataTemplate(
            name="task",
            description="AgentOS任务数据模板",
            schema={
                "task_id": {"type": "string", "required": True},
                "description": {"type": "string", "required": True},
                "status": {"type": "string", "enum": ["pending", "running", "completed", "failed"], "default": "pending"},
                "priority": {"type": "integer", "min": 1, "max": 10, "default": 5},
                "created_at": {"type": "string", "format": "datetime"},
                "metadata": {"type": "object", "default": {}}
            },
            generators={
                "task_id": lambda: f"task_{self.fake.uuid4()[:8]}",
                "description": lambda: self.fake.sentence(nb_words=10),
                "status": lambda: random.choice(["pending", "running", "completed", "failed"]),
                "priority": lambda: random.randint(1, 10),
                "created_at": lambda: datetime.now(timezone.utc).isoformat(),
                "metadata": lambda: {"source": "test_generator", "version": "1.0"}
            }
        )

        # 记忆模板
        templates['memory'] = TestDataTemplate(
            name="memory",
            description="AgentOS记忆数据模板",
            schema={
                "memory_id": {"type": "string", "required": True},
                "content": {"type": "string", "required": True},
                "layer": {"type": "string", "enum": ["L1", "L2", "L3", "L4"], "default": "L1"},
                "score": {"type": "float", "min": 0.0, "max": 1.0, "default": 1.0},
                "created_at": {"type": "string", "format": "datetime"},
                "metadata": {"type": "object", "default": {}}
            },
            generators={
                "memory_id": lambda: f"mem_{self.fake.uuid4()[:8]}",
                "content": lambda: self.fake.text(max_nb_chars=200),
                "layer": lambda: random.choice(["L1", "L2", "L3", "L4"]),
                "score": lambda: round(random.uniform(0.5, 1.0), 3),
                "created_at": lambda: datetime.now(timezone.utc).isoformat(),
                "metadata": lambda: {"category": random.choice(["personal", "work", "learning"]), "confidence": round(random.uniform(0.7, 1.0), 2)}
            }
        )

        # 会话模板
        templates['session'] = TestDataTemplate(
            name="session",
            description="AgentOS会话数据模板",
            schema={
                "session_id": {"type": "string", "required": True},
                "user_id": {"type": "string", "required": True},
                "status": {"type": "string", "enum": ["active", "inactive", "expired"], "default": "active"},
                "created_at": {"type": "string", "format": "datetime"},
                "last_activity": {"type": "string", "format": "datetime"},
                "metadata": {"type": "object", "default": {}}
            },
            generators={
                "session_id": lambda: f"sess_{self.fake.uuid4()[:8]}",
                "user_id": lambda: f"user_{self.fake.uuid4()[:8]}",
                "status": lambda: random.choice(["active", "inactive", "expired"]),
                "created_at": lambda: datetime.now(timezone.utc).isoformat(),
                "last_activity": lambda: datetime.now(timezone.utc).isoformat(),
                "metadata": lambda: {"device": random.choice(["web", "mobile", "api"]), "location": self.fake.city()}
            }
        )

        # 技能模板
        templates['skill'] = TestDataTemplate(
            name="skill",
            description="AgentOS技能数据模板",
            schema={
                "skill_id": {"type": "string", "required": True},
                "name": {"type": "string", "required": True},
                "version": {"type": "string", "pattern": r"^\d+\.\d+\.\d+$", "default": "1.0.0"},
                "description": {"type": "string", "required": True},
                "status": {"type": "string", "enum": ["active", "inactive", "deprecated"], "default": "active"},
                "parameters": {"type": "object", "default": {}},
                "created_at": {"type": "string", "format": "datetime"},
                "metadata": {"type": "object", "default": {}}
            },
            generators={
                "skill_id": lambda: f"skill_{self.fake.uuid4()[:8]}",
                "name": lambda: f"{self.fake.word()}_skill",
                "version": lambda: f"{random.randint(1,5)}.{random.randint(0,9)}.{random.randint(0,9)}",
                "description": lambda: self.fake.sentence(nb_words=15),
                "status": lambda: random.choice(["active", "inactive", "deprecated"]),
                "parameters": lambda: {
                    "timeout": random.randint(5, 60),
                    "retries": random.randint(0, 3),
                    "priority": random.randint(1, 10)
                },
                "created_at": lambda: datetime.now(timezone.utc).isoformat(),
                "metadata": lambda: {
                    "author": self.fake.name(),
                    "category": random.choice(["analysis", "communication", "automation"]),
                    "complexity": random.choice(["low", "medium", "high"])
                }
            }
        )

        return templates

    def load_data(self, data_type: str, filename: Optional[str] = None) -> TestDataSet:
        """
        加载测试数据。

        Args:
            data_type: 数据类型
            filename: 文件名，如果为None则使用默认文件

        Returns:
            测试数据集
        """
        if filename is None:
            filename = f"sample_{data_type}.json"

        file_path = self.data_dir / data_type / filename

        # 检查缓存
        cache_key = str(file_path)
        if cache_key in self.cache:
            file_mtime = file_path.stat().st_mtime if file_path.exists() else 0
            cache_mtime, cached_data = self.cache[cache_key]
            if file_mtime <= cache_mtime:
                return cached_data

        # 加载数据
        if not file_path.exists():
            # 如果文件不存在，生成默认数据
            dataset = self.generate_data(data_type, count=5)
            self.save_data(dataset, filename)
            return dataset

        try:
            with open(file_path, 'r', encoding='utf-8') as f:
                raw_data = json.load(f)

            # 转换为TestDataSet对象
            if data_type in raw_data:
                # 兼容旧格式
                data_list = raw_data[data_type] if isinstance(raw_data[data_type], list) else [raw_data[data_type]]
                dataset = TestDataSet(
                    name=data_type,
                    version="1.0.0",
                    description=f"Generated {data_type} data",
                    data=data_list,
                    metadata=raw_data.get("metadata", {})
                )
            else:
                dataset = TestDataSet(**raw_data)

            # 缓存数据
            self.cache[cache_key] = (file_path.stat().st_mtime, dataset)

            return dataset

        except (json.JSONDecodeError, IOError, KeyError) as e:
            # 如果加载失败，生成新数据
            print(f"警告: 加载数据失败 {file_path}: {e}，生成新数据")
            dataset = self.generate_data(data_type, count=5)
            self.save_data(dataset, filename)
            return dataset

    def save_data(self, dataset: TestDataSet, filename: Optional[str] = None) -> bool:
        """
        保存测试数据。

        Args:
            dataset: 测试数据集
            filename: 文件名

        Returns:
            是否保存成功
        """
        if filename is None:
            filename = f"sample_{dataset.name}.json"

        file_path = self.data_dir / dataset.name / filename
        file_path.parent.mkdir(parents=True, exist_ok=True)

        try:
            # 更新时间戳
            dataset.updated_at = datetime.now(timezone.utc).isoformat()

            # 转换为字典并保存
            data_dict = asdict(dataset)

            with open(file_path, 'w', encoding='utf-8') as f:
                json.dump(data_dict, f, indent=2, ensure_ascii=False)

            # 更新缓存
            self.cache[str(file_path)] = (file_path.stat().st_mtime, dataset)

            return True

        except (IOError, TypeError) as e:
            print(f"错误: 保存数据失败 {file_path}: {e}")
            return False

    def generate_data(self, data_type: str, count: int = 1, **overrides) -> TestDataSet:
        """
        生成测试数据。

        Args:
            data_type: 数据类型
            count: 生成数量
            **overrides: 覆盖字段

        Returns:
            测试数据集
        """
        if data_type not in self.templates:
            raise ValueError(f"未知的数据类型: {data_type}")

        template = self.templates[data_type]
        data_list = []

        for _ in range(count):
            item = {}

            # 生成字段
            for field_name, field_schema in template.schema.items():
                if field_name in overrides:
                    item[field_name] = overrides[field_name]
                elif field_name in template.generators:
                    item[field_name] = template.generators[field_name]()
                elif 'default' in field_schema:
                    item[field_name] = field_schema['default']
                elif field_schema.get('required', False):
                    raise ValueError(f"缺少必需字段: {field_name}")

            # 应用覆盖
            for key, value in overrides.items():
                if key not in item:
                    item[key] = value

            # 验证数据
            if template.validators:
                for validator in template.validators:
                    validator(item)

            data_list.append(item)

        return TestDataSet(
            name=data_type,
            version="1.0.0",
            description=f"Generated {data_type} data ({count} items)",
            data=data_list,
            metadata={
                "generated_by": "TestDataManager",
                "template": template.name,
                "count": count,
                "overrides": overrides
            }
        )

    def get_data(self, data_type: str, index: Optional[Union[int, str]] = None,
                filename: Optional[str] = None) -> Optional[Dict[str, Any]]:
        """
        获取单个数据项。

        Args:
            data_type: 数据类型
            index: 索引或ID
            filename: 文件名

        Returns:
            数据项
        """
        dataset = self.load_data(data_type, filename)

        if index is None:
            return dataset.data[0] if dataset.data else None

        if isinstance(index, int):
            if 0 <= index < len(dataset.data):
                return dataset.data[index]
            return None

        # 按ID查找
        for item in dataset.data:
            if item.get(f"{data_type}_id") == index:
                return item

        return None

    def validate_data(self, data_type: str, data: Dict[str, Any]) -> List[str]:
        """
        验证数据格式。

        Args:
            data_type: 数据类型
            data: 要验证的数据

        Returns:
            错误列表
        """
        if data_type not in self.templates:
            return [f"未知的数据类型: {data_type}"]

        template = self.templates[data_type]
        errors = []

        # 检查必需字段
        for field_name, field_schema in template.schema.items():
            if field_schema.get('required', False) and field_name not in data:
                errors.append(f"缺少必需字段: {field_name}")

            if field_name in data:
                value = data[field_name]
                field_type = field_schema.get('type')

                # 类型检查
                if field_type == 'string' and not isinstance(value, str):
                    errors.append(f"字段 {field_name} 应该是字符串")
                elif field_type == 'integer' and not isinstance(value, int):
                    errors.append(f"字段 {field_name} 应该是整数")
                elif field_type == 'float' and not isinstance(value, (int, float)):
                    errors.append(f"字段 {field_name} 应该是数字")
                elif field_type == 'object' and not isinstance(value, dict):
                    errors.append(f"字段 {field_name} 应该是对象")

                # 枚举值检查
                if 'enum' in field_schema and value not in field_schema['enum']:
                    errors.append(f"字段 {field_name} 值 {value} 不在允许的枚举值中: {field_schema['enum']}")

                # 数值范围检查
                if field_type in ['integer', 'float'] and isinstance(value, (int, float)):
                    if 'min' in field_schema and value < field_schema['min']:
                        errors.append(f"字段 {field_name} 值 {value} 小于最小值 {field_schema['min']}")
                    if 'max' in field_schema and value > field_schema['max']:
                        errors.append(f"字段 {field_name} 值 {value} 大于最大值 {field_schema['max']}")

        return errors

    def refresh_data(self, data_type: str, count: int = 5) -> TestDataSet:
        """
        刷新测试数据。

        Args:
            data_type: 数据类型
            count: 生成数量

        Returns:
            新的测试数据集
        """
        dataset = self.generate_data(data_type, count)
        self.save_data(dataset)
        return dataset

    def get_data_hash(self, data_type: str, filename: Optional[str] = None) -> str:
        """
        获取数据哈希值。

        Args:
            data_type: 数据类型
            filename: 文件名

        Returns:
            数据哈希值
        """
        dataset = self.load_data(data_type, filename)
        data_str = json.dumps(dataset.data, sort_keys=True, ensure_ascii=False)
        return hashlib.md5(data_str.encode()).hexdigest()

    def list_data_files(self, data_type: Optional[str] = None) -> List[Path]:
        """
        列出数据文件。

        Args:
            data_type: 数据类型，如果为None则列出所有

        Returns:
            文件路径列表
        """
        if data_type:
            type_dir = self.data_dir / data_type
            if not type_dir.exists():
                return []
            return list(type_dir.glob("*.json"))
        else:
            files = []
            for type_dir in self.data_dir.iterdir():
                if type_dir.is_dir():
                    files.extend(type_dir.glob("*.json"))
            return files

    def cleanup_cache(self):
        """清理缓存"""
        self.cache.clear()

    def get_statistics(self) -> Dict[str, Any]:
        """
        获取数据统计信息。

        Returns:
            统计信息
        """
        stats = {
            "data_types": list(self.templates.keys()),
            "total_files": 0,
            "total_records": 0,
            "cache_size": len(self.cache),
            "files_by_type": {}
        }

        for data_type in self.templates.keys():
            files = self.list_data_files(data_type)
            stats["files_by_type"][data_type] = len(files)
            stats["total_files"] += len(files)

            for file_path in files:
                try:
                    dataset = self.load_data(data_type, file_path.name)
                    stats["total_records"] += len(dataset.data)
                except Exception:
                    pass

        return stats


# 全局数据管理器实例
_data_manager = None

def get_data_manager() -> TestDataManager:
    """获取全局数据管理器实例"""
    global _data_manager
    if _data_manager is None:
        _data_manager = TestDataManager()
    return _data_manager


# 便捷函数
def load_test_data(data_type: str, filename: Optional[str] = None) -> TestDataSet:
    """加载测试数据"""
    return get_data_manager().load_data(data_type, filename)


def get_test_data(data_type: str, index: Optional[Union[int, str]] = None,
                 filename: Optional[str] = None) -> Optional[Dict[str, Any]]:
    """获取单个测试数据项"""
    return get_data_manager().get_data(data_type, index, filename)


def generate_test_data(data_type: str, count: int = 1, **overrides) -> TestDataSet:
    """生成测试数据"""
    return get_data_manager().generate_data(data_type, count, **overrides)
