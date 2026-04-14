#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""
AgentOS Python SDK 模块导入测试脚本
"""

import sys
import os

# 添加当前目录到路径
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

def test_imports():
    """测试所有模块导入"""
    print("=" * 60)
    print("AgentOS Python SDK v3.0.0 模块导入测试")
    print("=" * 60)
    
    # 测试 client 模块
    print("\n[1/5] 测试 client 模块...")
    try:
        from agentos.client import Client, APIClient, manager, MockClient
        print("  OK client 模块导入成功")
        print(f"    - Client: {Client}")
        print(f"    - APIClient: {APIClient}")
        print(f"    - manager: {manager}")
        print(f"    - MockClient: {MockClient}")
    except Exception as e:
        print(f"  FAIL client 模块导入失败: {e}")
        return False
    
    # 测试 types 模块
    print("\n[2/5] 测试 types 模块...")
    try:
        from agentos.types import (
            TaskStatus, MemoryLayer, SessionStatus, SkillStatus,
            Task, Memory, Session, Skill
        )
        print("  OK types 模块导入成功")
        print(f"    - TaskStatus: {TaskStatus}")
        print(f"    - MemoryLayer: {MemoryLayer}")
        print(f"    - Task: {Task}")
    except Exception as e:
        print(f"  FAIL types 模块导入失败: {e}")
        return False
    
    # 测试 utils 模块
    print("\n[3/5] 测试 utils 模块...")
    try:
        from agentos.utils import (
            get_string, get_int, get_float, get_bool,
            build_url, generate_id, extract_data_map
        )
        print("  OK utils 模块导入成功")
        print(f"    - get_string: {get_string}")
        print(f"    - build_url: {build_url}")
        print(f"    - generate_id: {generate_id}")
    except Exception as e:
        print(f"  FAIL utils 模块导入失败: {e}")
        return False
    
    # 测试 modules 模块
    print("\n[4/5] 测试 modules 模块...")
    try:
        from agentos.modules import (
            TaskManager, MemoryManager, SessionManager, SkillManager
        )
        print("  OK modules 模块导入成功")
        print(f"    - TaskManager: {TaskManager}")
        print(f"    - MemoryManager: {MemoryManager}")
        print(f"    - SessionManager: {SessionManager}")
        print(f"    - SkillManager: {SkillManager}")
    except Exception as e:
        print(f"  FAIL modules 模块导入失败: {e}")
        return False
    
    # 测试顶层模块导入
    print("\n[5/5] 测试顶层模块导入...")
    try:
        from agentos import (
            Client, TaskManager, MemoryManager, SessionManager, SkillManager,
            TaskStatus, MemoryLayer, Task, Memory, Session, Skill,
            CODE_TASK_FAILED, AgentOSError, MockClient
        )
        print("  OK 顶层模块导入成功")
    except Exception as e:
        print(f"  FAIL 顶层模块导入失败: {e}")
        return False
    
    # 测试向后兼容
    print("\n[向后兼容] 测试向后兼容导入...")
    try:
        from agentos import AgentOS, AsyncAgentOS
        print("  OK 向后兼容导入成功")
        print(f"    - AgentOS: {AgentOS}")
        print(f"    - AsyncAgentOS: {AsyncAgentOS}")
    except Exception as e:
        print(f"  FAIL 向后兼容导入失败: {e}")
        return False
    
    print("\n" + "=" * 60)
    print("所有模块导入测试通过!")
    print("=" * 60)
    return True


def test_functionality():
    """测试基本功能"""
    print("\n" + "=" * 60)
    print("基本功能测试")
    print("=" * 60)
    
    # 测试类型
    print("\n[类型测试]")
    from agentos.types import TaskStatus, MemoryLayer
    print(f"  TaskStatus.PENDING: {TaskStatus.PENDING}")
    print(f"  TaskStatus.PENDING.is_terminal(): {TaskStatus.PENDING.is_terminal()}")
    print(f"  TaskStatus.COMPLETED.is_terminal(): {TaskStatus.COMPLETED.is_terminal()}")
    print(f"  MemoryLayer.L1: {MemoryLayer.L1}")
    print(f"  MemoryLayer.L1.is_valid(): {MemoryLayer.L1.is_valid()}")
    
    # 测试工具函数
    print("\n[工具函数测试]")
    from agentos.utils import get_string, get_int, build_url, generate_id
    test_dict = {"name": "test", "count": 42}
    print(f"  get_string({{'name': 'test', 'count': 42}}, 'name'): {get_string(test_dict, 'name')}")
    print(f"  get_int({{'name': 'test', 'count': 42}}, 'count'): {get_int(test_dict, 'count')}")
    print(f"  build_url('/api/v1/tasks', {{'page': '1'}}): {build_url('/api/v1/tasks', {'page': '1'})}")
    print(f"  generate_id(): {generate_id()}")
    
    # 测试 Mock 客户端
    print("\n[Mock 客户端测试]")
    from agentos.client import MockClient, APIResponse
    mock = MockClient()
    mock.get_handler = lambda path, opts: APIResponse(success=True, data={"id": "123"})
    response = mock.get("/api/v1/test")
    print(f"  Mock response: success={response.success}, data={response.data}")
    
    print("\n" + "=" * 60)
    print("基本功能测试通过!")
    print("=" * 60)


if __name__ == "__main__":
    success = test_imports()
    if success:
        test_functionality()
    else:
        sys.exit(1)
