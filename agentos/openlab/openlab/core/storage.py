"""
OpenLab Core Storage Module

鏁版嵁瀛樺偍鎶借薄鏍稿績妯″潡
閬靛惊 AgentOS 鏋舵瀯璁捐鍘熷垯 V1.8
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
    """瀛樺偍绫诲瀷鏋氫妇"""
    MEMORY = "memory"           # 鍐呭瓨瀛樺偍
    SQLITE = "sqlite"           # SQLite 鏁版嵁搴?    FILE = "file"              # 鏂囦欢绯荤粺
    REDIS = "redis"            # Redis
    CUSTOM = "custom"          # 鑷畾涔?

class DataCategory(Enum):
    """鏁版嵁绫诲埆鏋氫妇"""
    TASK = "task"              # 浠诲姟鏁版嵁
    AGENT = "agent"            # Agent 鏁版嵁
    TOOL = "tool"              # 宸ュ叿鏁版嵁
    CHECKPOINT = "checkpoint"  # 妫€鏌ョ偣鏁版嵁
    LOG = "log"                # 鏃ュ織鏁版嵁
    METADATA = "metadata"      # 鍏冩暟鎹?

@dataclass
class StorageRecord:
    """
    瀛樺偍璁板綍
    
    閬靛惊鍘熷垯:
    - A-1 绠€绾﹁嚦涓婏細鏈€灏忓繀瑕佸瓧娈?    - E-5 鍛藉悕璇箟鍖栵細鍚嶇О鍗虫枃妗?    """
    key: str
    value: Any
    category: DataCategory = DataCategory.METADATA
    metadata: Dict[str, Any] = field(default_factory=dict)
    created_at: float = field(default_factory=time.time)
    updated_at: float = field(default_factory=time.time)
    expires_at: Optional[float] = None
    version: int = 1
    
    def to_dict(self) -> Dict[str, Any]:
        """搴忓垪鍖栦负瀛楀吀"""
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
        """浠庡瓧鍏稿弽搴忓垪鍖?""
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
    """鏌ヨ缁撴灉"""
    records: List[StorageRecord]
    total: int
    offset: int
    limit: int


class Storage(ABC):
    """
    瀛樺偍鎶借薄鍩虹被
    
    閬靛惊鍘熷垯:
    - K-2 鎺ュ彛濂戠害鍖栵細瀹屾暣鐨?docstring 鍜岀被鍨嬫敞瑙?    - K-4 鍙彃鎷旂瓥鐣ワ細鏀寔涓嶅悓瀛樺偍鍚庣
    - E-3 璧勬簮纭畾鎬э細鏄庣‘鐨勮祫婧愮敓鍛藉懆鏈?    """
    
    def __init__(self, storage_type: StorageType):
        """
        鍒濆鍖栧瓨鍌?        
        Args:
            storage_type: 瀛樺偍绫诲瀷
        """
        self.storage_type = storage_type
        self._initialized = False
    
    @property
    def initialized(self) -> bool:
        """鏄惁宸插垵濮嬪寲"""
        return self._initialized
    
    @abstractmethod
    async def initialize(self) -> None:
        """
        鍒濆鍖栧瓨鍌?        
        蹇呴』瀹炵幇鐨勬柟娉?        """
        self._initialized = True
    
    @abstractmethod
    async def close(self) -> None:
        """
        鍏抽棴瀛樺偍
        
        娓呯悊璧勬簮锛屽繀椤诲疄鐜?        """
        self._initialized = False
    
    @abstractmethod
    async def get(self, key: str) -> Optional[StorageRecord]:
        """
        鑾峰彇璁板綍
        
        Args:
            key: 璁板綍閿?            
        Returns:
            Optional[StorageRecord]: 璁板綍
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
        璁剧疆璁板綍
        
        Args:
            key: 璁板綍閿?            value: 璁板綍鍊?            category: 鏁版嵁绫诲埆
            metadata: 鍏冩暟鎹?            ttl: 鐢熷瓨鏃堕棿锛堢锛?            
        Returns:
            bool: 鏄惁鎴愬姛
        """
        pass
    
    @abstractmethod
    async def delete(self, key: str) -> bool:
        """
        鍒犻櫎璁板綍
        
        Args:
            key: 璁板綍閿?            
        Returns:
            bool: 鏄惁鎴愬姛
        """
        pass
    
    @abstractmethod
    async def exists(self, key: str) -> bool:
        """
        妫€鏌ヨ褰曟槸鍚﹀瓨鍦?        
        Args:
            key: 璁板綍閿?            
        Returns:
            bool: 鏄惁瀛樺湪
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
        鏌ヨ璁板綍
        
        Args:
            category: 鏁版嵁绫诲埆
            filter_func: 杩囨护鍑芥暟
            offset: 鍋忕Щ閲?            limit: 闄愬埗鏁伴噺
            
        Returns:
            QueryResult: 鏌ヨ缁撴灉
        """
        pass
    
    @abstractmethod
    async def clear(self) -> None:
        """
        娓呯┖鎵€鏈夎褰?        """
        pass
    
    async def get_json(self, key: str) -> Optional[Any]:
        """
        鑾峰彇 JSON 鏁版嵁
        
        Args:
            key: 璁板綍閿?            
        Returns:
            Optional[Any]: 瑙ｆ瀽鍚庣殑 JSON 鏁版嵁
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
        璁剧疆 JSON 鏁版嵁
        
        Args:
            key: 璁板綍閿?            value: 鏁版嵁鍊硷紙灏嗚 JSON 搴忓垪鍖栵級
            **kwargs: 鍏朵粬鍙傛暟
            
        Returns:
            bool: 鏄惁鎴愬姛
        """
        return await self.set(key, json.dumps(value), **kwargs)


