#!/bin/bash

# AgentOS ±ľµŘ´úÂëÖĘÁżĽě˛é˝Ĺ±ľ
# ÓĂÓÚÔÚĚá˝»Ç°Ľě˛é´úÂëÖŘ¸´ÂĘşÍČ¦¸´ÔÓ¶Č
# ĘąÓĂ·˝·¨: ./scripts/check-quality.sh [ÄżÂĽÂ·ľ¶]

set -e

# ŃŐÉ«¶¨Ňĺ
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# ăĐÖµĹäÖĂŁ¨ÓëCIŇ»ÖÂŁ©
DUPLICATE_THRESHOLD=5.0     # ÖŘ¸´ÂĘăĐÖµŁ¨%Ł©
AVG_COMPLEXITY_THRESHOLD=3.5 # Ć˝ľůČ¦¸´ÔÓ¶ČăĐÖµ
SINGLE_COMPLEXITY_THRESHOLD=10 # µĄ¸öşŻĘýČ¦¸´ÔÓ¶ČăĐÖµ

# Ä¬ČĎĽě˛éÄżÂĽ
CHECK_DIR="${1:-.}"

# ą¤ľßĽě˛éşŻĘý
check_tool() {
    local tool_name="$1"
    local install_cmd="$2"
    
    if ! command -v "$tool_name" &> /dev/null; then
        echo -e "${YELLOW}ľŻ¸ć: $tool_name Î´°˛×°${NC}"
        echo "°˛×°ĂüÁî: $install_cmd"
        return 1
    fi
    return 0
}

# Ľě˛éą¤ľßĘÇ·ńżÉÓĂ
echo -e "${BLUE}=== Ľě˛éą¤ľßżÉÓĂĐÔ ===${NC}"

JSCPD_AVAILABLE=0
LIZARD_AVAILABLE=0

if check_tool "jscpd" "npm install -g jscpd"; then
    JSCPD_AVAILABLE=1
    echo -e "${GREEN}? jscpd ŇŃ°˛×°${NC}"
else
    echo -e "${YELLOW}? jscpd Î´°˛×°Ł¬ĚřąýÖŘ¸´Ľě˛â${NC}"
fi

if check_tool "lizard" "pip install lizard"; then
    LIZARD_AVAILABLE=1
    echo -e "${GREEN}? lizard ŇŃ°˛×°${NC}"
else
    echo -e "${YELLOW}? lizard Î´°˛×°Ł¬ĚřąýČ¦¸´ÔÓ¶Č·ÖÎö${NC}"
fi

echo ""

# ´´˝¨±¨¸ćÄżÂĽ
REPORT_DIR="$CHECK_DIR/reports"
mkdir -p "$REPORT_DIR"

