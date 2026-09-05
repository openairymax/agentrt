# syntax=docker/dockerfile:1
# =============================================================================
# QEMU arm64/arm32 工具链镜像（0.1.11 E1 流水线优化）
#
# 问题（一手证据）：release.yml 的 qemu 腿每轮在容器内从源码重编全部
# deps（openssl/libcurl/libwebsockets/cmake，见 lib-builddeps.sh），
# build-linux-arm-64 实测 88 min → 主导整链墙钟并长期独占 runner。
# 方案：把「apt 依赖 + lib-builddeps.sh 自编译产物」一次性固化进 GHCR 镜像，
# release 腿 docker run 该镜像后只剩本项目 configure/build/package。
#
# 与 release.yml build-linux-arm-64 容器腿 apt 清单完全同源（SSoT）：
#   ubuntu:20.04 + build-essential/make/perl/curl/python3/... + lib-builddeps.sh
# glibc 2.31 基线不变（FROM 仍是 ubuntu:20.04）。
# 用法（见 .github/workflows/build-toolchain-images.yml）：
#   docker buildx build --platform linux/arm64,linux/arm/v7 \
#     -f .github/docker/qemu-toolchain/ubuntu.Dockerfile .
# =============================================================================
ARG TARGETPLATFORM
FROM --platform=$TARGETPLATFORM ubuntu:20.04

ENV DEBIAN_FRONTEND=noninteractive \
    LANG=C.UTF-8

# arm-64 leg apt 清单（release.yml 同源）+ 构建期必需的 ca-certificates/git
RUN apt-get update -qq \
 && apt-get install -y -qq --no-install-recommends \
      build-essential make perl curl python3 python3-pip \
      libsqlite3-dev libyaml-dev libcurl4-openssl-dev libssl-dev zlib1g-dev \
      libzstd-dev libmicrohttpd-dev libwebsockets-dev libevent-dev \
      libnghttp2-dev ca-certificates git pkg-config \
 && rm -rf /var/lib/apt/lists/*

# lib-builddeps.sh（tools SSoT）预构建 cmake/OpenSSL/libcurl/libwebsockets → /usr/local。
# 构建上下文 ctx/ 只含 tools/scripts/ci/release 一份拷贝（见 build workflow）。
COPY _tools/scripts/ci/release /opt/airy/release
# qemu 32 位/低配场景用小并行，镜像构建为一次性成本，此处用 4 兼顾速度与稳定。
RUN AIRY_JOBS=4 bash /opt/airy/release/lib-builddeps.sh \
 && rm -rf /opt/airy/release

# 供 release 腿复用（cmake/openssl 自编产物在 /usr/local）
CMD ["/bin/bash"]
