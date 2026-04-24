# Database Skill — 数据库技能

`openlab/contrib/skills/database_skill/` 提供智能体的数据库操作能力，支持多种数据库的连接查询和数据管理。

## 核心能力

- **多库支持**：支持 MySQL、PostgreSQL、SQLite 等关系型数据库
- **查询执行**：安全执行 SQL 查询，防止注入攻击
- **模式探索**：自动发现数据库表结构和关系
- **数据导出**：查询结果导出为 JSON、CSV 等格式

## 支持操作

| 操作 | 说明 |
|------|------|
| `db.connect` | 连接数据库 |
| `db.query` | 执行 SQL 查询 |
| `db.tables` | 获取表列表 |
| `db.schema` | 获取表结构 |
| `db.execute` | 执行写操作（INSERT/UPDATE/DELETE） |
| `db.disconnect` | 断开数据库连接 |

## 使用方式

```python
from contrib.skills.database_skill import DatabaseSkill

db = DatabaseSkill()

# 连接数据库
await db.connect(
    db_type="postgresql",
    host="localhost",
    port=5432,
    database="agentos",
    user="admin"
)

# 查询数据
results = await db.query("SELECT * FROM users WHERE status = 'active'")

# 获取表结构
schema = await db.schema("users")

# 断开连接
await db.disconnect()
```

---

*AgentOS OpenLab — Database Skill*
