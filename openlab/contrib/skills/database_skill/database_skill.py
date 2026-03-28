# Copyright (c) 2026 SPHARX. All Rights Reserved.
# "From data intelligence emerges."

"""
Database Operations Skill
======================

This module provides database operations capabilities for the openlab platform,
including SQL query execution, schema management, connection pooling, and caching.

Architecture:
- SQLAlchemy for ORM and database abstraction
- Connection pooling for efficient resource management
- Redis caching for query result caching
- Transaction management with rollback support
- Migration utilities for schema changes

Features:
- Multi-database support (PostgreSQL, MySQL, SQLite, Redis)
- Connection pooling and management
- SQL query execution with parameterized queries
- Schema introspection and management
- Transaction support with commit/rollback
- Query result caching
- Index analysis and optimization suggestions
"""

import json
import logging
import hashlib
import time
from dataclasses import dataclass, field
from datetime import datetime
from enum import Enum
from typing import Any, Dict, List, Optional, Tuple, Union

try:
    from sqlalchemy import create_engine, text, inspect, MetaData, Table, Column, Integer, String, Float, Boolean, DateTime, JSON
    from sqlalchemy.orm import sessionmaker, Session
    from sqlalchemy.pool import QueuePool, NullPool
    from sqlalchemy.exc import SQLAlchemyError, OperationalError
    SQLALCHEMY_AVAILABLE = True
except ImportError:
    SQLALCHEMY_AVAILABLE = False

try:
    import redis
    REDIS_AVAILABLE = True
except ImportError:
    REDIS_AVAILABLE = False


logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s - %(name)s - %(levelname)s - %(message)s'
)
logger = logging.getLogger(__name__)


class DatabaseType(Enum):
    """Supported database types."""
    POSTGRESQL = "postgresql"
    MYSQL = "mysql"
    SQLITE = "sqlite"
    REDIS = "redis"


class IsolationLevel(Enum):
    """Transaction isolation levels."""
    READ_COMMITTED = "READ COMMITTED"
    READ_UNCOMMITTED = "READ UNCOMMITTED"
    REPEATABLE_READ = "REPEATABLE READ"
    SERIALIZABLE = "SERIALIZABLE"


@dataclass
class ConnectionConfig:
    """Database connection configuration."""
    database_type: DatabaseType = DatabaseType.POSTGRESQL
    connection_string: str = ""
    pool_size: int = 5
    max_overflow: int = 10
    pool_timeout: int = 30
    pool_recycle: int = 3600
    echo: bool = False
    isolation_level: Optional[str] = None
    cache_enabled: bool = False
    cache_ttl: int = 300


@dataclass
class QueryResult:
    """Result of a database query."""
    success: bool
    rows: List[Dict[str, Any]] = field(default_factory=list)
    row_count: int = 0
    columns: List[str] = field(default_factory=list)
    last_insert_id: Optional[int] = None
    affected_rows: int = 0
    error: Optional[str] = None
    duration: float = 0.0
    cached: bool = False


@dataclass
class TableInfo:
    """Information about a database table."""
    name: str
    schema: Optional[str]
    columns: List[Dict[str, Any]]
    primary_key: Optional[str]
    foreign_keys: List[Dict[str, Any]]
    indexes: List[Dict[str, Any]]
    row_count: Optional[int] = None


@dataclass
class IndexInfo:
    """Information about a database index."""
    name: str
    table: str
    columns: List[str]
    unique: bool
    type: Optional[str] = None


