#!/bin/bash

# AgentOS 本地代码质量检查脚本
# 用于在提交前检查代码重复率和圈复杂度
# 使用方法: ./scripts/check-quality.sh [目录路径]

set -e

# 颜色定义
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# 阈值配置（与CI一致）
DUPLICATE_THRESHOLD=5.0     # 重复率阈值（%）
AVG_COMPLEXITY_THRESHOLD=3.5 # 平均圈复杂度阈值
SINGLE_COMPLEXITY_THRESHOLD=10 # 单个函数圈复杂度阈值

# 默认检查目录
CHECK_DIR="${1:-.}"

# 工具检查函数
check_tool() {
    local tool_name="$1"
    local install_cmd="$2"
    
    if ! command -v "$tool_name" &> /dev/null; then
        echo -e "${YELLOW}警告: $tool_name 未安装${NC}"
        echo "安装命令: $install_cmd"
        return 1
    fi
    return 0
}

# 检查工具是否可用
echo -e "${BLUE}=== 检查工具可用性 ===${NC}"

JSCPD_AVAILABLE=0
LIZARD_AVAILABLE=0

if check_tool "jscpd" "npm install -g jscpd"; then
    JSCPD_AVAILABLE=1
    echo -e "${GREEN}✓ jscpd 已安装${NC}"
else
    echo -e "${YELLOW}✗ jscpd 未安装，跳过重复检测${NC}"
fi

if check_tool "lizard" "pip install lizard"; then
    LIZARD_AVAILABLE=1
    echo -e "${GREEN}✓ lizard 已安装${NC}"
else
    echo -e "${YELLOW}✗ lizard 未安装，跳过圈复杂度分析${NC}"
fi

echo ""

# 创建报告目录
REPORT_DIR="$CHECK_DIR/reports"
mkdir -p "$REPORT_DIR"

# 1. 重复代码检测
if [ $JSCPD_AVAILABLE -eq 1 ]; then
    echo -e "${BLUE}=== 运行重复代码检测 (jscpd) ===${NC}"
    
    # 检查配置文件
    JSCPD_CONFIG="$CHECK_DIR/.jscpd.json"
    if [ ! -f "$JSCPD_CONFIG" ]; then
        JSCPD_CONFIG="../../.jscpd.json"
        if [ ! -f "$JSCPD_CONFIG" ]; then
            echo -e "${YELLOW}警告: 未找到.jscpd.json配置文件，使用默认阈值${NC}"
            JSCPD_CONFIG=""
        fi
    fi
    
    # 运行jscpd
    if [ -n "$JSCPD_CONFIG" ]; then
        jscpd --manager "$JSCPD_CONFIG" --output "$REPORT_DIR/jscpd-report" "$CHECK_DIR" 2>&1 | tee "$REPORT_DIR/jscpd.txt"
    else
        jscpd --threshold "$DUPLICATE_THRESHOLD" --format c,cpp,h,hpp --gitignore --output "$REPORT_DIR/jscpd-report" "$CHECK_DIR" 2>&1 | tee "$REPORT_DIR/jscpd.txt"
    fi
    
    # 解析结果
    if grep -q "Found.*duplicated lines" "$REPORT_DIR/jscpd.txt"; then
        DUPLICATE_PERCENT=$(grep -o "Found.*duplicated lines" "$REPORT_DIR/jscpd.txt" | grep -o '[0-9]*\.[0-9]*%' || echo "0%")
        DUPLICATE_VALUE=$(echo "$DUPLICATE_PERCENT" | sed 's/%//')
        
        echo -e "重复率: ${DUPLICATE_PERCENT}"
        
        # 检查阈值
        if (( $(echo "$DUPLICATE_VALUE > $DUPLICATE_THRESHOLD" | bc -l) )); then
            echo -e "${RED}❌ 错误: 重复率 ${DUPLICATE_PERCENT} 超过阈值 ${DUPLICATE_THRESHOLD}%${NC}"
            FAILED=1
        else
            echo -e "${GREEN}✓ 重复率在阈值内${NC}"
        fi
    else
        echo -e "${GREEN}✓ 未检测到重复代码${NC}"
    fi
else
    echo -e "${YELLOW}跳过重复检测 (jscpd 未安装)${NC}"
fi

echo ""

