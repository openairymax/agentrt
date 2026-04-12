# AgentOS State Manager
# Version: 2.0.0
# Last updated: 2026-04-11

"""
智能体状态管理器

提供统一的状态存储、查询、变更通知和持久化能力。
支持类型安全的状态访问、历史记录和快照功能。

设计原则:
1. 响应式 - 状态变更自动通知订阅者
2. 类型安全 - 支持泛型和类型检查
3. 可观测性 - 完整的变更历史和审计日志
4. 持久化支持 - 可选的状态快照和恢复
"""

import asyncio
import copy
import json
import logging
import time
import uuid
from dataclasses import dataclass, field
from datetime import datetime
from typing import Any, Callable, Dict, List, Optional, Type, TypeVar, Union

logger = logging.getLogger(__name__)

T = TypeVar('T')


@dataclass
class StateSnapshot:
    """状态快照"""
    snapshot_id: str = field(default_factory=lambda: str(uuid.uuid4()))
    timestamp: datetime = field(default_factory=datetime.now)
    state_data: Dict[str, Any] = field(default_factory=dict)
    version: int = 0
    metadata: Dict[str, Any] = field(default_factory=dict)

    def to_dict(self) -> Dict[str, Any]:
        """转换为字典（可序列化）"""
        return {
            "snapshot_id": self.snapshot_id,
            "timestamp": self.timestamp.isoformat(),
            "state_data": self.state_data,
            "version": self.version,
            "metadata": self.metadata
        }

    @classmethod
    def from_dict(cls, data: Dict[str, Any]) -> 'StateSnapshot':
        """从字典创建实例"""
        return cls(
            snapshot_id=data["snapshot_id"],
            timestamp=datetime.fromisoformat(data["timestamp"]),
            state_data=data["state_data"],
            version=data.get("version", 0),
            metadata=data.get("metadata", {})
        )


@dataclass
class StateChangeRecord:
    """状态变更记录"""
    record_id: str = field(default_factory=lambda: str(uuid.uuid4()))
    timestamp: datetime = field(default_factory=datetime.now)
    key: str = ""
    old_value: Any = None
    new_value: Any = None
    change_type: str = "set"  # set, delete, batch_update
    source: str = ""  # 变更来源标识

    def to_dict(self) -> Dict[str, Any]:
        """转换为字典"""
        return {
            "record_id": self.record_id,
            "timestamp": self.timestamp.isoformat(),
            "key": self.key,
            "old_value": self.old_value,
            "new_value": self.new_value,
            "change_type": self.change_type,
            "source": self.source
        }


