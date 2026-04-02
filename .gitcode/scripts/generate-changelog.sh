#!/usr/bin/env bash

# AgentOS 变更日志生成脚本
# 根据 Git 历史自动生成 CHANGELOG

set -e

VERSION=${1:-"unreleased"}
OUTPUT_FILE=${2:-"CHANGELOG.md"}
FROM_TAG=${3:-""}

echo "📝 生成变更日志..."
echo "   版本: $VERSION"
echo "   输出文件: $OUTPUT_FILE"

# 创建临时文件
TEMP_FILE=$(mktemp)

# 写入头部
cat > "$TEMP_FILE" << EOF
# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [$VERSION] - $(date +%Y-%m-%d)

EOF

# 如果有起始标签，只获取该标签之后的提交
if [ -n "$FROM_TAG" ]; then
    LOG_RANGE="$FROM_TAG..HEAD"
else
    LOG_RANGE=""
fi

# 按类型分类提交
echo "" >> "$TEMP_FILE"

# 新功能
FEAT_COMMITS=$(git log $LOG_RANGE --grep="^feat" --pretty=format:"%h %s (%an, %ar)" 2>/dev/null)
if [ -n "$FEAT_COMMITS" ]; then
    echo "### Added" >> "$TEMP_FILE"
    echo "" >> "$TEMP_FILE"
    echo "$FEAT_COMMITS" | while read -r line; do
        echo "- $line" >> "$TEMP_FILE"
    done
    echo "" >> "$TEMP_FILE"
fi

# Bug 修复
FIX_COMMITS=$(git log $LOG_RANGE --grep="^fix" --pretty=format:"%h %s (%an, %ar)" 2>/dev/null)
if [ -n "$FIX_COMMITS" ]; then
    echo "### Fixed" >> "$TEMP_FILE"
    echo "" >> "$TEMP_FILE"
    echo "$FIX_COMMITS" | while read -r line; do
        echo "- $line" >> "$TEMP_FILE"
    done
    echo "" >> "$TEMP_FILE"
fi

# 文档更新
DOCS_COMMITS=$(git log $LOG_RANGE --grep="^docs" --pretty=format:"%h %s (%an, %ar)" 2>/dev/null)
if [ -n "$DOCS_COMMITS" ]; then
    echo "### Documentation" >> "$TEMP_FILE"
    echo "" >> "$TEMP_FILE"
    echo "$DOCS_COMMITS" | while read -r line; do
        echo "- $line" >> "$TEMP_FILE"
    done
    echo "" >> "$TEMP_FILE"
fi

# 重构
REFACTOR_COMMITS=$(git log $LOG_RANGE --grep="^refactor" --pretty=format:"%h %s (%an, %ar)" 2>/dev/null)
if [ -n "$REFACTOR_COMMITS" ]; then
    echo "### Changed" >> "$TEMP_FILE"
    echo "" >> "$TEMP_FILE"
    echo "$REFACTOR_COMMITS" | while read -r line; do
        echo "- $line" >> "$TEMP_FILE"
    done
    echo "" >> "$TEMP_FILE"
fi

# 性能优化
PERF_COMMITS=$(git log $LOG_RANGE --grep="^perf" --pretty=format:"%h %s (%an, %ar)" 2>/dev/null)
if [ -n "$PERF_COMMITS" ]; then
    echo "### Performance" >> "$TEMP_FILE"
    echo "" >> "$TEMP_FILE"
    echo "$PERF_COMMITS" | while read -r line; do
        echo "- $line" >> "$TEMP_FILE"
    done
    echo "" >> "$TEMP_FILE"
fi

# 统计信息
TOTAL_COMMITS=$(git rev-list --count $LOG_RANGE HEAD 2>/dev/null || echo "?")
CONTRIBUTORS=$(git shortlog -sn $LOG_RANGE HEAD 2>/dev/null | wc -l)

echo "" >> "$TEMP_FILE"
echo "**Statistics:**" >> "$TEMP_FILE"
echo "- Total commits: $TOTAL_COMMITS" >> "$TEMP_FILE"
echo "- Contributors: $CONTRIBUTORS" >> "$TEMP_FILE"
echo "" >> "$TEMP_FILE"

# 如果输出文件存在，在前面插入新内容
if [ -f "$OUTPUT_FILE" ]; then
    cat "$TEMP_FILE" "$OUTPUT_FILE" > "${OUTPUT_FILE}.tmp"
    mv "${OUTPUT_FILE}.tmp" "$OUTPUT_FILE"
else
    cp "$TEMP_FILE" "$OUTPUT_FILE"
fi

rm -f "$TEMP_FILE"

echo "✅ 变更日志已生成: $OUTPUT_FILE"