class MemoryStorage(Storage):
    """
    鍐呭瓨瀛樺偍瀹炵幇
    
    鐗圭偣:
    - 蹇€熻闂?    - 閲嶅惎鍚庢暟鎹涪澶?    - 閫傚悎缂撳瓨鍜屼复鏃舵暟鎹?    """
    
    def __init__(self):
        super().__init__(StorageType.MEMORY)
        self._data: Dict[str, StorageRecord] = {}
        self._lock = asyncio.Lock()
    
    async def initialize(self) -> None:
        """鍒濆鍖?""
        await super().initialize()
    
    async def close(self) -> None:
        """鍏抽棴"""
        async with self._lock:
            self._data.clear()
        await super().close()
    
    async def get(self, key: str) -> Optional[StorageRecord]:
        """鑾峰彇璁板綍"""
        async with self._lock:
            record = self._data.get(key)
            if record:
                # 妫€鏌ユ槸鍚﹁繃鏈?                if record.expires_at and time.time() > record.expires_at:
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
        """璁剧疆璁板綍"""
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
            
            # 濡傛灉宸插瓨鍦紝澧炲姞鐗堟湰鍙?            existing = self._data.get(key)
            if existing:
                record.version = existing.version + 1
            
            self._data[key] = record
            return True
    
    async def delete(self, key: str) -> bool:
        """鍒犻櫎璁板綍"""
        async with self._lock:
            if key in self._data:
                del self._data[key]
                return True
            return False
    
    async def exists(self, key: str) -> bool:
        """妫€鏌ヨ褰曟槸鍚﹀瓨鍦?""
        async with self._lock:
            return key in self._data
    
    async def query(
        self,
        category: Optional[DataCategory] = None,
        filter_func: Optional[callable] = None,
        offset: int = 0,
        limit: int = 100
    ) -> QueryResult:
        """鏌ヨ璁板綍"""
        async with self._lock:
            records = list(self._data.values())
            
            # 鎸夌被鍒繃婊?            if category:
                records = [r for r in records if r.category == category]
            
            # 鎸夊嚱鏁拌繃婊?            if filter_func:
                records = [r for r in records if filter_func(r)]
            
            # 妫€鏌ヨ繃鏈?            now = time.time()
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
        """娓呯┖鎵€鏈夎褰?""
        async with self._lock:
            self._data.clear()
    
    def size(self) -> int:
        """鑾峰彇璁板綍鏁伴噺"""
        return len(self._data)


class SQLiteStorage(Storage):
    """
    SQLite 瀛樺偍瀹炵幇
    
    鐗圭偣:
    - 鎸佷箙鍖栧瓨鍌?    - 鏀寔澶嶆潅鏌ヨ
    - 閫傚悎浠诲姟鐘舵€併€佹鏌ョ偣绛夋暟鎹?    
    閬靛惊鍘熷垯:
    - E-3 璧勬簮纭畾鎬э細杩炴帴绠＄悊鍜屼簨鍔″鐞?    """
    
    def __init__(self, db_path: Union[str, Path]):
        """
        鍒濆鍖?SQLite 瀛樺偍
        
        Args:
            db_path: 鏁版嵁搴撴枃浠惰矾寰?        """
        super().__init__(StorageType.SQLITE)
        self.db_path = Path(db_path)
        self._conn: Optional[sqlite3.Connection] = None
        self._lock = asyncio.Lock()
    
    async def initialize(self) -> None:
        """鍒濆鍖栨暟鎹簱"""
        await super().initialize()
        
        # 纭繚鐩綍瀛樺湪
        self.db_path.parent.mkdir(parents=True, exist_ok=True)
        
        # 鍒涘缓杩炴帴
        self._conn = sqlite3.connect(
            str(self.db_path),
            check_same_thread=False
        )
        self._conn.row_factory = sqlite3.Row
        
        # 鍒涘缓琛?        await self._create_tables()
    
    async def _create_tables(self) -> None:
        """鍒涘缓鏁版嵁琛?""
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
        """鍏抽棴鏁版嵁搴撹繛鎺?""
        if self._conn:
            loop = asyncio.get_event_loop()
            await loop.run_in_executor(None, self._conn.close)
            self._conn = None
        await super().close()
    
    async def get(self, key: str) -> Optional[StorageRecord]:
        """鑾峰彇璁板綍"""
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
            
            # 妫€鏌ヨ繃鏈?            if record.expires_at and time.time() > record.expires_at:
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
        """璁剧疆璁板綍"""
        loop = asyncio.get_event_loop()
        now = time.time()
        
        def upsert():
            cursor = self._conn.cursor()
            
            # 妫€鏌ユ槸鍚﹀瓨鍦?            cursor.execute(
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
        """鍒犻櫎璁板綍"""
        loop = asyncio.get_event_loop()
        
        def delete():
            cursor = self._conn.cursor()
            cursor.execute("DELETE FROM records WHERE key = ?", (key,))
            self._conn.commit()
            return cursor.rowcount > 0
        
        return await loop.run_in_executor(None, delete)
    
    async def exists(self, key: str) -> bool:
        """妫€鏌ヨ褰曟槸鍚﹀瓨鍦?""
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
        """鏌ヨ璁板綍"""
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
            
            # 鑾峰彇鎬绘暟
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
            
            # 搴旂敤杩囨护鍑芥暟
            if filter_func and not filter_func(record):
                continue
            
            # 妫€鏌ヨ繃鏈?            if record.expires_at and time.time() > record.expires_at:
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
        """娓呯┖鎵€鏈夎褰?""
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
