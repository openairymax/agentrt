#!/usr/bin/env python3
"""
AgentOS 智能体应用框架使用示例

本示例展示如何使用AgentApplication构建完整的智能体应用，
包括生命周期管理、状态管理、配置管理、错误处理、事件系统、
技能框架、任务编排和插件系统。

运行方式:
    python framework_usage_example.py
"""

import asyncio
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent.parent))

from agentos.framework import (
    AgentApplication,
    AgentConfig,
    AgentContext,
    ExecutionContext,
    ExecutionResult,
    ErrorCode,
    LifecycleManager,
    AgentState,
    StateManager,
    ConfigurationCenter,
    ErrorHandlingFramework,
    ErrorLevel,
)


async def example_basic_usage():
    """示例1: 基本使用 - 完整生命周期"""
    print("\n" + "=" * 60)
    print("示例1: 基本使用 - 完整生命周期")
    print("=" * 60)

    app = AgentApplication(AgentConfig(
        name="demo_agent",
        version="1.0.0",
        description="Demo agent for framework showcase"
    ))

    await app.initialize()
    print(f"  初始化完成，状态: {app.state.value}")

    await app.start()
    print(f"  启动成功，状态: {app.state.value}")

    result = await app.execute("Hello, AgentOS Framework!")
    if result.success:
        print(f"  执行成功: {result.output}")
        print(f"  执行时间: {result.execution_time_ms:.2f}ms")
    else:
        print(f"  执行失败: {result.error.message}")

    await app.pause()
    print(f"  已暂停，状态: {app.state.value}")

    await app.resume()
    print(f"  已恢复，状态: {app.state.value}")

    info = await app.get_info()
    print(f"\n  应用信息:")
    print(f"    ID: {info['app_id'][:8]}...")
    print(f"    名称: {info['name']}")
    print(f"    版本: {info['version']}")
    print(f"    运行状态: {info['is_running']}")

    health = await app.health_check()
    print(f"  健康检查: {health['status']}")

    await app.stop()
    print(f"  已停止，状态: {app.state.value}")

    await app.destroy()
    print(f"  已销毁")


async def example_agent_context():
    """示例2: AgentContext 统一上下文"""
    print("\n" + "=" * 60)
    print("示例2: AgentContext 统一上下文")
    print("=" * 60)

    ctx = AgentContext(
        session_id="session-001",
        user_id="user-alice",
        user_permissions=["read", "write", "admin"],
        input_data="Analyze this data"
    )

    print(f"  请求ID: {ctx.request_id[:8]}...")
    print(f"  会话ID: {ctx.session_id}")
    print(f"  用户ID: {ctx.user_id}")
    print(f"  权限: {ctx.user_permissions}")
    print(f"  追踪ID: {ctx.trace_id[:8]}...")

    frame1 = ctx.push_frame("agent", "execute", input_type="str")
    print(f"\n  压入帧: {frame1.component}/{frame1.action}")

    frame2 = ctx.push_frame("skill", "analyze", skill_name="data_analyzer")
    print(f"  压入帧: {frame2.component}/{frame2.action}")

    ctx.scratchpad["intermediate_result"] = [1, 2, 3]
    print(f"  便签本: {ctx.scratchpad}")

    popped = ctx.pop_frame()
    print(f"\n  弹出帧: {popped.component}/{popped.action}")
    print(f"  当前栈深度: {len(ctx.execution_stack)}")

    exec_ctx = ExecutionContext(
        request_id=ctx.request_id,
        session_id=ctx.session_id,
        user_id=ctx.user_id,
        input_data=ctx.input_data
    )
    converted = exec_ctx.to_agent_context()
    print(f"\n  ExecutionContext -> AgentContext 转换:")
    print(f"    请求ID匹配: {converted.request_id == ctx.request_id}")


async def example_error_code():
    """示例3: ErrorCode 统一错误码"""
    print("\n" + "=" * 60)
    print("示例3: ErrorCode 统一错误码")
    print("=" * 60)

    app = AgentApplication(AgentConfig(name="error_code_demo"))
    await app.initialize()

    result = await app.execute("test")
    print(f"  正常执行: success={result.success}, error_code={result.error_code.name}")

    result = ExecutionResult(
        success=False,
        error_code=ErrorCode.NOT_INITIALIZED,
    )
    print(f"  未初始化: error_code={result.error_code.name} ({result.error_code.value})")

    result = ExecutionResult(
        success=False,
        error_code=ErrorCode.TIMEOUT,
    )
    print(f"  超时: error_code={result.error_code.name} ({result.error_code.value})")

    result = ExecutionResult(
        success=False,
        error_code=ErrorCode.PLUGIN_NOT_FOUND,
    )
    print(f"  插件未找到: error_code={result.error_code.name} ({result.error_code.value})")

    print(f"\n  全部错误码:")
    for code in ErrorCode:
        print(f"    {code.name} = {code.value}")

    await app.destroy()


async def example_lifecycle_hooks():
    """示例4: 生命周期钩子"""
    print("\n" + "=" * 60)
    print("示例4: 生命周期钩子")
    print("=" * 60)

    app = AgentApplication(AgentConfig(name="lifecycle_demo"))

    def on_start(metadata):
        print("  钩子: 应用正在启动...")

    def on_stop(metadata):
        print("  钩子: 应用正在停止...")

    def on_state_change(event):
        from_s = event.from_state.value if event.from_state else "None"
        print(f"  状态变更: {from_s} -> {event.to_state.value}")

    app.lifecycle.register_hook("on_start", on_start)
    app.lifecycle.register_hook("on_stop", on_stop)
    app.lifecycle.on_state_change(on_state_change)

    await app.initialize()
    await app.start()
    await app.pause()
    await app.resume()
    await app.stop()
    await app.destroy()


