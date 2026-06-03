# E-Commerce — 智能电商助手应用

**模块路径**: `agentos/openlab/app/ecommerce/`
**版本**: v0.1.0

> **Status**: 本模块作为 AgentOS v0.1.0 的正式组成部分，API 已稳定。本模块通过 JSON-RPC 2.0 协议与 AgentOS 核心运行时集成。

## 概述

E-Commerce 是基于 AgentOS 平台的智能电商助手应用，帮助商家管理商品、处理订单和优化运营。集成 Stripe 支付网关、JWT 认证、Redis 缓存等企业级组件，支持多数据库后端（SQLite/PostgreSQL/MySQL），提供完整的电商运营解决方案。

## 目录结构

```
ecommerce/
├── src/
│   ├── main.py                 # 应用入口
│   └── utils.py                # 工具函数
├── config.yaml                 # 应用配置（含 Stripe/JWT/Redis/安全策略）
├── manifest.json               # 应用清单
├── run.sh                      # 启动脚本
└── README.md                   # 本文件
```

## 核心能力

- **商品管理**：自动生成商品描述、分类和标签，支持多币种
- **订单处理**：智能订单分配、状态追踪和异常处理
- **支付集成**：Stripe 支付网关，支持 Webhook 事件处理
- **客户服务**：自动回复常见问题，智能推荐商品
- **数据分析**：销售数据分析、趋势预测和库存优化
- **安全认证**：JWT Token 认证，bcrypt 密码加密，速率限制

## 配置说明

`config.yaml` 主要配置项：

| 配置项 | 说明 | 默认值 |
|--------|------|--------|
| `database_url` | 数据库连接 | `sqlite:///./ecommerce.db` |
| `redis_url` | Redis 连接 | `redis://localhost:6379` |
| `jwt_secret_key` | JWT 密钥 | 需在生产环境更改 |
| `jwt_access_token_expire_minutes` | Token 过期时间 | 30 |
| `stripe_secret_key` | Stripe 密钥 | 测试密钥 |
| `stripe_currency` | 支付币种 | `usd` |
| `host` | 服务地址 | `0.0.0.0` |
| `port` | 服务端口 | 8000 |
| `security.bcrypt_rounds` | 加密轮数 | 12 |
| `security.rate_limit_requests` | 速率限制 | 100/60s |
| `products.default_currency` | 默认币种 | USD |
| `products.default_tax_rate` | 默认税率 | 0.08 |
| `uploads.max_file_size` | 上传限制 | 10MB |

## 接口说明

### 商品管理 API

| 接口 | 方法 | 说明 |
|------|------|------|
| `/api/products` | GET | 获取商品列表 |
| `/api/products` | POST | 创建商品 |
| `/api/products/{id}` | GET | 获取商品详情 |
| `/api/products/{id}` | PUT | 更新商品 |
| `/api/products/{id}` | DELETE | 删除商品 |

### 订单管理 API

| 接口 | 方法 | 说明 |
|------|------|------|
| `/api/orders` | GET | 获取订单列表 |
| `/api/orders` | POST | 创建订单 |
| `/api/orders/{id}` | GET | 获取订单详情 |
| `/api/orders/{id}/status` | PUT | 更新订单状态 |

### 支付 API

| 接口 | 方法 | 说明 |
|------|------|------|
| `/api/payments/create` | POST | 创建支付意图 |
| `/api/payments/webhook` | POST | Stripe Webhook 回调 |
| `/api/payments/confirm` | POST | 确认支付 |

## 依赖关系

- **核心依赖**: FastAPI, SQLAlchemy, Pydantic, PyYAML, python-jose, passlib, bcrypt
- **支付依赖**: stripe
- **缓存依赖**: redis
- **数据库驱动**: psycopg2-binary (PostgreSQL), pymysql (MySQL)
- **文件处理**: aiofiles, python-multipart

## 使用示例

```python
from ecommerce.src.main import ECommerceApp

app = ECommerceApp(config_path="config.yaml")

product = app.create_product(
    name="智能音箱",
    category="电子产品",
    price=299.00,
    description="AI 智能音箱，支持语音控制"
)

order = app.process_order(
    order_id="ORD-202401-001",
    customer_id="CUST-001",
    items=[{"product_id": product.id, "quantity": 2}]
)
```

---

© 2026 SPHARX Ltd. All Rights Reserved.
