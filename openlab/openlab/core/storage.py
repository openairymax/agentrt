"""
OpenLab Core Storage Module

数据存储抽象核心模块
遵循 AgentOS 架构设计原则 V1.8
"""

from abc import ABC, abstractmethod
from dataclasses import dataclass, field
from enum import Enum
from typing import Any, Dict, List, Optional, Union
import asyncio
import json
import sqlite3
import time
from pathlib import Path


class StorageType(Enum):
    """存储类型枚举"""
    MEMORY = "memory"           # 内存存储
    SQLITE = "sqlite"           # SQLite 数据库
    FILE = "file"              # 文件系统
    REDIS = "redis"            # Redis
    CUSTOM = "custom"          # 自定义


class DataCategory(Enum):
    """数据类别枚举"""
    TASK = "task"              # 任务数据
    AGENT = "agent"            # Agent 数据
    TOOL = "tool"              # 工具数据
    CHECKPOINT = "checkpoint"  # 检查点数据
    LOG = "log"                # 日志数据
    METADATA = "metadata"      # 元数据


@dataclass
class StorageRecord:
    """
    存储记录
    
    遵循原则:
    - A-1 简约至上：最小必要字段
    - E-5 命名语义化：名称即文档
    """
    key: str
    value: Any
    category: DataCategory = DataCategory.METADATA
    metadata: Dict[str, Any] = field(default_factory=dict)
    created_at: float = field(default_factory=time.time)
    updated_at: float = field(default_factory=time.time)
    expires_at: Optional[float] = None
    version: int = 1
    
    def to_dict(self) -> Dict[str, Any]:
        """序列化为字典"""
        return {
            "key": self.key,
            "value": self.value,
            "category": self.category.value,
            "metadata": self.metadata,
            "created_at": self.created_at,
            "updated_at": self.updated_at,
            "expires_at": self.expires_at,
            "version": self.version,
        }
    
    @classmethod
    def from_dict(cls, data: Dict[str, Any]) -> "StorageRecord":
        """从字典反序列化"""
        return cls(
            key=data["key"],
            value=data["value"],
            category=DataCategory(data.get("category", "metadata")),
            metadata=data.get("metadata", {}),
            created_at=data.get("created_at", time.time()),
            updated_at=data.get("updated_at", time.time()),
            expires_at=data.get("expires_at"),
            version=data.get("version", 1),
        )


@dataclass
class QueryResult:
    """查询结果"""
    records: List[StorageRecord]
    total: int
    offset: int
    limit: int


