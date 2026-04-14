# Copyright (c) 2026 SPHARX. All Rights Reserved.
# Dockerfile for AgentOS Multi-Protocol Gateway
# Version: 2.0.0
#
# 多阶段构建，支持MCP/A2A/OpenAI API多协议网关

# ==========================================================================
# 阶段1: 构建阶段
# ==========================================================================
FROM alpine:3.19 AS builder

LABEL maintainer="SPHARX <dev@spharx.cn>"
LABEL description="AgentOS Multi-Protocol Gateway Build Stage"

ARG BUILD_VERSION=2.0.0
ARG BUILD_DATE
ARG TARGETPLATFORM

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
    grpc-dev \
    protobuf-dev

WORKDIR /build

COPY CMakeLists.txt .
COPY src/ ./src/
COPY include/ ./include/

RUN mkdir -p build && cd build && \
    cmake .. -G "Ninja" \
    -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_TESTS=OFF \
    -DCMAKE_INSTALL_PREFIX=/usr \
    -DVERSION=${BUILD_VERSION} \
    -DENABLE_MCP=ON \
    -DENABLE_A2A=ON \
    -DENABLE_OPENAI=ON

RUN cd build && ninja -j$(nproc)
RUN cd build && DESTDIR=/install ninja install

# ==========================================================================
# 阶段2: 运行时镜像
# ==========================================================================
FROM alpine:3.19 AS runtime

LABEL maintainer="SPHARX <dev@spharx.cn>"
LABEL description="AgentOS Multi-Protocol Gateway Runtime"
LABEL version="${BUILD_VERSION}"
LABEL build-date="${BUILD_DATE}"
LABEL org.opencontainers.image.source="https://github.com/spharx/agentos"
LABEL org.opencontainers.image.title="AgentOS Multi-Protocol Gateway"
LABEL org.opencontainers.image.description="AgentOS Gateway with MCP/A2A/OpenAI API support"
LABEL org.opencontainers.image.vendor="SPHARX"

ENV AGENTOS_MODULE=gateway \
    AGENTOS_VERSION=${BUILD_VERSION} \
    AGENTOS_LOG_LEVEL=INFO \
    AGENTOS_HTTP_PORT=8080 \
    AGENTOS_WS_PORT=8081 \
    AGENTOS_METRICS_PORT=9090 \
    AGENTOS_MCP_ENABLED=true \
    AGENTOS_A2A_ENABLED=true \
    AGENTOS_OPENAI_ENABLED=true

RUN apk add --no-cache \
    libmicrohttpd \
    libwebsockets \
    openssl \
    cjson \
    ca-certificates \
    tzdata \
    curl \
    libstdc++ && \
    addgroup -g 1000 agentos && \
    adduser -u 1000 -G agentos -s /bin/sh -D agentos && \
    mkdir -p /var/log/agentos /var/run/agentos /etc/agentos/gateway && \
    chown -R agentos:agentos /var/log/agentos /var/run/agentos /etc/agentos

COPY --from=builder /install/ /
COPY manager/ /etc/agentos/gateway/
RUN chown -R agentos:agentos /etc/agentos

USER agentos
WORKDIR /home/agentos

HEALTHCHECK --interval=30s --timeout=10s --start-period=5s --retries=3 \
    CMD wget --no-verbose --tries=1 --spider http://localhost:8080/health || exit 1

EXPOSE 8080 8081 9090

ENTRYPOINT ["/usr/bin/gateway"]
CMD ["--manager", "/etc/agentos/gateway/manager.yaml"]

# ==========================================================================
# 阶段3: 开发镜像（可选）
# ==========================================================================
FROM builder AS development

LABEL description="AgentOS Multi-Protocol Gateway Development Image"

RUN apk add --no-cache \
    gdb \
    valgrind \
    clang-extra-tools \
    cppcheck \
    grpc-plugins

ENV CMAKE_BUILD_TYPE=Debug

USER root
CMD ["/bin/sh"]