class StateManager:
    """
    智能体状态管理器

    提供集中式的状态管理能力，包括：
    - 键值对状态存储和查询
    - 类型安全的get/set操作
    - 状态变更订阅和通知
    - 历史记录和审计追踪
    - 快照创建和恢复
    - 批量更新和事务支持

    使用示例:
        manager = StateManager()

        # 订阅状态变更
        manager.subscribe(lambda key, old_val, new_val: print(f"{key}: {old_val} -> {new_val}"))

        # 设置和获取状态
        await manager.set("user.name", "Alice")
        name = await manager.get("user.name")

        # 创建快照
        snapshot = await manager.snapshot()

        # 恢复到之前的状态
        await manager.restore(snapshot)
    """

    def __init__(
        self,
        max_history_size: int = 1000,
        enable_persistence: bool = False,
        persistence_path: Optional[str] = None
    ):
        """
        初始化状态管理器

        Args:
            max_history_size: 最大历史记录数
            enable_persistence: 是否启用持久化
            persistence_path: 持久化文件路径
        """
        self._state: Dict[str, Any] = {}
        self._state_history: List[StateChangeRecord] = []
        self._listeners: List[Callable[[str, Any, Any], None]] = []
        self._max_history_size: int = max_history_size
        self._enable_persistence: bool = enable_persistence
        self._persistence_path: Optional[str] = persistence_path
        self._snapshots: List[StateSnapshot] = []
        self._version: int = 0

        logger.info(
            f"StateManager initialized (history_size={max_history_size}, "
            f"persistence={enable_persistence})"
        )

    async def get(self, key: str, default: T = None, expected_type: Optional[Type[T]] = None) -> Union[T, Any]:
        """
        获取状态值

        Args:
            key: 状态键
            default: 默认值
            expected_type: 期望的类型（可选，用于类型检查）

        Returns:
            状态值，如果不存在则返回默认值
        """
        value = self._state.get(key, default)

        if value is not None and expected_type is not None:
            if not isinstance(value, expected_type):
                logger.warning(
                    f"Type mismatch for key '{key}': "
                    f"expected {expected_type.__name__}, got {type(value).__name__}"
                )

        return value

    async def get_typed(self, key: str, type_hint: Type[T]) -> Optional[T]:
        """
        获取带类型提示的状态值

        Args:
            key: 状态键
            type_hint: 类型提示

        Returns:
            类型化的状态值，如果不存在或类型不匹配则返回None
        """
        value = self._state.get(key)
        if value is None:
            return None

        if isinstance(value, type_hint):
            return value
        else:
            logger.warning(f"Type mismatch for key '{key}'")
            return None

    async def set(self, key: str, value: Any, source: str = "") -> bool:
        """
        设置状态值并通知监听者

        Args:
            key: 状态键
            value: 状态值
            source: 变更来源标识

        Returns:
            是否成功设置
        """
        old_value = self._state.get(key)

        # 记录变更
        record = StateChangeRecord(
            key=key,
            old_value=copy.deepcopy(old_value),
            new_value=copy.deepcopy(value),
            change_type="set",
            source=source
        )

        # 更新状态
        self._state[key] = value
        self._version += 1

        # 记录历史
        self._record_change(record)

        # 通知监听者
        await self._notify_listeners(key, old_value, value)

        # 可选：持久化
        if self._enable_persistence:
            await self._persist_state()

        logger.debug(f"State updated: {key} = {value}")
        return True

    async def delete(self, key: str, source: str = "") -> bool:
        """
        删除状态值

        Args:
            key: 状态键
            source: 变更来源

        Returns:
            是否成功删除
        """
        if key not in self._state:
            return False

        old_value = self._state.pop(key)

        # 记录删除
        record = StateChangeRecord(
            key=key,
            old_value=copy.deepcopy(old_value),
            new_value=None,
            change_type="delete",
            source=source
        )

        self._version += 1
        self._record_change(record)
        await self._notify_listeners(key, old_value, None)

        if self._enable_persistence:
            await self._persist_state()

        logger.debug(f"State deleted: {key}")
        return True

    async def get_all_states(self) -> Dict[str, Any]:
        """
        获取所有状态的副本

        Returns:
            所有状态的浅拷贝
        """
        return dict(self._state)

    async def get_keys_by_pattern(self, pattern: str) -> List[str]:
        """
        根据模式匹配获取键列表（简单前缀匹配）

        Args:
            pattern: 键模式（支持 * 通配符）

        Returns:
            匹配的键列表
        """
        import fnmatch
        matched_keys = []
        for key in self._state.keys():
            if fnmatch.fnmatch(key, pattern):
                matched_keys.append(key)
        return matched_keys

    async def update_batch(self, updates: Dict[str, Any], source: str = "") -> int:
        """
        批量更新状态

        Args:
            updates: 键值对字典
            source: 变更来源

        Returns:
            成功更新的数量
        """
        count = 0
        for key, value in updates.items():
            success = await self.set(key, value, source=f"batch:{source}")
            if success:
                count += 1
        return count

    async def clear(self, source: str = "") -> int:
        """
        清空所有状态

        Args:
            source: 操作来源

        Returns:
            清除的条目数量
        """
        keys_to_delete = list(self._state.keys())
        count = 0
        for key in keys_to_delete:
            if await self.delete(key, source=source):
                count += 1
        return count

    async def subscribe(self, callback: Callable[[str, Any, Any], None]) -> None:
        """
        订阅状态变更

        Args:
            callback: 回调函数 (key, old_value, new_value) -> None
        """
        self._listeners.append(callback)
        logger.debug(f"State subscriber added (total: {len(self._listeners)})")

    async def unsubscribe(self, callback: Callable[[str, Any, Any], None]) -> bool:
        """
        取消订阅

        Returns:
            是否成功移除
        """
        try:
            self._listeners.remove(callback)
            return True
        except ValueError:
            return False

    async def snapshot(self, metadata: Optional[Dict[str, Any]] = None) -> StateSnapshot:
        """
        创建当前状态的快照

        Args:
            metadata: 附加元数据

        Returns:
            状态快照对象
        """
        snapshot = StateSnapshot(
            state_data=copy.deepcopy(self._state),
            version=self._version,
            metadata=metadata or {}
        )

        self._snapshots.append(snapshot)
        logger.info(
            f"State snapshot created (id={snapshot.snapshot_id}, "
            f"keys={len(snapshot.state_data)}, version={self._version})"
        )

        return snapshot

    async def restore(self, snapshot: StateSnapshot) -> bool:
        """
        恢复到指定快照状态

        Args:
            snapshot: 要恢复的快照

        Returns:
            是否成功恢复
        """
        try:
            # 验证快照版本
            if snapshot.version > self._version:
                logger.warning(
                    f"Restoring from future snapshot (snapshot_v={snapshot.version}, "
                    f"current_v={self._version})"
                )

            # 备份当前状态
            old_state = dict(self._state)

            # 恢复状态
            self._state = copy.deepcopy(snapshot.state_data)
            self._version = snapshot.version

            # 记录恢复操作
            record = StateChangeRecord(
                key="*",
                old_value=old_state,
                new_value=dict(self._state),
                change_type="restore",
                source="snapshot_restore"
            )
            self._record_change(record)

            # 通知所有监听者
            for key in list(old_state.keys()) + list(self._state.keys()):
                old_val = old_state.get(key)
                new_val = self._state.get(key)
                if old_val != new_val:
                    await self._notify_listeners(key, old_val, new_val)

            logger.info(
                f"State restored from snapshot {snapshot.snapshot_id}"
            )
            return True

        except Exception as e:
            logger.error(f"Failed to restore state: {e}")
            return False

    async def get_snapshot_history(self, limit: int = 10) -> List[StateSnapshot]:
        """
        获取快照历史

        Args:
            limit: 最大返回数量

        Returns:
            快照列表（按时间倒序）
        """
        return list(reversed(self._snapshots[-limit:]))

    async def get_change_history(
        self,
        key_filter: Optional[str] = None,
        limit: int = 50
    ) -> List[StateChangeRecord]:
        """
        获取变更历史

        Args:
            key_filter: 键过滤模式（可选）
            limit: 最大返回数量

        Returns:
            变更记录列表（按时间倒序）
        """
        history = reversed(self._state_history[-limit:])

        if key_filter:
            import fnmatch
            history = [
                record for record in history
                if fnmatch.fnmatch(record.key, key_filter)
            ]

        return list(history)

    def get_stats(self) -> Dict[str, Any]:
        """
        获取统计信息

        Returns:
            统计数据字典
        """
        return {
            "total_keys": len(self._state),
            "total_changes": len(self._state_history),
            "current_version": self._version,
            "total_snapshots": len(self._snapshots),
            "total_subscribers": len(self._listeners),
            "persistence_enabled": self._enable_persistence,
            "memory_usage_bytes": len(str(self._state))
        }

    async def export_state(self) -> str:
        """
        导出当前状态为JSON字符串

        Returns:
            JSON格式的状态数据
        """
        data = {
            "version": self._version,
            "timestamp": datetime.now().isoformat(),
            "state": self._state,
            "stats": self.get_stats()
        }
        return json.dumps(data, indent=2, ensure_ascii=False, default=str)

    async def import_state(self, json_str: str) -> bool:
        """
        从JSON字符串导入状态

        Args:
            json_str: JSON格式状态数据

        Returns:
            是否成功导入
        """
        try:
            data = json.loads(json_str)

            # 先创建快照作为备份
            await self.snapshot(metadata={"reason": "pre_import_backup"})

            # 导入新状态
            imported_state = data.get("state", {})
            count = await self.update_batch(imported_state, source="import")

            logger.info(f"Imported {count} state entries")
            return True

        except Exception as e:
            logger.error(f"Failed to import state: {e}")
            return False

    async def _notify_listeners(self, key: str, old_value: Any, new_value: Any) -> None:
        """通知所有监听者"""
        for listener in self._listeners:
            try:
                if hasattr(listener, '__call__'):
                    result = listener(key, old_value, new_value)
                    if asyncio.iscoroutine(result):
                        await result
            except Exception as e:
                logger.error(f"State listener error: {e}")

    def _record_change(self, record: StateChangeRecord) -> None:
        """记录变更到历史"""
        self._state_history.append(record)

        if len(self._state_history) > self._max_history_size:
            self._state_history = self._state_history[-self._max_history_size:]

    async def _persist_state(self) -> None:
        """持久化状态到文件"""
        if not self._persistence_path:
            return

        try:
            json_data = await self.export_state()
            with open(self._persistence_path, 'w', encoding='utf-8') as f:
                f.write(json_data)
        except Exception as e:
            logger.error(f"Failed to persist state: {e}")


__all__ = [
    "StateManager",
    "StateSnapshot",
    "StateChangeRecord",
]
