# Copyright (c) 2026 SPHARX Ltd. All Rights Reserved. "From data intelligence emerges".
# AgentOS Docker Makefile
# 提供便捷的 Docker 操作命令

.PHONY: help build-kernel build-service build-all clean stop start restart logs shell-kernel shell-service health ps

# 变量定义
VERSION := 1.0.0.5
KERNEL_IMAGE := spharx/agentos-kernel
SERVICE_IMAGE := spharx/agentos-services
DOCKER_COMPOSE := docker-compose

# 默认目标
help:
	@echo "AgentOS Docker Makefile - 可用命令:"
	@echo ""
	@echo "构建相关:"
	@echo "  make build-kernel       - 构建内核镜像（生产版）"
	@echo "  make build-kernel-dev   - 构建内核镜像（开发版）"
	@echo "  make build-service      - 构建服务镜像（生产版）"
	@echo "  make build-service-dev  - 构建服务镜像（开发版）"
	@echo "  make build-all          - 构建所有镜像（生产版）"
	@echo "  make build-all-dev      - 构建所有镜像（开发版）"
	@echo ""
	@echo "运行相关:"
	@echo "  make start              - 启动所有服务"
	@echo "  make stop               - 停止所有服务"
	@echo "  make restart            - 重启所有服务"
	@echo "  make down               - 停止并删除容器"
	@echo "  make clean              - 清理所有容器、镜像和数据卷"
	@echo ""
	@echo "监控相关:"
	@echo "  make logs               - 查看所有日志"
	@echo "  make logs-kernel        - 查看内核日志"
	@echo "  make logs-service       - 查看服务日志"
	@echo "  make ps                 - 查看服务状态"
	@echo "  make health             - 检查服务健康状态"
	@echo ""
	@echo "调试相关:"
	@echo "  make shell-kernel       - 进入内核容器"
	@echo "  make shell-service      - 进入服务容器"
	@echo "  make inspect-kernel     - 检查内核容器详情"
	@echo "  make inspect-service    - 检查服务容器详情"
	@echo ""
	@echo "测试相关:"
	@echo "  make test               - 运行集成测试"
	@echo "  make benchmark          - 运行性能基准测试"
	@echo ""

# ==================== 构建相关 ====================

build-kernel:
	@echo "构建内核镜像（生产版）..."
	docker build \
		-f Dockerfile.kernel \
		-t $(KERNEL_IMAGE):$(VERSION) \
		-t $(KERNEL_IMAGE):latest \
		--target runtime \
		../../

build-kernel-dev:
	@echo "构建内核镜像（开发版）..."
	docker build \
		-f Dockerfile.kernel \
		-t $(KERNEL_IMAGE):$(VERSION)-dev \
		-t $(KERNEL_IMAGE):dev \
		--target builder \
		../../

build-service:
	@echo "构建服务镜像（生产版）..."
	docker build \
		-f Dockerfile.service \
		-t $(SERVICE_IMAGE):$(VERSION) \
		-t $(SERVICE_IMAGE):latest \
		../../

build-service-dev:
	@echo "构建服务镜像（开发版）..."
	docker build \
		-f Dockerfile.service \
		-t $(SERVICE_IMAGE):$(VERSION)-dev \
		-t $(SERVICE_IMAGE):dev \
		../../

build-all: build-kernel build-service
	@echo "所有镜像构建完成！"
	docker images | grep agentos

build-all-dev: build-kernel-dev build-service-dev
	@echo "所有开发镜像构建完成！"
	docker images | grep agentos

# ==================== 运行相关 ====================

start:
	@echo "启动所有服务..."
	$(DOCKER_COMPOSE) up -d
	@echo "使用 'make ps' 查看服务状态"

stop:
	@echo "停止所有服务..."
	$(DOCKER_COMPOSE) down

restart: stop start
	@echo "服务已重启"

down:
	@echo "停止并删除容器..."
	$(DOCKER_COMPOSE) down

clean:
	@echo "清理所有容器、镜像和数据卷..."
	$(DOCKER_COMPOSE) down -v
	docker image rm -f $(KERNEL_IMAGE):$(VERSION) || true
	docker image rm -f $(KERNEL_IMAGE):latest || true
	docker image rm -f $(KERNEL_IMAGE):$(VERSION)-dev || true
	docker image rm -f $(KERNEL_IMAGE):dev || true
	docker image rm -f $(SERVICE_IMAGE):$(VERSION) || true
	docker image rm -f $(SERVICE_IMAGE):latest || true
	docker image rm -f $(SERVICE_IMAGE):$(VERSION)-dev || true
	docker image rm -f $(SERVICE_IMAGE):dev || true
	docker builder prune -f
	docker image prune -f
	@echo "清理完成"

# ==================== 监控相关 ====================

logs:
	$(DOCKER_COMPOSE) logs -f

logs-kernel:
	$(DOCKER_COMPOSE) logs -f agentos-kernel

logs-service:
	$(DOCKER_COMPOSE) logs -f agentos-services

ps:
	$(DOCKER_COMPOSE) ps

health:
	@echo "检查服务健康状态..."
	@echo "内核服务:"
	docker inspect --format='{{.State.Health.Status}}' agentos-kernel || echo "未运行"
	@echo "服务层:"
	docker inspect --format='{{.State.Health.Status}}' agentos-services || echo "未运行"
	@echo "Redis:"
	docker inspect --format='{{.State.Health.Status}}' agentos-redis || echo "未运行"
	@echo "PostgreSQL:"
	docker inspect --format='{{.State.Health.Status}}' agentos-postgres || echo "未运行"

# ==================== 调试相关 ====================

shell-kernel:
	@echo "进入内核容器..."
	$(DOCKER_COMPOSE) exec agentos-kernel /bin/bash

shell-service:
	@echo "进入服务容器..."
	$(DOCKER_COMPOSE) exec agentos-services /bin/bash

inspect-kernel:
	@echo "内核容器详情:"
	docker inspect agentos-kernel | jq '.[0].State'

inspect-service:
	@echo "服务容器详情:"
	docker inspect agentos-services | jq '.[0].State'

# ==================== 测试相关 ====================

test:
	@echo "运行集成测试..."
	$(DOCKER_COMPOSE) exec agentos-kernel ctest --output-on-failure

benchmark:
	@echo "运行性能基准测试..."
	$(DOCKER_COMPOSE) exec agentos-kernel python3 /opt/agentos/benchmarks/run_benchmark.py

# ==================== 快捷命令 ====================

# 重新构建并重启
rebuild: build-all restart
	@echo "重新构建并重启完成"

# 查看资源使用情况
stats:
	docker stats agentos-kernel agentos-services

# 备份数据
backup:
	@echo "备份数据卷..."
	docker run --rm \
		-v $$(docker volume ls -q | grep redis):/data/redis \
		-v $$(docker volume ls -q | grep postgres):/data/postgres \
		-v $$(pwd)/backup:/backup \
		alpine tar czf /backup/data-backup-$$(date +%Y%m%d-%H%M%S).tar.gz /data

# 恢复数据
restore:
	@echo "请手动指定备份文件进行恢复"
	@echo "示例：docker run --rm -v backup_volume:/data -v ./backup:/backup alpine tar xzf /backup/data-backup.tar.gz -C /data"
