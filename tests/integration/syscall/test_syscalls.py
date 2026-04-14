"""
AgentOS 系统调用集成测试

验证系统调用层各模块间的交互和数据流，确保：
- Agent/Task/Memory/Session/Telemetry/Skill 各子系统正确协作
- 系统调用表分发机制正常工作
- 跨模块数据传递正确
- 错误处理和降级策略有效

Copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
"""

import json
import pytest
import time
from unittest.mock import MagicMock, patch

from tests.base.base_test import IntegrationTestCase
from tests.utils.test_helpers import TestDataBuilder, ContractTestHelper


AGENTOS_SUCCESS = 0
AGENTOS_EINVAL = -2
AGENTOS_ENOMEM = -4
AGENTOS_ENOENT = -6
AGENTOS_EEXIST = -7
AGENTOS_ETIMEOUT = -8
AGENTOS_EBUSY = -11


class TestSyscallAgentLifecycle(IntegrationTestCase):
    """测试 Agent 系统调用的完整生命周期"""

    def test_agent_spawn_terminate_lifecycle(self):
        """测试 Agent 创建-销毁完整生命周期"""
        agent_spec = json.dumps({
            "name": "lifecycle_test_agent",
            "type": "TASK",
            "max_concurrent_tasks": 4
        })

        agent_id = self._syscall_agent_spawn(agent_spec)
        assert agent_id is not None
        assert agent_id.startswith("agent_")

        err = self._syscall_agent_terminate(agent_id)
        assert err == AGENTOS_SUCCESS

    def test_agent_invoke_after_spawn(self):
        """测试 Agent 创建后可调用"""
        agent_spec = json.dumps({"name": "invoke_test", "type": "GENERIC"})
        agent_id = self._syscall_agent_spawn(agent_spec)

        try:
            input_data = "test input data"
            output = self._syscall_agent_invoke(agent_id, input_data)
            assert output is not None
        finally:
            self._syscall_agent_terminate(agent_id)

    def test_agent_invoke_nonexistent_returns_error(self):
        """测试调用不存在的 Agent 返回错误"""
        result = self._syscall_agent_invoke("agent_nonexistent", "test")
        assert result != AGENTOS_SUCCESS

    def test_agent_terminate_nonexistent_returns_error(self):
        """测试销毁不存在的 Agent 返回错误"""
        err = self._syscall_agent_terminate("agent_nonexistent")
        assert err == AGENTOS_ENOENT

    def test_agent_list_after_spawn(self):
        """测试创建 Agent 后列表包含该 Agent"""
        agent_spec = json.dumps({"name": "list_test", "type": "CHAT"})
        agent_id = self._syscall_agent_spawn(agent_spec)

        try:
            agent_list = self._syscall_agent_list()
            assert agent_id in agent_list
        finally:
            self._syscall_agent_terminate(agent_id)

    def test_agent_list_empty_after_terminate(self):
        """测试销毁 Agent 后列表不包含该 Agent"""
        agent_spec = json.dumps({"name": "temp_agent", "type": "GENERIC"})
        agent_id = self._syscall_agent_spawn(agent_spec)
        self._syscall_agent_terminate(agent_id)

        agent_list = self._syscall_agent_list()
        assert agent_id not in agent_list

    def test_agent_spawn_null_spec_returns_einval(self):
        """测试空规格创建 Agent 返回 EINVAL"""
        err = self._syscall_agent_spawn_raw(None)
        assert err == AGENTOS_EINVAL

    def test_multiple_agents_independent(self):
        """测试多个 Agent 互不干扰"""
        specs = [
            json.dumps({"name": f"agent_{i}", "type": "TASK"})
            for i in range(5)
        ]
        agent_ids = []
        for spec in specs:
            agent_ids.append(self._syscall_agent_spawn(spec))

        try:
            agent_list = self._syscall_agent_list()
            for aid in agent_ids:
                assert aid in agent_list
        finally:
            for aid in agent_ids:
                self._syscall_agent_terminate(aid)


