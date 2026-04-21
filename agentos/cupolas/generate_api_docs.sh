#!/bin/bash
# generate_api_docs.sh - cupolas 模块 Doxygen API 文档生成脚本
#
# SPDX-FileCopyrightText: 2026 SPHARX Ltd.
# SPDX-License-Identifier: Apache-2.0 OR BSD-3-Clause
#
# Copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
#
# 用途: 一键生成 cupolas 模块的 Doxygen API 文档
# 前提: 需要先安装 Doxygen (sudo apt install doxygen graphviz)

set -e

echo "========================================"
echo "  cupolas - AgentOS 安全穹顶"
echo "  Doxygen API 文档生成工具"
echo "========================================"
echo

# 检查 Doxygen 是否已安装
if ! command -v doxygen &> /dev/null; then
    echo "[ERROR] 未检测到 Doxygen!"
    echo
    echo "请按以下步骤安装:"
    echo "  Ubuntu/Debian:  sudo apt install doxygen graphviz"
    echo "  Fedora:         sudo dnf install doxygen graphviz"
    echo "  Arch Linux:     sudo pacman -S doxygen graphviz"
    echo "  macOS:          brew install doxygen graphviz"
    echo
    exit 1
fi

# 检查 graphviz 是否已安装 (用于生成调用图)
if ! command -v dot &> /dev/null; then
    echo "[WARNING] 未检测到 graphviz (dot 命令)!"
    echo "调用图/继承图将无法生成。"
    echo "安装命令: sudo apt install graphviz"
    echo
fi

# 获取脚本所在目录
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

echo "[INFO] 工作目录: $(pwd)"
echo

# 清理旧的文档输出
if [ -d "docs/api" ]; then
    echo "[INFO] 清理旧文档..."
    rm -rf docs/api
fi

# 运行 Doxygen
echo "[INFO] 开始生成 API 文档..."
echo
doxygen Doxyfile

# 检查结果
if [ $? -eq 0 ]; then
    echo
    echo "========================================"
    echo "  ✓ API 文档生成成功!"
    echo "========================================"
    echo
    echo "输出位置: $(pwd)/docs/api/html/index.html"
    echo
    
    # 尝试在浏览器中打开
    if command -v xdg-open &> /dev/null; then
        echo "正在打开浏览器..."
        xdg-open "$(pwd)/docs/api/html/index.html"
    elif command -v open &> /dev/null; then
        echo "正在打开浏览器..."
        open "$(pwd)/docs/api/html/index.html"
    fi
else
    echo
    echo "========================================"
    echo "  ✗ 文档生成失败!"
    echo "========================================"
    echo
    echo "请检查日志文件:"
    echo "  $(pwd)/docs/doxygen_warnings.log"
    echo
    exit 1
fi