class QueryCache:
    """Query result cache using Redis."""

    def __init__(self, redis_url: str, ttl: int = 300):
        self.redis_url = redis_url
        self.ttl = ttl
        self.client = None
        self._connect()

    def _connect(self):
        """Connect to Redis."""
        if REDIS_AVAILABLE:
            try:
                self.client = redis.from_url(redis_url=self.redis_url, decode_responses=True)
                self.client.ping()
                logger.info(f"Redis cache connected: {self.redis_url}")
            except Exception as e:
                logger.warning(f"Redis connection failed: {e}")
                self.client = None

    def _generate_key(self, query: str, params: Optional[Dict[str, Any]] = None) -> str:
        """Generate cache key from query and parameters."""
        key_data = json.dumps({"query": query, "params": params}, sort_keys=True)
        return f"db_cache:{hashlib.md5(key_data.encode()).hexdigest()}"

    def get(self, query: str, params: Optional[Dict[str, Any]] = None) -> Optional[Any]:
        """Get cached query result."""
        if not self.client:
            return None

        try:
            key = self._generate_key(query, params)
            cached = self.client.get(key)
            if cached:
                return json.loads(cached)
        except Exception as e:
            logger.warning(f"Cache get error: {e}")
        return None

    def set(self, query: str, params: Optional[Dict[str, Any]], result: Any, ttl: Optional[int] = None) -> bool:
        """Cache query result."""
        if not self.client:
            return False

        try:
            key = self._generate_key(query, params)
            self.client.setex(key, ttl or self.ttl, json.dumps(result))
            return True
        except Exception as e:
            logger.warning(f"Cache set error: {e}")
            return False

    def invalidate(self, pattern: Optional[str] = None) -> int:
        """Invalidate cached queries."""
        if not self.client:
            return 0

        try:
            if pattern:
                keys = self.client.keys(f"db_cache:{pattern}")
                if keys:
                    return self.client.delete(*keys)
            return 0
        except Exception as e:
            logger.warning(f"Cache invalidate error: {e}")
            return 0

    def clear(self) -> bool:
        """Clear all cached queries."""
        if not self.client:
            return False

        try:
            keys = self.client.keys("db_cache:*")
            if keys:
                self.client.delete(*keys)
            return True
        except Exception as e:
            logger.warning(f"Cache clear error: {e}")
            return False


