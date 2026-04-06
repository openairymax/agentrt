#!/usr/bin/env python3
"""
Doxygen 高级标签自动补全工具
用于批量添加缺失的 @ownership 和 @since 标签

使用方法:
    python3 add_advanced_doxygen_tags.py
"""

import os
import re
import sys
from pathlib import Path

# 配置
INCLUDE_DIR = Path("include")
TARGET_TAGS = ["@ownership", "@since"]

# 版本号 (可根据需要修改)
DEFAULT_SINCE_VERSION = "v1.0.0"

# ownership 规则映射表
OWNERSHIP_RULES = {
    # 输入参数: 调用者负责
    "const char*": "调用者负责字符串的生命周期",
    "heapstore_config_t*": "调用者负责 config 的生命周期",
    "heapstore_stats_t*": "调用者负责 stats 的分配和释放",
    "heapstore_metrics_t*": "调用者负责 metrics 的分配和释放",
    "heapstore_circuit_info_t*": "调用者负责 info 的分配和释放",
    "heapstore_agent_record_t*": "调用者负责 record 的生命周期",
    "heapstore_session_record_t*": "调用者负责 record 的生命周期",
    "heapstore_skill_record_t*": "调用者负责 record 的生命周期",
    "heapstore_batch_context_t*": "调用者通过 heapstore_batch_end() 释放",
    
    # 输出参数: 函数分配，调用者释放
    "heapstore_agent_record_t**": "函数内部分配，调用者负责 free()",
    "heapstore_session_record_t**": "函数内部分配，调用者负责 free()",
    "heapstore_skill_record_t**": "函数内部分配，调用者负责 free()",
    "heapstore_span_t**": "函数内部分配，调用者负责 free()",
    "uint64_t*": "调用者负责内存的分配和释放",
    "size_t*": "调用者负责变量的生命周期",
    "bool*": "调用者负责变量的生命周期",
    "char*": "调用者负责缓冲区的分配和释放",
}

# 线程安全规则
THREADSAFE_RULES = {
    # 使用全局锁的函数
    "init": ("是", "否"),
    "shutdown": ("是", "否"),
    # 纯线程安全函数
    "get_": ("是", "是"),
    "is_": ("是", "is"),
    "strerror": ("是", "是"),
    # 需要注意的函数
    "batch_begin": ("是", "否"),
    "batch_end": ("否", "否"),  # 批量上下文不应跨线程共享
}


def detect_ownership_from_params(func_decl):
    """
    从函数声明中推断所有权规则
    """
    if not func_decl:
        return None
    
    for pattern, rule in OWNERSHIP_RULES.items():
        if pattern in func_decl:
            return rule
    
    return "内部管理所有资源"


def detect_threadsafe_from_name(func_name):
    """
    从函数名推断线程安全性
    """
    if not func_name:
        return ("是", "否")
    
    for prefix, (ts, reent) in THREADSAFE_RULES.items():
        if prefix in func_name.lower():
            return (ts, reent)
    
    # 默认: 大多数公共API是线程安全的
    return ("是", "否")


def find_functions_without_tag(filepath, tag):
    """
    查找缺少指定标签的函数
    """
    with open(filepath, 'r', encoding='utf-8') as f:
        content = f.read()
    
    functions_missing = []
    
    # 匹配 Doxygen 注释块 + 函数声明的模式
    pattern = r'(/\*\*[\s\S]*?\*/)\s*\n\s*(^\w+[\s\*]+\w+\s*\([^)]*\)\s*;'
    
    matches = re.finditer(pattern, content, re.MULTILINE)
    
    for match in matches:
        doc_block = match.group(1)
        func_decl = match.group(2).strip()
        
        if tag not in doc_block:
            # 提取函数名
            func_match = re.search(r'\b(\w+)\s*\(', func_decl)
            if func_match:
                func_name = func_match.group(1)
                start_pos = match.start()
                end_pos = match.end()
                
                functions_missing.append({
                    'name': func_name,
                    'decl': func_decl,
                    'doc': doc_block,
                    'start': start_pos,
                    'end': end_pos,
                    'tag': tag
                })
    
    return functions_missing


def generate_ownership_text(func_decl):
    """生成@ownership标签文本"""
    return detect_ownership_from_params(func_decl) or "内部管理资源"