class Storage(ABC):
    """
    存储抽象基类
    
    遵循原则:
    - K-2 接口契约化：完整的 docstring 和类型注解
    - K-4 可插拔策略：支持不同存储后端
    - E-3 资源确定性：明确的资源生命周期
    """
    
    def __init__(self, storage_type: StorageType):
        """
        初始化存储
        
        Args:
            storage_type: 存储类型
        """
        self.storage_type = storage_type
        self._initialized = False
    
    @property
    def initialized(self) -> bool:
        """是否已初始化"""
        return self._initialized
    
    @abstractmethod
    async def initialize(self) -> None:
        """
        初始化存储
        
        必须实现的方法
        """
        self._initialized = True
    
    @abstractmethod
    async def close(self) -> None:
        """
        关闭存储
        
        清理资源，必须实现
        """
        self._initialized = False
    
    @abstractmethod
    async def get(self, key: str) -> Optional[StorageRecord]:
        """
        获取记录
        
        Args:
            key: 记录键
            
        Returns:
            Optional[StorageRecord]: 记录
        """
        pass
    
    @abstractmethod
    async def set(
        self,
        key: str,
        value: Any,
        category: DataCategory = DataCategory.METADATA,
        metadata: Optional[Dict[str, Any]] = None,
        ttl: Optional[float] = None
    ) -> bool:
        """
        设置记录
        
        Args:
            key: 记录键
            value: 记录值
            category: 数据类别
            metadata: 元数据
            ttl: 生存时间（秒）
            
        Returns:
            bool: 是否成功
        """
        pass
    
    @abstractmethod
    async def delete(self, key: str) -> bool:
        """
        删除记录
        
        Args:
            key: 记录键
            
        Returns:
            bool: 是否成功
        """
        pass
    
    @abstractmethod
    async def exists(self, key: str) -> bool:
        """
        检查记录是否存在
        
        Args:
            key: 记录键
            
        Returns:
            bool: 是否存在
        """
        pass
    
    @abstractmethod
    async def query(
        self,
        category: Optional[DataCategory] = None,
        filter_func: Optional[callable] = None,
        offset: int = 0,
        limit: int = 100
    ) -> QueryResult:
        """
        查询记录
        
        Args:
            category: 数据类别
            filter_func: 过滤函数
            offset: 偏移量
            limit: 限制数量
            
        Returns:
            QueryResult: 查询结果
        """
        pass
    
    @abstractmethod
    async def clear(self) -> None:
        """
        清空所有记录
        """
        pass
    
    async def get_json(self, key: str) -> Optional[Any]:
        """
        获取 JSON 数据
        
        Args:
            key: 记录键
            
        Returns:
            Optional[Any]: 解析后的 JSON 数据
        """
        record = await self.get(key)
        if record and record.value:
            if isinstance(record.value, str):
                return json.loads(record.value)
            return record.value
        return None
    
    async def set_json(
        self,
        key: str,
        value: Any,
        **kwargs
    ) -> bool:
        """
        设置 JSON 数据
        
        Args:
            key: 记录键
            value: 数据值（将被 JSON 序列化）
            **kwargs: 其他参数
            
        Returns:
            bool: 是否成功
        """
        return await self.set(key, json.dumps(value), **kwargs)


class MemoryStorage(Storage):
    """
    内存存储实现
    
    特点:
    - 快速访问
    - 重启后数据丢失
    - 适合缓存和临时数据
    """
    
    def __init__(self):
        super().__init__(StorageType.MEMORY)
        self._data: Dict[str, StorageRecord] = {}
        self._lock = asyncio.Lock()
    
    async def initialize(self) -> None:
        """初始化"""
        await super().initialize()
    
    async def close(self) -> None:
        """关闭"""
        async with self._lock:
            self._data.clear()
        await super().close()
    
    async def get(self, key: str) -> Optional[StorageRecord]:
        """获取记录"""
        async with self._lock:
            record = self._data.get(key)
            if record:
                # 检查是否过期
                if record.expires_at and time.time() > record.expires_at:
                    await self.delete(key)
                    return None
                return record
            return None
    
    async def set(
        self,
        key: str,
        value: Any,
        category: DataCategory = DataCategory.METADATA,
        metadata: Optional[Dict[str, Any]] = None,
        ttl: Optional[float] = None
    ) -> bool:
        """设置记录"""
        async with self._lock:
            now = time.time()
            record = StorageRecord(
                key=key,
                value=value,
                category=category,
                metadata=metadata or {},
                created_at=now,
                updated_at=now,
                expires_at=(now + ttl) if ttl else None,
            )
            
            # 如果已存在，增加版本号
            existing = self._data.get(key)
            if existing:
                record.version = existing.version + 1
            
            self._data[key] = record
            return True
    
    async def delete(self, key: str) -> bool:
        """删除记录"""
        async with self._lock:
            if key in self._data:
                del self._data[key]
                return True
            return False
    
    async def exists(self, key: str) -> bool:
        """检查记录是否存在"""
        async with self._lock:
            return key in self._data
    
    async def query(
        self,
        category: Optional[DataCategory] = None,
        filter_func: Optional[callable] = None,
        offset: int = 0,
        limit: int = 100
    ) -> QueryResult:
        """查询记录"""
        async with self._lock:
            records = list(self._data.values())
            
            # 按类别过滤
            if category:
                records = [r for r in records if r.category == category]
            
            # 按函数过滤
            if filter_func:
                records = [r for r in records if filter_func(r)]
            
            # 检查过期
            now = time.time()
            non_expired = []
            for record in records:
                if record.expires_at and now > record.expires_at:
                    await self.delete(record.key)
                else:
                    non_expired.append(record)
            records = non_expired
            
            total = len(records)
            records = records[offset:offset + limit]
            
            return QueryResult(
                records=records,
                total=total,
                offset=offset,
                limit=limit,
            )
    
    async def clear(self) -> None:
        """清空所有记录"""
        async with self._lock:
            self._data.clear()
    
    def size(self) -> int:
        """获取记录数量"""
        return len(self._data)