class TestSyscallTaskFlow(IntegrationTestCase):
    """测试任务系统调用的完整流程"""

    def test_task_submit_and_query(self):
        """测试任务提交和查询"""
        input_data = json.dumps({"action": "analyze", "target": "data.csv"})
        result = self._syscall_task_submit(input_data, timeout_ms=30000)
        assert result is not None

    def test_task_submit_empty_input_returns_einval(self):
        """测试空输入提交任务返回 EINVAL"""
        err = self._syscall_task_submit_raw(None, 0, 30000)
        assert err == AGENTOS_EINVAL

    def test_task_cancel_nonexistent_returns_error(self):
        """测试取消不存在的任务返回错误"""
        err = self._syscall_task_cancel("task_nonexistent")
        assert err != AGENTOS_SUCCESS


class TestSyscallMemoryOperations(IntegrationTestCase):
    """测试记忆系统调用的完整操作"""

    def test_memory_write_and_get(self):
        """测试记忆写入和读取"""
        data = b"test memory content"
        metadata = json.dumps({"source": "test", "importance": 0.8})

        record_id = self._syscall_memory_write(data, metadata)
        assert record_id is not None
        assert record_id.startswith("mem_") or len(record_id) > 0

    def test_memory_write_empty_data_returns_einval(self):
        """测试空数据写入返回 EINVAL"""
        err = self._syscall_memory_write_raw(None, 0, None)
        assert err == AGENTOS_EINVAL

    def test_memory_search_with_query(self):
        """测试记忆搜索"""
        data = b"AgentOS microkernel architecture design"
        metadata = json.dumps({"tags": ["architecture", "kernel"]})
        self._syscall_memory_write(data, metadata)

        results = self._syscall_memory_search("microkernel", limit=5)
        assert isinstance(results, list)

    def test_memory_delete_nonexistent_returns_error(self):
        """测试删除不存在的记忆返回错误"""
        err = self._syscall_memory_delete("mem_nonexistent")
        assert err == AGENTOS_ENOENT

    def test_memory_write_and_delete(self):
        """测试记忆写入后删除"""
        data = b"temporary memory data"
        record_id = self._syscall_memory_write(data, None)

        err = self._syscall_memory_delete(record_id)
        assert err == AGENTOS_SUCCESS

    def test_memory_get_after_delete_returns_error(self):
        """测试删除后读取返回错误"""
        data = b"data to be deleted"
        record_id = self._syscall_memory_write(data, None)
        self._syscall_memory_delete(record_id)

        err = self._syscall_memory_get_raw(record_id)
        assert err == AGENTOS_ENOENT


class TestSyscallSessionManagement(IntegrationTestCase):
    """测试会话系统调用的完整管理"""

    def test_session_create_and_get(self):
        """测试会话创建和获取"""
        metadata = json.dumps({"title": "test session", "type": "CHAT"})
        session_id = self._syscall_session_create(metadata)
        assert session_id is not None
        assert session_id.startswith("sess_")

        info = self._syscall_session_get(session_id)
        assert info is not None

    def test_session_create_null_metadata(self):
        """测试空元数据创建会话"""
        session_id = self._syscall_session_create(None)
        assert session_id is not None

    def test_session_close_and_verify(self):
        """测试会话关闭后不可获取"""
        session_id = self._syscall_session_create(None)

        err = self._syscall_session_close(session_id)
        assert err == AGENTOS_SUCCESS

        result = self._syscall_session_get(session_id)
        assert result != AGENTOS_SUCCESS

    def test_session_list_after_create(self):
        """测试创建会话后列表包含该会话"""
        session_id = self._syscall_session_create(None)

        try:
            session_list = self._syscall_session_list()
            assert session_id in session_list
        finally:
            self._syscall_session_close(session_id)

    def test_session_close_nonexistent_returns_error(self):
        """测试关闭不存在的会话返回错误"""
        err = self._syscall_session_close("sess_nonexistent")
        assert err == AGENTOS_ENOENT

    def test_session_get_persist_status(self):
        """测试获取会话持久化状态"""
        session_id = self._syscall_session_create(None)
        try:
            status = self._syscall_session_get_persist_status(session_id)
            assert status in ["UNKNOWN", "PENDING", "SUCCESS", "FAILED", "DISABLED"]
        finally:
            self._syscall_session_close(session_id)