# 1. ÖŘ¸´´úÂëĽě˛â
if [ $JSCPD_AVAILABLE -eq 1 ]; then
    echo -e "${BLUE}=== ÔËĐĐÖŘ¸´´úÂëĽě˛â (jscpd) ===${NC}"
    
    # Ľě˛éĹäÖĂÎÄĽţ
    JSCPD_CONFIG="$CHECK_DIR/.jscpd.json"
    if [ ! -f "$JSCPD_CONFIG" ]; then
        JSCPD_CONFIG="../../.jscpd.json"
        if [ ! -f "$JSCPD_CONFIG" ]; then
            echo -e "${YELLOW}ľŻ¸ć: Î´ŐŇµ˝.jscpd.jsonĹäÖĂÎÄĽţŁ¬ĘąÓĂÄ¬ČĎăĐÖµ${NC}"
            JSCPD_CONFIG=""
        fi
    fi
    
    # ÔËĐĐjscpd
    if [ -n "$JSCPD_CONFIG" ]; then
        jscpd --manager "$JSCPD_CONFIG" --output "$REPORT_DIR/jscpd-report" "$CHECK_DIR" 2>&1 | tee "$REPORT_DIR/jscpd.txt"
    else
        jscpd --threshold "$DUPLICATE_THRESHOLD" --format c,cpp,h,hpp --gitignore --output "$REPORT_DIR/jscpd-report" "$CHECK_DIR" 2>&1 | tee "$REPORT_DIR/jscpd.txt"
    fi
    
    # ˝âÎö˝áąű
    if grep -q "Found.*duplicated lines" "$REPORT_DIR/jscpd.txt"; then
        DUPLICATE_PERCENT=$(grep -o "Found.*duplicated lines" "$REPORT_DIR/jscpd.txt" | grep -o '[0-9]*\.[0-9]*%' || echo "0%")
        DUPLICATE_VALUE=$(echo "$DUPLICATE_PERCENT" | sed 's/%//')
        
        echo -e "ÖŘ¸´ÂĘ: ${DUPLICATE_PERCENT}"
        
        # Ľě˛éăĐÖµ
        if (( $(echo "$DUPLICATE_VALUE > $DUPLICATE_THRESHOLD" | bc -l) )); then
            echo -e "${RED}? ´íÎó: ÖŘ¸´ÂĘ ${DUPLICATE_PERCENT} ł¬ąýăĐÖµ ${DUPLICATE_THRESHOLD}%${NC}"
            FAILED=1
        else
            echo -e "${GREEN}? ÖŘ¸´ÂĘÔÚăĐÖµÄÚ${NC}"
        fi
    else
        echo -e "${GREEN}? Î´Ľě˛âµ˝ÖŘ¸´´úÂë${NC}"
    fi
else
    echo -e "${YELLOW}ĚřąýÖŘ¸´Ľě˛â (jscpd Î´°˛×°)${NC}"
fi

echo ""

# 2. Č¦¸´ÔÓ¶Č·ÖÎö
if [ $LIZARD_AVAILABLE -eq 1 ]; then
    echo -e "${BLUE}=== ÔËĐĐČ¦¸´ÔÓ¶Č·ÖÎö (lizard) ===${NC}"
    
    # Ľě˛éĹäÖĂÎÄĽţ
    LIZARD_CONFIG="$CHECK_DIR/.lizardrc"
    if [ ! -f "$LIZARD_CONFIG" ]; then
        LIZARD_CONFIG="../../.lizardrc"
        if [ ! -f "$LIZARD_CONFIG" ]; then
            echo -e "${YELLOW}ľŻ¸ć: Î´ŐŇµ˝.lizardrcĹäÖĂÎÄĽţŁ¬ĘąÓĂÄ¬ČĎăĐÖµ${NC}"
            LIZARD_CONFIG=""
        fi
    fi
    
    # ÔËĐĐlizard
    if [ -n "$LIZARD_CONFIG" ]; then
        lizard --manager "$LIZARD_CONFIG" --output_file "$REPORT_DIR/lizard-report.html" "$CHECK_DIR" 2>&1 | tee "$REPORT_DIR/lizard.txt"
    else
        lizard --CCN "$SINGLE_COMPLEXITY_THRESHOLD" --length 50 --arguments 6 --output_file "$REPORT_DIR/lizard-report.html" "$CHECK_DIR" 2>&1 | tee "$REPORT_DIR/lizard.txt"
    fi
    
    # ˝âÎö˝áąű
    if grep -q "Average cyclomatic complexity" "$REPORT_DIR/lizard.txt"; then
        AVG_CCN=$(grep "Average cyclomatic complexity" "$REPORT_DIR/lizard.txt" | grep -o '[0-9]*\.[0-9]*' || echo "0")
        
        echo -e "Ć˝ľůČ¦¸´ÔÓ¶Č: ${AVG_CCN}"
        
        # Ľě˛éĆ˝ľů¸´ÔÓ¶ČăĐÖµ
        if (( $(echo "$AVG_CCN > $AVG_COMPLEXITY_THRESHOLD" | bc -l) )); then
            echo -e "${RED}? ´íÎó: Ć˝ľůČ¦¸´ÔÓ¶Č ${AVG_CCN} ł¬ąýăĐÖµ ${AVG_COMPLEXITY_THRESHOLD}${NC}"
            FAILED=1
        else
            echo -e "${GREEN}? Ć˝ľůČ¦¸´ÔÓ¶ČÔÚăĐÖµÄÚ${NC}"
        fi
        
        # Ľě˛é¸ß¸´ÔÓ¶ČşŻĘý
        if grep -q ".* exceeds " "$REPORT_DIR/lizard.txt"; then
            HIGH_COMPLEXITY_COUNT=$(grep -c ".* exceeds " "$REPORT_DIR/lizard.txt" || echo "0")
            echo -e "¸ß¸´ÔÓ¶ČşŻĘýĘýÁż: ${HIGH_COMPLEXITY_COUNT}"
            
            if [ "$HIGH_COMPLEXITY_COUNT" -gt 0 ]; then
                echo -e "${YELLOW}??  ľŻ¸ć: ·˘ĎÖ ${HIGH_COMPLEXITY_COUNT} ¸öşŻĘýł¬ąý¸´ÔÓ¶ČăĐÖµ${NC}"
                echo "¸ß¸´ÔÓ¶ČşŻĘýÁĐ±í:"
                grep ".* exceeds " "$REPORT_DIR/lizard.txt" | head -10
                
                if [ "$HIGH_COMPLEXITY_COUNT" -gt 10 ]; then
                    echo "... ¸ü¶ŕşŻĘýÇë˛éż´ÍęŐű±¨¸ć: $REPORT_DIR/lizard.txt"
                fi
            fi
        fi
    else
        echo -e "${GREEN}? Č¦¸´ÔÓ¶Č·ÖÎöÍęłÉ${NC}"
    fi
