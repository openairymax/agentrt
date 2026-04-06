# AgentOS Python SDK - 长时间任务示例（带检查点）
# Version: 3.0.0
# Last updated: 2026-04-05
#
# 演示如何使用 CheckpointManager 实现长时间任务的断点续传

import time
import random
from typing import Dict, Any, List

from agentos.client import Client
from agentos.modules.task.manager import TaskManager
from agentos.modules.task.checkpoint import CheckpointManager


def long_running_task_example(
    task_id: str,
    total_steps: int = 1000,
    checkpoint_interval: int = 50
):
    """
    长时间运行任务示例（带检查点）
    
    Args:
        task_id: 任务 ID
        total_steps: 总步骤数
        checkpoint_interval: 检查点间隔
    
    Example:
        >>> long_running_task_example("task-123", total_steps=1000)
    """
    # 初始化客户端和检查点管理器
    client = Client()
    checkpoint_mgr = CheckpointManager("/tmp/agentos_checkpoints")
    
    # 尝试恢复上次的进度
    last_checkpoint = checkpoint_mgr.load_checkpoint(task_id)
    start_step = 0
    current_state: Dict[str, Any] = {}
    
    if last_checkpoint:
        start_step = int(last_checkpoint.progress * total_steps)
        current_state = last_checkpoint.state
        print(f"✓ 从检查点恢复：进度{last_checkpoint.progress:.1%}, 步骤{start_step}/{total_steps}")
        print(f"  检查点时间：{last_checkpoint.timestamp}")
        print(f"  检查点 ID: {last_checkpoint.checkpoint_id}")
    else:
        print(f"开始新任务：{task_id}, 总步骤：{total_steps}")
    
    # 执行任务
    for step in range(start_step, total_steps):
        try:
            # === 模拟任务处理 ===
            # 在实际应用中，这里是具体的业务逻辑
            result = process_step(step, current_state)
            
            # 更新状态
            current_state[f"step_{step}_result"] = result
            current_state["last_step"] = step
            current_state["total_processed"] = step - start_step + 1
            
            # === 定期创建检查点 ===
            if (step + 1) % checkpoint_interval == 0:
                progress = (step + 1) / total_steps
                
                checkpoint = checkpoint_mgr.create_checkpoint(
                    task_id=task_id,
                    state=current_state.copy(),
                    progress=progress,
                    metadata={
                        "stage": "processing",
                        "last_step": str(step),
                        "items_processed": str(step - start_step + 1)
                    }
                )
                
                print(f"✓ 检查点已保存：{checkpoint.checkpoint_id[:8]}... "
                      f"进度：{progress:.1%} ({step + 1}/{total_steps})")
            
            # 模拟处理延迟
            time.sleep(0.01)
            
        except Exception as e:
            print(f"✗ 步骤 {step} 处理失败：{e}")
            
            # 失败时创建紧急检查点
            emergency_checkpoint = checkpoint_mgr.create_checkpoint(
                task_id=task_id,
                state=current_state,
                progress=step / total_steps,
                metadata={
                    "stage": "error",
                    "error": str(e),
                    "failed_step": str(step)
                }
            )
            
            print(f"✓ 紧急检查点已保存：{emergency_checkpoint.checkpoint_id[:8]}...")
            raise
    
    # 任务完成
    final_checkpoint = checkpoint_mgr.create_checkpoint(
        task_id=task_id,
        state=current_state,
        progress=1.0,
        metadata={
            "stage": "completed",
            "completion_time": time.strftime("%Y-%m-%d %H:%M:%S"),
            "total_steps": str(total_steps)
        }
    )
    
    print(f"\n✓ 任务完成！")
    print(f"  最终检查点：{final_checkpoint.checkpoint_id[:8]}...")
    print(f"  总处理步骤：{total_steps}")
    
    # 清理旧检查点（保留最近 3 个，最多保留 12 小时）
    deleted = checkpoint_mgr.cleanup_old_checkpoints(
        task_id=task_id,
        keep_latest=3,
        max_age_hours=12
    )
    print(f"  清理了 {deleted} 个旧检查点")
    
    return current_state


def process_step(step: int, state: Dict[str, Any]) -> Dict[str, Any]:
    """
    处理单个步骤（示例逻辑）
    
    Args:
        step: 当前步骤
        state: 当前状态
    
    Returns:
        dict: 处理结果
    """
    # 模拟业务逻辑
    result = {
        "step": step,
        "timestamp": time.time(),
        "data": f"processed_data_{step}",
        "value": random.randint(1, 100)
    }
    
    # 模拟可能的失败（5% 概率）
    if random.random() < 0.05:
        raise RuntimeError(f"模拟失败：步骤 {step}")
    
    return result


