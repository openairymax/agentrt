# Docker 容器化部署（已迁移）

> **此目录已弃用。** Docker 容器化部署配置已统一迁移至 [`deploy/docker/`](../../../deploy/docker/)。

原 `scripts/ops/deploy/` 下的双 Dockerfile（`Dockerfile.kernel` + `Dockerfile.service`）架构已合并为
`deploy/docker/Dockerfile`（基于 Alpine 3.19 的多阶段、多 daemon 统一运行时镜像）。

迁移对照：

| 原文件（已删除） | 迁移目标 |
|------------------|----------|
| `Dockerfile.kernel` / `Dockerfile.service` | `deploy/docker/Dockerfile` |
| `docker-compose.yml` / `docker-compose.prod.yml` | `deploy/docker/docker-compose.yml` / `docker-compose.prod.yml` |
| `docker-compose.staging.yml` | `deploy/docker/docker-compose.staging.yml` |
| `docker-compose.preview.yml` | `deploy/docker/docker-compose.preview.yml` |
| `secrets/` | `deploy/docker/secrets/` |
| `build.sh` / `quickstart.sh` / `check_config.sh` / `Makefile` | 已移除（统一使用 `deploy/docker/` 下的 docker-compose 编排） |

详细使用说明请参阅 [`deploy/docker/README.md`](../../../deploy/docker/README.md)。

---

© 2026 SPHARX Ltd. All Rights Reserved.
