# Copyright (c) 2026 SPHARX. All Rights Reserved.
# "From data intelligence emerges."

"""
Unit Tests for Core Storage Module
==================================

Tests for openlab.core.storage module.
These tests verify the Storage, MemoryStorage, SQLiteStorage, and related classes.
"""

import pytest
import asyncio
from typing import Dict, Any
import sys
import os
import tempfile

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))


class TestStorageType:
    """Tests for StorageType enum."""

    def test_storage_type_values(self):
        """Test StorageType enum has correct values."""
        from openlab.core.storage import StorageType

        assert StorageType.MEMORY.value == "memory"
        assert StorageType.SQLITE.value == "sqlite"
        assert StorageType.FILE.value == "file"
        assert StorageType.REDIS.value == "redis"
        assert StorageType.CUSTOM.value == "custom"


class TestDataCategory:
    """Tests for DataCategory enum."""

    def test_category_values(self):
        """Test DataCategory enum has correct values."""
        from openlab.core.storage import DataCategory

        assert DataCategory.TASK.value == "task"
        assert DataCategory.AGENT.value == "agent"
        assert DataCategory.TOOL.value == "tool"
        assert DataCategory.CHECKPOINT.value == "checkpoint"


class TestStorageRecord:
    """Tests for StorageRecord dataclass."""

    def test_record_creation(self):
        """Test StorageRecord can be created."""
        from openlab.core.storage import StorageRecord

        record = StorageRecord(key="test-key", value={"data": "test"})
        
        assert record.key == "test-key"
        assert record.value == {"data": "test"}
        assert record.category == DataCategory.METADATA
        assert record.version == 1

    def test_record_serialization(self):
        """Test record to_dict/from_dict roundtrip."""
        from openlab.core.storage import StorageRecord

        original = StorageRecord(
            key="key-001",
            value="value_data",
            category=DataCategory.TASK,
            metadata={"author": "Spharx AgentOS Team"},
            version=3,
        )
        
        data = original.to_dict()
        restored = StorageRecord.from_dict(data)
        
        assert restored.key == original.key
        assert restored.value == original.value
        assert restored.category == original.category
        assert restored.version == 3


class TestQueryResult:
    """Tests for QueryResult dataclass."""

    def test_result_creation(self):
        """Test QueryResult creation."""
        from openlab.core.storage import QueryResult, StorageRecord
        
        records = [
            StorageRecord(key="k1", value="v1"),
            StorageRecord(key="k2", value="v2"),
        ]
        
        result = QueryResult(
            records=records,
            total=2,
            offset=0,
            limit=10,
        )
        
        assert len(result.records) == 2
        assert result.total == 2
        assert result.offset == 0
        assert result.limit == 10


