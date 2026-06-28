# Copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
# Dockerfile for AgentRT — Multi-Daemon Runtime Image
# P1.17: docker-compose 集成测试环境搭建
# Version: 0.1.1
#
# 多阶段构建，支持所有 16 个 daemon 的统一运行时镜像。
# 通过 AGENTOS_SERVICE_NAME 环境变量选择启动的 daemon。

# ==========================================================================
# 阶段1: 构建阶段 — 编译所有 daemon 二进制文件
# ==========================================================================
FROM alpine:3.19 AS builder

LABEL maintainer="SPHARX Team <devops@spharx.cn>"
LABEL description="AgentRT Multi-Daemon Build Stage"

ARG BUILD_VERSION=0.1.1
ARG BUILD_DATE

RUN apk add --no-cache \
    build-base \
    cmake \
    ninja \
    pkgconfig \
    libmicrohttpd-dev \
    libwebsockets-dev \
    openssl-dev \
    cjson-dev \
    git \
    curl-dev \
    yaml-dev \
    libuv-dev \
    protobuf-c-dev \
    sqlite-dev \
    libevent-dev \
    linux-headers

WORKDIR /build

# 复制完整项目源码
COPY . .

# 创建构建目录并配置 CMake
RUN mkdir -p build && cd build && \
    cmake .. -G "Ninja" \
    -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_TESTS=OFF \
    -DCMAKE_INSTALL_PREFIX=/usr \
    -DVERSION=${BUILD_VERSION} \
    -DENABLE_MCP=ON \
    -DENABLE_A2A=ON \
    -DENABLE_OPENAI=ON

# 编译所有 daemon
RUN cd build && ninja -j$(nproc)

# 安装到临时目录
RUN cd build && DESTDIR=/install ninja install

# ==========================================================================
# 阶段2: 运行时镜像 — 所有 daemon 共享
# ==========================================================================
FROM alpine:3.19 AS runtime

LABEL maintainer="SPHARX Team <devops@spharx.cn>"
LABEL description="AgentRT Multi-Daemon Runtime"
LABEL version="${BUILD_VERSION}"
LABEL org.opencontainers.image.source="https://github.com/spharx/agentos"
LABEL org.opencontainers.image.title="AgentRT Runtime"
LABEL org.opencontainers.image.vendor="SPHARX"

# 默认环境变量
ENV AGENTOS_VERSION=${BUILD_VERSION} \
    AGENTOS_LOG_LEVEL=INFO \
    AGENTOS_TEST_MODE=0 \
    AGENTOS_CONFIG_PATH=/etc/agentrt/agentos.yaml \
    AGENTOS_DEPLOY_ENV=production

RUN apk add --no-cache \
    libmicrohttpd \
    libwebsockets \
    openssl \
    cjson \
    ca-certificates \
    tzdata \
    curl \
    wget \
    yaml \
    libuv \
    libstdc++ \
    protobuf-c \
    bash && \
    addgroup -g 1000 agentos && \
    adduser -u 1000 -G agentos -s /bin/sh -D agentos && \
    mkdir -p /var/log/agentos /var/run/agentos /etc/agentrt && \
    chown -R agentos:agentos /var/log/agentos /var/run/agentos /etc/agentrt

# 从构建阶段复制安装文件
COPY --from=builder /install/ /

# 复制多 daemon 启动入口脚本
COPY deploy/docker/entrypoint.sh /usr/local/bin/agentrt-entrypoint.sh
RUN chmod +x /usr/local/bin/agentrt-entrypoint.sh

USER agentos
WORKDIR /home/agentos

HEALTHCHECK --interval=15s --timeout=5s --start-period=10s --retries=3 \
    CMD wget --no-verbose --tries=1 --spider http://localhost:${AGENTOS_SERVICE_PORT:-8080}/healthz || exit 1

EXPOSE 8080 8081 9090

ENTRYPOINT ["/usr/local/bin/agentrt-entrypoint.sh"]
CMD []