async def example_state_management():
    """示例5: 状态管理"""
    print("\n" + "=" * 60)
    print("示例5: 状态管理")
    print("=" * 60)

    app = AgentApplication(AgentConfig(name="state_demo"))
    await app.initialize()

    state_mgr = app.state_manager

    await state_mgr.set("user.name", "Alice")
    await state_mgr.set("counter", 42)
    await state_mgr.set("settings.theme", "dark")

    name = await state_mgr.get("user.name")
    counter = await state_mgr.get_typed("counter", int)
    print(f"  user.name = {name}")
    print(f"  counter = {counter} (type: int)")

    snapshot = await state_mgr.snapshot(metadata={"reason": "checkpoint"})
    print(f"\n  快照: {snapshot.snapshot_id[:8]}... ({len(snapshot.state_data)} keys)")

    await state_mgr.set("user.name", "Bob")
    restored = await state_mgr.restore(snapshot)
    name_after = await state_mgr.get("user.name")
    print(f"  恢复后 user.name = {name_after} (恢复{'成功' if restored else '失败'})")

    stats = state_mgr.get_stats()
    print(f"  统计: {stats['total_keys']} keys, {stats['total_changes']} changes")

    await app.destroy()


async def example_config_management():
    """示例6: 配置管理"""
    print("\n" + "=" * 60)
    print("示例6: 配置管理")
    print("=" * 60)

    app = AgentApplication(AgentConfig(name="config_demo"))
    await app.initialize()

    config_center = app.config_center

    test_config = {
        "database.host": "localhost",
        "database.port": 5432,
        "api.timeout": 30,
        "features.enable_cache": True,
    }

    count = await config_center.load_from_dict(test_config)
    print(f"  加载了 {count} 个配置项")

    db_host = config_center.get("database.host")
    api_timeout = config_center.get_int("api.timeout")
    enable_cache = config_center.get_bool("features.enable_cache")
    print(f"  database.host = {db_host}")
    print(f"  api.timeout = {api_timeout}")
    print(f"  features.enable_cache = {enable_cache}")

    def on_timeout_changed(old_val, new_val):
        print(f"  配置变更: api.timeout {old_val} -> {new_val}")

    config_center.watch("api.timeout", on_timeout_changed)
    await config_center.set("api.timeout", 60)

    validation_result = await config_center.validate()
    print(f"  验证: {'通过' if validation_result.is_valid else '失败'}")

    await app.destroy()


async def example_event_system():
    """示例7: 事件系统"""
    print("\n" + "=" * 60)
    print("示例7: 事件系统")
    print("=" * 60)

    app = AgentApplication(AgentConfig(name="event_demo"))
    await app.initialize()
    await app.start()

    def on_custom(data):
        print(f"  收到自定义事件: {data}")

    app.on("custom.event", on_custom)

    count = await app.emit("custom.event", {"message": "Hello events!"})
    print(f"  事件已发出，{count} 个监听者收到")

    result = await app.execute("Test with events")
    print(f"  执行结果: success={result.success}")

    event_stats = app.event_bus.get_stats()
    print(f"  EventBus 统计: {event_stats.get('total_published', 0)} published")

    await app.stop()
    await app.destroy()


async def example_skill_execution():
    """示例8: 技能执行"""
    print("\n" + "=" * 60)
    print("示例8: 技能执行（通过execute集成）")
    print("=" * 60)

    app = AgentApplication(AgentConfig(name="skill_demo"))
    await app.initialize()
    await app.start()

    result = await app.execute({
        "skill": "nonexistent_skill",
        "parameters": {"input": "test"}
    })
    print(f"  不存在的技能: success={result.success}")

    result = await app.execute("simple input")
    print(f"  简单输入: success={result.success}, output={result.output}")

    skill_stats = app.skill_registry.get_stats()
    print(f"  技能注册表: {skill_stats.get('total_skills', 0)} skills")

    await app.stop()
    await app.destroy()


async def example_health_check():
    """示例9: 健康检查"""
    print("\n" + "=" * 60)
    print("示例9: 健康检查")
    print("=" * 60)

    app = AgentApplication(AgentConfig(name="health_demo"))
    await app.initialize()

    health = await app.health_check()
    print(f"  未启动时: {health['status']}")
    for k, v in health['details'].items():
        print(f"    {k}: {v}")

    await app.start()

    health = await app.health_check()
    print(f"\n  运行中: {health['status']}")
    for k, v in health['details'].items():
        print(f"    {k}: {v}")

    await app.stop()
    await app.destroy()


async def main():
    print("=" * 70)
    print("AgentOS 智能体应用框架 使用示例")
    print("=" * 70)

    try:
        await example_basic_usage()
        await example_agent_context()
        await example_error_code()
        await example_lifecycle_hooks()
        await example_state_management()
        await example_config_management()
        await example_event_system()
        await example_skill_execution()
        await example_health_check()

        print("\n" + "=" * 70)
        print("所有示例运行完成!")
        print("=" * 70)

    except Exception as e:
        print(f"\n示例运行出错: {e}")
        import traceback
        traceback.print_exc()


if __name__ == "__main__":
    asyncio.run(main())
