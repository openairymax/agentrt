#!/usr/bin/env bash

# AgentOS 贡献者统计脚本
# 统计项目贡献者的代码行数和提交次数

set -e

echo "📊 AgentOS 贡献者统计"
echo "======================="
echo ""

# 获取所有作者
AUTHORS=$(git shortlog -sn --all | awk '{$1=""; print $0}' | sed 's/^ //')

TOTAL_ADDED=0
TOTAL_REMOVED=0

echo "👥 贡献者列表:"
echo ""

printf "%-25s %-10s %-10s %-10s %-15s\n" "Author" "Commits" "Added" "Removed" "Net Lines"
printf "%-25s %-10s %-10s %-10s %-15s\n" "-----" "--------" "-----" "-------" "---------"

for author in $AUTHORS; do
    # 获取提交次数
    COMMITS=$(git log --author="$author" --oneline | wc -l)
    
    # 获取增删行数
    STATS=$(git log --author="$author" --pretty=tformat: --numstat --all | \
        awk '{ add += $1; subs += $2 } END { printf "%+d %+d", add, subs-add }')
    
    ADDED=$(echo $STATS | awk '{print $1}')
    REMOVED=$(echo $STATS | awk '{print $2}')
    NET=$((ADDED - REMOVED))
    
    TOTAL_ADDED=$((TOTAL_ADDED + ADDED))
    TOTAL_REMOVED=$((TOTAL_REMOVED + REMOVED))
    
    printf "%-25s %-10d %-10d %-10d %-+15d\n" "$author" "$COMMITS" "$ADDED" "$REMOVED" "$NET"
done

echo ""
echo "📈 总计:"
echo "   总添加行数: $TOTAL_ADDED"
echo "   总删除行数: $TOTAL_REMOVED"
echo "   净增长行数: $((TOTAL_ADDED - TOTAL_REMOVED))"
echo ""

# 模块统计
echo "📦 各模块贡献统计:"
echo ""

MODULES="atoms daemon commons gateway heapstore manager toolkit openlab manuals tests"

for module in $MODULES; do
    if [ -d "$module" ]; then
        COMMITS=$(git log --oneline -- "$module" | wc -l)
        if [ $COMMITS -gt 0 ]; then
            echo "   📁 $module: $COMMITS 次提交"
        fi
    fi
done

echo ""
echo "💡 使用方法: ./generate-stats.sh > stats.md"
