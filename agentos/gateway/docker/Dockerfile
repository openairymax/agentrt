# Copyright (c) 2026 SPHARX. All Rights Reserved.
# Dockerfile for AgentOS gateway Module
# Version: 1.0.0
#
# 多阶段构建，优化镜像大小和安全性

# ==========================================================================
# 阶段1: 构建阶段
# ==========================================================================
FROM alpine:3.19 AS builder

LABEL maintainer="SPHARX <dev@spharx.cn>"
LABEL description="AgentOS gateway Module Build Stage"

# 设置构建参数
ARG BUILD_VERSION=1.0.0
ARG BUILD_DATE
ARG TARGETPLATFORM

# 安装构建依赖
RUN apk add --no-cache \
    build-base \
    cmake \
    ninja \
    pkgconfig \
    libmicrohttpd-dev \
    libwebsockets-dev \
    openssl-dev \
    cjson-dev \
    git

# 设置工作目录
WORKDIR /build

# 复制源代码
COPY CMakeLists.txt .
COPY src/ ./src/
COPY include/ ./include/

# 构建配置
RUN mkdir -p build && cd build && \
    cmake .. -G "Ninja" \
    -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_TESTS=OFF \
    -DCMAKE_INSTALL_PREFIX=/usr \
    -DVERSION=${BUILD_VERSION}

# 执行构建
RUN cd build && ninja -j$(nproc)

# 安装到临时目录
RUN cd build && DESTDIR=/install ninja install

# ==========================================================================
# 阶段2: 运行时镜像
# ==========================================================================
FROM alpine:3.19 AS runtime

LABEL maintainer="SPHARX <dev@spharx.cn>"
LABEL description="AgentOS gateway Module Runtime"
LABEL version="${BUILD_VERSION}"
LABEL build-date="${BUILD_DATE}"
LABEL org.opencontainers.image.source="https://github.com/spharx/agentos"
LABEL org.opencontainers.image.title="AgentOS gateway Module"
LABEL org.opencontainers.image.description="AgentOS Gateway Layer"
LABEL org.opencontainers.image.vendor="SPHARX"

# 设置环境变量
ENV AGENTOS_MODULE=gateway \
    AGENTOS_VERSION=${BUILD_VERSION} \
    AGENTOS_LOG_LEVEL=INFO \
    AGENTOS_HTTP_PORT=8080 \
    AGENTOS_WS_PORT=8081

# 安装运行时依赖
RUN apk add --no-cache \
    libmicrohttpd \
    libwebsockets \
    openssl \
    cjson \
    ca-certificates \
    tzdata && \
    # 创建非root用户
    addgroup -g 1000 agentos && \
    adduser -u 1000 -G agentos -s /bin/sh -D agentos && \
    # 创建必要的目录
    mkdir -p /var/log/agentos /var/run/agentos && \
    chown -R agentos:agentos /var/log/agentos /var/run/agentos

# 从构建阶段复制产物
COPY --from=builder /install/ /

# 复制配置文件
COPY manager/ /etc/agentos/gateway/
RUN chown -R agentos:agentos /etc/agentos

# 切换到非root用户
USER agentos

# 设置工作目录
WORKDIR /home/agentos

# 健康检查
HEALTHCHECK --interval=30s --timeout=10s --start-period=5s --retries=3 \
    CMD wget --no-verbose --tries=1 --spider http://localhost:8080/health || exit 1

# 暴露端口
EXPOSE 8080 8081 9090

# 启动命令
ENTRYPOINT ["/usr/bin/gateway"]
CMD ["--manager", "/etc/agentos/gateway/manager.yaml"]

# ==========================================================================
# 阶段3: 开发镜像（可选）
# ==========================================================================
FROM builder AS development

LABEL description="AgentOS gateway Module Development Image"

# 安装开发工具
RUN apk add --no-cache \
    gdb \
    valgrind \
    clang-extra-tools \
    cppcheck

# 设置调试环境
ENV CMAKE_BUILD_TYPE=Debug

# 保持root用户以便调试
USER root

# 默认命令
CMD ["/bin/sh"]

