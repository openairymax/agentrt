# Database Skill

Database operations skill for SQL query execution, schema management, and data manipulation.

## Features

- Multi-database support (PostgreSQL, MySQL, SQLite, Redis)
- Connection pooling for efficient resource management
- SQL query execution with parameterized queries
- Schema introspection and management
- Transaction support with commit/rollback
- Query result caching with Redis
- Index analysis and optimization suggestions
- Table backup functionality

## Installation

```bash
pip install sqlalchemy psycopg2-binary pymysql redis
```

## Usage

```python
from database_skill import DatabaseSkill, DatabaseType

# Create and initialize database connection
db = DatabaseSkill({
    "database_type": "postgresql",
    "connection_string": "postgresql://user:pass@localhost:5432/mydb",
    "pool_size": 5
})
result = db.initialize()

# Execute queries
result = db.execute_query("SELECT * FROM users WHERE age > :age", {"age": 18})
print(result.rows)

# Get table info
result = db.get_table_info("users")
print(result.rows[0]["columns"])

# Create table
db.create_table("products", [
    {"name": "id", "type": "SERIAL PRIMARY KEY"},
    {"name": "name", "type": "VARCHAR(255)", "nullable": False},
    {"name": "price", "type": "DECIMAL(10,2)"}
])

# Use transaction
tx = db.begin_transaction()
try:
    db.execute_query("INSERT INTO orders (user_id, total) VALUES (:uid, :total)", {"uid": 1, "total": 99.99})
    db.commit_transaction(tx)
except:
    db.rollback_transaction(tx)

# Close connection
db.close()
```

## Configuration

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| database_type | string | "postgresql" | Database type: postgresql, mysql, sqlite, redis |
| connection_string | string | "" | Database connection URL |
| pool_size | integer | 5 | Connection pool size |
| max_overflow | integer | 10 | Max pool overflow |
| pool_timeout | integer | 30 | Pool timeout in seconds |
| echo | boolean | false | Echo SQL queries |
| cache_enabled | boolean | true | Enable query caching |
| cache_ttl | integer | 300 | Cache TTL in seconds |

## API Reference

### Methods

- `initialize()` - Initialize database connection
- `connect()` - Test connection
- `execute_query(query, params)` - Execute SQL query
- `execute_many(query, params_list)` - Execute batch queries
- `get_tables()` - List all tables
- `get_table_info(table_name)` - Get table schema info
- `get_indexes(table_name)` - Get table indexes
- `analyze_query(query)` - Analyze query execution plan
- `create_table(table_name, columns)` - Create new table
- `drop_table(table_name)` - Drop table
- `truncate_table(table_name)` - Truncate table
- `backup_table(table_name)` - Backup table
- `begin_transaction()` - Begin transaction
- `commit_transaction(tx)` - Commit transaction
- `rollback_transaction(tx)` - Rollback transaction
- `invalidate_cache()` - Clear query cache
- `health_check()` - Check connection health
- `close()` - Close connection
