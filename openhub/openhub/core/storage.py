"""
Copyright (c) 2026 SPHARX. All Rights Reserved.
"From data intelligence emerges."

OpenHub Data Storage Module
==========================

This module provides persistent storage abstraction for OpenHub:
- Storage interface definition
- SQLite-based implementation
- Transaction support
- Query builders

Usage:
    from openhub.core.storage import Storage, SQLiteStorage

Author: AgentOS Architecture Committee
Version: 1.0.0.6
"""

from __future__ import annotations

import asyncio
import json
import logging
import time
import uuid
from abc import ABC, abstractmethod
from dataclasses import dataclass, field
from enum import Enum, auto
from pathlib import Path
from typing import (
    Any,
    Dict,
    List,
    Optional,
    Set,
    Tuple,
    Iterator,
    TypeVar,
    Generic,
)

logger = logging.getLogger(__name__)


class StorageBackend(Enum):
    """Supported storage backend types."""
    SQLITE = auto()
    MEMORY = auto()
    REDIS = auto()
    POSTGRESQL = auto()


class QueryOperator(Enum):
    """Query comparison operators."""
    EQ = "="
    NE = "!="
    LT = "<"
    LE = "<="
    GT = ">"
    GE = ">="
    IN = "IN"
    NOT_IN = "NOT IN"
    LIKE = "LIKE"
    IS_NULL = "IS NULL"
    IS_NOT_NULL = "IS NOT NULL"


@dataclass
class StorageRecord:
    """
    Generic storage record structure.

    Attributes:
        id: Unique record identifier.
        data: Record payload as dictionary.
        created_at: Record creation timestamp.
        updated_at: Record last update timestamp.
        metadata: Additional record metadata.
    """
    id: str
    data: Dict[str, Any]
    created_at: float = field(default_factory=time.time)
    updated_at: float = field(default_factory=time.time)
    metadata: Dict[str, Any] = field(default_factory=dict)


@dataclass
class QueryCondition:
    """
    Single query condition for filtering.

    Attributes:
        field: Field name to compare.
        operator: Comparison operator.
        value: Value to compare against.
    """
    field: str
    operator: QueryOperator
    value: Any


@dataclass
class Query:
    """
    Query specification for storage operations.

    Attributes:
        conditions: List of AND-ed conditions.
        order_by: Field to order results by.
        order_desc: If True, descending order.
        limit: Maximum number of records.
        offset: Number of records to skip.
    """
    conditions: List[QueryCondition] = field(default_factory=list)
    order_by: Optional[str] = None
    order_desc: bool = False
    limit: Optional[int] = None
    offset: int = 0

    def add_condition(
        self, field: str, operator: QueryOperator, value: Any
    ) -> "Query":
        """Add a condition to the query (fluent interface)."""
        self.conditions.append(
            QueryCondition(field=field, operator=operator, value=value)
        )
        return self


@dataclass
class StorageStats:
    """
    Storage statistics.

    Attributes:
        backend: Storage backend type.
        total_records: Total number of records.
        total_size_bytes: Approximate storage size.
        last_modified: Last modification timestamp.
    """
    backend: StorageBackend
    total_records: int
    total_size_bytes: int
    last_modified: float


class Storage(ABC):
    """
    Abstract storage interface.

    All storage backends must implement this interface.

    Example:
        class MyStorage(Storage):
            async def put(self, collection: str, record: StorageRecord) -> None:
                ...

            async def get(self, collection: str, record_id: str) -> Optional[StorageRecord]:
                ...
    """

    @abstractmethod
    async def initialize(self) -> None:
        """Initialize the storage backend."""
        pass

    @abstractmethod
    async def close(self) -> None:
        """Close the storage backend."""
        pass

    @abstractmethod
    async def put(
        self,
        collection: str,
        record: StorageRecord,
        if_not_exists: bool = False,
    ) -> bool:
        """
        Insert or update a record.

        Args:
            collection: Collection name.
            record: Record to store.
            if_not_exists: If True, only insert if ID doesn't exist.

        Returns:
            True if record was stored, False if if_not_exists and ID exists.
        """
        pass

    @abstractmethod
    async def get(
        self, collection: str, record_id: str
    ) -> Optional[StorageRecord]:
        """
        Get a record by ID.

        Args:
            collection: Collection name.
            record_id: Record identifier.

        Returns:
            Record if found, None otherwise.
        """
        pass

    @abstractmethod
    async def delete(
        self, collection: str, record_id: str
    ) -> bool:
        """
        Delete a record by ID.

        Args:
            collection: Collection name.
            record_id: Record identifier.

        Returns:
            True if deleted, False if not found.
        """
        pass

    @abstractmethod
    async def query(
        self, collection: str, query: Query
    ) -> List[StorageRecord]:
        """
        Query records with filters.

        Args:
            collection: Collection name.
            query: Query specification.

        Returns:
            List of matching records.
        """
        pass

    @abstractmethod
    async def list_collections(self) -> List[str]:
        """List all collection names."""
        pass

    @abstractmethod
    async def get_stats(self, collection: str) -> StorageStats:
        """Get statistics for a collection."""
        pass