class SQLiteStorage(Storage):
    """
    SQLite 存储实现
    
    特点:
    - 持久化存储
    - 支持复杂查询
    - 适合任务状态、检查点等数据
    
    遵循原则:
    - E-3 资源确定性：连接管理和事务处理
    """
    
    def __init__(self, db_path: Union[str, Path]):
        """
        初始化 SQLite 存储
        
        Args:
            db_path: 数据库文件路径
        """
        super().__init__(StorageType.SQLITE)
        self.db_path = Path(db_path)
        self._conn: Optional[sqlite3.Connection] = None
        self._lock = asyncio.Lock()
    
    async def initialize(self) -> None:
        """初始化数据库"""
        await super().initialize()
        
        # 确保目录存在
        self.db_path.parent.mkdir(parents=True, exist_ok=True)
        
        # 创建连接
        self._conn = sqlite3.connect(
            str(self.db_path),
            check_same_thread=False
        )
        self._conn.row_factory = sqlite3.Row
        
        # 创建表
        await self._create_tables()
    
    async def _create_tables(self) -> None:
        """创建数据表"""
        loop = asyncio.get_event_loop()
        
        def create():
            cursor = self._conn.cursor()
            cursor.execute("""
                CREATE TABLE IF NOT EXISTS records (
                    key TEXT PRIMARY KEY,
                    value TEXT,
                    category TEXT,
                    metadata TEXT,
                    created_at REAL,
                    updated_at REAL,
                    expires_at REAL,
                    version INTEGER DEFAULT 1
                )
            """)
            cursor.execute("""
                CREATE INDEX IF NOT EXISTS idx_category 
                ON records(category)
            """)
            cursor.execute("""
                CREATE INDEX IF NOT EXISTS idx_expires 
                ON records(expires_at)
            """)
            self._conn.commit()
        
        await loop.run_in_executor(None, create)
    
    async def close(self) -> None:
        """关闭数据库连接"""
        if self._conn:
            loop = asyncio.get_event_loop()
            await loop.run_in_executor(None, self._conn.close)
            self._conn = None
        await super().close()
    
    async def get(self, key: str) -> Optional[StorageRecord]:
        """获取记录"""
        loop = asyncio.get_event_loop()
        
        def fetch():
            cursor = self._conn.cursor()
            cursor.execute(
                "SELECT * FROM records WHERE key = ?",
                (key,)
            )
            row = cursor.fetchone()
            return row
        
        row = await loop.run_in_executor(None, fetch)
        
        if row:
            record = StorageRecord(
                key=row["key"],
                value=json.loads(row["value"]) if row["value"] else None,
                category=DataCategory(row["category"]),
                metadata=json.loads(row["metadata"]) if row["metadata"] else {},
                created_at=row["created_at"],
                updated_at=row["updated_at"],
                expires_at=row["expires_at"],
                version=row["version"],
            )
            
            # 检查过期
            if record.expires_at and time.time() > record.expires_at:
                await self.delete(key)
                return None
            
            return record
        
        return None
    
    async def set(
        self,
        key: str,
        value: Any,
        category: DataCategory = DataCategory.METADATA,
        metadata: Optional[Dict[str, Any]] = None,
        ttl: Optional[float] = None
    ) -> bool:
        """设置记录"""
        loop = asyncio.get_event_loop()
        now = time.time()
        
        def upsert():
            cursor = self._conn.cursor()
            
            # 检查是否存在
            cursor.execute(
                "SELECT version FROM records WHERE key = ?",
                (key,)
            )
            row = cursor.fetchone()
            
            version = (row["version"] + 1) if row else 1
            
            cursor.execute("""
                INSERT OR REPLACE INTO records 
                (key, value, category, metadata, created_at, updated_at, expires_at, version)
                VALUES (?, ?, ?, ?, ?, ?, ?, ?)
            """, (
                key,
                json.dumps(value) if value is not None else None,
                category.value,
                json.dumps(metadata or {}),
                now,
                now,
                (now + ttl) if ttl else None,
                version,
            ))
            self._conn.commit()
            return True
        
        return await loop.run_in_executor(None, upsert)
    
    async def delete(self, key: str) -> bool:
        """删除记录"""
        loop = asyncio.get_event_loop()
        
        def delete():
            cursor = self._conn.cursor()
            cursor.execute("DELETE FROM records WHERE key = ?", (key,))
            self._conn.commit()
            return cursor.rowcount > 0
        
        return await loop.run_in_executor(None, delete)
    
    async def exists(self, key: str) -> bool:
        """检查记录是否存在"""
        loop = asyncio.get_event_loop()
        
        def check():
            cursor = self._conn.cursor()
            cursor.execute(
                "SELECT 1 FROM records WHERE key = ? LIMIT 1",
                (key,)
            )
            return cursor.fetchone() is not None
        
        return await loop.run_in_executor(None, check)
    
    async def query(
        self,
        category: Optional[DataCategory] = None,
        filter_func: Optional[callable] = None,
        offset: int = 0,
        limit: int = 100
    ) -> QueryResult:
        """查询记录"""
        loop = asyncio.get_event_loop()
        
        def fetch():
            cursor = self._conn.cursor()
            
            if category:
                cursor.execute(
                    "SELECT * FROM records WHERE category = ? LIMIT ? OFFSET ?",
                    (category.value, limit, offset)
                )
            else:
                cursor.execute(
                    "SELECT * FROM records LIMIT ? OFFSET ?",
                    (limit, offset)
                )
            
            rows = cursor.fetchall()
            
            # 获取总数
            if category:
                cursor.execute(
                    "SELECT COUNT(*) FROM records WHERE category = ?",
                    (category.value,)
                )
            else:
                cursor.execute("SELECT COUNT(*) FROM records")
            
            total = cursor.fetchone()[0]
            return rows, total
        
        rows, total = await loop.run_in_executor(None, fetch)
        
        records = []
        for row in rows:
            record = StorageRecord(
                key=row["key"],
                value=json.loads(row["value"]) if row["value"] else None,
                category=DataCategory(row["category"]),
                metadata=json.loads(row["metadata"]) if row["metadata"] else {},
                created_at=row["created_at"],
                updated_at=row["updated_at"],
                expires_at=row["expires_at"],
                version=row["version"],
            )
            
            # 应用过滤函数
            if filter_func and not filter_func(record):
                continue
            
            # 检查过期
            if record.expires_at and time.time() > record.expires_at:
                await self.delete(record.key)
            else:
                records.append(record)
        
        return QueryResult(
            records=records,
            total=total,
            offset=offset,
            limit=limit,
        )
    
    async def clear(self) -> None:
        """清空所有记录"""
        loop = asyncio.get_event_loop()
        
        def truncate():
            cursor = self._conn.cursor()
            cursor.execute("DELETE FROM records")
            self._conn.commit()
        
        await loop.run_in_executor(None, truncate)


__all__ = [
    "Storage",
    "StorageType",
    "DataCategory",
    "StorageRecord",
    "QueryResult",
    "MemoryStorage",
    "SQLiteStorage",
]
