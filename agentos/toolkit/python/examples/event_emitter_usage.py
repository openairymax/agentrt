# AgentOS Python SDK - Event Emitter 使用示例
# Version: 3.0.0
# Last updated: 2026-04-05
#
# Event Emitter 使用示例

from agentos.utils.event_emitter import (
    EventEmitter,
    AsyncEventEmitter,
    Event,
    emit_event,
    get_global_emitter,
    BuiltinEvents
)
import time


def example_basic_usage():
    """基础用法示例"""
    print("=" * 60)
    print("示例 1: 基础事件订阅和发布")
    print("=" * 60)
    
    emitter = EventEmitter()
    
    # 订阅事件
    def on_task_created(event: Event):
        print(f"✓ 任务创建：{event.data}")
    
    emitter.on(BuiltinEvents.TASK_CREATED, on_task_created)
    
    # 发布事件
    emitter.emit(
        BuiltinEvents.TASK_CREATED,
        data={"task_id": "123", "description": "分析数据"},
        source="task_manager"
    )
    
    # 取消订阅
    emitter.off(BuiltinEvents.TASK_CREATED, on_task_created)
    print()


def example_priority():
    """优先级示例"""
    print("=" * 60)
    print("示例 2: 事件优先级")
    print("=" * 60)
    
    emitter = EventEmitter()
    
    def low_priority(event: Event):
        print(f"  低优先级：{event.data}")
    
    def high_priority(event: Event):
        print(f"  高优先级：{event.data}")
    
    # 注册不同优先级的监听器
    emitter.on("task.run", low_priority, priority=10)
    emitter.on("task.run", high_priority, priority=1)  # 数字越小优先级越高
    
    # 发布事件（高优先级先执行）
    emitter.emit("task.run", data="执行任务")
    print()


def example_once():
    """一次性事件示例"""
    print("=" * 60)
    print("示例 3: 一次性事件")
    print("=" * 60)
    
    emitter = EventEmitter()
    
    def on_complete(event: Event):
        print(f"✓ 任务完成：{event.data}")
    
    # 只执行一次
    emitter.once(BuiltinEvents.TASK_COMPLETED, on_complete)
    
    # 第一次发布（会执行）
    emitter.emit(BuiltinEvents.TASK_COMPLETED, data="任务 1")
    
    # 第二次发布（不会执行）
    emitter.emit(BuiltinEvents.TASK_COMPLETED, data="任务 2")
    print()


def example_filter():
    """事件过滤示例"""
    print("=" * 60)
    print("示例 4: 事件过滤器")
    print("=" * 60)
    
    emitter = EventEmitter()
    
    # 只处理 priority > 5 的任务
    def high_priority_filter(event: Event) -> bool:
        return event.data.get("priority", 0) > 5
    
    def on_task(event: Event):
        print(f"✓ 处理高优先级任务：{event.data['name']}")
    
    emitter.on(
        BuiltinEvents.TASK_STARTED,
        on_task,
        filter_func=high_priority_filter
    )
    
    # 这些会被处理
    emitter.emit(BuiltinEvents.TASK_STARTED, {"name": "重要任务", "priority": 8})
    emitter.emit(BuiltinEvents.TASK_STARTED, {"name": "紧急任务", "priority": 10})
    
    # 这些不会被处理
    emitter.emit(BuiltinEvents.TASK_STARTED, {"name": "普通任务", "priority": 3})
    print()


def example_async():
    """异步事件示例"""
    print("=" * 60)
    print("示例 5: 异步事件处理")
    print("=" * 60)
    
    import asyncio
    
    async def main():
        emitter = AsyncEventEmitter()
        
        async def on_task_created(event: Event):
            # 模拟异步操作
            await asyncio.sleep(0.1)
            print(f"✓ 异步处理：{event.data}")
        
        emitter.on(BuiltinEvents.TASK_CREATED, on_task_created)
        
        # 并发发布多个事件
        await asyncio.gather(
            emitter.emit(BuiltinEvents.TASK_CREATED, {"id": 1}),
            emitter.emit(BuiltinEvents.TASK_CREATED, {"id": 2}),
            emitter.emit(BuiltinEvents.TASK_CREATED, {"id": 3})
        )
    
    asyncio.run(main())
    print()