class DatabaseSkill:
    """Main database operations skill class."""

    def __init__(self, manager: Optional[Dict[str, Any]] = None):
        """Initialize the database skill with configuration."""
        self.manager = self._parse_config(manager or {})
        self.engine = None
        self.session_factory = None
        self.cache = None
        self._connection = None
        self._start_time = None

    def _parse_config(self, manager: Dict[str, Any]) -> ConnectionConfig:
        """Parse configuration dictionary."""
        db_type = manager.get("database_type", "postgresql")
        try:
            db_type_enum = DatabaseType(db_type)
        except ValueError:
            db_type_enum = DatabaseType.POSTGRESQL

        return ConnectionConfig(
            database_type=db_type_enum,
            connection_string=manager.get("connection_string", ""),
            pool_size=manager.get("pool_size", 5),
            max_overflow=manager.get("max_overflow", 10),
            pool_timeout=manager.get("pool_timeout", 30),
            pool_recycle=manager.get("pool_recycle", 3600),
            echo=manager.get("echo", False),
            isolation_level=manager.get("isolation_level")
        )

    def initialize(self) -> Dict[str, Any]:
        """Initialize database connection."""
        self._start_time = time.time()

        if self.manager.database_type == DatabaseType.REDIS:
            return self._initialize_redis()

        if not SQLALCHEMY_AVAILABLE:
            return {"success": False, "error": "SQLAlchemy not available. Install with: pip install sqlalchemy"}

        try:
            if self.manager.database_type == DatabaseType.SQLITE:
                self.engine = create_engine(
                    self.manager.connection_string or "sqlite:///./database.db",
                    echo=self.manager.echo,
                    poolclass=NullPool
                )
            else:
                self.engine = create_engine(
                    self.manager.connection_string,
                    echo=self.manager.echo,
                    poolclass=QueuePool,
                    pool_size=self.manager.pool_size,
                    max_overflow=self.manager.max_overflow,
                    pool_timeout=self.manager.pool_timeout,
                    pool_recycle=self.manager.pool_recycle,
                    pool_pre_ping=True
                )

                if self.manager.isolation_level:
                    from sqlalchemy import event

                    @event.listens_for(self.engine, "connect")
                    def set_isolation_level(dbapi_conn, connection_record):
                        cursor = dbapi_conn.cursor()
                        # 使用白名单验证隔离级别，防止SQL注入
                        valid_levels = {
                            "READ COMMITTED", "READ UNCOMMITTED",
                            "REPEATABLE READ", "SERIALIZABLE"
                        }
                        level = self.manager.isolation_level.upper() if self.manager.isolation_level else None
                        if level in valid_levels:
                            cursor.execute(f"SET SESSION TRANSACTION ISOLATION LEVEL {level}")
                        cursor.close()

            self.session_factory = sessionmaker(bind=self.engine)

            if self.manager.cache_enabled and REDIS_AVAILABLE:
                self.cache = QueryCache(
                    redis_url=self.manager.connection_string,
                    ttl=self.manager.cache_ttl
                )

            logger.info(f"Database initialized: {self.manager.database_type.value}")
            return {"success": True, "database_type": self.manager.database_type.value}

        except Exception as e:
            logger.error(f"Database initialization failed: {e}")
            return {"success": False, "error": str(e)}

    def _initialize_redis(self) -> Dict[str, Any]:
        """Initialize Redis connection."""
        if not REDIS_AVAILABLE:
            return {"success": False, "error": "Redis not available. Install with: pip install redis"}

        try:
            self._connection = redis.from_url(
                self.manager.connection_string or "redis://localhost:6379",
                decode_responses=True
            )
            self._connection.ping()

            if self.manager.cache_enabled:
                self.cache = QueryCache(
                    redis_url=self.manager.connection_string,
                    ttl=self.manager.cache_ttl
                )

            logger.info("Redis initialized successfully")
            return {"success": True, "database_type": "redis"}

        except Exception as e:
            logger.error(f"Redis initialization failed: {e}")
            return {"success": False, "error": str(e)}

    def connect(self) -> Dict[str, Any]:
        """Establish a database connection."""
        if self.engine:
            try:
                with self.engine.connect() as conn:
                    return {"success": True, "message": "Connection established"}
            except Exception as e:
                return {"success": False, "error": str(e)}
        elif self._connection:
            try:
                self._connection.ping()
                return {"success": True, "message": "Connection established"}
            except Exception as e:
                return {"success": False, "error": str(e)}
        return {"success": False, "error": "Database not initialized"}

    def execute_query(
        self,
        query: str,
        params: Optional[Dict[str, Any]] = None,
        use_cache: bool = True,
        commit: bool = True
    ) -> QueryResult:
        """Execute a SQL query and return results."""
        if not self.engine and not self._connection:
            return QueryResult(success=False, error="Database not initialized")

        start_time = time.time()

        if self.manager.database_type == DatabaseType.REDIS:
            return self._execute_redis(query, params)

        if use_cache and self.cache:
            cached = self.cache.get(query, params)
            if cached:
                return QueryResult(
                    success=True,
                    rows=cached,
                    row_count=len(cached),
                    columns=cached[0].keys() if cached else [],
                    cached=True,
                    duration=time.time() - start_time
                )

        try:
            with self.engine.connect() as conn:
                result = conn.execute(text(query), params or {})

                if result.returns_rows:
                    rows = [dict(row._mapping) for row in result.fetchall()]
                    columns = list(result.keys())
                    row_count = len(rows)

                    if self.cache and use_cache:
                        self.cache.set(query, params, rows)

                    return QueryResult(
                        success=True,
                        rows=rows,
                        row_count=row_count,
                        columns=columns,
                        duration=time.time() - start_time
                    )
                else:
                    affected = result.rowcount
                    if commit:
                        conn.commit()

                    last_id = None
                    if result.last_inserted_ids:
                        last_id = result.last_inserted_ids[0]

                    return QueryResult(
                        success=True,
                        affected_rows=affected,
                        last_insert_id=last_id,
                        duration=time.time() - start_time
                    )

        except SQLAlchemyError as e:
            logger.error(f"Query execution failed: {e}")
            return QueryResult(success=False, error=str(e), duration=time.time() - start_time)

    def _execute_redis(self, command: str, params: Optional[Dict[str, Any]] = None) -> QueryResult:
        """Execute a Redis command."""
        start_time = time.time()

        try:
            if command.upper() == "GET":
                value = self._connection.get(params.get("key", ""))
                return QueryResult(
                    success=True,
                    rows=[{"key": params.get("key"), "value": value}] if value else [],
                    row_count=1 if value else 0
                )
            elif command.upper() == "SET":
                key = params.get("key", "")
                value = params.get("value", "")
                ex = params.get("ex")
                self._connection.set(key, value, ex=ex)
                return QueryResult(success=True, affected_rows=1)
            elif command.upper() == "DEL":
                keys = params.get("keys", [])
                count = self._connection.delete(*keys)
                return QueryResult(success=True, affected_rows=count)
            elif command.upper() == "KEYS":
                pattern = params.get("pattern", "*")
                keys = self._connection.keys(pattern)
                return QueryResult(
                    success=True,
                    rows=[{"key": k} for k in keys],
                    row_count=len(keys)
                )
            elif command.upper() == "EXPIRE":
                key = params.get("key", "")
                seconds = params.get("seconds", 0)
                result = self._connection.expire(key, seconds)
                return QueryResult(success=True, affected_rows=1 if result else 0)
            elif command.upper() == "TTL":
                key = params.get("key", "")
                ttl = self._connection.ttl(key)
                return QueryResult(success=True, rows=[{"key": key, "ttl": ttl}])
            else:
                result = self._connection.execute(command, **(params or {}))
                return QueryResult(success=True, rows=[{"result": result}])
        except Exception as e:
            return QueryResult(success=False, error=str(e), duration=time.time() - start_time)

    def execute_many(self, query: str, params_list: List[Dict[str, Any]], commit: bool = True) -> QueryResult:
        """Execute a query with multiple parameter sets."""
        if not self.engine:
            return QueryResult(success=False, error="Database not initialized")

        start_time = time.time()

        try:
            with self.engine.connect() as conn:
                result = conn.execute(text(query), params_list)

                if commit:
                    conn.commit()

                return QueryResult(
                    success=True,
                    affected_rows=result.rowcount * len(params_list),
                    duration=time.time() - start_time
                )
        except SQLAlchemyError as e:
            logger.error(f"Batch execution failed: {e}")
            return QueryResult(success=False, error=str(e), duration=time.time() - start_time)

    def get_tables(self, schema: Optional[str] = None) -> QueryResult:
        """Get list of tables in the database."""
        if not self.engine:
            return QueryResult(success=False, error="Database not initialized")

        start_time = time.time()

        try:
            inspector = inspect(self.engine)

            if self.manager.database_type == DatabaseType.POSTGRESQL:
                query = """
                    SELECT table_name, table_schema
                    FROM information_schema.tables
                    WHERE table_schema = :schema OR :schema IS NULL
                    ORDER BY table_schema, table_name
                """
                params = {"schema": schema or "public"}
            elif self.manager.database_type == DatabaseType.MYSQL:
                query = "SHOW TABLES"
                params = {}
            elif self.manager.database_type == DatabaseType.SQLITE:
                query = "SELECT name FROM sqlite_master WHERE type='table' ORDER BY name"
                params = {}
            else:
                return QueryResult(success=False, error="Unsupported database type for table listing")

            result = self.execute_query(query, params, use_cache=False)
            return result

        except Exception as e:
            return QueryResult(success=False, error=str(e), duration=time.time() - start_time)

    def get_table_info(self, table_name: str, schema: Optional[str] = None) -> QueryResult:
        """Get detailed information about a table."""
        if not self.engine:
            return QueryResult(success=False, error="Database not initialized")

        start_time = time.time()

        try:
            inspector = inspect(self.engine)

            columns = []
            for col in inspector.get_columns(table_name, schema=schema):
                columns.append({
                    "name": col["name"],
                    "type": str(col["type"]),
                    "nullable": col["nullable"],
                    "default": col.get("default")
                })

            primary_keys = inspector.get_pk_constraint(table_name, schema=schema)
            foreign_keys = inspector.get_foreign_keys(table_name, schema=schema)
            indexes = inspector.get_indexes(table_name, schema=schema)

            table_info = TableInfo(
                name=table_name,
                schema=schema,
                columns=columns,
                primary_key=primary_keys.get("constrained_columns", [None])[0] if primary_keys.get("constrained_columns") else None,
                foreign_keys=foreign_keys,
                indexes=indexes
            )

            return QueryResult(
                success=True,
                rows=[{
                    "name": table_info.name,
                    "schema": table_info.schema,
                    "columns": table_info.columns,
                    "primary_key": table_info.primary_key,
                    "foreign_keys": table_info.foreign_keys,
                    "indexes": table_info.indexes
                }]
            )

        except Exception as e:
            return QueryResult(success=False, error=str(e), duration=time.time() - start_time)

    def get_indexes(self, table_name: str, schema: Optional[str] = None) -> QueryResult:
        """Get indexes for a table."""
        if not self.engine:
            return QueryResult(success=False, error="Database not initialized")

        start_time = time.time()

        try:
            inspector = inspect(self.engine)
            indexes = inspector.get_indexes(table_name, schema=schema)

            index_list = []
            for idx in indexes:
                index_list.append({
                    "name": idx["name"],
                    "table": table_name,
                    "columns": idx["column_names"],
                    "unique": idx.get("unique", False),
                    "type": idx.get("type")
                })

            return QueryResult(
                success=True,
                rows=index_list,
                row_count=len(index_list)
            )

        except Exception as e:
            return QueryResult(success=False, error=str(e), duration=time.time() - start_time)

    def analyze_query(self, query: str) -> QueryResult:
        """Analyze a query execution plan."""
        if not self.engine:
            return QueryResult(success=False, error="Database not initialized")

        start_time = time.time()

        try:
            if self.manager.database_type == DatabaseType.POSTGRESQL:
                explain_query = f"EXPLAIN ANALYZE {query}"
            elif self.manager.database_type == DatabaseType.MYSQL:
                explain_query = f"EXPLAIN {query}"
            elif self.manager.database_type == DatabaseType.SQLITE:
                explain_query = f"EXPLAIN QUERY PLAN {query}"
            else:
                return QueryResult(success=False, error="Unsupported database type for query analysis")

            return self.execute_query(explain_query, use_cache=False)

        except Exception as e:
            return QueryResult(success=False, error=str(e), duration=time.time() - start_time)

    def create_table(self, table_name: str, columns: List[Dict[str, Any]], primary_key: Optional[str] = None, if_not_exists: bool = True) -> QueryResult:
        """Create a new table."""
        if not self.engine:
            return QueryResult(success=False, error="Database not initialized")

        start_time = time.time()

        try:
            # 验证表名（防止SQL注入）
            if not self._is_valid_identifier(table_name):
                return QueryResult(success=False, error=f"Invalid table name: {table_name}")

            from sqlalchemy import Table, Column, MetaData
            from sqlalchemy.sql import ddl

            metadata = MetaData()

            # 构建列定义
            sa_columns = []
            for col in columns:
                col_name = col.get("name")
                if not col_name or not self._is_valid_identifier(col_name):
                    return QueryResult(success=False, error=f"Invalid column name: {col_name}")

                col_type_str = col.get("type", "VARCHAR(255)")
                nullable = col.get("nullable", True)
                default = col.get("default")

                # 映射类型字符串到SQLAlchemy类型
                sa_type = self._parse_column_type(col_type_str)

                sa_col = Column(
                    col_name,
                    sa_type,
                    nullable=nullable,
                    default=default
                )
                sa_columns.append(sa_col)

            # 创建表对象
            table = Table(
                table_name,
                metadata,
                *sa_columns,
                if_not_exists=if_not_exists
            )

            # 生成并执行DDL
            with self.engine.connect() as conn:
                for stmt in ddl.CreateTable(table).compile(self.engine).string.split(';'):
                    if stmt.strip():
                        conn.execute(text(stmt.strip()))
                conn.commit()

            return QueryResult(success=True, duration=time.time() - start_time)

        except Exception as e:
            logger.error(f"Create table failed: {e}")
            return QueryResult(success=False, error=str(e), duration=time.time() - start_time)

    def _is_valid_identifier(self, name: str) -> bool:
        """验证标识符是否合法（防止SQL注入）"""
        if not name or not isinstance(name, str):
            return False
        # 只允许字母、数字、下划线，且不能以数字开头
        import re
        return bool(re.match(r'^[a-zA-Z_][a-zA-Z0-9_]*$', name))

    def _parse_column_type(self, type_str: str):
        """解析列类型字符串为SQLAlchemy类型"""
        from sqlalchemy import Integer, String, Float, Boolean, DateTime, Text, Numeric

        type_mapping = {
            'INT': Integer,
            'INTEGER': Integer,
            'BIGINT': Integer,
            'VARCHAR': String,
            'TEXT': Text,
            'FLOAT': Float,
            'REAL': Float,
            'DOUBLE': Float,
            'BOOLEAN': Boolean,
            'BOOL': Boolean,
            'DATETIME': DateTime,
            'TIMESTAMP': DateTime,
            'NUMERIC': Numeric,
            'DECIMAL': Numeric,
        }

        # 提取基本类型（去掉括号内的长度参数）
        base_type = type_str.upper().split('(')[0].strip()

        if base_type in type_mapping:
            # 处理带长度的类型，如 VARCHAR(255)
            if '(' in type_str:
                try:
                    length = int(type_str[type_str.index('(')+1:type_str.index(')')])
                    if base_type in ['VARCHAR']:
                        return type_mapping[base_type](length)
                except (ValueError, IndexError):
                    pass
            return type_mapping[base_type]()

        # 默认返回String
        return String(255)

    def drop_table(self, table_name: str, if_exists: bool = True, cascade: bool = False) -> QueryResult:
        """Drop a table."""
        if not self.engine:
            return QueryResult(success=False, error="Database not initialized")

        start_time = time.time()

        try:
            # 验证表名（防止SQL注入）
            if not self._is_valid_identifier(table_name):
                return QueryResult(success=False, error=f"Invalid table name: {table_name}")

            from sqlalchemy import Table, MetaData, DDL

            metadata = MetaData()
            table = Table(table_name, metadata)

            # 使用SQLAlchemy的DDL构造器
            if if_exists:
                drop_sql = f"DROP TABLE IF EXISTS {table_name}"
            else:
                drop_sql = f"DROP TABLE {table_name}"

            if cascade and self.manager.database_type == DatabaseType.POSTGRESQL:
                drop_sql += " CASCADE"

            with self.engine.connect() as conn:
                conn.execute(text(drop_sql))
                conn.commit()

            return QueryResult(success=True, duration=time.time() - start_time)

        except Exception as e:
            logger.error(f"Drop table failed: {e}")
            return QueryResult(success=False, error=str(e), duration=time.time() - start_time)

    def truncate_table(self, table_name: str, restart_identity: bool = False) -> QueryResult:
        """Truncate a table."""
        if not self.engine:
            return QueryResult(success=False, error="Database not initialized")

        start_time = time.time()

        try:
            # 验证表名（防止SQL注入）
            if not self._is_valid_identifier(table_name):
                return QueryResult(success=False, error=f"Invalid table name: {table_name}")

            if self.manager.database_type == DatabaseType.POSTGRESQL:
                truncate_sql = f"TRUNCATE TABLE {table_name}"
                if restart_identity:
                    truncate_sql += " RESTART IDENTITY CASCADE"
            elif self.manager.database_type == DatabaseType.MYSQL:
                truncate_sql = f"TRUNCATE TABLE {table_name}"
            elif self.manager.database_type == DatabaseType.SQLITE:
                truncate_sql = f"DELETE FROM {table_name}"
            else:
                return QueryResult(success=False, error="Unsupported database type for truncate")

            with self.engine.connect() as conn:
                conn.execute(text(truncate_sql))
                conn.commit()

            return QueryResult(success=True, duration=time.time() - start_time)

        except Exception as e:
            logger.error(f"Truncate table failed: {e}")
            return QueryResult(success=False, error=str(e), duration=time.time() - start_time)

    def backup_table(self, table_name: str, backup_name: Optional[str] = None) -> QueryResult:
        """Create a backup copy of a table."""
        if not self.engine:
            return QueryResult(success=False, error="Database not initialized")

        start_time = time.time()

        try:
            # 验证表名（防止SQL注入）
            if not self._is_valid_identifier(table_name):
                return QueryResult(success=False, error=f"Invalid table name: {table_name}")

            backup_name = backup_name or f"{table_name}_backup_{datetime.now().strftime('%Y%m%d%H%M%S')}"

            # 验证备份表名
            if not self._is_valid_identifier(backup_name):
                return QueryResult(success=False, error=f"Invalid backup table name: {backup_name}")

            backup_sql = f"CREATE TABLE {backup_name} AS SELECT * FROM {table_name}"

            with self.engine.connect() as conn:
                conn.execute(text(backup_sql))
                conn.commit()

            return QueryResult(
                success=True,
                rows=[{"original_table": table_name, "backup_table": backup_name}],
                duration=time.time() - start_time
            )

        except Exception as e:
            logger.error(f"Backup table failed: {e}")
            return QueryResult(success=False, error=str(e), duration=time.time() - start_time)

    def begin_transaction(self, isolation_level: Optional[str] = None) -> Dict[str, Any]:
        """Begin a new transaction."""
        if not self.engine:
            return {"success": False, "error": "Database not initialized"}

        try:
            connection = self.engine.connect()
            transaction = connection.begin()

            return {
                "success": True,
                "connection": connection,
                "transaction": transaction,
                "isolation_level": isolation_level or self.manager.isolation_level
            }
        except Exception as e:
            return {"success": False, "error": str(e)}

    def commit_transaction(self, transaction) -> Dict[str, Any]:
        """Commit a transaction."""
        try:
            transaction.commit()
            return {"success": True}
        except Exception as e:
            return {"success": False, "error": str(e)}

    def rollback_transaction(self, transaction) -> Dict[str, Any]:
        """Rollback a transaction."""
        try:
            transaction.rollback()
            return {"success": True}
        except Exception as e:
            return {"success": False, "error": str(e)}

    def invalidate_cache(self, pattern: Optional[str] = None) -> Dict[str, Any]:
        """Invalidate query cache."""
        if not self.cache:
            return {"success": False, "error": "Cache not enabled"}

        try:
            count = self.cache.invalidate(pattern)
            return {"success": True, "invalidated": count}
        except Exception as e:
            return {"success": False, "error": str(e)}

    def health_check(self) -> Dict[str, Any]:
        """Check database connection health."""
        if not self.engine and not self._connection:
            return {"success": False, "error": "Database not initialized"}

        try:
            start = time.time()
            if self._connection:
                self._connection.ping()
                return {"success": True, "latency_ms": (time.time() - start) * 1000}
            else:
                with self.engine.connect() as conn:
                    conn.execute(text("SELECT 1"))
                return {"success": True, "latency_ms": (time.time() - start) * 1000}
        except Exception as e:
            return {"success": False, "error": str(e)}

    def close(self) -> Dict[str, Any]:
        """Close database connection."""
        try:
            if self._connection:
                self._connection.close()
                self._connection = None

            if self.engine:
                self.engine.dispose()
                self.engine = None

            self.session_factory = None

            logger.info("Database connection closed")
            return {"success": True}
        except Exception as e:
            return {"success": False, "error": str(e)}

    def __enter__(self):
        """Context manager entry."""
        self.initialize()
        return self

    def __exit__(self, exc_type, exc_val, exc_tb):
        """Context manager exit."""
        self.close()


def create_skill(manager: Optional[Dict[str, Any]] = None) -> DatabaseSkill:
    """Factory function to create a DatabaseSkill instance."""
    return DatabaseSkill(manager)