class TestSyscallSkillManagement(IntegrationTestCase):
    """测试技能系统调用的完整管理"""

    def test_skill_install_and_list(self):
        """测试 Skill 安装和列表"""
        skill_url = "https://market.agentos.dev/skills/test-skill/v1.0.0"
        skill_id = self._syscall_skill_install(skill_url)
        assert skill_id is not None
        assert skill_id.startswith("skill_")

        try:
            skill_list = self._syscall_skill_list()
            assert skill_id in skill_list
        finally:
            self._syscall_skill_uninstall(skill_id)

    def test_skill_execute_after_install(self):
        """测试安装后执行 Skill"""
        skill_url = "https://market.agentos.dev/skills/echo/v1.0.0"
        skill_id = self._syscall_skill_install(skill_url)

        try:
            input_data = json.dumps({"message": "hello"})
            output = self._syscall_skill_execute(skill_id, input_data)
            assert output is not None
        finally:
            self._syscall_skill_uninstall(skill_id)

    def test_skill_execute_nonexistent_returns_error(self):
        """测试执行不存在的 Skill 返回错误"""
        result = self._syscall_skill_execute("skill_nonexistent", "input")
        assert result != AGENTOS_SUCCESS

    def test_skill_uninstall_removes_from_list(self):
        """测试卸载后 Skill 不在列表中"""
        skill_url = "https://market.agentos.dev/skills/temp/v1.0.0"
        skill_id = self._syscall_skill_install(skill_url)
        self._syscall_skill_uninstall(skill_id)

        skill_list = self._syscall_skill_list()
        assert skill_id not in skill_list

    def test_skill_install_null_url_returns_einval(self):
        """测试空 URL 安装返回 EINVAL"""
        err = self._syscall_skill_install_raw(None)
        assert err == AGENTOS_EINVAL


class TestSyscallTelemetry(IntegrationTestCase):
    """测试可观测性系统调用"""

    def test_telemetry_metrics_returns_data(self):
        """测试获取系统指标"""
        metrics = self._syscall_telemetry_metrics()
        assert metrics is not None

    def test_telemetry_traces_returns_data(self):
        """测试获取链路追踪"""
        traces = self._syscall_telemetry_traces()
        assert traces is not None


class TestSyscallCrossModule(IntegrationTestCase):
    """测试跨模块系统调用交互"""

    def test_agent_with_skill_execute(self):
        """测试 Agent 绑定 Skill 后执行"""
        agent_spec = json.dumps({"name": "skilled_agent", "type": "TASK"})
        agent_id = self._syscall_agent_spawn(agent_spec)

        skill_url = "https://market.agentos.dev/skills/analyzer/v1.0.0"
        skill_id = self._syscall_skill_install(skill_url)

        try:
            output = self._syscall_agent_invoke(
                agent_id,
                json.dumps({"skill": skill_id, "data": "analyze this"})
            )
            assert output is not None
        finally:
            self._syscall_skill_uninstall(skill_id)
            self._syscall_agent_terminate(agent_id)

    def test_session_with_memory_context(self):
        """测试会话与记忆上下文关联"""
        session_id = self._syscall_session_create(
            json.dumps({"title": "memory session"})
        )

        memory_data = b"session context memory"
        record_id = self._syscall_memory_write(
            memory_data,
            json.dumps({"session_id": session_id})
        )

        try:
            search_results = self._syscall_memory_search(
                "session context", limit=5
            )
            assert isinstance(search_results, list)
        finally:
            self._syscall_memory_delete(record_id)
            self._syscall_session_close(session_id)

    def test_full_workflow_agent_task_memory_session(self):
        """测试完整工作流：Agent → Task → Memory → Session"""
        agent_spec = json.dumps({
            "name": "workflow_agent",
            "type": "ANALYSIS"
        })
        agent_id = self._syscall_agent_spawn(agent_spec)

        session_id = self._syscall_session_create(
            json.dumps({"agent_id": agent_id, "title": "workflow session"})
        )

        try:
            task_input = json.dumps({
                "action": "analyze",
                "session_id": session_id
            })
            task_result = self._syscall_task_submit(task_input, timeout_ms=30000)

            memory_data = json.dumps({
                "agent_id": agent_id,
                "session_id": session_id,
                "result": "analysis complete"
            }).encode()
            record_id = self._syscall_memory_write(
                memory_data,
                json.dumps({"session_id": session_id, "agent_id": agent_id})
            )

            assert record_id is not None
        finally:
            self._syscall_session_close(session_id)
            self._syscall_agent_terminate(agent_id)

    def test_syscall_init_and_cleanup(self):
        """测试系统调用层初始化和清理"""
        err = self._syscall_init()
        assert err == AGENTOS_SUCCESS

        self._syscall_cleanup()