# 2. 圈复杂度分析
if [ $LIZARD_AVAILABLE -eq 1 ]; then
    echo -e "${BLUE}=== 运行圈复杂度分析 (lizard) ===${NC}"
    
    # 检查配置文件
    LIZARD_CONFIG="$CHECK_DIR/.lizardrc"
    if [ ! -f "$LIZARD_CONFIG" ]; then
        LIZARD_CONFIG="../../.lizardrc"
        if [ ! -f "$LIZARD_CONFIG" ]; then
            echo -e "${YELLOW}警告: 未找到.lizardrc配置文件，使用默认阈值${NC}"
            LIZARD_CONFIG=""
        fi
    fi
    
    # 运行lizard
    if [ -n "$LIZARD_CONFIG" ]; then
        lizard --manager "$LIZARD_CONFIG" --output_file "$REPORT_DIR/lizard-report.html" "$CHECK_DIR" 2>&1 | tee "$REPORT_DIR/lizard.txt"
    else
        lizard --CCN "$SINGLE_COMPLEXITY_THRESHOLD" --length 50 --arguments 6 --output_file "$REPORT_DIR/lizard-report.html" "$CHECK_DIR" 2>&1 | tee "$REPORT_DIR/lizard.txt"
    fi
    
    # 解析结果
    if grep -q "Average cyclomatic complexity" "$REPORT_DIR/lizard.txt"; then
        AVG_CCN=$(grep "Average cyclomatic complexity" "$REPORT_DIR/lizard.txt" | grep -o '[0-9]*\.[0-9]*' || echo "0")
        
        echo -e "平均圈复杂度: ${AVG_CCN}"
        
        # 检查平均复杂度阈值
        if (( $(echo "$AVG_CCN > $AVG_COMPLEXITY_THRESHOLD" | bc -l) )); then
            echo -e "${RED}❌ 错误: 平均圈复杂度 ${AVG_CCN} 超过阈值 ${AVG_COMPLEXITY_THRESHOLD}${NC}"
            FAILED=1
        else
            echo -e "${GREEN}✓ 平均圈复杂度在阈值内${NC}"
        fi
        
        # 检查高复杂度函数
        if grep -q ".* exceeds " "$REPORT_DIR/lizard.txt"; then
            HIGH_COMPLEXITY_COUNT=$(grep -c ".* exceeds " "$REPORT_DIR/lizard.txt" || echo "0")
            echo -e "高复杂度函数数量: ${HIGH_COMPLEXITY_COUNT}"
            
            if [ "$HIGH_COMPLEXITY_COUNT" -gt 0 ]; then
                echo -e "${YELLOW}⚠️  警告: 发现 ${HIGH_COMPLEXITY_COUNT} 个函数超过复杂度阈值${NC}"
                echo "高复杂度函数列表:"
                grep ".* exceeds " "$REPORT_DIR/lizard.txt" | head -10
                
                if [ "$HIGH_COMPLEXITY_COUNT" -gt 10 ]; then
                    echo "... 更多函数请查看完整报告: $REPORT_DIR/lizard.txt"
                fi
            fi
        fi
    else
        echo -e "${GREEN}✓ 圈复杂度分析完成${NC}"
    fi
else
    echo -e "${YELLOW}跳过圈复杂度分析 (lizard 未安装)${NC}"
fi

echo ""

# 总结
echo -e "${BLUE}=== 质量检查总结 ===${NC}"

if [ $JSCPD_AVAILABLE -eq 1 ] && [ $LIZARD_AVAILABLE -eq 1 ]; then
    if [ -n "${FAILED+set}" ]; then
        echo -e "${RED}❌ 质量检查未通过${NC}"
        echo "详情请查看报告文件:"
        echo "  - $REPORT_DIR/jscpd.txt (重复检测)"
        echo "  - $REPORT_DIR/lizard.txt (复杂度分析)"
        echo "  - $REPORT_DIR/jscpd-report/ (重复检测HTML报告)"
        echo "  - $REPORT_DIR/lizard-report.html (复杂度HTML报告)"
        exit 1
    else
        echo -e "${GREEN}✅ 所有质量检查通过${NC}"
        echo "报告文件:"
        echo "  - $REPORT_DIR/jscpd.txt (重复检测)"
        echo "  - $REPORT_DIR/lizard.txt (复杂度分析)"
        echo "  - $REPORT_DIR/jscpd-report/ (重复检测HTML报告)"
        echo "  - $REPORT_DIR/lizard-report.html (复杂度HTML报告)"
        exit 0
    fi
else
    echo -e "${YELLOW}⚠️  部分检查未执行，请安装缺失的工具${NC}"
    
    if [ $JSCPD_AVAILABLE -eq 0 ]; then
        echo "需要安装: npm install -g jscpd"
    fi
    
    if [ $LIZARD_AVAILABLE -eq 0 ]; then
        echo "需要安装: pip install lizard"
    fi
    
    echo "安装后重新运行检查"
    exit 0
fi