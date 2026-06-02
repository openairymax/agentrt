# Database Skill — 数据库操作技能

**模块路径**: `agentos/openlab/contrib/skills/database_skill/`
**版本**: v0.1.0

> **Status**: 本模块作为 AgentOS v0.1.0 的正式组成部分，API 已稳定。本模块通过 JSON-RPC 2.0 协议与 AgentOS 核心运行时集成。

## 概述

Database Skill 为 AgentOS 智能体提供数据库操作能力，支持多种关系型数据库的连接查询和数据管理。内置 SQL 注入防护机制，提供安全的查询执行环境。支持模式探索、数据导出等高级功能，适用于数据分析、报表生成和数据库管理等场景。

## 目录结构

```
database_skill/
└── README.md                   # 本文件
```

## 核心能力

- **多库支持**：支持 MySQL、PostgreSQL、SQLite 等关系型数据库
- **安全查询**：参数化查询执行，防止 SQL 注入攻击
- **模式探索**：自动发现数据库表结构、列信息和关系
- **数据导出**：查询结果导出为 JSON、CSV 等格式
- **连接池管理**：自动管理数据库连接池，优化连接复用
- **事务支持**：支持事务操作，确保数据一致性

## 接口说明

### 操作接口

| 操作 | 说明 | 参数 |
|------|------|------|
| `db.connect` | 连接数据库 | db_type, host, port, database, user, password |
| `db.query` | 执行 SQL 查询（SELECT） | sql, params |
| `db.execute` | 执行写操作（INSERT/UPDATE/DELETE） | sql, params |
| `db.tables` | 获取表列表 | schema |
| `db.schema` | 获取表结构 | table_name |
| `db.columns` | 获取列信息 | table_name |
| `db.indexes` | 获取索引信息 | table_name |
| `db.export` | 导出查询结果 | sql, format (json/csv) |
| `db.transaction` | 执行事务 | operations |
| `db.disconnect` | 断开数据库连接 | — |

### 连接参数

| 参数 | 说明 | 默认值 |
|------|------|--------|
| `db_type` | 数据库类型 | `postgresql` |
| `host` | 主机地址 | `localhost` |
| `port` | 端口号 | 5432 (PG) / 3306 (MySQL) |
| `database` | 数据库名 | 必填 |
| `user` | 用户名 | 必填 |
| `password` | 密码 | 必填 |
| `pool_size` | 连接池大小 | 5 |
| `max_overflow` | 最大溢出连接 | 10 |
| `echo` | SQL 日志 | `false` |

## 依赖关系

- **核心依赖**: AgentOS OpenLab Core, SQLAlchemy
- **数据库驱动**: psycopg2 (PostgreSQL), pymysql (MySQL), sqlite3 (SQLite, 内置)
- **安装**: `pip install -e ".[ecommerce]"`

## 使用示例

```python
from contrib.skills.database_skill import DatabaseSkill

db = DatabaseSkill()

await db.connect(
    db_type="postgresql",
    host="localhost",
    port=5432,
    database="agentos",
    user="admin"
)

results = await db.query("SELECT * FROM users WHERE status = :status", {"status": "active"})

schema = await db.schema("users")

await db.export("SELECT * FROM orders", format="csv", path="orders.csv")

await db.disconnect()
```

---

© 2026 SPHARX Ltd. All Rights Reserved.