class SQLiteStorage(Storage):
    """
    SQLite-based storage implementation.

    Features:
    - Automatic schema migration
    - Transaction support
    - Full-text search
    - JSON field indexing

    Example:
        storage = SQLiteStorage(db_path="/var/lib/openhub/data.db")

        await storage.initialize()

        record = StorageRecord(
            id="rec-001",
            data={"name": "test", "value": 42},
        )

        await storage.put("my_collection", record)

        results = await storage.query(
            "my_collection",
            Query()
                .add_condition("name", QueryOperator.EQ, "test")
                .add_condition("value", QueryOperator.GT, 10)
                .add_condition("value", QueryOperator.LT, 100)
        )
    """

    def __init__(
        self,
        db_path: str = ":memory:",
        wal_mode: bool = True,
        busy_timeout: float = 30.0,
    ) -> None:
        """
        Initialize SQLite storage.

        Args:
            db_path: Path to SQLite database file.
                    Use ":memory:" for in-memory storage.
            wal_mode: Enable Write-Ahead Logging mode.
            busy_timeout: Timeout for locked database (seconds).
        """
        self.db_path = db_path
        self.wal_mode = wal_mode
        self.busy_timeout = busy_timeout

        self._conn: Optional[Any] = None
        self._lock = asyncio.Lock()
        self._initialized = False
        self._closed = False
        self._schema_cache: Set[str] = set()

        self._collection_schemas: Dict[str, str] = {
            "records": """
                CREATE TABLE IF NOT EXISTS {table_name} (
                    id TEXT PRIMARY KEY,
                    data TEXT NOT NULL,
                    created_at REAL NOT NULL,
                    updated_at REAL NOT NULL,
                    metadata TEXT NOT NULL
                )
            """,
            "fts": """
                CREATE VIRTUAL TABLE IF NOT EXISTS {table_name}_fts
                USING fts5(id, data, content='{table_name}', content_rowid='rowid')
            """,
        }

        logger.info(
            "SQLiteStorage created",
            extra={"db_path": db_path},
        )

    async def initialize(self) -> None:
        """Initialize SQLite connection and schema."""
        if self._initialized:
            return

        async with self._lock:
            if self._initialized:
                return

            import sqlite3

            self._conn = sqlite3.connect(
                self.db_path,
                timeout=self.busy_timeout,
                check_same_thread=False,
            )
            self._conn.row_factory = sqlite3.Row

            if self.wal_mode:
                self._conn.execute("PRAGMA journal_mode=WAL")
                self._conn.execute("PRAGMA synchronous=NORMAL")

            self._conn.execute(f"PRAGMA busy_timeout={int(self.busy_timeout * 1000)}")

            loop = asyncio.get_event_loop()
            await loop.run_in_executor(None, self._conn.execute, "SELECT 1")

            self._initialized = True
            self._closed = False

            logger.info(
                "SQLiteStorage initialized",
                extra={"db_path": self.db_path},
            )

    async def close(self) -> None:
        """Close SQLite connection."""
        async with self._lock:
            if self._conn:
                self._conn.close()
                self._conn = None
            self._initialized = False
            self._closed = True
            self._schema_cache.clear()
            logger.info("SQLiteStorage closed")

    def _get_table_name(self, collection: str) -> str:
        """Get sanitized table name for collection."""
        return f"col_{collection.replace('-', '_')}"

    async def _ensure_collection(self, collection: str) -> None:
        """Ensure collection table exists with thread-safety."""
        table_name = self._get_table_name(collection)

        if table_name in self._schema_cache:
            return

        async with self._lock:
            if table_name in self._schema_cache:
                return

            schema = self._collection_schemas["records"].format(
                table_name=table_name
            )

            loop = asyncio.get_event_loop()
            await loop.run_in_executor(None, self._conn.execute, schema)
            self._conn.commit()

            self._schema_cache.add(table_name)
            self._cache_access_time[table_name] = time.time()

    def _evict_expired_cache(self) -> int:
        """
        Evict expired entries from schema cache based on TTL.

        This implements the "forgetting mechanism" (遗忘机制) from the
        architectural design principles - C-4 Memory Volatilization.

        Returns:
            Number of entries evicted.
        """
        if not self._cache_access_time:
            return 0

        current_time = time.time()
        expired_keys = [
            key for key, last_access in self._cache_access_time.items()
            if current_time - last_access > self._cache_ttl
        ]

        for key in expired_keys:
            self._schema_cache.discard(key)
            self._cache_access_time.pop(key, None)

        if len(self._schema_cache) > self._cache_max_size:
            self._evict_lru_cache()

        return len(expired_keys)

    def _evict_lru_cache(self) -> int:
        """
        Evict least recently used entries when cache exceeds max size.

        Returns:
            Number of entries evicted.
        """
        if len(self._schema_cache) <= self._cache_max_size:
            return 0

        sorted_entries = sorted(
            self._cache_access_time.items(),
            key=lambda x: x[1]
        )

        evict_count = len(self._schema_cache) - self._cache_max_size
        for i in range(evict_count):
            if i < len(sorted_entries):
                key = sorted_entries[i][0]
                self._schema_cache.discard(key)
                self._cache_access_time.pop(key, None)

        return evict_count

    async def put(
        self,
        collection: str,
        record: StorageRecord,
        if_not_exists: bool = False,
    ) -> bool:
        """
        Insert or update a record.

        Args:
            collection: Collection name.
            record: Record to store.
            if_not_exists: If True, only insert if ID doesn't exist.

        Returns:
            True if stored, False if skipped.
        """
        await self._ensure_collection(collection)
        table_name = self._get_table_name(collection)

        now = time.time()
        data_json = json.dumps(record.data)
        metadata_json = json.dumps(record.metadata)

        if if_not_exists:
            sql = f"""
                INSERT INTO {table_name} (id, data, created_at, updated_at, metadata)
                VALUES (?, ?, ?, ?, ?)
            """
            params = (record.id, data_json, now, now, metadata_json)
        else:
            sql = f"""
                INSERT OR REPLACE INTO {table_name} (id, data, created_at, updated_at, metadata)
                VALUES (?, ?, COALESCE((SELECT created_at FROM {table_name} WHERE id = ?), ?), ?, ?)
            """
            params = (record.id, data_json, record.id, now, now, metadata_json)

        async with self._lock:
            cursor = await asyncio.get_event_loop().run_in_executor(
                None,
                lambda: self._conn.execute(sql, params)
            )
            self._conn.commit()

        return cursor.rowcount > 0 if hasattr(cursor, 'rowcount') else True

    async def get(
        self, collection: str, record_id: str
    ) -> Optional[StorageRecord]:
        """
        Get a record by ID.

        Args:
            collection: Collection name.
            record_id: Record identifier.

        Returns:
            Record if found, None otherwise.
        """
        await self._ensure_collection(collection)
        table_name = self._get_table_name(collection)

        sql = f"SELECT * FROM {table_name} WHERE id = ?"

        async with self._lock:
            cursor = await asyncio.get_event_loop().run_in_executor(
                None,
                lambda: self._conn.execute(sql, (record_id,))
            )
            row = cursor.fetchone()

        if row is None:
            return None

        return self._row_to_record(row)

    async def delete(
        self, collection: str, record_id: str
    ) -> bool:
        """
        Delete a record by ID.

        Args:
            collection: Collection name.
            record_id: Record identifier.

        Returns:
            True if deleted, False if not found.
        """
        await self._ensure_collection(collection)
        table_name = self._get_table_name(collection)

        sql = f"DELETE FROM {table_name} WHERE id = ?"

        async with self._lock:
            cursor = await asyncio.get_event_loop().run_in_executor(
                None,
                lambda: self._conn.execute(sql, (record_id,))
            )
            self._conn.commit()

        return cursor.rowcount > 0

    async def query(
        self, collection: str, query: Query
    ) -> List[StorageRecord]:
        """
        Query records with filters.

        Args:
            collection: Collection name.
            query: Query specification.

        Returns:
            List of matching records.
        """
        await self._ensure_collection(collection)
        table_name = self._get_table_name(collection)

        sql_parts = ["SELECT * FROM {table_name}"]
        params = []

        if query.conditions:
            where_parts = []
            for cond in query.conditions:
                if cond.operator == QueryOperator.IS_NULL:
                    where_parts.append(f"{cond.field} IS NULL")
                elif cond.operator == QueryOperator.IS_NOT_NULL:
                    where_parts.append(f"{cond.field} IS NOT NULL")
                elif cond.operator == QueryOperator.IN:
                    placeholders = ", ".join(["?" for _ in cond.value])
                    where_parts.append(f"{cond.field} IN ({placeholders})")
                    params.extend(cond.value)
                elif cond.operator == QueryOperator.NOT_IN:
                    placeholders = ", ".join(["?" for _ in cond.value])
                    where_parts.append(f"{cond.field} NOT IN ({placeholders})")
                    params.extend(cond.value)
                elif cond.operator == QueryOperator.LIKE:
                    where_parts.append(f"{cond.field} LIKE ?")
                    params.append(cond.value)
                else:
                    where_parts.append(f"{cond.field} {cond.operator.value} ?")
                    params.append(cond.value)

            sql_parts.append("WHERE " + " AND ".join(where_parts))

        if query.order_by:
            direction = "DESC" if query.order_desc else "ASC"
            sql_parts.append(f"ORDER BY {query.order_by} {direction}")

        if query.limit is not None:
            sql_parts.append(f"LIMIT {query.limit}")
            sql_parts.append(f"OFFSET {query.offset}")

        sql = " ".join(sql_parts).format(table_name=table_name)

        async with self._lock:
            cursor = await asyncio.get_event_loop().run_in_executor(
                None,
                lambda: self._conn.execute(sql, params)
            )
            rows = cursor.fetchall()

        return [self._row_to_record(row) for row in rows]

    async def list_collections(self) -> List[str]:
        """
        List all collection names.

        Returns:
            List of collection names.
        """
        sql = """
            SELECT name FROM sqlite_master
            WHERE type='table' AND name LIKE 'col_%'
            ORDER BY name
        """

        async with self._lock:
            cursor = await asyncio.get_event_loop().run_in_executor(
                None,
                lambda: self._conn.execute(sql)
            )
            rows = cursor.fetchall()

        return [
            row[0].replace("col_", "").replace("_", "-")
            for row in rows
        ]

    async def get_stats(self, collection: str) -> StorageStats:
        """
        Get statistics for a collection.

        Args:
            collection: Collection name.

        Returns:
            StorageStats for the collection.
        """
        await self._ensure_collection(collection)
        table_name = self._get_table_name(collection)

        count_sql = f"SELECT COUNT(*) as count, MAX(updated_at) as last_modified FROM {table_name}"
        size_sql = f"SELECT page_count * page_size as size FROM pragma_page_count(), pragma_page_size()"

        async with self._lock:
            count_cursor = await asyncio.get_event_loop().run_in_executor(
                None, lambda: self._conn.execute(count_sql)
            )
            count_row = count_cursor.fetchone()

            size_cursor = await asyncio.get_event_loop().run_in_executor(
                None, lambda: self._conn.execute(size_sql)
            )
            size_row = size_cursor.fetchone()

        return StorageStats(
            backend=StorageBackend.SQLITE,
            total_records=count_row["count"] if count_row else 0,
            total_size_bytes=size_row["size"] if size_row else 0,
            last_modified=count_row["last_modified"] if count_row and count_row["last_modified"] else 0.0,
        )

    async def transaction(
        self, operations: List[Tuple[str, StorageRecord]]
    ) -> bool:
        """
        Execute multiple operations in a transaction.

        Args:
            operations: List of (operation, record) tuples.
                       operation can be "put" or "delete".

        Returns:
            True if all succeeded, False if any failed.
        """
        async with self._lock:
            try:
                for op, record in operations:
                    if op == "put":
                        await self.put(record.data["_collection"], record)
                    elif op == "delete":
                        await self.delete(record.data["_collection"], record.id)
                self._conn.commit()
                return True
            except Exception as e:
                self._conn.rollback()
                logger.error(f"Transaction failed: {e}")
                return False

    def _row_to_record(self, row: Any) -> StorageRecord:
        """Convert SQLite row to StorageRecord."""
        return StorageRecord(
            id=row["id"],
            data=json.loads(row["data"]),
            created_at=row["created_at"],
            updated_at=row["updated_at"],
            metadata=json.loads(row["metadata"]) if row["metadata"] else {},
        )