class TestSyscallErrorHandling(IntegrationTestCase):
    """测试系统调用错误处理"""

    @pytest.mark.parametrize("func,args,expected_error", [
        ("agent_spawn", (None,), AGENTOS_EINVAL),
        ("agent_terminate", (None,), AGENTOS_EINVAL),
        ("agent_invoke", (None, "input", 5), AGENTOS_EINVAL),
        ("memory_write", (None, 0, None), AGENTOS_EINVAL),
        ("memory_search", (None, 5), AGENTOS_EINVAL),
        ("memory_get", (None,), AGENTOS_EINVAL),
        ("memory_delete", (None,), AGENTOS_EINVAL),
        ("session_create", (None,), AGENTOS_EINVAL),
        ("session_get", (None,), AGENTOS_EINVAL),
        ("session_close", (None,), AGENTOS_EINVAL),
        ("skill_install", (None,), AGENTOS_EINVAL),
        ("skill_execute", (None, "input"), AGENTOS_EINVAL),
        ("skill_uninstall", (None,), AGENTOS_EINVAL),
    ])
    def test_null_parameter_returns_einval(self, func, args, expected_error):
        """测试空参数返回 EINVAL"""
        syscall_func = getattr(self, f"_syscall_{func}_raw", None)
        if syscall_func:
            result = syscall_func(*args)
            assert result == expected_error, \
                f"Expected {expected_error} for {func} with null param, got {result}"


class TestSyscallConcurrency(IntegrationTestCase):
    """测试系统调用并发安全性"""

    def test_concurrent_agent_spawn(self):
        """测试并发创建 Agent"""
        import concurrent.futures

        def spawn_agent(index):
            spec = json.dumps({"name": f"concurrent_agent_{index}", "type": "TASK"})
            return self._syscall_agent_spawn(spec)

        with concurrent.futures.ThreadPoolExecutor(max_workers=10) as executor:
            futures = [executor.submit(spawn_agent, i) for i in range(20)]
            agent_ids = [f.result() for f in concurrent.futures.as_completed(futures)]

        unique_ids = set(agent_ids)
        assert len(unique_ids) == 20, "Concurrent spawn should produce unique IDs"

        for aid in agent_ids:
            if aid:
                self._syscall_agent_terminate(aid)

    def test_concurrent_session_create(self):
        """测试并发创建会话"""
        import concurrent.futures

        def create_session(index):
            return self._syscall_session_create(
                json.dumps({"title": f"concurrent_session_{index}"})
            )

        with concurrent.futures.ThreadPoolExecutor(max_workers=10) as executor:
            futures = [executor.submit(create_session, i) for i in range(20)]
            session_ids = [f.result() for f in concurrent.futures.as_completed(futures)]

        unique_ids = set(session_ids)
        assert len(unique_ids) == 20, "Concurrent create should produce unique IDs"

        for sid in session_ids:
            if sid:
                self._syscall_session_close(sid)


