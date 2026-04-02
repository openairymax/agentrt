#!/usr/bin/env bash

# AgentOS 分支清理脚本
# 用于清理已合并到主分支的本地分支

set -e

echo "🧹 AgentOS 分支清理工具"
echo "========================"

# 获取当前分支
CURRENT_BRANCH=$(git symbolic-ref --short HEAD 2>/dev/null || echo "HEAD")

# 获取默认主分支
MAIN_BRANCH="main"
if git show-ref --verify --quiet refs/heads/master; then
    MAIN_BRANCH="master"
fi

echo ""
echo "📌 当前分支: $CURRENT_BRANCH"
echo "📌 主分支: $MAIN_BRANCH"
echo ""

# 确保不在受保护的分支上
if [[ "$CURRENT_BRANCH" == "$MAIN_BRANCH" || "$CURRENT_BRANCH" == "develop" ]]; then
    echo "✅ 当前在受保护分支，安全"
else
    echo "⚠️  当前在功能分支，建议先切换到主分支"
fi

echo ""
echo "📋 以下是可以清理的已合并分支:"
echo ""

# 列出已合并的分支（排除当前分支和主分支）
MERGED_BRANCHES=$(git branch --merged $MAIN_BRANCH | grep -v "\*" | grep -v "$MAIN_BRANCH" | grep -v "develop" | sed 's/^[ \t]*//')

if [ -z "$MERGED_BRANCHES" ]; then
    echo "   没有需要清理的分支"
else
    echo "$MERGED_BRANCHES" | while read -r branch; do
        # 显示分支信息
        LAST_COMMIT=$(git log -1 --format="%ai - %s" "$branch")
        echo "   📁 $branch ($LAST_COMMIT)"
    done
    
    BRANCH_COUNT=$(echo "$MERGED_BRANCHES" | wc -l)
    echo ""
    echo "   共 $BRANCH_COUNT 个分支可以清理"
    
    read -p "是否删除这些分支？(y/N): " -n 1 -r
    echo ""
    
    if [[ $REPLY =~ ^[Yy]$ ]]; then
        echo "$MERGED_BRANCHES" | while read -r branch; do
            echo "   🗑️  删除: $branch"
            git branch -d "$branch"
        done
        echo "✅ 清理完成！"
    else
        echo "❌ 取消操作"
    fi
fi

# 清理远程跟踪的已删除分支
echo ""
echo "🔄 清理远程跟踪分支..."
git remote prune origin 2>/dev/null || true
echo "✅ 远程跟踪分支已清理"

echo ""
echo "💡 提示：使用 'git branch -a' 查看所有分支"