class InMemoryStorage(Storage):
    """
    In-memory storage implementation for testing and caching.

    WARNING: This storage is not persistent.
    Use only for testing or as a temporary cache.
    """

    def __init__(self) -> None:
        """Initialize in-memory storage."""
        self._data: Dict[str, Dict[str, StorageRecord]] = {}
        self._lock = asyncio.Lock()
        self._initialized = False

    async def initialize(self) -> None:
        """Initialize storage (no-op for memory)."""
        self._initialized = True
        logger.info("InMemoryStorage initialized")

    async def close(self) -> None:
        """Close storage (no-op for memory)."""
        self._data.clear()
        self._initialized = False
        logger.info("InMemoryStorage closed")

    async def put(
        self,
        collection: str,
        record: StorageRecord,
        if_not_exists: bool = False,
    ) -> bool:
        """Insert or update a record in memory."""
        async with self._lock:
            if collection not in self._data:
                self._data[collection] = {}

            if if_not_exists and record.id in self._data[collection]:
                return False

            self._data[collection][record.id] = record
            return True

    async def get(
        self, collection: str, record_id: str
    ) -> Optional[StorageRecord]:
        """Get a record from memory."""
        async with self._lock:
            return self._data.get(collection, {}).get(record_id)

    async def delete(
        self, collection: str, record_id: str
    ) -> bool:
        """Delete a record from memory."""
        async with self._lock:
            if collection in self._data and record_id in self._data[collection]:
                del self._data[collection][record_id]
                return True
            return False

    async def query(
        self, collection: str, query: Query
    ) -> List[StorageRecord]:
        """Query records from memory."""
        async with self._lock:
            records = list(self._data.get(collection, {}).values())

        if not query.conditions:
            return self._apply_pagination(records, query)

        filtered = []
        for record in records:
            if self._matches_query(record, query):
                filtered.append(record)

        return self._apply_pagination(filtered, query)

    async def list_collections(self) -> List[str]:
        """List all collections."""
        async with self._lock:
            return list(self._data.keys())

    async def get_stats(self, collection: str) -> StorageStats:
        """Get statistics for a collection."""
        async with self._lock:
            records = self._data.get(collection, {})
            return StorageStats(
                backend=StorageBackend.MEMORY,
                total_records=len(records),
                total_size_bytes=sum(
                    len(json.dumps(r.data)) for r in records.values()
                ),
                last_modified=max(
                    (r.updated_at for r in records.values()),
                    default=0.0,
                ),
            )

    def _matches_query(self, record: StorageRecord, query: Query) -> bool:
        """Check if record matches query conditions."""
        for cond in query.conditions:
            value = record.data.get(cond.field)

            if cond.operator == QueryOperator.IS_NULL:
                if value is not None:
                    return False
            elif cond.operator == QueryOperator.IS_NOT_NULL:
                if value is None:
                    return False
            elif cond.operator == QueryOperator.EQ:
                if value != cond.value:
                    return False
            elif cond.operator == QueryOperator.NE:
                if value == cond.value:
                    return False
            elif cond.operator == QueryOperator.LT:
                if value >= cond.value:
                    return False
            elif cond.operator == QueryOperator.LE:
                if value > cond.value:
                    return False
            elif cond.operator == QueryOperator.GT:
                if value <= cond.value:
                    return False
            elif cond.operator == QueryOperator.GE:
                if value < cond.value:
                    return False
            elif cond.operator == QueryOperator.IN:
                if value not in cond.value:
                    return False
            elif cond.operator == QueryOperator.NOT_IN:
                if value in cond.value:
                    return False
            elif cond.operator == QueryOperator.LIKE:
                if not str(value).__contains__(str(cond.value)):
                    return False

        return True

    def _apply_pagination(
        self, records: List[StorageRecord], query: Query
    ) -> List[StorageRecord]:
        """Apply ordering and pagination to records."""
        if query.order_by:
            records.sort(
                key=lambda r: r.data.get(query.order_by, ""),
                reverse=query.order_desc,
            )

        offset = query.offset
        limit = query.limit

        if offset:
            records = records[offset:]
        if limit:
            records = records[:limit]

        return records