def resume_failed_task(task_id: str, total_steps: int = 1000):
    """
    恢复失败的任务
    
    Args:
        task_id: 任务 ID
        total_steps: 总步骤数
    
    Example:
        >>> resume_failed_task("task-123")
    """
    checkpoint_mgr = CheckpointManager("/tmp/agentos_checkpoints")
    
    # 查找失败的检查点
    checkpoints = checkpoint_mgr.list_checkpoints(task_id)
    
    if not checkpoints:
        print(f"✗ 未找到任务 {task_id} 的检查点")
        return
    
    # 查找最后一个失败的检查点
    failed_checkpoint = None
    for cp in checkpoints:
        if cp.metadata.get("stage") == "error":
            failed_checkpoint = cp
            break
    
    if failed_checkpoint:
        print(f"发现失败的检查点：")
        print(f"  检查点 ID: {failed_checkpoint.checkpoint_id}")
        print(f"  失败时间：{failed_checkpoint.timestamp}")
        print(f"  失败步骤：{failed_checkpoint.metadata.get('failed_step')}")
        print(f"  错误信息：{failed_checkpoint.metadata.get('error')}")
        print(f"  进度：{failed_checkpoint.progress:.1%}")
        
        # 恢复到失败前的状态
        state_before_failure = checkpoint_mgr.load_checkpoint(
            task_id,
            checkpoint_id=failed_checkpoint.checkpoint_id
        )
        
        if state_before_failure:
            print(f"\n准备从步骤 {state_before_failure.state.get('last_step')} 恢复执行...")
            # 继续执行任务
            long_running_task_example(task_id, total_steps)
    else:
        print(f"任务 {task_id} 没有失败的检查点，可能已完成或从未执行")


def batch_processing_with_checkpoints(
    task_id: str,
    items: List[Any],
    batch_size: int = 100
):
    """
    批量处理任务（带检查点）
    
    Args:
        task_id: 任务 ID
        items: 待处理项列表
        batch_size: 每批处理数量
    
    Example:
        >>> items = list(range(10000))
        >>> batch_processing_with_checkpoints("batch-123", items)
    """
    checkpoint_mgr = CheckpointManager("/tmp/agentos_checkpoints")
    
    # 恢复进度
    last_checkpoint = checkpoint_mgr.load_checkpoint(task_id)
    processed_index = 0
    processed_items = []
    
    if last_checkpoint:
        processed_index = last_checkpoint.state.get("processed_index", 0)
        processed_items = last_checkpoint.state.get("processed_items", [])
        print(f"✓ 恢复批量处理：已处理 {processed_index}/{len(items)} 项")
    
    # 分批处理
    total_batches = (len(items) + batch_size - 1) // batch_size
    
    for batch_idx in range(processed_index // batch_size, total_batches):
        start_idx = batch_idx * batch_size
        end_idx = min(start_idx + batch_size, len(items))
        
        batch = items[start_idx:end_idx]
        
        # 处理批次
        for i, item in enumerate(batch):
            result = process_item(item)
            processed_items.append(result)
        
        # 更新进度
        current_index = end_idx
        progress = current_index / len(items)
        
        # 创建检查点
        checkpoint_mgr.create_checkpoint(
            task_id=task_id,
            state={
                "processed_index": current_index,
                "processed_items": processed_items,
                "last_batch_idx": batch_idx
            },
            progress=progress,
            metadata={
                "stage": "batch_processing",
                "current_batch": str(batch_idx),
                "total_batches": str(total_batches)
            }
        )
        
        print(f"✓ 批次 {batch_idx + 1}/{total_batches} 完成 "
              f"(进度：{progress:.1%})")
    
    print(f"\n✓ 批量处理完成！共处理 {len(processed_items)} 项")
    return processed_items


def process_item(item: Any) -> Dict[str, Any]:
    """处理单个项目（示例）"""
    return {
        "item": item,
        "result": f"processed_{item}",
        "timestamp": time.time()
    }


if __name__ == "__main__":
    # 示例 1: 长时间任务
    print("=" * 60)
    print("示例 1: 长时间运行任务（带检查点）")
    print("=" * 60)
    long_running_task_example("demo-task-001", total_steps=200, checkpoint_interval=20)
    
    # 示例 2: 批量处理
    print("\n" + "=" * 60)
    print("示例 2: 批量处理任务（带检查点）")
    print("=" * 60)
    items = list(range(500))
    batch_processing_with_checkpoints("demo-batch-001", items, batch_size=50)
