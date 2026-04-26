"""
OpenLab Core Storage Module

Data storage abstraction core module
Following AgentOS architecture design principles V1.8
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
    MEMORY = "memory"
    SQLITE = "sqlite"
    FILE = "file"
    REDIS = "redis"
    CUSTOM = "custom"


class DataCategory(Enum):
    TASK = "task"
    AGENT = "agent"
    TOOL = "tool"
    CHECKPOINT = "checkpoint"
    LOG = "log"
    METADATA = "metadata"


@dataclass
class StorageRecord:
    key: str
    value: Any
    category: DataCategory = DataCategory.METADATA
    metadata: Dict[str, Any] = field(default_factory=dict)
    created_at: float = field(default_factory=time.time)
    updated_at: float = field(default_factory=time.time)
    expires_at: Optional[float] = None
    version: int = 1

    def to_dict(self) -> Dict[str, Any]:
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
    records: List[StorageRecord]
    total: int
    offset: int
    limit: int


class Storage(ABC):

    def __init__(self, storage_type: StorageType):
        self.storage_type = storage_type
        self._initialized = False

    @property
    def initialized(self) -> bool:
        return self._initialized

    @abstractmethod
    async def initialize(self) -> None:
        self._initialized = True

    @abstractmethod
    async def close(self) -> None:
        self._initialized = False

    @abstractmethod
    async def get(self, key: str) -> Optional[StorageRecord]:
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
        pass

    @abstractmethod
    async def delete(self, key: str) -> bool:
        pass

    @abstractmethod
    async def exists(self, key: str) -> bool:
        pass

    @abstractmethod
    async def query(
        self,
        category: Optional[DataCategory] = None,
        filter_func: Optional[callable] = None,
        offset: int = 0,
        limit: int = 100
    ) -> QueryResult:
        pass

    @abstractmethod
    async def clear(self) -> None:
        pass

    async def get_json(self, key: str) -> Optional[Any]:
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
        return await self.set(key, json.dumps(value), **kwargs)


class MemoryStorage(Storage):

    def __init__(self):
        super().__init__(StorageType.MEMORY)
        self._data: Dict[str, StorageRecord] = {}
        self._lock = asyncio.Lock()

    async def initialize(self) -> None:
        await super().initialize()

    async def close(self) -> None:
        async with self._lock:
            self._data.clear()
        await super().close()

    async def get(self, key: str) -> Optional[StorageRecord]:
        async with self._lock:
            record = self._data.get(key)
            if record:
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

            existing = self._data.get(key)
            if existing:
                record.version = existing.version + 1

            self._data[key] = record
            return True

    async def delete(self, key: str) -> bool:
        async with self._lock:
            if key in self._data:
                del self._data[key]
                return True
            return False

    async def exists(self, key: str) -> bool:
        async with self._lock:
            return key in self._data

    async def query(
        self,
        category: Optional[DataCategory] = None,
        filter_func: Optional[callable] = None,
        offset: int = 0,
        limit: int = 100
    ) -> QueryResult:
        async with self._lock:
            records = list(self._data.values())

            if category:
                records = [r for r in records if r.category == category]

            if filter_func:
                records = [r for r in records if filter_func(r)]

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
        async with self._lock:
            self._data.clear()

    def size(self) -> int:
        return len(self._data)


class SQLiteStorage(Storage):

    def __init__(self, db_path: Union[str, Path]):
        super().__init__(StorageType.SQLITE)
        self.db_path = Path(db_path)
        self._conn: Optional[sqlite3.Connection] = None
        self._lock = asyncio.Lock()

    async def initialize(self) -> None:
        await super().initialize()

        self.db_path.parent.mkdir(parents=True, exist_ok=True)

        self._conn = sqlite3.connect(
            str(self.db_path),
            check_same_thread=False
        )
        self._conn.row_factory = sqlite3.Row

        await self._create_tables()

    async def _create_tables(self) -> None:
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
        if self._conn:
            loop = asyncio.get_event_loop()
            await loop.run_in_executor(None, self._conn.close)
            self._conn = None
        await super().close()

    async def get(self, key: str) -> Optional[StorageRecord]:
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
        loop = asyncio.get_event_loop()
        now = time.time()

        def upsert():
            cursor = self._conn.cursor()

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
        loop = asyncio.get_event_loop()

        def delete():
            cursor = self._conn.cursor()
            cursor.execute("DELETE FROM records WHERE key = ?", (key,))
            self._conn.commit()
            return cursor.rowcount > 0

        return await loop.run_in_executor(None, delete)

    async def exists(self, key: str) -> bool:
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

            if filter_func and not filter_func(record):
                continue

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