class TestMemoryStorage:
    """Tests for MemoryStorage class."""

    @pytest.fixture
    async def storage(self):
        """Create and initialize a memory storage."""
        from openlab.core.storage import MemoryStorage
        store = MemoryStorage()
        await store.initialize()
        yield store
        await store.close()

    @pytest.mark.asyncio
    async def test_initialization(self, storage):
        """Test storage initialization."""
        assert storage.initialized is True
        assert storage.size() == 0

    @pytest.mark.asyncio
    async def test_set_and_get(self, storage):
        """Test setting and getting values."""
        await storage.set("key-1", "value-1")
        
        record = await storage.get("key-1")
        
        assert record is not None
        assert record.value == "value-1"

    @pytest.mark.asyncio
    async def test_get_nonexistent(self, storage):
        """Test getting non-existent key returns None."""
        record = await storage.get("nonexistent")
        
        assert record is None

    @pytest.mark.asyncio
    async def test_exists(self, storage):
        """Test exists check."""
        assert await storage.exists("new-key") is False
        
        await storage.set("new-key", "value")
        assert await storage.exists("new-key") is True

    @pytest.mark.asyncio
    async def test_delete(self, storage):
        """Test deleting a key."""
        await storage.set("delete-me", "value")
        
        success = await storage.delete("delete-me")
        
        assert success is True
        assert await storage.exists("delete-me") is False

    @pytest.mark.asyncio
    async def test_update_increments_version(self, storage):
        """Test updating increments version."""
        await storage.set("version-key", "v1")
        r1 = await storage.get("version-key")
        v1 = r1.version
        
        await storage.set("version-key", "v2")
        r2 = await storage.get("version-key")
        v2 = r2.version
        
        assert v2 > v1

    @pytest.mark.asyncio
    async def test_json_operations(self, storage):
        """Test JSON get/set operations."""
        await storage.set_json("json-key", {"nested": {"data": [1, 2, 3]}})
        
        result = await storage.get_json("json-key")
        
        assert result is not None
        assert result["nested"]["data"] == [1, 2, 3]

    @pytest.mark.asyncio
    async def test_query_all(self, storage):
        """Test querying all records."""
        await storage.set("q1", "v1", category=DataCategory.TASK)
        await storage.set("q2", "v2", category=DataCategory.AGENT)
        await storage.set("q3", "v3", category=DataCategory.TASK)
        
        result = await storage.query()
        
        assert result.total == 3
        assert len(result.records) == 3

    @pytest.mark.asyncio
    async def test_query_by_category(self, storage):
        """Test querying by category."""
        await storage.set("t1", "v1", category=DataCategory.TASK)
        await storage.set("a1", "v2", category=DataCategory.AGENT)
        await storage.set("t2", "v3", category=DataCategory.TASK)
        
        result = await storage.query(category=DataCategory.TASK)
        
        assert result.total == 2

    @pytest.mark.asyncio
    async def test_clear(self, storage):
        """Test clearing all records."""
        await storage.set("c1", "v1")
        await storage.set("c2", "v2")
        
        await storage.clear()
        
        assert storage.size() == 0


class TestSQLiteStorage:
    """Tests for SQLiteStorage class."""

    @pytest.fixture
    async def storage(self):
        """Create and initialize a SQLite storage with temp file."""
        from openlab.core.storage import SQLiteStorage
        import tempfile
        
        with tempfile.NamedTemporaryFile(suffix=".db", delete=False) as f:
            db_path = f.name
        
        store = SQLiteStorage(db_path=db_path)
        await store.initialize()
        yield store
        await store.close()
        
        # Cleanup
        os.unlink(db_path)

    @pytest.mark.asyncio
    async def test_initialization(self, storage):
        """Test SQLite storage initialization."""
        assert storage.initialized is True
        assert storage._conn is not None

    @pytest.mark.asyncio
    async def test_set_and_get(self, storage):
        """Test setting and getting values in SQLite."""
        await storage.set("sqlite-key", "sqlite-value")
        
        record = await storage.get("sqlite-key")
        
        assert record is not None
        assert record.value == "sqlite-value"

    @pytest.mark.asyncio
    async def test_persistence_across_instances(self, storage):
        """Test data persists across storage instances."""
        # Write with first instance
        await storage.set("persist-key", "persist-value")
        await storage.close()
        
        # Create new instance (same DB file)
        from openlab.core.storage import SQLiteStorage
        store2 = SQLiteStorage(db_path=storage.db_path)
        await store2.initialize()
        
        record = await store2.get("persist-key")
        
        assert record is not None
        assert record.value == "persist-value"
        
        await store2.close()

    @pytest.mark.asyncio
    async def test_exists(self, storage):
        """Test exists check in SQLite."""
        assert await storage.exists("new-sqlite") is False
        
        await storage.set("new-sqlite", "value")
        assert await storage.exists("new-sqlite") is True

    @pytest.mark.asyncio
    async def test_delete(self, storage):
        """Test deleting from SQLite."""
        await storage.set("del-sqlite", "value")
        
        success = await storage.delete("del-sqlite")
        
        assert success is True

    @pytest.mark.asyncio
    async def test_query(self, storage):
        """Test querying in SQLite."""
        await storage.set("sq1", "v1", category=DataCategory.TASK)
        await storage.set("sq2", "v2", category=DataCategory.AGENT)
        
        result = await storage.query(category=DataCategory.TASK)
        
        assert result.total >= 1


if __name__ == "__main__":
    pytest.main([__file__, "-v"])
