#!/usr/bin/env python3
"""
更新manager模块中的openlab路径引用
将 openlab/contrib/ 更新为 agentos/openlab/contrib/
"""

import os
import re
import sys

def update_file(file_path):
    """更新单个文件中的openlab路径"""
    try:
        with open(file_path, 'r', encoding='utf-8') as f:
            content = f.read()
        
        # 统计替换前的情况
        old_pattern = r'openlab/contrib/(agents|skills)/'
        matches_before = len(re.findall(old_pattern, content))
        
        if matches_before == 0:
            print(f"  文件 {file_path} 中未找到需要替换的路径")
            return 0
        
        # 执行替换
        new_content = re.sub(
            r'openlab/contrib/(agents|skills)/',
            r'agentos/openlab/contrib/\1/',
            content
        )
        
        # 检查是否实际更改
        if new_content == content:
            print(f"  文件 {file_path} 无需更改")
            return 0
        
        # 创建备份
        backup_path = file_path + '.bak'
        with open(backup_path, 'w', encoding='utf-8') as f:
            f.write(content)
        
        # 写入更新后的内容
        with open(file_path, 'w', encoding='utf-8') as f:
            f.write(new_content)
        
        # 统计替换后的情况
        matches_after = len(re.findall(old_pattern, new_content))
        changes = matches_before - matches_after
        
        print(f"  ✅ 更新 {file_path}: {changes} 处路径已修复")
        print(f"     备份文件: {backup_path}")
        
        return changes
        
    except Exception as e:
        print(f"  ❌ 处理文件 {file_path} 时出错: {e}")
        return 0

def main():
    """主函数"""
    base_dir = "d:/SPHARX-CN/SpharxWorks/AgentOS/agentos/manager"
    
    # 需要更新的文件
    files_to_update = [
        os.path.join(base_dir, "skill", "registry.yaml"),
        os.path.join(base_dir, "agent", "registry.yaml")
    ]
    
    print("🔍 开始更新manager模块中的openlab路径引用")
    print("=" * 60)
    
    total_changes = 0
    total_files = 0
    
    for file_path in files_to_update:
        if not os.path.exists(file_path):
            print(f"❌ 文件不存在: {file_path}")
            continue
        
        print(f"\n📄 处理文件: {os.path.relpath(file_path, base_dir)}")
        changes = update_file(file_path)
        
        if changes > 0:
            total_changes += changes
            total_files += 1
    
    print("\n" + "=" * 60)
    print(f"📊 更新完成:")
    print(f"  ✅ 更新文件数: {total_files}")
    print(f"  🔄 修复路径数: {total_changes}")
    print(f"  💾 备份文件已保存为 .bak 扩展名")
    
    if total_changes == 0:
        print("\n⚠️  未找到需要修复的路径引用")
        print("   检查路径模式是否为 'openlab/contrib/(agents|skills)/'")
    
    return 0

if __name__ == "__main__":
    sys.exit(main())