def generate_since_text():
    """生成@since标签文本"""
    return DEFAULT_SINCE_VERSION


def add_tags_to_file(filepath, dry_run=False):
    """
    为单个文件添加缺失的标签
    """
    modified = False
    added_count = {tag: 0 for tag in TARGET_TAGS}
    
    print(f"\n处理文件: {filepath}")
    
    with open(filepath, 'r', encoding='utf-8') as f:
        content = f.read()
    
    original_content = content
    
    for tag in TARGET_TAGS:
        missing_funcs = find_functions_without_tag(filepath, tag)
        
        if missing_funcs:
            print(f"  发现 {len(missing_funcs)} 个函数缺少 @{tag.replace('@', '')} 标签")
            
            for func_info in reversed(missing_funcs):  # 反向遍历避免位置偏移
                doc_block = func_info['doc']
                func_decl = func_info['decl']
                
                # 在 */ 前插入新标签
                if tag == "@ownership":
                    new_tag_line = f" * @ownership {generate_ownership_text(func_decl)}\n"
                elif tag == "@since":
                    new_tag_line = f" * @since {generate_since_text()}\n"
                else:
                    continue
                
                # 找到 */ 的位置并插入
                insert_pos = func_info['doc'].rfind("*/")
                if insert_pos > 0:
                    new_doc = doc_block[:insert_pos] + new_tag_line + doc_block[insert_pos:]
                    
                    # 替换原内容
                    old_text = content[func_info['start']:func_info['end']]
                    new_text = new_doc + "\n" + func_decl + ";"
                    
                    content = content[:func_info['start']] + new_text + content[func_info['end']:]
                    added_count[tag] += 1
                    modified = True
    
    if modified and not dry_run:
        with open(filepath, 'w', encoding='utf-8') as f:
            f.write(content)
        print(f"  ✅ 已添加标签: {added_count}")
    elif dry_run:
        print(f"  📋 [DRY RUN] 将添加: {added_count}")
    
    return modified, added_count


def main():
    """主函数"""
    print("=" * 60)
    print("Doxygen 高级标签自动补全工具")
    print("=" * 60)
    
    dry_run = "--dry-run" in sys.argv
    
    total_added = {tag: 0 for tag in TARGET_TAGS}
    files_modified = 0
    
    # 处理所有头文件
    header_files = list(INCLUDE_DIR.glob("*.h"))
    
    print(f"\n扫描目录: {INCLUDE_DIR}")
    print(f"发现头文件: {len(header_files)} 个")
    print(f"模式: {'预览 (DRY RUN)' if dry_run else '实际修改'}")
    
    for h_file in sorted(header_files):
        try:
            modified, counts = add_tags_to_file(h_file, dry_run)
            if modified:
                files_modified += 1
                for tag, count in counts.items():
                    total_added[tag] += count
        except Exception as e:
            print(f"  ⚠️  处理文件出错: {h_file.name} - {e}")
    
    # 输出统计
    print("\n" + "=" * 60)
    print("处理结果统计")
    print("=" * 60)
    print(f"修改文件数: {files_modified}/{len(header_files)}")
    print()
    for tag in TARGET_TAGS:
        tag_name = tag.replace('@', '')
        print(f"@{tag_name} 标签: +{total_added[tag]} 个")
    
    # 计算新的覆盖率
    total_funcs = sum(
        len(re.findall(r'^\w+[\s\*]+\w+\s*\(', open(f, 'r').read(), re.MULTILINE))
        for f in header_files
    )
    
    print()
    print("预估新覆盖率:")
    for tag in TARGET_TAGS:
        tag_name = tag.replace('@', '')
        current = len(re.findall(rf'{tag}', 
            ''.join(open(f, 'r').read() for f in header_files)))
        
        if not dry_run:
            new_total = current
        else:
            new_total = current + total_added[tag]
        
        pct = min(100, (new_total / max(total_funcs, 1)) * 100)
        status = "✅" if pct >= 90 else "⚠️ "
        print(f"  {status} @{tag_name}: ~{pct:.1f}% ({new_total}/{total_funcs})")
    
    print()
    if dry_run:
        print("💡 提示: 使用 --dry-run 参数仅预览，不加参数将实际修改文件")
    else:
        print("✅ 所有修改已保存!")
    
    return 0


if __name__ == "__main__":
    sys.exit(main())
