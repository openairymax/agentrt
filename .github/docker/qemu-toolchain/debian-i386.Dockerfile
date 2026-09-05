# syntax=docker/dockerfile:1
# =============================================================================
# i386 (linux-x86-32) 工具链镜像（0.1.12 I1 流水线优化）
#
# 问题：release.yml build-linux-32（x86-32 腿）每轮在 debian:bullseye i386
# 容器内从零执行「snapshot apt 源改写 + apt 安装 + lib-builddeps.sh 自编译
# deps」，与 amd64/arm64 腿镜像化前同病（每轮 ~5-8min 纯环境准备）。
# 方案：与 ubuntu.Dockerfile（arm64/arm32/amd64 用）同模式，把环境一次性
# 固化进 GHCR 镜像，release 腿 docker run 后只剩项目构建/打包。
#
# 为什么不用 ubuntu.Dockerfile（ubuntu:20.04）：官方镜像无 linux/386 清单
# （v0.1.9 Release run 实证 "no matching manifest for linux/386"），x86-32
# 基座必须是带 i386 清单的镜像。选 debian:bullseye-20240812-slim（glibc
# 2.31 与 focal 同基线，与 release.yml 旧腿同源配方）。
#
# bullseye 已全面 EOL（标准 2024-08 / LTS 2026-08-31）：浮动 debian:bullseye
# 的 LTS 重建与 archive 仅折叠 LTS 运行时（-dev 不随更）形成不可解依赖
# （0.1.10 Release 实证 "held broken packages"）。故镜像钉版至 LTS 前日期
# + apt 源指向 snapshot.debian.org 同时间戳（20240812T000000Z），运行时与
# -dev 版本一致，可复现（对齐 0.2.0 方案 C1）。
#
# 执行模型：linux/386 容器在 x86_64 hosted runner 上经内核 IA32 兼容
# （CONFIG_IA32_EMULATION）**原生执行，零 QEMU**（与 arm64 宿主跑 arm/v7
# 的 CONFIG_COMPAT 同理）——镜像构建即对宿主兼容性的 fail-fast 验证。
# =============================================================================
FROM debian:bullseye-20240812-slim

ENV DEBIAN_FRONTEND=noninteractive \
    LANG=C.UTF-8

# bullseye EOL：apt 源钉 snapshot.debian.org（与镜像同时间戳），跳过
# Valid-Until 校验。与 release.yml build-linux-32 旧腿配方逐字同源。
RUN printf "%s\n" \
      "deb http://snapshot.debian.org/archive/debian/20240812T000000Z/ bullseye main" \
      "deb http://snapshot.debian.org/archive/debian-security/20240812T000000Z/ bullseye-security main" \
      > /etc/apt/sources.list \
 && printf "%s\n" "Acquire::Check-Valid-Until \"false\";" \
      > /etc/apt/apt.conf.d/99archived \
 && apt-get update -qq \
 && apt-get install -y -qq --no-install-recommends \
      build-essential make perl curl python3 python3-pip python3-venv \
      libsqlite3-dev libyaml-dev libcurl4-openssl-dev libssl-dev zlib1g-dev \
      libzstd-dev libmicrohttpd-dev libwebsockets-dev libevent-dev \
      libnghttp2-dev ca-certificates git pkg-config \
 && rm -rf /var/lib/apt/lists/*

# lib-builddeps.sh（tools SSoT）预构建 cmake/OpenSSL/libcurl/libwebsockets → /usr/local。
# 构建上下文 ctx/ 只含 tools/scripts/ci/release 一份拷贝（见 build workflow）。
COPY _tools/scripts/ci/release /opt/airy/release
# 32 位用户态地址空间受限：lib-builddeps 自编译用小并行（AIRY_JOBS=2，
# 与 release.yml x86-32 腿运行值一致），避免 32 位链接期地址空间不足。
RUN AIRY_JOBS=2 bash /opt/airy/release/lib-builddeps.sh \
 && rm -rf /opt/airy/release

# 供 release 腿复用（cmake/openssl 自编产物在 /usr/local）
CMD ["/bin/bash"]
