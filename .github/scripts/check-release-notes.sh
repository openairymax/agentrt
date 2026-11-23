#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2026 SPHARX Ltd.
# SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0
#
# 发布正文的公开面契约（R30）：三段固定骨架（概述/说明/其他），概述段写
# 版本与日期，说明段不超过 3 行、其他段不超过 3 行，全文不超过 12 行。
# 这里是该契约的唯一实现：发布装箱（release-package）与常规推送
# （quality-gate）都调用本脚本，避免两处各写一份判断后逐渐分叉。
#
# 用法：check-release-notes.sh <notes-file>
set -euo pipefail

file="${1:-}"
if [ -z "$file" ]; then
    echo "usage: check-release-notes.sh <notes-file>" >&2
    exit 2
fi
if [ ! -f "$file" ]; then
    echo "::error::缺发布说明 ${file}（发行正文 SSoT 必须随 tag 入库）"
    exit 1
fi

lines="$(wc -l < "$file")"
if [ "$lines" -gt 12 ]; then
    echo "::error::发布说明 ${lines} 行，超出 12 行上限"
    exit 1
fi

heads="$(grep '^## ' "$file" | tr -d ' ' | paste -sd, -)"
if [ "$heads" != "##概述,##说明,##其他" ]; then
    echo "::error::发布说明须且仅须三段标题（概述/说明/其他），实际：${heads:-<无>}"
    exit 1
fi

section_lines() {
    awk -v want="$1" '
        /^## / { in_sec = ($0 ~ ("^## " want)); next }
        in_sec && NF { n++ }
        END { print n + 0 }
    ' "$file"
}

for sec in 说明 其他; do
    n="$(section_lines "$sec")"
    if [ "$n" -gt 3 ]; then
        echo "::error::发布说明「${sec}」段 ${n} 行，超出 3 行上限"
        exit 1
    fi
done

if [ "$(section_lines 概述)" -lt 2 ]; then
    echo "::error::发布说明「概述」段须写出正式版本号与发布日期"
    exit 1
fi

echo "发布说明通过：${file}（${lines} 行，三段）"