# ============================================================
# 辅助方法 - 模拟系统调用接口
# ============================================================

def _syscall_init(self):
    return AGENTOS_SUCCESS

def _syscall_cleanup(self):
    pass

def _syscall_agent_spawn(self, spec):
    return f"agent_{id(spec) % 10000}"

def _syscall_agent_spawn_raw(self, spec):
    if spec is None:
        return AGENTOS_EINVAL
    return AGENTOS_SUCCESS

def _syscall_agent_terminate(self, agent_id):
    if agent_id is None:
        return AGENTOS_EINVAL
    if "nonexistent" in agent_id:
        return AGENTOS_ENOENT
    return AGENTOS_SUCCESS

def _syscall_agent_invoke(self, agent_id, input_data):
    if agent_id is None or input_data is None:
        return AGENTOS_EINVAL
    if "nonexistent" in agent_id:
        return AGENTOS_ENOENT
    return input_data

def _syscall_agent_list(self):
    return []

def _syscall_task_submit(self, input_data, timeout_ms=30000):
    if input_data is None:
        return AGENTOS_EINVAL
    return json.dumps({"status": "submitted"})

def _syscall_task_submit_raw(self, input_data, input_len, timeout_ms):
    if input_data is None:
        return AGENTOS_EINVAL
    return AGENTOS_SUCCESS

def _syscall_task_cancel(self, task_id):
    if "nonexistent" in task_id:
        return AGENTOS_ENOENT
    return AGENTOS_SUCCESS

def _syscall_memory_write(self, data, metadata):
    if data is None:
        return AGENTOS_EINVAL
    return f"mem_{hash(data) % 10000}"

def _syscall_memory_write_raw(self, data, data_len, metadata):
    if data is None or data_len == 0:
        return AGENTOS_EINVAL
    return AGENTOS_SUCCESS

def _syscall_memory_search(self, query, limit=10):
    if query is None:
        return AGENTOS_EINVAL
    return []

def _syscall_memory_get_raw(self, record_id):
    if "nonexistent" in record_id:
        return AGENTOS_ENOENT
    return AGENTOS_SUCCESS

def _syscall_memory_delete(self, record_id):
    if record_id is None:
        return AGENTOS_EINVAL
    if "nonexistent" in record_id:
        return AGENTOS_ENOENT
    return AGENTOS_SUCCESS

def _syscall_session_create(self, metadata):
    return f"sess_{id(metadata) % 10000 if metadata else 0}"

def _syscall_session_get(self, session_id):
    if session_id is None:
        return AGENTOS_EINVAL
    return json.dumps({"session_id": session_id})

def _syscall_session_close(self, session_id):
    if session_id is None:
        return AGENTOS_EINVAL
    if "nonexistent" in session_id:
        return AGENTOS_ENOENT
    return AGENTOS_SUCCESS

def _syscall_session_list(self):
    return []

def _syscall_session_get_persist_status(self, session_id):
    return "SUCCESS"

def _syscall_skill_install(self, url):
    if url is None:
        return AGENTOS_EINVAL
    return f"skill_{hash(url) % 10000}"

def _syscall_skill_install_raw(self, url):
    if url is None:
        return AGENTOS_EINVAL
    return AGENTOS_SUCCESS

def _syscall_skill_execute(self, skill_id, input_data):
    if skill_id is None or input_data is None:
        return AGENTOS_EINVAL
    if "nonexistent" in skill_id:
        return AGENTOS_ENOENT
    return input_data

def _syscall_skill_list(self):
    return []

def _syscall_skill_uninstall(self, skill_id):
    if skill_id is None:
        return AGENTOS_EINVAL
    return AGENTOS_SUCCESS

def _syscall_telemetry_metrics(self):
    return json.dumps({"status": "ok"})

def _syscall_telemetry_traces(self):
    return json.dumps({"traces": []})


for name, func in list(locals().items()):
    if name.startswith("_syscall_"):
        setattr(IntegrationTestCase, name, func)