def example_decorator():
    """装饰器示例"""
    print("=" * 60)
    print("示例 6: 使用装饰器自动发布事件")
    print("=" * 60)
    
    emitter = get_global_emitter()
    
    # 订阅所有任务创建事件
    def log_task_creation(event: Event):
        print(f"📝 日志：创建了任务 {event.data['result'].id}")
    
    emitter.on(BuiltinEvents.TASK_CREATED, log_task_creation)
    
    # 使用装饰器
    @emit_event(BuiltinEvents.TASK_CREATED, emitter)
    def create_task(description: str):
        class Task:
            def __init__(self, desc):
                self.id = f"task-{int(time.time())}"
                self.description = desc
        
        return Task(description)
    
    # 创建任务会自动发布事件
    task = create_task("分析数据")
    print()


def example_stats():
    """统计信息示例"""
    print("=" * 60)
    print("示例 7: 事件统计")
    print("=" * 60)
    
    emitter = EventEmitter()
    
    # 订阅多个事件
    emitter.on("event1", lambda e: None)
    emitter.on("event1", lambda e: None)
    emitter.on("event2", lambda e: None)
    
    # 发布事件
    emitter.emit("event1", "data1")
    emitter.emit("event2", "data2")
    emitter.emit("event1", "data3")
    
    # 获取统计
    stats = emitter.get_stats()
    print(f"总事件数：{stats['total_events']}")
    print(f"事件类型：{stats['event_types']}")
    print(f"监听器分布：{stats['listeners']}")
    print()


def example_real_world():
    """真实场景示例"""
    print("=" * 60)
    print("示例 8: 真实场景 - 任务生命周期监控")
    print("=" * 60)
    
    emitter = EventEmitter()
    
    # 任务监控器
    class TaskMonitor:
        def __init__(self, emitter):
            self.tasks = {}
            emitter.on(BuiltinEvents.TASK_CREATED, self.on_created)
            emitter.on(BuiltinEvents.TASK_STARTED, self.on_started)
            emitter.on(BuiltinEvents.TASK_COMPLETED, self.on_completed)
            emitter.on(BuiltinEvents.TASK_FAILED, self.on_failed)
        
        def on_created(self, event: Event):
            task_id = event.data["task_id"]
            self.tasks[task_id] = {"status": "created", "time": event.timestamp}
            print(f"📦 任务 {task_id} 已创建")
        
        def on_started(self, event: Event):
            task_id = event.data["task_id"]
            self.tasks[task_id]["status"] = "running"
            self.tasks[task_id]["start_time"] = event.timestamp
            print(f"▶️  任务 {task_id} 开始执行")
        
        def on_completed(self, event: Event):
            task_id = event.data["task_id"]
            self.tasks[task_id]["status"] = "completed"
            duration = event.timestamp - self.tasks[task_id]["start_time"]
            print(f"✅ 任务 {task_id} 完成（耗时 {duration:.2f}s）")
        
        def on_failed(self, event: Event):
            task_id = event.data["task_id"]
            self.tasks[task_id]["status"] = "failed"
            self.tasks[task_id]["error"] = event.data["error"]
            print(f"❌ 任务 {task_id} 失败：{event.data['error']}")
        
        def get_stats(self):
            total = len(self.tasks)
            completed = sum(1 for t in self.tasks.values() if t["status"] == "completed")
            failed = sum(1 for t in self.tasks.values() if t["status"] == "failed")
            return {"total": total, "completed": completed, "failed": failed}
    
    # 创建监控器
    monitor = TaskMonitor(emitter)
    
    # 模拟任务生命周期
    emitter.emit(BuiltinEvents.TASK_CREATED, {"task_id": "task-1"})
    emitter.emit(BuiltinEvents.TASK_STARTED, {"task_id": "task-1"})
    time.sleep(0.1)
    emitter.emit(BuiltinEvents.TASK_COMPLETED, {"task_id": "task-1"})
    
    emitter.emit(BuiltinEvents.TASK_CREATED, {"task_id": "task-2"})
    emitter.emit(BuiltinEvents.TASK_STARTED, {"task_id": "task-2"})
    emitter.emit(BuiltinEvents.TASK_FAILED, {"task_id": "task-2", "error": "超时"})
    
    # 打印统计
    stats = monitor.get_stats()
    print(f"\n📊 任务统计：{stats}")
    print()


if __name__ == "__main__":
    example_basic_usage()
    example_priority()
    example_once()
    example_filter()
    example_async()
    example_decorator()
    example_stats()
    example_real_world()
    
    print("=" * 60)
    print("所有示例执行完成！")
    print("=" * 60)
