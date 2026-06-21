#!/bin/bash
# =============================================================================
# cleanup_builds.sh - AgentOS 构建产物清理脚本
# 版本: 1.0.0
# 用途: 清理源码目录中所有不应存在的构建产物
# =============================================================================

set -e  # 遇到错误立即退出

# 获取脚本所在目录的父目录（即AgentOS根目录）
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
AGENTOS_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"

echo "=================================================="
echo "🧹 AgentOS 构建产物清理脚本 v1.0.0"
echo "=================================================="
echo ""

# 切换到AgentOS目录
cd "$AGENTOS_ROOT"

echo "📍 当前目录: $(pwd)"
echo ""

# ===========================================
# 1. CMake构建产物
# ===========================================
echo "📦 清理CMake构建产物..."

# 查找并删除所有CMakeCache.txt
find . -maxdepth 4 -name "CMakeCache.txt" -type f 2>/dev/null | while read file; do
    echo "  🗑️  删除: $file"
    rm -f "$file"
done

# 查找并删除所有CMakeFiles目录
find . -maxdepth 4 -type d -name "CMakeFiles" 2>/dev/null | while read dir; do
    echo "  🗑️  删除目录: $dir"
    rm -rf "$dir"
done

# 查找并删除cmake_install.cmake
find . -maxdepth 4 -name "cmake_install.cmake" -type f 2>/dev/null | while read file; do
    echo "  🗑️  删除: $file"
    rm -f "$file"
done

# 查找并删除CTestTestfile.cmake
find . -maxdepth 4 -name "CTestTestfile.cmake" -type f 2>/dev/null | while read file; do
    echo "  🗑️  删除: $file"
    rm -f "$file"
done

# 查找并删除compile_commands.json符号链接
find . -maxdepth 3 -name "compile_commands.json" -type l 2>/dev/null | while read file; do
    echo "  🗑️  删除符号链接: $file"
    rm -f "$file"
done

echo "✅ CMake构建产物清理完成"
echo ""

# ===========================================
# 2. 构建输出目录
# ===========================================
echo "🏗️  清理构建输出目录..."

# 删除所有build目录
find . -maxdepth 4 -type d -name "build" 2>/dev/null | grep -v "\.git" | grep -v "node_modules" | while read dir; do
    echo "  🗑️  删除目录: $dir"
    rm -rf "$dir"
done

# 删除所有_build目录
find . -maxdepth 4 -type d -name "_build" 2>/dev/null | while read dir; do
    echo "  🗑️  删除目录: $dir"
    rm -rf "$dir"
done

# 删除AgentRT-build目录（如果存在）
if [ -d "$AGENTOS_ROOT/AgentRT-build" ]; then
    echo "  ⚠️  AgentRT-build目录存在（保留，不删除）"
fi

echo "✅ 构建输出目录清理完成"
echo ""

# ===========================================
# 3. Python缓存
# ===========================================
echo "🐍 清理Python缓存..."

# 删除__pycache__目录
find . -type d -name "__pycache__" 2>/dev/null | while read dir; do
    echo "  🗑️  删除: $dir"
    rm -rf "$dir"
done

# 删除.pyc文件
find . -name "*.pyc" -type f 2>/dev/null | while read file; do
    echo "  🗑️  删除: $file"
    rm -f "$file"
done

# 删除.pyo文件
find . -name "*.pyo" -type f 2>/dev/null | while read file; do
    rm -f "$file"
done

# 删除.pytest_cache
if [ -d ".pytest_cache" ]; then
    echo "  🗑️  删除: .pytest_cache"
    rm -rf .pytest_cache
fi

echo "✅ Python缓存清理完成"
echo ""

# ===========================================
# 4. 库文件和可执行文件
# ===========================================
echo "🔧 清理编译产物..."

# 删除.so文件（排除.git目录）
find . -maxdepth 4 -name "*.so" -type f -not -path "./.git/*" 2>/dev/null | while read file; do
    echo "  🗑️  删除: $file"
    rm -f "$file"
done

# 删除.so.*文件
find . -maxdepth 4 -name "*.so.*" -type f -not -path "./.git/*" 2>/dev/null | while read file; do
    echo "  🗑️  删除: $file"
    rm -f "$file"
done

echo "✅ 编译产物清理完成"
echo ""

# ===========================================
# 5. 验证清理结果
# ===========================================
echo "=================================================="
echo "✅ 清理完成！验证结果..."
echo "=================================================="
echo ""

# 检查是否还有CMake产物
cmake_files=$(find . -maxdepth 4 \( -name "CMakeCache.txt" -o -name "cmake_install.cmake" -o -name "CTestTestfile.cmake" \) 2>/dev/null | wc -l)
if [ "$cmake_files" -gt 0 ]; then
    echo "⚠️  警告: 还有 $cmake_files 个CMake文件未清理"
    find . -maxdepth 4 \( -name "CMakeCache.txt" -o -name "cmake_install.cmake" -o -name "CTestTestfile.cmake" \) 2>/dev/null
else
    echo "✅ CMake产物: 完全清理"
fi

# 检查build目录
build_dirs=$(find . -maxdepth 4 -type d -name "build" 2>/dev/null | grep -v "\.git" | wc -l)
if [ "$build_dirs" -gt 0 ]; then
    echo "⚠️  警告: 还有 $build_dirs 个build目录"
    find . -maxdepth 4 -type d -name "build" 2>/dev/null | grep -v "\.git"
else
    echo "✅ build目录: 完全清理"
fi

# 检查Python缓存
pycache=$(find . -type d -name "__pycache__" 2>/dev/null | wc -l)
if [ "$pycache" -gt 0 ]; then
    echo "⚠️  警告: 还有 $pycache 个__pycache__目录"
else
    echo "✅ Python缓存: 完全清理"
fi

echo ""
echo "=================================================="
echo "🎉 清理脚本执行完成！"
echo "=================================================="