else
    echo -e "${YELLOW}ĚřąýČ¦¸´ÔÓ¶Č·ÖÎö (lizard Î´°˛×°)${NC}"
fi

echo ""

# ×Ü˝á
echo -e "${BLUE}=== ÖĘÁżĽě˛é×Ü˝á ===${NC}"

if [ $JSCPD_AVAILABLE -eq 1 ] && [ $LIZARD_AVAILABLE -eq 1 ]; then
    if [ -n "${FAILED+set}" ]; then
        echo -e "${RED}? ÖĘÁżĽě˛éÎ´Í¨ąý${NC}"
        echo "ĎęÇéÇë˛éż´±¨¸ćÎÄĽţ:"
        echo "  - $REPORT_DIR/jscpd.txt (ÖŘ¸´Ľě˛â)"
        echo "  - $REPORT_DIR/lizard.txt (¸´ÔÓ¶Č·ÖÎö)"
        echo "  - $REPORT_DIR/jscpd-report/ (ÖŘ¸´Ľě˛âHTML±¨¸ć)"
        echo "  - $REPORT_DIR/lizard-report.html (¸´ÔÓ¶ČHTML±¨¸ć)"
        exit 1
    else
        echo -e "${GREEN}? ËůÓĐÖĘÁżĽě˛éÍ¨ąý${NC}"
        echo "±¨¸ćÎÄĽţ:"
        echo "  - $REPORT_DIR/jscpd.txt (ÖŘ¸´Ľě˛â)"
        echo "  - $REPORT_DIR/lizard.txt (¸´ÔÓ¶Č·ÖÎö)"
        echo "  - $REPORT_DIR/jscpd-report/ (ÖŘ¸´Ľě˛âHTML±¨¸ć)"
        echo "  - $REPORT_DIR/lizard-report.html (¸´ÔÓ¶ČHTML±¨¸ć)"
        exit 0
    fi
else
    echo -e "${YELLOW}??  ˛ż·ÖĽě˛éÎ´Ö´ĐĐŁ¬Çë°˛×°Č±Ę§µÄą¤ľß${NC}"
    
    if [ $JSCPD_AVAILABLE -eq 0 ]; then
        echo "ĐčŇŞ°˛×°: npm install -g jscpd"
    fi
    
    if [ $LIZARD_AVAILABLE -eq 0 ]; then
        echo "ĐčŇŞ°˛×°: pip install lizard"
    fi
    
    echo "°˛×°şóÖŘĐÂÔËĐĐĽě˛é"
    exit 0
fi