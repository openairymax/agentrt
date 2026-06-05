# Gateway Docker

AgentOS Gateway 模块的 Docker 容器化部署配置。

## 文件

| 文件 | 说明 |
|------|------|
| `Dockerfile` | Gateway 镜像构建文件 |
| `docker-compose.yml` | 基础服务编排 |
| `docker-compose.dev.yml` | 开发环境编排 |
| `docker-compose.prod.yml` | 生产环境编排 |
| `.env.example` | 环境变量模板 |
| `monitoring/` | 监控配置（Prometheus + Grafana） |

## 使用

```bash
# 开发环境
docker-compose -f docker-compose.yml -f docker-compose.dev.yml up -d

# 生产环境
docker-compose -f docker-compose.yml -f docker-compose.prod.yml up -d
```

## 版本

v0.1.0